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

#include <stdint.h>

// ── safety cooldown lock: ยังล็อกอยู่ไหม (v2.9.0) ─────
// ย้ายมาไว้ในนี้เพื่อให้เทสต์ถึง — ตัวจับเวลา cooldown ใน pumpSafetyCheck() เดิมไม่มีเทสต์เลย
// (code review v2.8.0 จับได้) และตรรกะ "วนรอบ" แบบนี้คือชนิดที่อ่านโค้ดเปล่าๆ แล้วมองไม่เห็นบั๊ก
//
// รับ now เข้ามาเป็นพารามิเตอร์ ไม่เรียก millis() เอง → เทสต์ป้อนเวลาใกล้จุดวนรอบได้ตรงๆ
//
// ⚠️ ต้องใช้ uint32_t/int32_t ตายตัว ห้ามใช้ unsigned long/long:
//   บน ESP32  long = 32 bit → การลบวนรอบที่ 2^32 แล้ว cast ได้เครื่องหมายถูก
//   บนเครื่องเทสต์ (macOS/Linux 64-bit) long = 64 bit → การลบ "ไม่วนรอบ" ที่ 2^32
//   ถ้าเขียนด้วย long เทสต์จะผ่านโดยไม่ได้ทดสอบพฤติกรรมจริงบนบอร์ดเลย (false confidence)
//
// ⚠️ กัน lockUntil == 0 (ยังไม่เคยตั้งล็อก) แยกก่อน — ไม่งั้นพอ now > 2^31 (~24.8 วัน)
// (int32_t)(now - 0) จะติดลบ = รายงานว่า "ล็อกอยู่" ตลอดกาลทั้งที่ไม่เคยล็อก
//
// ⚠️ ถูกต้องเฉพาะในหน้าต่าง ±24.8 วันรอบ deadline — ผู้เรียกต้องล้าง lockUntil เป็น 0 เมื่อหมดอายุ
// (pumpSafetyCheck() ทำให้ทุกรอบ loop) ไม่งั้น deadline เก่าเกิน 24.8 วันจะวนกลับมาอ่านว่า "ล็อกอยู่"
inline bool lockIsActive(uint32_t now, uint32_t lockUntil) {
  if (lockUntil == 0) return false;
  return (int32_t)(now - lockUntil) < 0;
}

// ── pump/fan safety timers — max runtime + cooldown ที่พักคู่กัน (v2.9.0 ย้ายมาให้เทสต์ถึง) ──
// ตรรกะนี้เคยอยู่ใน pumpSafetyCheck() ของ .ino ทั้งก้อน = เทสต์ไม่ถึงเลย (code review v2.8.0 จับได้)
// ทั้งที่เป็นโค้ดที่ "คุมความปลอดภัยจริง" และมีบั๊กมาแล้ว 2 รอบ (false water alarm, lock desync)
// แยกส่วนที่เป็นตรรกะเวลา/สถานะล้วนออกมา — ส่วนที่เหลือใน .ino คือสั่งรีเลย์ + log + Firebase alert
//
// กฎ (v2.8.0 ตามคำสั่งหน้างาน):
//   - จับเวลาเดินต่อเนื่องแยกช่อง · manual = คนคุมเอง ไม่จับเวลา ไม่ตัด
//   - ครบ maxRuntime ช่องใดช่องหนึ่ง → ตัดทั้งคู่ (เฉพาะช่อง auto) + ล็อกพักพร้อมกันทั้งคู่
//   - ล็อกตั้ง "ทั้งคู่เสมอ" ไม่ผูกโหมด · การสั่งรีเลย์ปิดทำเฉพาะช่อง auto (ดูเหตุผลใน .ino)
struct SafetyTimerState {
  uint32_t pumpOnSince;    // เวลาเริ่มเดินปั๊ม (0 = ไม่ได้เดิน/ไม่จับ)
  uint32_t fanOnSince;     // เวลาเริ่มเดินพัดลม (0 = ไม่ได้เดิน/ไม่จับ)
  uint32_t pumpLockUntil;  // deadline ปลดล็อกปั๊ม (0 = ไม่ล็อก)
  uint32_t fanLockUntil;   // deadline ปลดล็อกพัดลม (0 = ไม่ล็อก)
};

