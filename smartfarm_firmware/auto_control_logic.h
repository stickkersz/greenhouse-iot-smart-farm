// ── Auto Control decision math — pure, ไม่แตะ Arduino/hardware เลย ──
// แยกจาก autoControl() ใน smartfarm_firmware.ino เพื่อทดสอบด้วย g++ ธรรมดา
// (ไม่ต้องใช้ Arduino toolchain) — ดู tests/auto_control_logic.test.cpp
//
// โมเดล 2026-07-14 (v1.7.0 — เพิ่ม evaporative cooling: ปั๊มช่วยพัดลมลดอุณหภูมิ):
//
//   พัดลม (CH3) ← อุณหภูมิอากาศ
//       ร้อน (≥ fanOnTemp)  → เปิด · เย็น (≤ fanOffTemp) → ปิด · ระหว่างกลาง → คงสถานะ
//
//   ปั๊มน้ำ (CH4) ← ทำงาน 2 หน้าที่ (เปิดถ้าหน้าที่ใดหน้าที่หนึ่งต้องการ):
//       1) เพิ่มความชื้น: อากาศแห้ง (< pumpOnHum) เปิด · ชื้น (≥ pumpOffHum) ปิด
//       2) ช่วยระบายความร้อน (evaporative): อากาศร้อน (≥ fanOnTemp) เปิด · เย็น (≤ fanOffTemp) ปิด
//          — แต่ข้ามถ้าอากาศชื้นแล้ว (≥ pumpOffHum) เพราะพ่นน้ำในอากาศอิ่มตัว = ไม่เย็นลง แค่ท่วม
//       เซนเซอร์ความชื้นตาย (humidity == 0) → ปิดทั้ง 2 หน้าที่เสมอ (safety)
//
// แต่ละหน้าที่เป็น latch (hysteresis) แยกกัน กันรีเลย์กระพริบ + กันค้างเปิดเมื่ออีกหน้าที่เลิกต้องการ
// stateless ล้วนไม่ได้ (OR ของ 2 hysteresis loop จะค้าง/กระพริบ) จึงส่ง latch ปัจจุบันเข้ามาแล้วคืนค่าใหม่
// เงื่อนไข config ที่ถูกต้อง (ผู้เรียกตรวจก่อน): fanOnTemp > fanOffTemp, pumpOffHum > pumpOnHum
#ifndef AUTO_CONTROL_LOGIC_H
#define AUTO_CONTROL_LOGIC_H

struct AutoControlInputs {
  float airTemp;      // ค่าเฉลี่ยอุณหภูมิอากาศ (°C) — คุมพัดลม + ช่วยคุมปั๊ม (cooling)
  float airHumidity;  // ค่าเฉลี่ยความชื้นอากาศ (%RH) — คุมปั๊ม · 0 = เซนเซอร์ตาย
  float fanOnTemp;    // ร้อน ≥ ค่านี้ → พัดลมเปิด + ปั๊มช่วยระบายความร้อน
  float fanOffTemp;   // เย็น ≤ ค่านี้ → พัดลมปิด + เลิกช่วยระบายความร้อน
  float pumpOnHum;    // ปั๊มเปิดเพิ่มความชื้นเมื่อ < ค่านี้
  float pumpOffHum;   // ปั๊มปิด (เพิ่มความชื้น) เมื่อ ≥ ค่านี้ · และเป็นเพดานกันปั๊ม cooling ในอากาศชื้น
  bool  pumpHumidifyOn;    // latch ปัจจุบัน: ปั๊มกำลังเปิดเพื่อเพิ่มความชื้นอยู่ไหม
  bool  pumpCoolAssistOn;  // latch ปัจจุบัน: ปั๊มกำลังเปิดเพื่อช่วยระบายความร้อนอยู่ไหม
};

struct AutoControlDecisions {
  bool fanOpen, fanClose;            // คำสั่งพัดลม (คู่กับสถานะรีเลย์ปัจจุบัน)
  bool pumpHumidifyOn, pumpCoolAssistOn;  // latch ใหม่ (firmware เก็บกลับไปใช้รอบหน้า)
  bool pumpOn;                       // สถานะปั๊มที่ต้องการ = humidify OR cool
};

inline AutoControlDecisions computeAutoDecisions(const AutoControlInputs& in) {
  AutoControlDecisions d;
  const bool dead = (in.airHumidity == 0);   // เซนเซอร์ความชื้นตาย

  // พัดลม — อุณหภูมิอากาศ (dead-zone ระหว่าง off..on = ทั้งคู่ false = คงสถานะ)
  d.fanOpen  = (in.airTemp >= in.fanOnTemp);
  d.fanClose = (in.airTemp <= in.fanOffTemp);

  // ปั๊ม หน้าที่ 1: เพิ่มความชื้น (latch hysteresis pumpOnHum/pumpOffHum)
  bool hum = in.pumpHumidifyOn;
  if      (dead)                          hum = false;
  else if (in.airHumidity <  in.pumpOnHum)  hum = true;
  else if (in.airHumidity >= in.pumpOffHum) hum = false;
  // else: คงสถานะ

  // ปั๊ม หน้าที่ 2: ช่วยระบายความร้อน (latch hysteresis fanOnTemp/fanOffTemp + humid gate)
  bool cool = in.pumpCoolAssistOn;
  if      (dead || in.airHumidity >= in.pumpOffHum) cool = false;  // ตาย/อากาศชื้นเกิน → ไม่ช่วย (พ่นแล้วไม่เย็น)
  else if (in.airTemp >= in.fanOnTemp)              cool = true;
  else if (in.airTemp <= in.fanOffTemp)             cool = false;
  // else: คงสถานะ

  d.pumpHumidifyOn   = hum;
  d.pumpCoolAssistOn = cool;
  d.pumpOn           = hum || cool;
  return d;
}

#endif
