// ── Auto Control decision math — pure, ไม่แตะ Arduino/hardware เลย ──
// แยกจาก autoControl() ใน smartfarm_firmware.ino เพื่อทดสอบด้วย g++ ธรรมดา
// (ไม่ต้องใช้ Arduino toolchain) — ดู tests/auto_control_logic.test.cpp
//
// โมเดล 2026-07-15 (v1.8.0 — พัดลม+ปั๊มทำงานคู่กันทั้งคุมอุณหภูมิและคุมความชื้น):
//
//   2 latch (hysteresis) อ่านจากเซนเซอร์อากาศ ใช้ร่วมกันทั้ง 2 ช่อง:
//       ร้อน (hotOn) ← อุณหภูมิ  | ≥ fanOnTemp เปิด · ≤ fanOffTemp ปิด · ระหว่างกลางคงสถานะ
//       แห้ง (dryOn) ← ความชื้น  | < pumpOnHum เปิด · ≥ pumpOffHum ปิด · ระหว่างกลางคงสถานะ
//
//   พัดลม (CH3, ดูดเข้า) = ร้อน OR แห้ง
//       ร้อน → ระบายความร้อน · แห้ง → ดูดอากาศนอกเข้ามา + กระจายละอองน้ำจากปั๊ม
//       ไม่มี humid gate — พัดลมต้องระบายความร้อนได้เสมอแม้อากาศชื้น (พัดลมไม่ได้พ่นน้ำ ไม่มีอะไรเสีย)
//
//   ปั๊มน้ำ (CH4) = (ร้อน AND อากาศยังไม่อิ่มตัว) OR แห้ง
//       humid gate: ร้อนแต่ชื้นแล้ว (≥ pumpOffHum) → ปั๊มไม่ช่วย เพราะพ่นน้ำในอากาศอิ่มตัว
//       = ไม่เย็นลง แค่ท่วม (พัดลมยังเปิดอยู่ — นี่คือเคสเดียวที่ 2 ช่องแยกกัน)
//       เซนเซอร์ความชื้นตาย (humidity == 0) → ปั๊มปิดเสมอ (safety) · พัดลมยังตามอุณหภูมิปกติ
//
// ทำไมต้อง latch: 2 ช่องนี้ต่างมี 2 hysteresis loop ต่อ 1 รีเลย์ — stateless OR ของ 2 loop
// จะค้างเปิด (loop นึงเลิกต้องการตอนอีก loop อยู่ dead-zone) หรือกระพริบ จึงส่ง latch ปัจจุบัน
// เข้ามาแล้วคืนค่าใหม่ให้ firmware เก็บข้ามรอบ — อย่าเปลี่ยนกลับเป็น stateless open/close
// เงื่อนไข config ที่ถูกต้อง (ผู้เรียกตรวจก่อน): fanOnTemp > fanOffTemp, pumpOffHum > pumpOnHum
#ifndef AUTO_CONTROL_LOGIC_H
#define AUTO_CONTROL_LOGIC_H

struct AutoControlInputs {
  float airTemp;      // ค่าเฉลี่ยอุณหภูมิอากาศ (°C) — คุม latch ร้อน
  float airHumidity;  // ค่าเฉลี่ยความชื้นอากาศ (%RH) — คุม latch แห้ง + humid gate ของปั๊ม · 0 = เซนเซอร์ตาย
  float fanOnTemp;    // ร้อน ≥ ค่านี้ → latch ร้อนเปิด
  float fanOffTemp;   // เย็น ≤ ค่านี้ → latch ร้อนปิด
  float pumpOnHum;    // แห้ง < ค่านี้ → latch แห้งเปิด
  float pumpOffHum;   // ชื้น ≥ ค่านี้ → latch แห้งปิด · และเป็นเพดาน humid gate ของปั๊ม
  bool  hotOn;        // latch ปัจจุบัน: อากาศร้อนอยู่ไหม
  bool  dryOn;        // latch ปัจจุบัน: อากาศแห้งอยู่ไหม
};

struct AutoControlDecisions {
  bool hotOn, dryOn;   // latch ใหม่ (firmware เก็บกลับไปใช้รอบหน้า)
  bool fanOn;          // สถานะพัดลมที่ต้องการ = ร้อน OR แห้ง
  bool pumpOn;         // สถานะปั๊มที่ต้องการ = (ร้อน AND ไม่อิ่มตัว) OR แห้ง
};

inline AutoControlDecisions computeAutoDecisions(const AutoControlInputs& in) {
  AutoControlDecisions d;
  const bool dead      = (in.airHumidity == 0);            // เซนเซอร์ความชื้นตาย
  const bool saturated = !dead && (in.airHumidity >= in.pumpOffHum);  // อากาศชื้นเกินจะพ่นน้ำ

  // latch ร้อน — อุณหภูมิอากาศ (ไม่ผูกกับความชื้น: พัดลมต้องระบายความร้อนได้เสมอ)
  bool hot = in.hotOn;
  if      (in.airTemp >= in.fanOnTemp)  hot = true;
  else if (in.airTemp <= in.fanOffTemp) hot = false;
  // else: คงสถานะ

  // latch แห้ง — ความชื้นอากาศ
  bool dry = in.dryOn;
  if      (dead)                            dry = false;
  else if (in.airHumidity <  in.pumpOnHum)  dry = true;
  else if (in.airHumidity >= in.pumpOffHum) dry = false;
  // else: คงสถานะ

  d.hotOn = hot;
  d.dryOn = dry;

  // พัดลม — ร้อนหรือแห้ง เปิดได้ทั้งคู่ (เซนเซอร์ความชื้นตายก็ยังตามอุณหภูมิได้)
  d.fanOn = hot || dry;

  // ปั๊ม — ช่วยระบายความร้อนเฉพาะตอนอากาศยังไม่อิ่มตัว · เพิ่มความชื้นตอนแห้ง · ตาย = ปิดเสมอ
  d.pumpOn = !dead && ((hot && !saturated) || dry);
  return d;
}

#endif