struct SafetyTimerInputs {
  uint32_t now;
  bool     pumpRelayOn;    // สถานะรีเลย์ปั๊มตอนนี้
  bool     fanRelayOn;     // สถานะรีเลย์พัดลมตอนนี้
  bool     pumpAuto;       // ปั๊มอยู่โหมด auto หรือ schedule (ไม่ใช่ manual)
  bool     fanAuto;
  uint32_t maxRuntimeMs;   // เดินต่อเนื่องได้สูงสุด
  uint32_t cooldownMs;     // พักหลังถูกตัด
};

struct SafetyTimerResult {
  SafetyTimerState state;  // state ใหม่ที่ผู้เรียกต้องเก็บกลับ
  bool cutPumpRelay;       // ต้องสั่งปิดรีเลย์ปั๊มรอบนี้
  bool cutFanRelay;        // ต้องสั่งปิดรีเลย์พัดลมรอบนี้
  bool pumpMaxed;          // ปั๊มเป็นตัว trigger — ใช้เลือกข้อความ alert (ปั๊มเดินนาน = สงสัยน้ำหมด)
  bool fanMaxed;           // พัดลมเป็นตัว trigger — ไม่ใช่เรื่องน้ำ ห้ามปลุก water alarm
  bool didCutoff;          // เกิด cutoff รอบนี้ (pumpMaxed || fanMaxed)
};

inline SafetyTimerResult stepSafetyTimers(const SafetyTimerInputs& in, SafetyTimerState st) {
  SafetyTimerResult r{st, false, false, false, false, false};

  // 1) ล้างล็อกที่หมดอายุให้กลับเป็น 0 — จำเป็นเพราะ lockIsActive() ถูกต้องเฉพาะหน้าต่าง ±24.8 วัน
  //    รอบ deadline · ถ้าปล่อยค่าเก่าค้าง deadline ที่ผ่านไปเกิน 24.8 วันจะวนกลับมาอ่านว่า "ล็อกอยู่"
  if (st.pumpLockUntil != 0 && !lockIsActive(in.now, st.pumpLockUntil)) st.pumpLockUntil = 0;
  if (st.fanLockUntil  != 0 && !lockIsActive(in.now, st.fanLockUntil))  st.fanLockUntil  = 0;

  // 2) จับเวลาเดินต่อเนื่อง — เริ่มจับเมื่อรีเลย์เปิด "และ" อยู่โหมด auto · นอกนั้นรีเซ็ต
  //    manual ไม่จับเวลา = ไม่มีวันถูกตัด (คนสั่งเองต้องคุมเอง — เจตนาเดิมของระบบ)
  if (in.pumpRelayOn && in.pumpAuto) { if (st.pumpOnSince == 0) st.pumpOnSince = in.now; }
  else                                 st.pumpOnSince = 0;
  if (in.fanRelayOn  && in.fanAuto)  { if (st.fanOnSince  == 0) st.fanOnSince  = in.now; }
  else                                 st.fanOnSince  = 0;

  // 3) ครบเวลาหรือยัง — ลบกันแบบ unsigned (ทน millis() วนรอบ) ไม่ใช่เทียบ absolute
  r.pumpMaxed = (st.pumpOnSince != 0) && ((uint32_t)(in.now - st.pumpOnSince) >= in.maxRuntimeMs);
  r.fanMaxed  = (st.fanOnSince  != 0) && ((uint32_t)(in.now - st.fanOnSince)  >= in.maxRuntimeMs);
  r.didCutoff = r.pumpMaxed || r.fanMaxed;

  // 4) cutoff — ล็อกทั้งคู่เสมอ (ไม่ผูกโหมด) · สั่งปิดรีเลย์เฉพาะช่อง auto ที่กำลังเปิดอยู่
  if (r.didCutoff) {
    st.pumpLockUntil = in.now + in.cooldownMs;
    st.fanLockUntil  = in.now + in.cooldownMs;
    // ⚠️ deadline อาจล้นเป็นเลขเล็กได้ตอน now ใกล้ 2^32 — lockIsActive() รับมือด้วยผลต่าง signed
    //    แต่ถ้าล้นแล้วได้ 0 พอดี จะกลายเป็น "ไม่ล็อก" → เลื่อนเป็น 1 (1ms ไม่มีผลเชิงพฤติกรรม)
    if (st.pumpLockUntil == 0) st.pumpLockUntil = 1;
    if (st.fanLockUntil  == 0) st.fanLockUntil  = 1;
    r.cutPumpRelay = in.pumpAuto && in.pumpRelayOn;
    r.cutFanRelay  = in.fanAuto  && in.fanRelayOn;
    st.pumpOnSince = 0;
    st.fanOnSince  = 0;
  }

  r.state = st;
  return r;
}

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
  // ── v2.7.0 vent (ระบายความชื้น) — append ท้าย struct เจตนา: initializer เก่า (10 ค่า) จะ zero-init 2 ตัวนี้
  //    → ventThreshold=0 = ฟีเจอร์ปิด = พฤติกรรมเดิมเป๊ะ (backward compatible ไม่ต้องแก้เทสต์เก่า) ──
  float ventThreshold; // ค่า humidity_vent ดิบจาก config · ≤0 = ปิดฟีเจอร์
                       // ventOn/ventOff คำนวณ "ข้างใน" computeAutoDecisions() ไม่ใช่ที่ firmware
                       // — ไม่งั้น clamp อยู่ชั้น .ino ที่เทสต์ไม่ถึง แล้วเทสต์ต้องเขียน clamp ซ้ำเอง
                       //   (เขียนซ้ำ = จูน VENT_HYST แล้วเทสต์ยังผ่านทั้งที่ firmware เพี้ยน)
  bool  wetOn;         // latch ปัจจุบัน: กำลังระบายความชื้นอยู่ไหม (คุมพัดลมเท่านั้น)
};

