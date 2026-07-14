// ── Auto Control decision math — pure, ไม่แตะ Arduino/hardware เลย ──
// แยกจาก autoControl() ใน smartfarm_firmware.ino เพื่อทดสอบด้วย g++ ธรรมดา
// (ไม่ต้องใช้ Arduino toolchain) — ดู tests/auto_control_logic.test.cpp
//
// โมเดล 2026-07-14 (rewrite ให้พนักงานเข้าใจง่าย — 1 อุปกรณ์ = 1 เซนเซอร์):
//
//   พัดลม (CH3) ← อุณหภูมิอากาศอย่างเดียว
//       อากาศร้อน (≥ fanOnTemp)  → เปิด
//       อากาศเย็น (≤ fanOffTemp) → ปิด
//       ระหว่างกลาง (dead-zone)   → คงสถานะเดิม (hysteresis กันกระพริบ)
//       * เดิมผูกกับอุณหภูมิน้ำด้วย (evaporative pad) — ถอดออกแล้ว น้ำเหลือแค่แจ้งเตือน
//
//   ปั๊มน้ำ (CH4) ← ความชื้นอากาศอย่างเดียว
//       อากาศแห้ง (< pumpOnHum)  → เปิด (พ่นน้ำเพิ่มความชื้น)
//       อากาศชื้น (≥ pumpOffHum) → ปิด
//       เซนเซอร์ตาย (humidity == 0) → ปิดเสมอ ห้ามเปิด (safety กันปั๊มไหม้แห้ง/น้ำท่วม)
//
// เงื่อนไข config ที่ถูกต้อง (ผู้เรียกต้องตรวจก่อน): fanOnTemp > fanOffTemp, pumpOffHum > pumpOnHum
// ช่องว่างนี้ = hysteresis กันรีเลย์กระพริบเมื่อค่าแกว่งรอบ threshold
#ifndef AUTO_CONTROL_LOGIC_H
#define AUTO_CONTROL_LOGIC_H

struct AutoControlInputs {
  float airTemp;      // ค่าเฉลี่ยอุณหภูมิอากาศ (°C) — คุมพัดลม
  float airHumidity;  // ค่าเฉลี่ยความชื้นอากาศ (%RH) — คุมปั๊ม · 0 = เซนเซอร์ตาย
  float fanOnTemp;    // พัดลมเปิดเมื่ออากาศ ≥ ค่านี้
  float fanOffTemp;   // พัดลมปิดเมื่ออากาศ ≤ ค่านี้
  float pumpOnHum;    // ปั๊มเปิดเมื่อความชื้น < ค่านี้
  float pumpOffHum;   // ปั๊มปิดเมื่อความชื้น ≥ ค่านี้
};

struct AutoControlDecisions {
  bool fanOpen, fanClose, pumpOpen, pumpClose;
};

inline AutoControlDecisions computeAutoDecisions(const AutoControlInputs& in) {
  AutoControlDecisions d;

  // พัดลม — อุณหภูมิอากาศ (dead-zone ระหว่าง off..on = ทั้งคู่ false = คงสถานะ)
  d.fanOpen  = (in.airTemp >= in.fanOnTemp);
  d.fanClose = (in.airTemp <= in.fanOffTemp);

  // ปั๊ม — ความชื้น + safety: เซนเซอร์ตาย (==0) บังคับปิด ห้ามเปิด
  d.pumpOpen  = (in.airHumidity > 0 && in.airHumidity < in.pumpOnHum);
  d.pumpClose = (in.airHumidity == 0 || in.airHumidity >= in.pumpOffHum);

  return d;
}

#endif
