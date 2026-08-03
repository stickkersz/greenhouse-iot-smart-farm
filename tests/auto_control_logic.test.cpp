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
// {sensorOk, airTemp, airHumidity, fanOnTemp, fanOffTemp, pumpOnHum, pumpOffHum,
//  hotLatch, dryLatch, pumpHeatLatch, ventThreshold, wetLatch}
//
// ⚠️ ventThreshold = 0 เขียนไว้ "ชัดๆ" เจตนา — 0 = ปิดฟีเจอร์ vent = พฤติกรรมก่อน v2.7.0
// เดิมละไว้ให้ zero-init เอง (initializer 10 ค่า) ซึ่งได้ผลเหมือนกันแต่มี 2 ปัญหา:
//   1. -Wmissing-field-initializers เตือน 4 จุด = noise กลบ warning จริงที่อาจโผล่มาทีหลัง
//   2. อ่านโค้ดแล้วไม่รู้ว่าเทสต์นี้ vent เปิดหรือปิด ต้องไปนับลำดับ field ในสตรักต์เอง
//      → เคยทำให้เข้าใจผิดมาแล้วว่า sweep 2 ก้อนที่ "หน้าตาเหมือนกัน" ทดสอบ config เดียวกัน
//        ทั้งที่ความยาว initializer ต่างกัน = คนละ config คนละเรื่อง
// เขียนครบทุก field = อ่านออกทันทีว่า config ไหน ไม่ต้องรู้กฎ zero-init ของ C++
static AutoControlInputs base() {
  return {true, 30, 65, 35, 32, 60, 75, false, false, false, 0, false};   // 30°C ชื้น 65% (ไม่ร้อน ไม่แห้ง) latch ปิดหมด · vent ปิด
}

// ── Crossing harness ──────────────────────────────────
// ป้อน "ลำดับ" ค่าเซนเซอร์ต่อเนื่อง ป้อน latch กลับทุกรอบเหมือน firmware จริง แล้วนับครั้งที่รีเลย์สลับ
// (fixed-point sweep จับ chatter ไม่ได้ เพราะมันป้อนค่าเดิมซ้ำ ไม่เคยข้ามเส้น — บั๊ก v1.8.0 รอบแรกหลุดตรงนี้)
struct SwitchCount { int fan, pump; };

