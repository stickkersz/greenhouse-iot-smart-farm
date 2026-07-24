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
// {sensorOk, airTemp, airHumidity, fanOnTemp, fanOffTemp, pumpOnHum, pumpOffHum, hotLatch, dryLatch, pumpHeatLatch}
static AutoControlInputs base() {
  return {true, 30, 65, 35, 32, 60, 75, false, false, false};   // 30°C ชื้น 65% (ไม่ร้อน ไม่แห้ง) latch ปิดหมด
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
                                  (seed & 1) != 0, (seed & 2) != 0, (seed & 4) != 0};
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
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75;
    in.airTemp = 30; in.airHumidity = 82;
    auto d = computeAutoDecisions(in);
    check(d.wetOn && d.fanOn, "RH 82 ≥ vent 80 -> wet latch เปิด, พัดลมเปิดไล่ความชื้น");
    check(!d.pumpOn, "RH 82 -> ปั๊มปิด (vent คุมพัดลมเท่านั้น + humid gate)");
  }
  {
    // dead-zone 75..80: latch คงสถานะ (hysteresis กันกระพริบ)
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75; in.airTemp = 30; in.airHumidity = 77;
    in.wetOn = true;
    check(computeAutoDecisions(in).wetOn, "RH 77 dead-zone, wet เปิด -> คงเปิด");
    in.wetOn = false;
    check(!computeAutoDecisions(in).wetOn, "RH 77 dead-zone, wet ปิด -> คงปิด");
  }
  {
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75; in.airTemp = 30; in.airHumidity = 74;
    in.wetOn = true;
    check(!computeAutoDecisions(in).wetOn, "RH 74 ≤ ventOff 75 -> wet latch ปิด");
  }
  {
    // ปิดฟีเจอร์ (ventOnHum=0 จาก base) -> RH สูงแค่ไหนก็ไม่เปิด wet (backward compatible กับเทสต์/โค้ดเก่า)
    auto in = base(); in.airTemp = 30; in.airHumidity = 95;
    check(!computeAutoDecisions(in).wetOn && !computeAutoDecisions(in).fanOn,
          "ventOnHum=0 (ปิดฟีเจอร์) -> RH 95 ไม่มี wet, พัดลมไม่เปิดจาก vent");
  }
  {
    // vent ไม่แตะ latch อื่น: ร้อน+ชื้นเกิน -> พัดลมเปิด (ร้อน OR wet), ปั๊มปิด (humid gate เดิม)
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75; in.airTemp = 37; in.airHumidity = 85;
    auto d = computeAutoDecisions(in);
    check(d.fanOn && d.wetOn && d.hotOn, "ร้อน+ชื้นเกิน+vent -> พัดลมเปิด, wet+hot ติดทั้งคู่");
    check(!d.pumpOn, "ร้อน+ชื้นเกิน -> ปั๊มยังปิด (vent ไม่ปลุกปั๊ม)");
  }
  {
    // crossing: RH แกว่งรอบ ventOn=80 (ไม่ตกใต้ ventOff=75) -> พัดลมสลับ ≤1
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75; in.airTemp = 30;
    const float t[] = {30, 30, 30, 30, 30, 30};
    const float h[] = {79.5f, 80.4f, 79.6f, 80.3f, 79.7f, 80.5f};
    auto c = runSequence(in, t, h, 6);
    check(c.fan <= 1, "RH แกว่งรอบ 80 -> พัดลมสลับ ≤1 ครั้ง (hysteresis กลืน)");
    check(c.pump == 0, "RH แกว่งรอบ 80 -> ปั๊มไม่สลับเลย (vent ไม่แตะปั๊ม)");
  }
  {
    // sensor เสีย ต้องล้าง wet latch ด้วย
    auto in = base(); in.ventOnHum = 80; in.ventOffHum = 75; in.sensorOk = false;
    in.airTemp = 30; in.airHumidity = 90; in.wetOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.wetOn && !d.fanOn, "sensorOk=false -> ล้าง wet latch + พัดลมปิด");
  }

  {
    // เอกสารอันตราย: ถ้า ventOff แตะ 0 (ventOn เล็กเกินไป) wet latch จะค้างเปิดถาวร — RH ไม่มีทาง ≤0
    // firmware กันด้วยการปิดฟีเจอร์เมื่อ thresh_hum_vent ≤ VENT_HYST (autoControl ตั้ง ventOn=0) — เทสต์นี้ยืนยันเหตุผล
    auto in = base(); in.ventOnHum = 4; in.ventOffHum = 0; in.airTemp = 30; in.airHumidity = 30; in.wetOn = true;
    check(computeAutoDecisions(in).wetOn, "ventOff=0: wet ค้างเปิดที่ RH 30 (เหตุผลที่ firmware clamp ventOn > VENT_HYST)");
  }

  printf("== v2.7.0: invariant pumpOn -> fanOn ยังครบเมื่อเปิด vent (sweep) ==\n");
  {
    bool violated = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 16; latch++) {   // 4 latch = 16 combo
          AutoControlInputs in = {true, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0,
                                  80, 75, (latch & 8) != 0};
          auto d = computeAutoDecisions(in);
          if (d.pumpOn && !d.fanOn) violated = true;
        }
      }
    }
    check(!violated, "vent เปิด: ทุก state pumpOn -> fanOn (vent เพิ่มแค่ fan ไม่แตะ pump)");
  }

  printf("== sensorOk=false: ไม่มีชุดค่า/latch ใดปลุกรีเลย์ได้เลย (sweep ทั้งกริด) ==\n");
  {
    bool leaked = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 8; latch++) {
          AutoControlInputs in = {false, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0};
          auto d = computeAutoDecisions(in);
          if (d.fanOn || d.pumpOn || d.hotOn || d.dryOn || d.pumpHeatOn) leaked = true;
        }
      }
    }
    check(!leaked, "sensorOk=false ทุก (temp,humidity,latch) -> รีเลย์ปิด + latch ล้างหมด");
  }

  printf("== พัดลมเปิดเสมอเมื่อปั๊มเปิด (ปั๊มไม่มีทางทำงานลำพัง) ==\n");
  {
    bool violated = false;
    for (float t = 20; t <= 45; t += 0.5f) {
      for (float h = 0; h <= 100; h += 2.5f) {
        for (int latch = 0; latch < 8; latch++) {   // latch 3 ตัว = 8 combo (รวม combo ที่หลุด sync)
          AutoControlInputs in = {true, t, h, 35, 32, 60, 75,
                                  (latch & 1) != 0, (latch & 2) != 0, (latch & 4) != 0};
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
