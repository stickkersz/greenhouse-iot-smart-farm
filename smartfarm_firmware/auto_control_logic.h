// ── Auto Control decision math — pure, ไม่แตะ Arduino/hardware เลย ──
// แยกจาก autoControl() ใน smartfarm_firmware.ino เพื่อทดสอบด้วย g++ ธรรมดา
// (ไม่ต้องใช้ Arduino toolchain) — ดู tests/auto_control_logic.test.cpp
//
// โมเดล 2026-07-15 (v1.8.0 — พัดลม+ปั๊มทำงานคู่กันทั้งคุมอุณหภูมิและคุมความชื้น):
//
//   3 latch (hysteresis) อ่านจากเซนเซอร์อากาศ:
//       ร้อน     (hotOn)      ← อุณหภูมิ | ≥ fanOnTemp เปิด · ≤ fanOffTemp ปิด · ระหว่างกลางคงสถานะ
//       แห้ง     (dryOn)      ← ความชื้น | < pumpOnHum เปิด · ≥ pumpOffHum ปิด · ระหว่างกลางคงสถานะ
//       ปั๊มไล่ร้อน (pumpHeatOn) ← เหมือน latch ร้อน แต่ถูก humid gate ล้างได้ (ดูล่าง)
//
//   พัดลม (CH3, ดูดเข้า) = ร้อน OR แห้ง OR ชื้นเกิน(vent)
//       ร้อน → ระบายความร้อน · แห้ง → ดูดอากาศนอกเข้ามา + กระจายละอองน้ำจากปั๊ม
//       ชื้นเกิน(vent) → เปิดพัดลมไล่ความชื้นออก (เช่น ฝนตก/อากาศอิ่มน้ำ) — latch "เปียก" (wet) v2.7.0
//       ไม่มี humid gate — พัดลมต้องระบายความร้อนได้เสมอแม้อากาศชื้น (พัดลมไม่ได้พ่นน้ำ ไม่มีอะไรเสีย)
//
//   latch เปียก (wetOn) v2.7.0 — ระบายความชื้น (คุมพัดลมเท่านั้น ไม่แตะปั๊ม):
//       RH ≥ ventOnHum เปิด · ≤ ventOffHum ปิด · ระหว่างกลางคงสถานะ (hysteresis กันกระพริบ)
//       ventOnHum ≤ 0 = ปิดฟีเจอร์ (เข้ากันได้ย้อนหลัง — โค้ด/เทสต์เก่าที่ไม่ตั้งค่าจะไม่เห็น wet เลย)
//       เคสจริง: บางวันฝนตก RH พุ่ง 80%+ ทั้งที่ไม่ร้อน — ก่อน v2.7.0 พัดลมนิ่ง ความชื้นค้างในโรงเรือน
//       ตอนนี้ RH ข้าม ventOnHum → พัดลมเปิดไล่ความชื้น · ปั๊มยังปิด (humid gate เดิมคุมอยู่แล้ว) = ไล่อย่างเดียวไม่พ่นเพิ่ม
//
//   ปั๊มน้ำ (CH4) = ปั๊มไล่ร้อน OR แห้ง
//       humid gate: ร้อนแต่ชื้นแล้ว (≥ pumpOffHum) → ล้าง latch ปั๊มไล่ร้อน เพราะพ่นน้ำในอากาศอิ่มตัว
//       = ไม่เย็นลง แค่ท่วม (พัดลมยังเปิดอยู่ — นี่คือเคสเดียวที่ 2 ช่องแยกกัน)
//
//   เซนเซอร์เชื่อไม่ได้ (sensorOk == false) → ปิดทั้ง 2 ช่อง + ล้าง latch ทุกตัว
//       SHT35 ให้ทั้งอุณหภูมิและความชื้นจากชิปเดียว — ถ้ามันอ่านไม่ได้ ค่าเก่าทั้งคู่ ไม่ใช่แค่ความชื้น
//       จึงไม่มีอะไรให้ตัดสินใจได้เลย ปิดหมดปลอดภัยกว่าสั่งรีเลย์จากค่าเก่าเป็นชั่วโมง
//       (ผู้ใช้เลือกเอง 2026-07-15 — ทางเลือกอื่นที่พิจารณา: พัดลมเปิดค้างกันพืชร้อน / คงสถานะเดิม)
//       ล้าง latch ด้วย เพื่อให้ตอนเซนเซอร์ฟื้น ระบบเริ่มคิดใหม่จากค่าสด ไม่ใช่สานต่อ latch เก่า
//
// ⚠️ ทำไม humid gate ต้องอยู่ "ข้างใน" latch ไม่ใช่ AND ที่ output (บั๊ก v1.8.0 รอบแรก แก้ 2026-07-15):
// gate แบบ stateless (pumpOn = ร้อน AND ไม่อิ่มตัว) ไม่มีความจำ — ความชื้นแกว่งข้าม pumpOffHum
// ทีไรปั๊มสลับทุกที (วัดได้ 5 สวิตช์ / 6 รอบ ที่ temp ใน dead-zone, RH แกว่งรอบ 75)
// พอ gate ล้าง latch แทน การจะติดใหม่ต้องรอ airTemp ข้าม fanOnTemp อีกครั้ง ความชื้นตกกลับมาเฉยๆ
// ไม่ปลุกปั๊ม → กระพริบไม่ได้ · การสวิตช์ปั๊มคือต้นเหตุ sensor latch-up ของโปรเจกต์นี้ ห้ามให้รัว
//
// ทำไมต้อง latch ทั่วไป: ทั้ง 2 ช่องมี 2 hysteresis loop ต่อ 1 รีเลย์ — stateless OR ของ 2 loop
// จะค้างเปิด (loop นึงเลิกต้องการตอนอีก loop อยู่ dead-zone) หรือกระพริบ จึงส่ง latch ปัจจุบัน
// เข้ามาแล้วคืนค่าใหม่ให้ firmware เก็บข้ามรอบ — อย่าเปลี่ยนกลับเป็น stateless open/close
// เงื่อนไข config ที่ถูกต้อง (ผู้เรียกตรวจก่อน): fanOnTemp > fanOffTemp, pumpOffHum > pumpOnHum
#ifndef AUTO_CONTROL_LOGIC_H
#define AUTO_CONTROL_LOGIC_H