static SwitchCount runSequence(AutoControlInputs in, const float* temps, const float* hums, int n) {
  SwitchCount c{0, 0};
  bool prevFan = false, prevPump = false, first = true;
  for (int i = 0; i < n; i++) {
    in.airTemp = temps[i];
    in.airHumidity = hums[i];
    AutoControlDecisions d = computeAutoDecisions(in);
    in.hotOn = d.hotOn; in.dryOn = d.dryOn; in.pumpHeatOn = d.pumpHeatOn; in.wetOn = d.wetOn;   // firmware เก็บ latch ข้ามรอบ
    if (!first) {
      if (d.fanOn  != prevFan)  c.fan++;
      if (d.pumpOn != prevPump) c.pump++;
    }
    prevFan = d.fanOn; prevPump = d.pumpOn; first = false;
  }
  return c;
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

  printf("== Safety v2.0.0: เซนเซอร์เชื่อไม่ได้ -> ปิดทุกช่อง ล้าง latch ==\n");
  {
    // เคสที่ v1.8.0 พลาด: เซนเซอร์เสียตอนอากาศร้อนจัด — ค่าเก่าบอก 40°C แต่เชื่อไม่ได้แล้ว
    auto in = base(); in.sensorOk = false; in.airTemp = 40; in.airHumidity = 45;
    in.hotOn = true; in.dryOn = true; in.pumpHeatOn = true;   // latch เปิดค้างทั้งหมด
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "sensorOk=false -> ปิดทั้ง 2 ช่อง (แม้ค่าเก่าบอกร้อน+แห้ง)");
    check(!d.hotOn && !d.dryOn && !d.pumpHeatOn, "sensorOk=false -> ล้าง latch ทุกตัว (ฟื้นแล้วคิดใหม่จากค่าสด)");
  }
  {
    // 0%RH เป็นไปไม่ได้ในโรงเรือนจริง = เซนเซอร์/สายมีปัญหา ถึงแม้ sensorOk ยังไม่ทันตก
    auto in = base(); in.airTemp = 40; in.airHumidity = 0;
    in.hotOn = true; in.pumpHeatOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.fanOn && !d.pumpOn, "humidity=0 -> ปิดทั้ง 2 ช่อง (0%RH เชื่อไม่ได้)");
  }
  {
    // เซนเซอร์ฟื้น: ต้องเริ่มจาก latch เปล่า ไม่สานต่อของเก่า — 34°C อยู่ dead-zone จึงยังไม่ปลุกพัดลม
    auto in = base(); in.sensorOk = false; in.airTemp = 40; in.airHumidity = 45; in.hotOn = true;
    auto dead = computeAutoDecisions(in);
    in.sensorOk = true; in.airTemp = 34; in.airHumidity = 65;
    in.hotOn = dead.hotOn; in.dryOn = dead.dryOn; in.pumpHeatOn = dead.pumpHeatOn;
    auto back = computeAutoDecisions(in);
    check(!back.fanOn, "ฟื้นที่ 34°C (dead-zone) -> ยังไม่เปิด ต้องรอข้าม 35 จริง (latch ไม่สานต่อ)");
  }
  {
    // เซนเซอร์ฟื้นแล้วร้อนจริง -> กลับมาทำงานได้ ไม่ค้างดับถาวร
    auto in = base(); in.sensorOk = true; in.airTemp = 37; in.airHumidity = 50;
    auto d = computeAutoDecisions(in);
    check(d.fanOn && d.pumpOn, "ฟื้นแล้วร้อน 37 + แห้ง 50 -> กลับมาเปิดทั้งคู่ (ไม่ค้างดับ)");
  }

  printf("== จุดคงที่: sweep temp x humidity ที่ค่าคงที่ latch ต้องนิ่ง ==\n");
  printf("   (หมายเหตุ: sweep นี้ *ไม่* พิสูจน์ว่าไม่กระพริบ — ค่าไม่เคยข้ามเส้น ดูหมวด Crossing ด้านล่าง)\n");
  {
    // ป้อน latch กลับเข้าไป — จุดคงที่ต้องไม่พลิก ทั้ง fan และ pump · ทุก latch เริ่มต้น 8 แบบ
    bool unstable = false;
    for (int seed = 0; seed < 8; seed++) {
      for (float t = 20; t <= 45; t += 1.0f) {
        for (float h = 10; h <= 100; h += 5.0f) {
          AutoControlInputs in = {true, t, h, 35, 32, 60, 75,
                                  (seed & 1) != 0, (seed & 2) != 0, (seed & 4) != 0,
                                  0, false};   // vent ปิด (ventThreshold=0) — ทดสอบ path ก่อน v2.7.0
          auto a = computeAutoDecisions(in);
          in.hotOn = a.hotOn; in.dryOn = a.dryOn; in.pumpHeatOn = a.pumpHeatOn;
          auto b = computeAutoDecisions(in);   // รอบ 2 ด้วย latch ที่นิ่งแล้ว
          if (b.fanOn != a.fanOn || b.pumpOn != a.pumpOn) unstable = true;
        }
      }
    }
    check(!unstable, "fan+pump ถึงจุดคงที่ทุก (temp,humidity) จาก latch เริ่มต้นทั้ง 8 แบบ");
  }

  printf("== Crossing: เซนเซอร์แกว่งข้ามเส้น ต้องไม่ทำรีเลย์กระพริบ (regression v1.8.0) ==\n");
  {
    // บั๊กจริงที่หลุดรอบแรก: latch ร้อนติดอยู่, temp ค้างใน dead-zone, RH แกว่งรอบ pumpOffHum=75
    // gate แบบ stateless ให้ 5 สวิตช์/6 รอบ · gate ที่อยู่ใน latch ต้องให้ ≤1 (ตัดครั้งเดียวแล้วนิ่ง)
    auto in = base(); in.hotOn = true; in.pumpHeatOn = true;
    const float t[] = {33, 33, 33, 33, 33, 33};
    const float h[] = {74.5f, 75.2f, 74.8f, 75.1f, 74.6f, 75.3f};
    auto c = runSequence(in, t, h, 6);
    printf("       (ปั๊มสลับจริง %d ครั้ง / 6 รอบ — บั๊กเดิมได้ 5)\n", c.pump);
    check(c.pump <= 1, "RH แกว่งรอบ 75 ตอน latch ร้อนติด -> ปั๊มสลับ ≤1 ครั้ง");
    check(c.fan == 0,  "RH แกว่งรอบ 75 -> พัดลมไม่สลับเลย (ร้อนอยู่ ต้องเปิดค้าง)");
  }
  {
    // RH แกว่งรอบ pumpOnHum=60 (เส้นเปิดปั๊ม) — hysteresis 60..75 ต้องกลืนไว้
    auto in = base(); in.airTemp = 30;
    const float t[] = {30, 30, 30, 30, 30, 30};
    const float h[] = {59.5f, 60.4f, 59.6f, 60.3f, 59.7f, 60.5f};
    auto c = runSequence(in, t, h, 6);
    check(c.pump <= 1, "RH แกว่งรอบ 60 -> ปั๊มสลับ ≤1 ครั้ง (ติดแล้วค้างจนถึง 75)");
    check(c.fan  <= 1, "RH แกว่งรอบ 60 -> พัดลมสลับ ≤1 ครั้ง");
  }
  {
    // temp แกว่งรอบ fanOnTemp=35 — hysteresis 32..35 ต้องกลืนไว้
    auto in = base(); in.airHumidity = 65;
    const float t[] = {34.6f, 35.3f, 34.7f, 35.2f, 34.8f, 35.4f};
    const float h[] = {65, 65, 65, 65, 65, 65};
    auto c = runSequence(in, t, h, 6);
    check(c.fan  <= 1, "temp แกว่งรอบ 35 -> พัดลมสลับ ≤1 ครั้ง");
    check(c.pump <= 1, "temp แกว่งรอบ 35 -> ปั๊มสลับ ≤1 ครั้ง");
  }
  {
    // แกว่งทั้ง 2 แกนพร้อมกันรอบเส้นของตัวเอง — worst case จริงหน้างาน
    auto in = base(); in.hotOn = true; in.pumpHeatOn = true;
    const float t[] = {34.8f, 35.2f, 34.6f, 35.3f, 34.7f, 35.1f, 34.9f, 35.2f};
    const float h[] = {74.7f, 75.2f, 74.5f, 75.3f, 74.8f, 75.1f, 74.6f, 75.4f};
    auto c = runSequence(in, t, h, 8);
    check(c.pump <= 1, "แกว่งทั้ง temp และ RH รอบเส้น -> ปั๊มสลับ ≤1 ครั้ง");
    check(c.fan  == 0, "แกว่งทั้ง 2 แกน -> พัดลมเปิดค้าง ไม่สลับ");
  }
  {
    // ปั๊มไล่ร้อนโดน gate ล้างแล้ว ต้องไม่ติดใหม่จนกว่า temp จะข้าม 35 อีกครั้ง (แม้ RH ตกกลับมา)
    auto in = base(); in.hotOn = true; in.pumpHeatOn = true;
    const float t[] = {33, 33, 33, 36};     // ชื้นตัดปั๊ม -> RH ตกกลับ (ยังไม่ติด) -> temp ข้าม 35 (ติดใหม่)
    const float h[] = {76, 70, 70, 70};
    AutoControlInputs s = in;
    bool pumpAfter[4];
    for (int i = 0; i < 4; i++) {
      s.airTemp = t[i]; s.airHumidity = h[i];
      auto d = computeAutoDecisions(s);
      s.hotOn = d.hotOn; s.dryOn = d.dryOn; s.pumpHeatOn = d.pumpHeatOn;
      pumpAfter[i] = d.pumpOn;
    }
    check(!pumpAfter[0], "ชื้น 76 -> ปั๊มตัด");
    check(!pumpAfter[1] && !pumpAfter[2], "RH ตกกลับมา 70 แต่ temp ยัง 33 -> ปั๊มยังไม่ติด (ต้องรอ temp ข้าม 35)");
    check(pumpAfter[3], "temp ขึ้น 36 -> ปั๊มติดใหม่ (re-arm ได้จริง ไม่ค้างดับถาวร)");
  }

  printf("== v2.7.0 vent: พัดลมไล่ความชื้นเมื่อ RH สูง (ปั๊มไม่แตะ) · ventOn=80 ventOff=75 ==\n");
  {
    // ฝนตก: ไม่ร้อน ไม่แห้ง แต่ RH พุ่งเกิน vent -> พัดลมเปิดไล่ความชื้น ปั๊มปิด
    auto in = base(); in.ventThreshold = 80;
    in.airTemp = 30; in.airHumidity = 82;
    auto d = computeAutoDecisions(in);
    check(d.wetOn && d.fanOn, "RH 82 ≥ vent 80 -> wet latch เปิด, พัดลมเปิดไล่ความชื้น");
    check(!d.pumpOn, "RH 82 -> ปั๊มปิด (vent คุมพัดลมเท่านั้น + humid gate)");
  }
  {
    // dead-zone 75..80: latch คงสถานะ (hysteresis กันกระพริบ)
    auto in = base(); in.ventThreshold = 80; in.airTemp = 30; in.airHumidity = 77;
    in.wetOn = true;
    check(computeAutoDecisions(in).wetOn, "RH 77 dead-zone, wet เปิด -> คงเปิด");
    in.wetOn = false;
    check(!computeAutoDecisions(in).wetOn, "RH 77 dead-zone, wet ปิด -> คงปิด");
  }
  {
    auto in = base(); in.ventThreshold = 80; in.airTemp = 30; in.airHumidity = 74;
    in.wetOn = true;
    check(!computeAutoDecisions(in).wetOn, "RH 74 ≤ ventOff 75 -> wet latch ปิด");
  }
  {
    // ปิดฟีเจอร์ (ventThreshold=0 จาก base) -> RH สูงแค่ไหนก็ไม่เปิด wet (backward compatible กับเทสต์/โค้ดเก่า)
    auto in = base(); in.airTemp = 30; in.airHumidity = 95;
    check(!computeAutoDecisions(in).wetOn && !computeAutoDecisions(in).fanOn,
          "ventThreshold=0 (ปิดฟีเจอร์) -> RH 95 ไม่มี wet, พัดลมไม่เปิดจาก vent");
  }
  {
    // user ปิดฟีเจอร์ "ขณะพัดลมไล่ชื้นอยู่" — wet ต้องถูกล้าง ไม่ใช่ค้างเปิดถาวร
    // (ครอบ branch `ventOn <= 0 -> wet = false` ที่ auto_control_logic.h:106
    //  ซึ่งเทสต์ข้างบนไม่แตะเพราะ base() ให้ wetOn=false มาแล้ว — ลบบรรทัดนั้นทิ้งก็ยัง PASS)
    auto in = base(); in.airTemp = 30; in.airHumidity = 95; in.wetOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.wetOn && !d.fanOn, "ตั้ง humidity_vent=0 ขณะ wet ติดอยู่ที่ RH 95 -> ล้าง latch + พัดลมปิด");
  }
  {
    // vent ไม่แตะ latch อื่น: ร้อน+ชื้นเกิน -> พัดลมเปิด (ร้อน OR wet), ปั๊มปิด (humid gate เดิม)
    auto in = base(); in.ventThreshold = 80; in.airTemp = 37; in.airHumidity = 85;
    auto d = computeAutoDecisions(in);
    check(d.fanOn && d.wetOn && d.hotOn, "ร้อน+ชื้นเกิน+vent -> พัดลมเปิด, wet+hot ติดทั้งคู่");
    check(!d.pumpOn, "ร้อน+ชื้นเกิน -> ปั๊มยังปิด (vent ไม่ปลุกปั๊ม)");
  }
  {
    // crossing: RH แกว่งรอบ ventOn=80 (ไม่ตกใต้ ventOff=75) -> พัดลมสลับ ≤1
    auto in = base(); in.ventThreshold = 80; in.airTemp = 30;
    const float t[] = {30, 30, 30, 30, 30, 30};
    const float h[] = {79.5f, 80.4f, 79.6f, 80.3f, 79.7f, 80.5f};
    auto c = runSequence(in, t, h, 6);
    check(c.fan <= 1, "RH แกว่งรอบ 80 -> พัดลมสลับ ≤1 ครั้ง (hysteresis กลืน)");
    check(c.pump == 0, "RH แกว่งรอบ 80 -> ปั๊มไม่สลับเลย (vent ไม่แตะปั๊ม)");
  }
  {
    // sensor เสีย ต้องล้าง wet latch ด้วย
    auto in = base(); in.ventThreshold = 80; in.sensorOk = false;
    in.airTemp = 30; in.airHumidity = 90; in.wetOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.wetOn && !d.fanOn, "sensorOk=false -> ล้าง wet latch + พัดลมปิด");
  }

  {
    // เดิมอันตราย: ventThreshold เล็กเกิน -> ventOff แตะ 0 -> wet ค้างเปิดถาวร (RH ไม่มีทาง ≤0)
    // ตอนนี้ clamp อยู่ใน computeAutoDecisions() แล้ว สถานะนั้นสร้างไม่ได้อีก — ปิดฟีเจอร์ทิ้งเลย
    auto in = base(); in.ventThreshold = 4; in.airTemp = 30; in.airHumidity = 30; in.wetOn = true;
    check(!computeAutoDecisions(in).wetOn, "ventThreshold=4 (เล็กเกิน) -> clamp ปิดฟีเจอร์ + ล้าง wet (ไม่ค้าง)");
  }
  {
    // guard ต้อง "เลือกได้จริง" ไม่ใช่จริงเสมอ/เท็จเสมอ — เช็คตรงเส้น hmax + VENT_HYST
    // (เทสต์ sweep ด้านล่างใช้ ventIsUsable() ตัวเดียวกันนี้ ถ้า guard เพี้ยน sweep จะกลายเป็น vacuous)
    check(!ventIsUsable(4,  75), "ventIsUsable: 4 (≤VENT_HYST) -> false");
    check(!ventIsUsable(76, 75), "ventIsUsable: gap 1 < VENT_HYST -> false (ช่องที่ปั๊มตีกับ vent)");
    check(!ventIsUsable(79, 75), "ventIsUsable: gap 4 < VENT_HYST -> false");
    check( ventIsUsable(80, 75), "ventIsUsable: gap 5 พอดี -> true (เส้นที่ preset ทั้ง 3 ตัวอยู่)");
    check( ventIsUsable(90, 75), "ventIsUsable: gap 15 -> true");
    check( ventIsUsable(90, 85), "ventIsUsable: preset summer (85/90) -> true");
  }

  printf("== v2.7.0: invariant pumpOn -> fanOn ยังครบเมื่อเปิด vent (sweep) ==\n");
  {
    bool violated = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 16; latch++) {   // 4 latch = 16 combo
          AutoControlInputs in = {true, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0,
                                  80, (latch & 8) != 0};   // vent 80 = pumpOffHum 75 + VENT_HYST
          auto d = computeAutoDecisions(in);
          if (d.pumpOn && !d.fanOn) violated = true;
        }
      }
    }
    check(!violated, "vent เปิด: ทุก state pumpOn -> fanOn (vent เพิ่มแค่ fan ไม่แตะ pump)");
  }

  printf("== vent ไม่ตีกับปั๊ม: เว้น vent จาก hmax ≥ VENT_HYST แล้ว ห้ามมี state ที่ wet+pump ติดพร้อมกัน ==\n");
  {
    // wet ค้างได้ทั้งช่วง [ventOff, ventOn) แต่ปั๊มถูกบล็อกเฉพาะตอน RH ≥ hmax (saturated)
    // ถ้า ventOff < hmax จะเหลื่อมกัน = พัดลมไล่ชื้นออกขณะปั๊มพ่นเข้า · ต้อง ventOn ≥ hmax + VENT_HYST
    // guarded อ่านจาก ventIsUsable() ตัวจริงที่ firmware ใช้ — ไม่ใช่เขียน clamp ซ้ำในเทสต์
    // (ถ้าเขียนซ้ำ: จูน VENT_HYST เป็น 3 แล้วเทสต์ยังผ่านทั้งที่ firmware เพี้ยน)
    // grid หยาบพอครอบทุกจุดตัด threshold แต่ไม่ระเบิดเวลารัน (เดิม 1% ทุกแกน = ~8 วินาที)
    bool conflict = false, conflictUnguarded = false;
    for (float hmax = 40; hmax <= 90; hmax += 5.0f) {
      for (float vent = 10; vent <= 100; vent += 1.0f) {
        const bool guarded = ventIsUsable(vent, hmax);
        for (float t = 20; t <= 45; t += 2.5f) {
          for (float h = 0.5f; h <= 100; h += 1.0f) {
            for (int latch = 0; latch < 16; latch++) {
              AutoControlInputs in = {true, t, h, 35, 32, hmax - 15, hmax,
                                      (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0,
                                      vent, (latch & 8) != 0};
              auto d = computeAutoDecisions(in);
              if (d.wetOn && d.pumpOn) { if (guarded) conflict = true; else conflictUnguarded = true; }
            }
          }
        }
      }
    }
    check(!conflict, "vent ≥ hmax + VENT_HYST -> ไม่มี state ใดที่ปั๊มพ่นขณะพัดลมไล่ชื้น");
    // clamp อยู่ใน computeAutoDecisions() แล้ว config ที่เว้นไม่ถึงจึงไม่มี wet เลย -> ตีกันไม่ได้ตั้งแต่ต้น
    // (ก่อน refactor clamp อยู่ที่ .ino สภาพตีกันสร้างได้จริง — นั่นคือบั๊กที่ทำให้ต้องย้ายมาตรงนี้)
    check(!conflictUnguarded, "config ที่เว้นไม่ถึง VENT_HYST -> clamp ปิด vent ทิ้ง ไม่เหลือ state ตีกัน");
    // ความ "ไม่ vacuous" พิสูจน์ที่ ventIsUsable() ด้านบนแทน (guard เลือกได้จริงตรงเส้น)
  }

  printf("== sensorOk=false: ไม่มีชุดค่า/latch ใดปลุกรีเลย์ได้เลย (sweep ทั้งกริด) ==\n");
  {
    bool leaked = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 8; latch++) {
          AutoControlInputs in = {false, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0,
                                  0, false};   // vent ปิด — sensorOk=false ต้องปิดหมดอยู่แล้วไม่ว่า vent จะตั้งไว้เท่าไร
          auto d = computeAutoDecisions(in);
          if (d.fanOn || d.pumpOn || d.hotOn || d.dryOn || d.pumpHeatOn) leaked = true;
        }
      }
    }
    check(!leaked, "sensorOk=false ทุก (temp,humidity,latch) -> รีเลย์ปิด + latch ล้างหมด");
  }

  printf("== พัดลมเปิดเสมอเมื่อปั๊มเปิด (ปั๊มไม่มีทางทำงานลำพัง) ==\n");
  // ⚠️ อย่ายุบรวมกับ sweep "invariant ... เมื่อเปิด vent" ด้านบน — ก้อนนั้นตั้ง ventThreshold=80 เสมอ
  // (wet ติดใหม่ได้เองทุกครั้งที่ h ข้าม 80) จึงไม่เคยผ่าน branch ventOn <= 0 เลย
  // ก้อนนี้ initializer 10 ค่า -> ventThreshold zero-init = 0 = ฟีเจอร์ vent ปิด = path backward-compat
  // (deployment ที่ humidity_vent=0) ซึ่งเป็น config เดียวที่ครอบคลุมได้จากตรงนี้เท่านั้น
  {
    bool violated = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 8; latch++) {   // latch 3 ตัว = 8 combo (รวม combo ที่หลุด sync)
          AutoControlInputs in = {true, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0,
                                  0, false};   // vent ปิด — คู่กับ sweep "vent เปิด" ด้านบน (คนละ config เจตนา)
          auto d = computeAutoDecisions(in);
          if (d.pumpOn && !d.fanOn) violated = true;   // พ่นน้ำโดยไม่มีลม = ท่วม ไม่ระเหย
        }
      }
    }
    check(!violated, "vent ปิด (ventThreshold=0): ทุก state pumpOn -> fanOn (ไม่มีพ่นน้ำโดยพัดลมดับ)");
  }

  // ── v2.9.0: lockIsActive() — cooldown lock ทนต่อ millis() วนรอบ ──────
  // ตัวจับเวลา cooldown เดิมไม่มีเทสต์เลย (code review v2.8.0 จับได้) และเดิมเทียบ millis() ตรงๆ
  // ซึ่งพังเงียบๆ ที่ ~49.7 วัน · เคสวนรอบเป็นชนิดที่อ่านโค้ดเปล่าๆ แล้วมองไม่เห็น จึงต้องมีเทสต์
  printf("\n== v2.9.0: lockIsActive() — cooldown lock ทนต่อ millis() วนรอบ ==\n");
  {
    const uint32_t COOLDOWN = 5UL * 60 * 1000;   // 5 นาที เท่า PUMP_COOLDOWN_MS/FAN_COOLDOWN_MS

    // ── เคสปกติ (ไม่เกี่ยววนรอบ) ──
    check(!lockIsActive(1000, 0),                 "lockUntil = 0 (ไม่เคยตั้งล็อก) -> ไม่ล็อก");
    check( lockIsActive(1000, 1000 + COOLDOWN),   "เพิ่งตั้งล็อก -> ล็อกอยู่");
    check( lockIsActive(1000 + COOLDOWN - 1, 1000 + COOLDOWN), "ก่อนหมดอายุ 1ms -> ยังล็อก");
    check(!lockIsActive(1000 + COOLDOWN,     1000 + COOLDOWN), "ถึงเวลาพอดี -> ปลดล็อก");
    check(!lockIsActive(1000 + COOLDOWN + 1, 1000 + COOLDOWN), "หลังหมดอายุ -> ปลดล็อก");

    // ── lockUntil == 0 ตอน now เลย 2^31 (~24.8 วัน) ──
    // นี่คือเหตุผลที่ต้องกัน lockUntil==0 แยก: (int32_t)(now - 0) ติดลบ = "ล็อกอยู่" ทั้งที่ไม่เคยล็อก
    // ถ้าไม่กัน = พัดลม+ปั๊มไม่เคยเปิดเลยหลังบอร์ดรันครบ 24.8 วัน
    check(!lockIsActive(0x90000000u, 0), "now > 2^31 แต่ lockUntil = 0 -> ยังต้องไม่ล็อก");

    // ── เคสวนรอบจริง: ตั้งล็อกก่อนวนรอบ deadline ตกไปหลังวนรอบ ──
    // now ใกล้ 2^32 → now + COOLDOWN ล้นไปเป็นเลขเล็ก · โค้ดเดิม (millis() < lockUntil) จะเห็น
    // "millis() ใหญ่กว่า lockUntil" = ปลดล็อกทันที = ปั๊มไม่ได้พักจริงตามที่ safety สั่ง
    {
      uint32_t nowBefore = 0xFFFFFFFFu - 1000;          // เหลืออีก 1 วิ ก่อนวนรอบ
      uint32_t until     = nowBefore + COOLDOWN;        // ล้น → กลายเป็นเลขเล็ก
      check(until < nowBefore, "sanity: deadline ล้นจริง (until < now)");
      check( lockIsActive(nowBefore, until),            "ตั้งล็อกคาบวนรอบ -> ล็อกอยู่ (เดิมปลดทันที = บั๊ก)");
      check( lockIsActive(500, until),                  "หลังวนรอบ ยังไม่ถึง deadline -> ยังล็อก");
      check(!lockIsActive(until, until),                "หลังวนรอบ ถึง deadline พอดี -> ปลดล็อก");
      check(!lockIsActive(until + 1000, until),         "หลังวนรอบ เลย deadline -> ปลดล็อก");
    }

    // ── เคสวนรอบอีกทาง: ล็อกค้างอยู่แล้ว millis() วนรอบกลับไปเริ่มที่ 0 ──
    // อันตรายที่สุดของโค้ดเดิม: millis() เล็กกว่า lockUntil ไปอีก ~49 วัน = ล็อกปิด 2 ช่องยาว 49 วัน
    {
      uint32_t until = 0xFFFFFFFFu - 100;   // deadline อยู่ก่อนวนรอบเล็กน้อย
      check(!lockIsActive(1000, until),
            "millis() วนรอบไปแล้ว deadline อยู่ในอดีต -> ปลดล็อก (เดิมล็อกค้าง 49 วัน = บั๊ก)");
    }

    // ── ข้อจำกัดที่รู้ตัว: deadline เก่าเกิน 24.8 วัน จะวนกลับมาอ่านว่า "ล็อกอยู่" ──
    // เทสต์นี้ "ยืนยันข้อจำกัด" ไม่ใช่ยืนยันความถูกต้อง — จึงต้องมี pumpSafetyCheck() ล้าง lockUntil
    // เป็น 0 ทุกรอบเมื่อหมดอายุ ถ้าใครถอดการล้างนั้นออก บั๊กนี้จะกลับมา
    check(lockIsActive(0x90000000u, 1000),
          "ข้อจำกัด: deadline เก่าเกิน 24.8 วัน อ่านว่าล็อก -> ผู้เรียกต้องล้างเป็น 0 เมื่อหมดอายุ");
  }

  // ── v2.9.0: stepSafetyTimers() — max runtime 15 นาที + พักคู่กัน 5 นาที ──────
  // ตรรกะนี้เคยอยู่ใน pumpSafetyCheck() ของ .ino ทั้งก้อน = ไม่มีเทสต์เลย (code review v2.8.0 จับได้)
  // ทั้งที่มีบั๊กมาแล้ว 2 รอบ: false water alarm (พัดลม trigger แต่เตือนเรื่องน้ำ) และ lock desync
  printf("\n== v2.9.0: stepSafetyTimers() — max runtime + cooldown พักคู่กัน ==\n");
  {
    const uint32_t MAXRUN = 15UL * 60 * 1000;   // PUMP_MAX_RUNTIME_MS จริง
    const uint32_t COOL   =  5UL * 60 * 1000;   // PUMP_COOLDOWN_MS จริง

    // helper: ประกอบ inputs ให้อ่านง่าย
    auto mk = [&](uint32_t now, bool pumpOn, bool fanOn, bool pumpAuto, bool fanAuto) {
      SafetyTimerInputs in;
      in.now = now; in.pumpRelayOn = pumpOn; in.fanRelayOn = fanOn;
      in.pumpAuto = pumpAuto; in.fanAuto = fanAuto;
      in.maxRuntimeMs = MAXRUN; in.cooldownMs = COOL;
      return in;
    };
    const SafetyTimerState ZERO{0, 0, 0, 0};

    // ── เริ่มจับเวลา ──
    {
      auto r = stepSafetyTimers(mk(1000, true, true, true, true), ZERO);
      check(r.state.pumpOnSince == 1000 && r.state.fanOnSince == 1000, "รีเลย์เปิด+auto -> เริ่มจับเวลาทั้ง 2 ช่อง");
      check(!r.didCutoff, "เพิ่งเริ่มเดิน -> ยังไม่ตัด");
    }
    // ── manual ไม่จับเวลา = ไม่มีวันถูกตัด ──
    {
      auto r = stepSafetyTimers(mk(1000, true, true, false, false), ZERO);
      check(r.state.pumpOnSince == 0 && r.state.fanOnSince == 0, "manual ทั้ง 2 ช่อง -> ไม่จับเวลาเลย");
      auto r2 = stepSafetyTimers(mk(1000 + MAXRUN * 10, true, true, false, false), r.state);
      check(!r2.didCutoff, "manual เดินนานเท่าไรก็ไม่ถูกตัด (คนสั่งเองต้องคุมเอง)");
    }
    // ── รีเลย์ปิด -> รีเซ็ตตัวจับเวลา ──
    {
      SafetyTimerState st{1000, 1000, 0, 0};
      auto r = stepSafetyTimers(mk(5000, false, false, true, true), st);
      check(r.state.pumpOnSince == 0 && r.state.fanOnSince == 0, "รีเลย์ปิด -> รีเซ็ตตัวจับเวลาเป็น 0");
    }
    // ── ขอบ 15 นาที ──
    {
      SafetyTimerState st{1000, 0, 0, 0};
      auto before = stepSafetyTimers(mk(1000 + MAXRUN - 1, true, false, true, true), st);
      check(!before.didCutoff, "ปั๊มเดิน 15 นาที ขาด 1ms -> ยังไม่ตัด");
      auto at = stepSafetyTimers(mk(1000 + MAXRUN, true, false, true, true), st);
      check(at.didCutoff && at.pumpMaxed && !at.fanMaxed, "ปั๊มเดินครบ 15 นาที -> ตัด + pumpMaxed");
    }
    // ── ปั๊ม trigger: ล็อกทั้งคู่ + ตัดเฉพาะช่องที่เปิดอยู่ ──
    {
      SafetyTimerState st{1000, 0, 0, 0};              // ปั๊มเดินอยู่ พัดลมไม่ได้เดิน
      uint32_t now = 1000 + MAXRUN;
      auto r = stepSafetyTimers(mk(now, true, false, true, true), st);
      check(r.state.pumpLockUntil == now + COOL && r.state.fanLockUntil == now + COOL,
            "cutoff -> ล็อกทั้งปั๊มและพัดลม พร้อมกัน 5 นาที (พักคู่กัน)");
      check(r.cutPumpRelay && !r.cutFanRelay, "cutoff -> สั่งปิดเฉพาะรีเลย์ที่เปิดอยู่ (พัดลมปิดอยู่แล้ว)");
      check(r.state.pumpOnSince == 0 && r.state.fanOnSince == 0, "cutoff -> รีเซ็ตตัวจับเวลาทั้งคู่");
    }
    // ── พัดลม trigger: ต้องไม่ใช่ pumpMaxed (กัน false water alarm — บั๊ก v2.8.0) ──
    {
      SafetyTimerState st{0, 1000, 0, 0};              // พัดลมเดินอยู่ ปั๊มไม่ได้เดิน
      auto r = stepSafetyTimers(mk(1000 + MAXRUN, false, true, true, true), st);
      check(r.didCutoff && r.fanMaxed && !r.pumpMaxed,
            "พัดลมเป็นตัว trigger -> fanMaxed เท่านั้น (pumpMaxed=false = ไม่ปลุก water alarm)");
      check(r.cutFanRelay && !r.cutPumpRelay, "พัดลม trigger -> ปิดเฉพาะพัดลม (ปั๊มไม่ได้เดิน)");
    }
    // ── ⚠️ regression ของบั๊ก lock desync (review v2.8.0) ──
    // ปั๊มเป็น manual ตอน cutoff -> ต้องยัง "ตั้ง pumpLockUntil" ไว้ ไม่งั้นสลับกลับเป็น auto
    // กลางช่วงพักแล้วปั๊มเปิดได้ทันทีขณะพัดลมยังล็อก = ปั๊มพ่นน้ำโดยไม่มีพัดลม
    {
      SafetyTimerState st{0, 1000, 0, 0};              // พัดลม auto เดินครบ · ปั๊มเป็น manual
      uint32_t now = 1000 + MAXRUN;
      auto r = stepSafetyTimers(mk(now, false, true, /*pumpAuto=*/false, /*fanAuto=*/true), st);
      check(r.state.pumpLockUntil == now + COOL,
            "desync: ปั๊มเป็น manual ตอน cutoff -> ยังต้องตั้ง pumpLockUntil (กันสลับกลับ auto แล้วพ่นน้ำ)");
      check(!r.cutPumpRelay, "desync: ปั๊ม manual -> ห้ามสั่งปิดรีเลย์เขา (คนคุมเอง)");
      check(lockIsActive(now + 1000, r.state.pumpLockUntil),
            "desync: สลับปั๊มกลับเป็น auto กลางช่วงพัก -> ยังติดล็อกอยู่ เปิดไม่ได้");
    }
    // ── ล็อกหมดอายุแล้วถูกล้างเป็น 0 ──
    {
      SafetyTimerState st{0, 0, 1000 + COOL, 1000 + COOL};
      auto r = stepSafetyTimers(mk(1000 + COOL + 1, false, false, true, true), st);
      check(r.state.pumpLockUntil == 0 && r.state.fanLockUntil == 0,
            "ล็อกหมดอายุ -> ล้างเป็น 0 (กัน deadline เก่าวนกลับมาอ่านว่าล็อก)");
    }
    // ── ตัวจับเวลาเดินข้าม millis() วนรอบ ──
    {
      uint32_t start = 0xFFFFFFFFu - (MAXRUN / 2);   // เริ่มเดินก่อนวนรอบ
      SafetyTimerState st{start, 0, 0, 0};
      uint32_t now = start + MAXRUN;                 // ครบ 15 นาที "หลัง" วนรอบไปแล้ว
      check(now < start, "sanity: เวลาปัจจุบันวนรอบไปแล้วจริง");
      auto r = stepSafetyTimers(mk(now, true, false, true, true), st);
      check(r.didCutoff && r.pumpMaxed, "ตัวจับเวลาเดินคาบ millis() วนรอบ -> ยังตัดถูกเวลา");
    }
    // ── deadline ล้นเป็น 0 พอดี -> ต้องไม่กลายเป็น "ไม่ล็อก" ──
    {
      uint32_t now = 0u - COOL;                      // now + COOL ล้นเป็น 0 เป๊ะ
      SafetyTimerState st{now - MAXRUN, 0, 0, 0};
      auto r = stepSafetyTimers(mk(now, true, false, true, true), st);
      check(r.didCutoff, "sanity: cutoff เกิดขึ้นจริงในเคสนี้");
      check(r.state.pumpLockUntil != 0 && lockIsActive(now, r.state.pumpLockUntil),
            "deadline ล้นเป็น 0 พอดี -> เลื่อนเป็น 1 ไม่ให้อ่านว่า 'ไม่ล็อก'");
    }
  }

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "SOME FAILED", failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