// deadband ของ wet latch · ปิดที่ ventOn - VENT_HYST กันพัดลมกระพริบที่เส้น
// ⚠️ นี่คือ "แหล่งความจริง" ของค่านี้ทั้งโปรเจกต์ — .ino ต้องอ้างค่านี้ ห้าม #define ซ้ำ
//
// ค่านี้ต้องมีสำเนาอยู่อีก 2 ที่ เพราะเป็น runtime ที่ import กันไม่ได้:
//   1. dashboard/index.html  → const VENT_HYST_PCT
//   2. database.rules.json   → เขียนเลขลงไปตรงๆ 3 จุด (Firebase rules ไม่มีตัวแปร/import เลย)
// **จูนค่านี้ = ต้องแก้ทั้ง 3 ที่** · `npm run test:sync` จะ fail ถ้าไม่ตรงกัน (tests/vent_hyst_sync.check.js)
// จึงไม่ต้องจำเอง — แต่ห้ามข้าม test:sync ตอนจูน ไม่งั้น firmware กับ rules จะขัดกันเงียบๆ
// (rules ปฏิเสธค่าที่ firmware ยอมรับ = ตั้งค่าจาก dashboard ไม่ได้ โดยไม่มีอะไรบอกว่าทำไม)
static const float VENT_HYST = 5.0f;

// vent ใช้งานได้ไหม — เงื่อนไขเดียว ใช้ร่วมทุกชั้น
// ต้อง ventOff (= vent - VENT_HYST) ≥ pumpOffHum ไม่งั้นมีช่วง RH ที่ wet ยังค้าง (พัดลมไล่ชื้นออก)
// แต่ปั๊มยังไม่ถูก humid gate ตัด (พ่นเข้า) = ตีกันเอง · จึงต้อง vent ≥ pumpOffHum + VENT_HYST
// (preset ทั้ง 3 ตัวอยู่พอดีเส้นนี้ จึงเป็น ≥ ไม่ใช่ >)
inline bool ventIsUsable(float ventThreshold, float pumpOffHum) {
  return ventThreshold > VENT_HYST && ventThreshold >= pumpOffHum + VENT_HYST;
}

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

  // latch เปียก (vent) — ระบายความชื้น · clamp อยู่ตรงนี้ (ไม่ใช่ที่ .ino) เทสต์จึงกินโค้ดจริง
  // ventThreshold ที่ใช้ไม่ได้ (ต่ำเกิน / เว้นจาก pumpOffHum ไม่ถึง VENT_HYST) → ปิดฟีเจอร์
  const float ventOn  = ventIsUsable(in.ventThreshold, in.pumpOffHum) ? in.ventThreshold : 0.0f;
  const float ventOff = ventOn - VENT_HYST;   // ventOn > VENT_HYST การันตี ventOff > 0
  // hysteresis เดียวกับ latch อื่น: ข้าม ventOn ติด · ตกใต้ ventOff ดับ · กลางคงสถานะ
  bool wet = in.wetOn;
  if (ventOn <= 0)                       wet = false;   // ฟีเจอร์ปิด
  else if (in.airHumidity >= ventOn)     wet = true;
  else if (in.airHumidity <= ventOff)    wet = false;
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