struct AutoControlInputs {
  bool  sensorOk;     // เซนเซอร์อากาศเชื่อได้ไหม (false = อ่านพลาดติดกันนานเกิน → ค่าเก่า) · false = ปิดทุกช่อง
  float airTemp;      // ค่าเฉลี่ยอุณหภูมิอากาศ (°C) — คุม latch ร้อน
  float airHumidity;  // ค่าเฉลี่ยความชื้นอากาศ (%RH) — คุม latch แห้ง + humid gate ของปั๊ม
  float fanOnTemp;    // ร้อน ≥ ค่านี้ → latch ร้อนเปิด
  float fanOffTemp;   // เย็น ≤ ค่านี้ → latch ร้อนปิด
  float pumpOnHum;    // แห้ง < ค่านี้ → latch แห้งเปิด
  float pumpOffHum;   // ชื้น ≥ ค่านี้ → latch แห้งปิด · และเป็นเพดาน humid gate ของปั๊ม
  bool  hotOn;        // latch ปัจจุบัน: อากาศร้อนอยู่ไหม (คุมพัดลม)
  bool  dryOn;        // latch ปัจจุบัน: อากาศแห้งอยู่ไหม (คุมทั้ง 2 ช่อง)
  bool  pumpHeatOn;   // latch ปัจจุบัน: ปั๊มกำลังไล่ร้อนอยู่ไหม (โดน humid gate ล้างได้)
  // ── v2.7.0 vent (ระบายความชื้น) — append ท้าย struct เจตนา: initializer เก่า (10 ค่า) จะ zero-init 3 ตัวนี้
  //    → ventOnHum=0 = ฟีเจอร์ปิด = พฤติกรรมเดิมเป๊ะ (backward compatible ไม่ต้องแก้เทสต์เก่า) ──
  float ventOnHum;    // RH ≥ ค่านี้ → wet latch เปิด (พัดลมไล่ความชื้น) · ≤0 = ปิดฟีเจอร์
  float ventOffHum;   // RH ≤ ค่านี้ → wet latch ปิด (= ventOnHum - deadband · firmware คำนวณให้)
  bool  wetOn;        // latch ปัจจุบัน: กำลังระบายความชื้นอยู่ไหม (คุมพัดลมเท่านั้น)
};

