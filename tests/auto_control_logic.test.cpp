// ทดสอบ auto_control_logic.h ตรงๆ ด้วย g++ ธรรมดา — ไม่ต้องใช้ Arduino toolchain
// รัน: g++ -std=c++17 -I smartfarm_firmware tests/auto_control_logic.test.cpp -o /tmp/auto_control_test && /tmp/auto_control_test
#include <cstdio>
#include <cstdlib>
#include "../smartfarm_firmware/auto_control_logic.h"

static int failures = 0;

void check(bool cond, const char* name) {
  if (cond) { printf("  PASS  %s\n", name); }
  else      { printf("  FAIL  %s\n", name); failures++; }
}

// default จริง: fanOnTemp=35 fanOffTemp=32 pumpOnHum=60 pumpOffHum=75
// {airTemp, airHumidity, fanOnTemp, fanOffTemp, pumpOnHum, pumpOffHum, hotLatch, dryLatch}
static AutoControlInputs base() {
  return {30, 65, 35, 32, 60, 75, false, false};   // 30°C ชื้น 65% (ไม่ร้อน ไม่แห้ง) latch ปิดทั้งคู่
}

int main() {
  printf("== ตารางหลัก v1.8.0: พัดลม+ปั๊มเปิดคู่กัน ยกเว้นร้อน+ชื้นเกิน ==\n");
  {
    auto in = base(); in.airTemp = 37; in.airHumidity = 65;   // ร้อน + ไม่ชื้นเกิน
    auto d = computeAutoDecisions(in);
    check(d.fanOn && d.pumpOn, "ร้อน+ไม่ชื้น -> fan ON, pump ON");
  }
  {
    auto in = base(); in.airTemp = 30; in.airHumidity = 50;   // แห้ง + ไม่ร้อน
    auto d = computeAutoDecisions(in);
    check(d.fanOn && d.pumpOn, "แห้ง+ไม่ร้อน -> fan ON, pump ON (พัดลมช่วยเพิ่มความชื้น)");
  }
  {
    auto in = base(); in.airTemp = 30; in.airHumidity = 80;   // เย็น + ชื้น
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "เย็น+ชื้น -> fan OFF, pump OFF");
  }
  {
    auto in = base(); in.airTemp = 37; in.airHumidity = 80;   // ร้อน + ชื้นเกิน = เคสเดียวที่แยกกัน
    auto d = computeAutoDecisions(in);
    check(d.fanOn,  "ร้อน+ชื้นเกิน -> fan ON (ระบายความร้อนได้ฟรี)");
    check(!d.pumpOn, "ร้อน+ชื้นเกิน -> pump OFF (humid gate)");
  }

  printf("== latch ร้อน: hysteresis + dead-zone คงสถานะ · ไม่ผูกกับความชื้น ==\n");
  {
    auto in = base(); in.airTemp = 36;
    check(computeAutoDecisions(in).hotOn, "airTemp=36>=35 -> latch ร้อนเปิด");
    in.airTemp = 30;
    check(!computeAutoDecisions(in).hotOn, "airTemp=30<=32 -> latch ร้อนปิด");
  }
  {
    auto in = base(); in.airTemp = 33.5f; in.hotOn = true;
    check(computeAutoDecisions(in).hotOn, "airTemp=33.5 dead-zone, latch เปิด -> คงเปิด");
    in.hotOn = false;
    check(!computeAutoDecisions(in).hotOn, "airTemp=33.5 dead-zone, latch ปิด -> คงปิด");
  }
  {
    // humid gate ต้องไม่แตะ latch ร้อน ไม่งั้นพัดลมจะดับตอนร้อน+ชื้น
    auto in = base(); in.airTemp = 37; in.airHumidity = 90;
    check(computeAutoDecisions(in).hotOn, "ชื้น 90% ไม่ล้าง latch ร้อน (พัดลมยังต้องเปิด)");
  }

  printf("== latch แห้ง: hysteresis + dead-zone คงสถานะ ==\n");
  {
    auto in = base(); in.airHumidity = 50;
    check(computeAutoDecisions(in).dryOn, "humidity=50<60 -> latch แห้งเปิด");
    in.airHumidity = 78;
    check(!computeAutoDecisions(in).dryOn, "humidity=78>=75 -> latch แห้งปิด");
  }
  {
    auto in = base(); in.airHumidity = 65; in.dryOn = true;
    auto d = computeAutoDecisions(in);
    check(d.dryOn && d.fanOn && d.pumpOn, "humidity=65 dead-zone, latch เปิด -> คงเปิดทั้ง 2 ช่อง");
  }

  printf("== ไม่ค้างเปิด: เคสที่ stateless OR จะพลาด ==\n");
  {
    // ร้อนจบแล้ว ความชื้นอยู่กลาง dead-zone (65) — ทั้ง 2 ช่องต้องดับ
    auto in = base(); in.airTemp = 30; in.airHumidity = 65;
    in.hotOn = true; in.dryOn = false;
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "เย็นลงแล้ว + ชื้น 65 กลาง dead-zone -> ดับทั้งคู่ (ไม่ค้าง)");
  }
  {
    // แห้งจบแล้ว (ชื้นถึง 78) อุณหภูมิอยู่กลาง dead-zone (33) — latch ร้อนปิดอยู่ ต้องไม่ปลุกช่องไหน
    auto in = base(); in.airTemp = 33; in.airHumidity = 78;
    in.hotOn = false; in.dryOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "ชื้นพอแล้ว + temp 33 กลาง dead-zone -> ดับทั้งคู่ (ไม่ค้าง)");
  }

  printf("== ร้อน AND แห้ง -> latch เปิดทั้งคู่ ==\n");
  {
    auto in = base(); in.airTemp = 37; in.airHumidity = 45;
    auto d = computeAutoDecisions(in);
    check(d.hotOn && d.dryOn && d.fanOn && d.pumpOn, "ร้อน+แห้ง -> latch เปิดทั้งคู่, fan+pump ON");
  }
  {
    // แห้งมากต้อง override humid gate ไม่ได้ (แห้ง = ไม่มีทางชื้นเกิน) — sanity ว่า gate ไม่บล็อก humidify
    auto in = base(); in.airTemp = 40; in.airHumidity = 20;
    check(computeAutoDecisions(in).pumpOn, "ร้อนจัด+แห้งจัด -> pump ON (gate ไม่บล็อก)");
  }

  printf("== Safety: เซนเซอร์ความชื้นตาย (0) ==\n");
  {
    auto in = base(); in.airTemp = 40; in.airHumidity = 0;
    in.hotOn = true; in.dryOn = true;                  // แม้ latch เปิดค้างอยู่
    auto d = computeAutoDecisions(in);
    check(!d.pumpOn, "humidity=0 -> pump ปิดเสมอ (safety)");
    check(!d.dryOn,  "humidity=0 -> latch แห้งถูกล้าง (เชื่อค่าไม่ได้)");
    check(d.fanOn && d.hotOn, "humidity=0 -> พัดลมยังตามอุณหภูมิได้ (40>=35 เปิด)");
  }
  {
    auto in = base(); in.airTemp = 30; in.airHumidity = 0;
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "humidity=0 + ไม่ร้อน -> ดับทั้งคู่");
  }

  printf("== ไม่กระพริบ: sweep temp x humidity ทั้ง 2 ช่อง latch นิ่ง ==\n");
  {
    // ป้อน latch กลับเข้าไป — จุดคงที่ต้องไม่พลิก ทั้ง fan และ pump
    bool unstable = false;
    for (float t = 20; t <= 45; t += 1.0f) {
      for (float h = 10; h <= 100; h += 5.0f) {
        AutoControlInputs in = {t, h, 35, 32, 60, 75, false, false};
        auto a = computeAutoDecisions(in);
        in.hotOn = a.hotOn; in.dryOn = a.dryOn;
        auto b = computeAutoDecisions(in);   // รอบ 2 ด้วย latch ที่นิ่งแล้ว
        if (b.fanOn != a.fanOn || b.pumpOn != a.pumpOn) unstable = true;
      }
    }
    check(!unstable, "fan+pump ถึงจุดคงที่ทุก (temp,humidity)");
  }
  {
    // sweep เดิม แต่เริ่มจาก latch เปิดค้างทั้งคู่ (worst case ค้าง)
    bool unstable = false;
    for (float t = 20; t <= 45; t += 1.0f) {
      for (float h = 10; h <= 100; h += 5.0f) {
        AutoControlInputs in = {t, h, 35, 32, 60, 75, true, true};
        auto a = computeAutoDecisions(in);
        in.hotOn = a.hotOn; in.dryOn = a.dryOn;
        auto b = computeAutoDecisions(in);
        if (b.fanOn != a.fanOn || b.pumpOn != a.pumpOn) unstable = true;
      }
    }
    check(!unstable, "เริ่มจาก latch ค้างเปิด ก็ยังถึงจุดคงที่ทุก (temp,humidity)");
  }

  printf("== พัดลมเปิดเสมอเมื่อปั๊มเปิด (ปั๊มไม่มีทางทำงานลำพัง) ==\n");
  {
    bool violated = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 4; latch++) {
          AutoControlInputs in = {t, h, 35, 32, 60, 75, (latch & 1) != 0, (latch & 2) != 0};
          auto d = computeAutoDecisions(in);
          if (d.pumpOn && !d.fanOn) violated = true;   // พ่นน้ำโดยไม่มีลม = ท่วม ไม่ระเหย
        }
      }
    }
    check(!violated, "ทุก state: pumpOn -> fanOn (ไม่มีพ่นน้ำโดยพัดลมดับ)");
  }

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "SOME FAILED", failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