struct AutoControlDecisions {
  bool hotOn, dryOn, pumpHeatOn, wetOn;  // latch ใหม่ (firmware เก็บกลับไปใช้รอบหน้า)
  bool fanOn;          // สถานะพัดลมที่ต้องการ = ร้อน OR แห้ง OR ชื้นเกิน(vent)
  bool pumpOn;         // สถานะปั๊มที่ต้องการ = ปั๊มไล่ร้อน OR แห้ง (vent ไม่แตะปั๊ม)
};

inline AutoControlDecisions computeAutoDecisions(const AutoControlInputs& in) {
  AutoControlDecisions d;

  // เชื่อค่าเซนเซอร์ไม่ได้ → ปิดทุกช่อง ล้าง latch ทุกตัว ออกทันที
  // (0%RH ก็ถือว่าเชื่อไม่ได้ — โรงเรือนจริงเป็นไปไม่ได้ ถ้าเห็นแปลว่าเซนเซอร์/สายมีปัญหา)
  if (!in.sensorOk || in.airHumidity <= 0) {
    d.hotOn = d.dryOn = d.pumpHeatOn = d.wetOn = false;
    d.fanOn = d.pumpOn = false;
    return d;
  }

  const bool saturated = (in.airHumidity >= in.pumpOffHum);  // อากาศชื้นเกินจะพ่นน้ำ

  // latch ร้อน — อุณหภูมิอากาศ (ไม่ผูกกับความชื้น: พัดลมต้องระบายความร้อนได้เสมอ)
  bool hot = in.hotOn;
  if      (in.airTemp >= in.fanOnTemp)  hot = true;
  else if (in.airTemp <= in.fanOffTemp) hot = false;
  // else: คงสถานะ

  // latch แห้ง — ความชื้นอากาศ
  bool dry = in.dryOn;
  if      (in.airHumidity <  in.pumpOnHum)  dry = true;
  else if (in.airHumidity >= in.pumpOffHum) dry = false;
  // else: คงสถานะ

  // latch ปั๊มไล่ร้อน — เกณฑ์อุณหภูมิเดียวกับ latch ร้อน แต่ humid gate "ล้าง" ได้
  // ล้างแล้วต้องรอ airTemp ข้าม fanOnTemp ใหม่ถึงติดอีก — นี่คือสิ่งที่กันปั๊มกระพริบที่เส้น pumpOffHum
  bool pumpHeat = in.pumpHeatOn;
  if      (saturated)                   pumpHeat = false;
  else if (in.airTemp >= in.fanOnTemp)  pumpHeat = true;
  else if (in.airTemp <= in.fanOffTemp) pumpHeat = false;
  // else: คงสถานะ

  // latch เปียก (vent) — ระบายความชื้น · ventOnHum ≤ 0 = ปิดฟีเจอร์ (backward compatible)
  // hysteresis เดียวกับ latch อื่น: ข้าม ventOnHum ติด · ตกใต้ ventOffHum ดับ · กลางคงสถานะ
  bool wet = in.wetOn;
  if (in.ventOnHum <= 0)                    wet = false;   // ฟีเจอร์ปิด
  else if (in.airHumidity >= in.ventOnHum)  wet = true;
  else if (in.airHumidity <= in.ventOffHum) wet = false;
  // else: คงสถานะ

  d.hotOn      = hot;
  d.dryOn      = dry;
  d.pumpHeatOn = pumpHeat;
  d.wetOn      = wet;

  // พัดลม — ร้อน หรือ แห้ง หรือ ชื้นเกิน(ไล่ความชื้น) เปิดได้ทุกเหตุผล
  d.fanOn = hot || dry || wet;

  // ปั๊ม — ไล่ร้อน (ผ่าน gate แล้ว) หรือ เพิ่มความชื้นตอนแห้ง
  // `hot &&` กัน latch 2 ตัวหลุด sync (ในทางปฏิบัติ pumpHeat ⊆ hot เพราะใช้เกณฑ์อุณหภูมิเดียวกัน)
  // — ค้ำ invariant "ปั๊มเปิด → พัดลมเปิด" ไว้ ไม่ให้ปั๊มพ่นน้ำตอนพัดลมดับ
  d.pumpOn = (hot && pumpHeat) || dry;
  return d;
}

#endif
