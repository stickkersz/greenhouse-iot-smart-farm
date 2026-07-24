/*
  smartfarm_firmware.ino
  Greenhouse IoT Smart Farm — บริษัท ปุ๋ยไวกิ้ง จำกัด
  จัดทำโดย: Tonkla (IT Intern) | มิถุนายน 2569
  Version: 2.8.0
  Changelog v2.8.0 (2026-07-24) — พัดลมกลับมาพักคู่ปั๊ม (max run 15 นาที + พัก 5 นาที พร้อมกัน):
    - ตามคำสั่งหน้างาน: ให้พัดลมมี max-runtime 15 นาทีเท่าปั๊ม แล้วพัก 5 นาทีพร้อมกัน
    - ⚠️ นี่คือการ "ย้อน" decouple ของ v2.6.0 — v2.6.0 แยกพัดลมออกเพราะ CSV 2026-07-20 พบว่า
      การพักคู่ปั๊มทำให้พัดลมดับ ~33% ของช่วงร้อน 7 ชม. = ต้นเหตุ temperature overshoot
      v2.8.0 ยอมรับ trade-off นี้ตามที่ผู้ใช้สั่ง (ถ้าอากาศร้อนตอนปั๊มพัก พัดลมจะดับตามไปด้วย)
    - pumpSafetyCheck() รวมเป็นเช็คเดียว: จับเวลาเดินต่อเนื่องแยกช่อง (pumpOnSince/fanOnSince)
      ครบ 15 นาทีช่องใดช่องหนึ่ง → ตัด "ทั้งคู่" (เฉพาะช่อง auto) + ล็อกพัก 5 นาทีพร้อมกัน
    - re-add fanLockUntil + fanOnSince (ถอดไปตอน v2.6.0) · gate fan-ON ด้วย millis() >= fanLockUntil
    - checkSchedule(): พัดลมบน schedule เคารพ fanLockUntil ด้วย (เดิมเฉพาะปั๊ม)
    - status/fan_locked กลับมา push ค่าจริง (millis() < fanLockUntil) — เดิม pin false ไว้ตั้งแต่ v2.6.0
    - dashboard: แก้ข้อความ noti ปั๊มพัก "เดินครบ 10 นาที" -> "15 นาที" (ค้างมาตั้งแต่ v2.5.0 bump)
    - FAN_MAX_RUNTIME_MS / FAN_COOLDOWN_MS = ค่าเดียวกับปั๊ม (15/5 นาที) · เป็น #define ต้อง reflash
  Changelog v2.7.1 (2026-07-24) — ปิดช่อง "ปั๊มพ่นน้ำขณะพัดลมไล่ความชื้น" + ย้าย clamp เข้า logic ที่เทสต์ได้:
    - ⚠️ บั๊ก: v2.7.0 ต้องการแค่ humidity_vent > humidity_max ซึ่ง "ไม่พอ"
      wet latch ค้างเปิดได้ทั้งช่วง [ventOff, ventOn) แต่ปั๊มถูก humid gate ตัดเฉพาะตอน RH ≥ humidity_max
      ถ้า ventOff < humidity_max จะมีช่วง RH ที่พัดลมไล่ชื้น "ออก" พร้อมกับปั๊มพ่นชื้น "เข้า" = ตีกันเอง
      ตัวอย่างจริง: humidity_max=79 humidity_vent=80 → ventOff=75 · ที่ RH 76 + อากาศร้อน = เปิดพร้อมกัน
      แก้: เงื่อนไขใหม่ humidity_vent ≥ humidity_max + VENT_HYST (preset ทั้ง 3 ตัวอยู่พอดีเส้นนี้ จึงเป็น ≥)
    - ย้าย clamp (ventOn/ventOff + ventIsUsable) จาก autoControl() ใน .ino → auto_control_logic.h
      เหตุผล: clamp ที่อยู่ชั้น .ino เทสต์ไม่ถึง เทสต์เลยต้องเขียน clamp ซ้ำเอง = จูน VENT_HYST แล้ว
      เทสต์ยังผ่านทั้งที่ firmware เพี้ยน · ตอนนี้ sweep กินโค้ดจริง (พิสูจน์: แก้ VENT_HYST 5→3 เทสต์ FAIL)
      AutoControlInputs รับ ventThreshold ดิบตัวเดียว (แทน ventOnHum/ventOffHum) — initializer 10 ค่าเดิม
      ยัง zero-init = ปิดฟีเจอร์ = backward compatible เหมือนเดิม
    - ถอด #define VENT_HYST ออกจาก .ino — macro ไป shadow const ใน header (จูนที่ header ไม่มีผล)
    - [AUTO] log เตือนเมื่อ config ทำให้ vent ถูกปิด (เดิมปิดเงียบๆ หาไม่เจอจาก log)
      จำ "ค่าที่เตือนไปแล้ว" ไม่ใช่ bool flag — ไม่งั้นตั้งค่าผิดค่าที่ 2 จะเงียบ
    - fanWhy: เปลี่ยนจาก if/else ladder เป็นตาราง bitmask 8 ช่อง — เดิมเคส "ร้อน+เปียก" ไม่รายงาน wet
    - database.rules.json: บังคับ vent ≥ humidity_max + 5 ทั้ง 2 ทาง (กัน partial update แหกกฎ)
      + ปิดช่องเดียวกันที่ temp_off / humidity_min (เดิม update({temp_off:40}) ผ่าน → autoControl
      ติด guard `ton <= toff` ทุกรอบ = รีเลย์ค้างถาวรเงียบๆ)
    - dashboard: validation ตรงกับ firmware, badge เช็ค ventUsable ก่อนบอกว่า "พัดลมไล่ความชื้น",
      ดัก .set() reject (เดิมเงียบ user นึกว่าบันทึกแล้ว), รวม preset-match ที่ถูกก๊อป 2 ที่เป็นฟังก์ชันเดียว
    - เทสต์: rules 64 ผ่านหมด (เพิ่ม vent/partial-update/preset round-trip), logic ALL PASS
  Changelog v2.7.0 (2026-07-23) — พัดลมไล่ความชื้น (vent) เมื่อ RH สูง (เช่นฝนตก) ตั้งค่าจาก dashboard:
    - ปัญหา: บางวันฝนตก RH พุ่ง 80%+ ทั้งที่ไม่ร้อน — เดิมพัดลมนิ่ง (fan = ร้อน OR แห้ง เท่านั้น)
      ความชื้นค้างในโรงเรือน · ตอนนี้เพิ่ม latch "เปียก" (wet): RH ≥ humidity_vent → พัดลมเปิดไล่ความชื้น
    - auto_control_logic.h: fan = ร้อน OR แห้ง OR ชื้นเกิน(vent) · pump ไม่เปลี่ยน (vent คุมพัดลมเท่านั้น)
      wet latch มี hysteresis: เปิด ≥ ventOn · ปิด ≤ ventOff (= ventOn - VENT_HYST 5%) กันพัดลมกระพริบ
      ventOnHum ≤ 0 = ปิดฟีเจอร์ (backward compatible — โค้ด/เทสต์เก่าไม่เห็น wet เลย)
      ⚠️ v2.7.1 แก้สัญญานี้: field เปลี่ยนเป็น ventThreshold ตัวเดียว และต้อง ≥ humidity_max + VENT_HYST
    - threshold ใหม่ thresh_hum_vent (default 80) — sync จาก Firebase thresholds/humidity_vent + persist NVS
    - dashboard: เพิ่มช่องตั้ง "พัดลมไล่ความชื้นที่ RH ≥ __%" + ใส่ใน preset ฤดู (ร้อน/ปกติ/ฝน)
    - เทสต์ vent ครบ (crossing/hysteresis/ปิดฟีเจอร์/sensor เสีย/invariant pumpOn→fanOn) ผ่าน 100%
  Changelog v2.6.0 (2026-07-23) — แยกพัดลมออกจาก pump safety lock (fan-lock decouple):
    - ปัญหา: v2.3.0 ตอน pump safety cutoff (เดิน 15 นาที) ตัดพัดลม (CH3) ด้วย + ล็อกพักคู่กัน 5 นาที
      หลักฐาน CSV 2026-07-20: ร้อน ≥34°C ต่อเนื่อง 7 ชม. อากาศแห้ง (43-49%) แต่ระบบดับไป-มา
      ~33% ของเวลา (10-15 เดิน + 5 พัก คู่กัน) = ต้นเหตุ overshoot อุณหภูมิคุมไม่อยู่
    - แก้: pumpSafetyCheck() ตัด "เฉพาะปั๊ม" — พัดลมไม่ถูกแตะ · ถอด fanLockUntil + gate ใน autoControl()
      พัดลมทำงานตาม latch (ร้อน/แห้ง) อิสระ พักเองเมื่อ latch เคลียร์ (ไม่มี forced rest — ผู้ใช้เลือก 2026-07-23)
    - ปลอดภัย: invariant pumpOn→fanOn ยังครบ · "fan วิ่งลำพัง" มีอยู่แล้วในเคส ร้อน+ชื้นเกิน (auto_control_logic.h เดิม)
      กฎ "พัดลมพักคู่ปั๊ม" เป็นแค่ add-on ระดับ .ino (v2.3.0) ไม่ใช่ pure logic — ถอดได้ไม่แตะ decision math
    - status/fan_locked คงไว้เป็น false เสมอ (กัน dashboard พังจาก field หาย) · pump cutoff log/alert แก้เป็น "ตัดปั๊ม" อย่างเดียว
    - ไม่แตะ auto_control_logic.h / tests — decision logic fan/pump ไม่เปลี่ยน · PUMP_MAX_RUNTIME_MS ยัง 15 นาที (v2.5.0)
  Changelog v2.5.0 (2026-07-20) — ยืดเวลาปั๊มเดินต่อเนื่อง 10 → 15 นาที:
    - หลักฐาน: smartfarm_24h_2026-07-20*.csv จริง — อุณหภูมิ ≥34°C ต่อเนื่อง 11:00-16:00 เฉลี่ย 38.4°C
      สูงสุด 41.1°C ทั้งที่ความชื้นช่วงนั้นแค่ 38-49% (แห้ง เข้าเงื่อนไข evaporative cooling ที่ควรได้ผลดีสุด)
    - แก้: PUMP_MAX_RUNTIME_MS (10UL→15UL นาที) — ให้ความชื้นสะสม/ระบายความร้อนได้นานขึ้นก่อนถูกบังคับตัด
      PUMP_COOLDOWN_MS (พัก 5 นาที) ไม่เปลี่ยน · ค่าเป็น #define ต้อง reflash (ไม่ใช่ threshold ปรับสดจาก dashboard)
    - ⚠️ พัดลม "ยังพักคู่ปั๊ม" hard ตอน safety cutoff เหมือนเดิม (v2.3.0 — ตัดสินใจ 2026-07-17 คงไว้)
      เคยพิจารณาแยกพัดลมออกจาก lock กันช่วง blackout แต่ผู้ใช้เลือกคงพฤติกรรมพักคู่กัน (2026-07-20)
    - ไม่แตะ auto_control_logic.h / tests — decision logic fan/pump ไม่เปลี่ยน มีแค่ตัวเลข timeout
  Changelog v2.4.0 (2026-07-17) — ออฟไลน์แล้วอุปกรณ์ไม่ดับ (persist config ข้าม reboot/ไฟดับ):
    - อาการหน้างาน: WiFi หลุด → relay ที่คนสั่ง manual ดับหมด · เหตุ 2 อย่างรวมกัน:
      1) WiFi หลุด 30 นาที → ESP.restart() (หรือ brownout) → setup() รีเซ็ต ch_isAuto/ch_manual/threshold
         กลับเป็น compile default → อุปกรณ์ manual-ON ดับ + threshold ที่ตั้งจาก dashboard หาย
      2) applyManualControl() ถูกเรียกเฉพาะใน poll block ที่ gate ด้วย fbReady() → ออฟไลน์ = manual ไม่เคยถูกขับ
    - แก้ 1: เก็บ control config ลง NVS (flash) ทุกครั้งที่ dashboard เปลี่ยนค่า (saveControlState) แล้ว
      restore ตอนบูตก่อนต่อ WiFi (loadControlState) → reboot/ไฟดับ ไม่ทำลายสถานะ (RTC memory ไม่รอดไฟตก แต่ NVS รอด)
      · เขียนเฉพาะตอนค่าเปลี่ยนจริง (controlDirty) กัน NVS wear
    - แก้ 2: เรียก applyManualControl() ใน sensor block ทุก 30 วิ (ไม่ผูก fbReady) + เรียกใน setup() หลัง restore
      → manual channel ถูกบังคับสถานะจริงทั้งตอนออฟไลน์และหลัง reboot · poll 1.5 วิ ยังคุมตอนออนไลน์ให้ตอบไว
    - หมายเหตุ: auto control ทำงานออฟไลน์อยู่แล้ว (อ่าน SHT35 + threshold ใน RAM) — v2.4.0 เพิ่มให้ manual +
      ค่า threshold ที่คนตั้งไว้รอดข้าม reboot ด้วย · pump 10-นาที safety ยังคุม manual-ON pump เหมือนเดิม
  Changelog v2.3.0 (2026-07-17) — พัดลมพักพร้อมปั๊มตอน safety cutoff:
    - เดิม: pump safety ตัดปั๊มเมื่อเดินเกิน 10 นาที + พัก 5 นาที (pumpLockUntil) แต่ "พัดลม" วิ่งต่อทั้งช่วงพัก
    - ใหม่: ตอน cutoff ตัดพัดลม (CH3) ด้วย + ล็อกพักพร้อมกัน 5 นาที (fanLockUntil) — พัดลม+ปั๊มพักคู่กัน
    - พักแบบ hard: ระหว่าง 5 นาที พัดลมไม่เปิดกลับแม้ยังร้อน (ผู้ใช้เลือก 2026-07-17 — ไม่มี hot override)
      · autoControl() gate ฝั่ง fan-ON ด้วย millis() >= fanLockUntil (คู่ขนานกับ pump gate เดิม)
    - manual ยังสั่งพัดลมฝืน lock ได้ (สอดคล้องหลักเดิม: ผู้ใช้ไม่ถูกล็อกออกจากระบบตัวเอง) เหมือน pump
    - push status/fan_locked ขึ้น dashboard (คู่กับ pump_locked เดิม)
  Changelog v2.2.1 (2026-07-16) — DS18B20 ไม่เคยฟื้นถ้าบูตแล้วไม่เจอ (บั๊กจริง ไม่ใช่แค่สาย) + ปิด regression NTP:
    - ⚠️ regression จาก v2.2.0 (code review จับได้): บูตแบบ offline setup() ข้าม syncNTP() แต่ยังเซ็ต
      lastNtpSync=millis() → loop() รอ 6 ชม.ก่อน sync · เมื่อก่อน v2.2.0 reboot ทิ้งเคสนี้ไปเลยจึงไม่เคยเจอ
      แต่ v2.2.0 ตั้งใจให้วิ่งต่อแบบ offline = เคสนี้กลายเป็นทางปกติ · ระหว่างรอ 6 ชม. timeValid()=false
      → schedule (พัดลม/ปั๊มตามเวลา) ไม่ทำงาน + hourly log ถูกทิ้งทุกชั่วโมง (getHourlyPath คืน "")
      แก้: นาฬิกายังไม่ valid → retry NTP ทุก 60 วิ (NTP_RETRY_INVALID_MS) ไม่ใช่รอ 6 ชม.
      ครอบเคส "บูต online แต่ NTP ช้า/ไม่ตอบ" ด้วย · gate ด้วย WiFi connected ก่อนเช็ค interval
    - ⚠️ hourly log ผูกกับ "ขอบชั่วโมงนาฬิกาจริง" แทน millis LOG_INTERVAL (code review finding #2):
      millis รีเซ็ตตอน reboot แต่ accumulator ย้ายไป RTC (v2.2.0) ไม่รีเซ็ต = เข้ากันไม่ได้ · ถ้าบอร์ด
      reboot ก่อนครบชั่วโมง (เช่นยังหาสาเหตุ restart ไม่เจอ) → `now - lastLogTime` ไม่มีวันครบ = ไม่เคย
      flush เลย + accumulator (RTC) โตข้าม reboot ไปเรื่อยๆ แล้วสุดท้ายตกถัง %H ผิดชั่วโมง
      แก้: จำ path ของชั่วโมงที่กำลังสะสมไว้ใน RTC (rtcLogPath) · loop flush เมื่อ getHourlyPath() เปลี่ยน
      → ตกถังถูกชั่วโมงเสมอไม่ว่า reboot กี่ครั้ง · ลบ lastLogTime ทิ้ง (LOG_INTERVAL ใน config.h เลิกใช้)
    - ⚠️ pushHourlyLog() reset accumulator ทุกทางออก (code review finding #3): เดิม h_count==0 return
      โดยไม่ reset → เซนเซอร์อากาศตายแต่ค่าน้ำยังสะสม = น้ำโตไม่หยุด ไม่ถูกเขียน ไม่ถูกล้าง (แย่ขึ้นเพราะ RTC
      ทำให้ค้างข้าม reboot ด้วย) · ตอนนี้แยกเขียนอากาศ/น้ำอิสระ เขียนเท่าที่มี sample จริง แล้ว reset เสมอ
  Changelog v2.2.1-ds (2026-07-16) — DS18B20 ไม่เคยฟื้นถ้าบูตแล้วไม่เจอ (บั๊กจริง ไม่ใช่แค่สาย):
    - ⚠️ DallasTemperature 4.0.6 · getAddress() ขึ้นต้นด้วย `if (index < devices)` โดย `devices`
      ถูกตั้งจาก begin() ที่เดียวเท่านั้น · เราเรียก begin() ครั้งเดียวตอน setup()
      → บูตแล้วไม่เจอเซนเซอร์ (สายหลวม/ยังไม่ได้ต่อ/บัสสะดุดจังหวะนั้น) = devices ค้าง 0 ตลอดกาล
      → getTempCByIndex(0) คืน -127 ทันทีโดย "ไม่แตะบัสเลย" แม้ต่อสายคืนถูกต้องแล้วก็ตาม
      = เสียบสายตอนบอร์ดรันอยู่ไม่มีวันขึ้น ต้องรีบูตเท่านั้น · ทำให้ไล่ปัญหาสายหลงทางหนักมาก
      (แก้สายถูกแล้วแต่ระบบยังบอกพัง → เข้าใจผิดว่าสายยังผิด → รื้อสายที่ถูกอยู่แล้วทิ้ง)
    - requestTemperatures() ใช้ skip() (broadcast) จึงยิง convert ได้ปกติ — เซนเซอร์อาจทำงานถูกต้องอยู่
      ทุกประการ แต่ฝั่ง "อ่าน" ถูกปิดประตูด้วย devices ที่ค้างมาตั้งแต่บูต
    - แก้: อ่านน้ำไม่ได้ติดกัน DS_REINIT_EVERY (3 รอบ ≈ 90 วิ) → ds18b20.begin() สแกนบัสใหม่
      + log จำนวนที่เจอ · เป็นกลไกเดียวกับที่ SHT35 ได้ไปตั้งแต่ v2.0.0 (SHT_REINIT_EVERY) — DS18B20 ตกสำรวจ
    - log ค่าเสียบอกวิธีอ่านแล้ว: -127 = ไม่มีใครตอบบนบัส (สาย/pull-up/เซนเซอร์ตาย) · 85 = เจอตัวแล้ว
      แต่ convert ไม่จบ = ไฟเลี้ยงไม่พอ · 2 ค่านี้ชี้คนละปัญหาคนละทางแก้
    - หมายเหตุฮาร์ดแวร์ (ไม่ใช่โค้ด): วัดได้ 4V ทั้งขา data และ VCC = ไม่ใช่รางไฟที่มีจริงบนบอร์ดนี้
      (3V3=3.3V, VIN≈4.5-4.7V) · ~3.9V คือค่าที่ได้เมื่อ pull-up ไปเกาะราง 5V/VIN แล้วโดน ESD diode
      ของ GPIO4 clamp ลงมาที่ VDD+0.6 — เกินสเปค GPIO (abs max 3.6V) · pull-up 4.7k ต้องไป 3V3 เท่านั้น
  Changelog v2.2.0 (2026-07-16) — "ระบบไม่ต่อเนื่อง": reboot คือทางออกสุดท้าย ไม่ใช่ตัวจัดการ error:
    - ⚠️ ต้นเหตุหลักของอาการ "รีสตาร์ทแล้ววนต่อ WiFi error ซ้ำๆ": setup() reboot ตอนต่อ WiFi ไม่ติด
      (และตอน Firebase auth ไม่ผ่าน 4 ครั้ง) — ขัดกับ loop() ที่ทน offline ได้ 30 นาทีและคุมโรงเรือนต่อได้ทั้งช่วง
      ผลคือ restart 1 ครั้งจากสาเหตุอะไรก็ตาม + เราเตอร์ยังไม่ฟื้น = reboot loop ไม่รู้จบ
      ทุกรอบ relay ดับ + hourly accumulator หาย · setup() ไม่ reboot อีกแล้ว วิ่งต่อแบบ offline
    - wifiMulti.run() ไม่ใช่ poll ราคาถูก — ข้างในมัน scanNetworks() (~2-4 วิ) + disconnect() + begin()
      แล้วรอจนครบ timeout ตัวเอง · เดิมเรียก 40 ครั้ง + delay(500) = แต่ละรอบไปฆ่า attempt ของรอบก่อนทิ้ง
      churn ได้ถึง ~6 นาที และต่อติดแบบสุ่ม (= อาการ "บางทีติดบางทีไม่ติด") → เรียกครั้งเดียว ใส่ timeout จริง
    - Firebase.reconnectWiFi(false) — ตั้งแต่ v2.1.0 เราจัดการ WiFi เองด้วย wifiMulti แล้ว
      ปล่อย true ไว้ = มี 2 ตัวแย่งกันจัดการวิทยุตัวเดียว (lib เรียก WiFi.reconnect() SSID เดิม สวนทาง wifiMulti)
    - Firebase auth ย้ายไป retry ใน loop() ทุก 60 วิ แทนการ reboot — ยังกันเคสที่ v1.5.0 ตั้งใจกัน
      ("บอร์ดวิ่งต่อแบบไม่ auth ตลอดไป เงียบสนิท") ด้วยการ retry จริง + log ไม่ใช่ด้วยการ reboot
    - WiFi offline 30 นาที → restart "ครั้งเดียว" (RTC จำข้าม reboot) ไม่ใช่ทุก 30 นาทีตลอดกาล
      เราเตอร์เจ๊งยาว 5 ชม. = reboot 1 ครั้ง ไม่ใช่ 10 ครั้ง (ผู้ใช้ตัดสินใจ 2026-07-16 แทนของเดิม 2026-07-15)
      ต่อ WiFi ได้เมื่อไหร่ = คืนสิทธิ์ restart ให้เคสค้างครั้งหน้า
    - hourly accumulator ย้ายไป RTC_NOINIT_ATTR — รอด panic/WDT/ESP.restart() (ไม่รอดไฟดับ/brownout
      ซึ่ง RTC RAM ไม่การันตี) · มี magic number ตรวจ ถ้าเจอขยะ = เริ่มนับใหม่
      ⚠️ RTC_DATA_ATTR ใช้ไม่ได้ที่นี่ — bootloader โหลด segment .rtc.data ทับทุก reset ที่ไม่ใช่ deep-sleep
    - เพิ่ม boot diagnostics: esp_reset_reason() + heap + boot count — เดิมไม่เคย log เลย
      "บอร์ดรีสตาร์ทเอง" จึงไล่ไม่ได้ว่า brownout (ไฟตกตอน relay สวิตช์) / PANIC (โค้ด crash) /
      TASK_WDT (loop ค้าง) — คนละสาเหตุคนละวิธีแก้ · ขึ้น Firebase ด้วย (dashboard เห็นย้อนหลัง)
    - log free heap + largest free block ทุก push — จับ fragmentation (Firebase client จอง String/SSL
      ทุกรอบ · ถ้า largest block หดทั้งที่ free ยังเยอะ = fragment → malloc พลาด → crash หลังรันหลายชั่วโมง)
    - หมายเหตุ: hourly log ยังใช้ millis-based LOG_INTERVAL อยู่ — reboot ทำให้ขอบชั่วโมงเลื่อน
      (accumulator รอดแล้ว แต่ไปตกถัง %H ของตอน push) · ยังไม่แก้รอบนี้ ทำทีละอย่างตามหลักโปรเจกต์
  Changelog v2.1.1 (2026-07-15) — SSL guard ครอบไม่ครบ + เก็บ log spam ที่เหลือ:
    - lastPushTime ตั้งเฉพาะหลัง pushToFirebase() ไม่ตั้งตอน pushStatus() → กด Manual แล้ว pushStatus()
      ยิง SSL 13 ครั้ง แต่ guard "เว้น 2 วิก่อน poll" ไม่รู้ตัว → control poll รอบถัดไป (1.5 วิ) ชน SSL
      = เคสที่ guard ตั้งใจกันพอดีแต่ครอบไม่ถึง · ย้ายไปตั้งใน pushStatus() จุดเดียว (ทุก caller ผ่านมันหมด)
      มีมาตั้งแต่ v1.8.0 · ไม่ใช่สาเหตุที่ Serial Monitor ของ Arduino IDE ค้าง (อันนั้นเป็นฝั่ง IDE)
    - latch log ที่เหลืออีก 3 ตัว (แนวเดียวกับ v2.0.0): "[DS18B20] ค่าน้ำผิดปกติ" (สายไม่ต่อ = ขึ้นทุก
      30 วิ ตลอดกาล), "[Firebase] ยังไม่พร้อม" (เน็ตดับ), "[SCHED] นาฬิกายังไม่ sync" (NTP ไม่ติด)
      ทั้ง 3 พิมพ์ครั้งเดียวตอนเข้าสถานะ + พิมพ์อีกครั้งตอนกลับมาปกติ
    - sketch.yaml: บอกวิธีสร้าง config.h — clone ใหม่ build ไม่ผ่านเพราะไฟล์นี้อยู่ใน .gitignore
  Changelog v2.1.0 (2026-07-15) — ปิดช่องโหว่ตอน WiFi หลุด:
    - buzzer/alert เคยอยู่ในกรอบ `if (Firebase.ready())` = เน็ตดับแล้วอากาศร้อนวิกฤต คนหน้างานไม่ได้ยินอะไรเลย
      ทั้งที่ buzzer เป็นอุปกรณ์ local ไม่ต้องใช้เน็ต · ย้าย checkAlerts() ออกนอก guard
      แยกเป็น reportAlert(): serial+buzzer ทำเสมอ · Firebase ส่งเฉพาะตอนมีเน็ต (ยุบ copy-paste 4 ชุดด้วย)
    - WiFi หลุดหลังบูตแล้วต่อไม่กลับ: core auto-reconnect ลองเฉพาะ SSID "ตัวเดิม" เท่านั้น
      ถ้าเราเตอร์ตัวนั้นหายถาวร มันไม่ลองตัวสำรองใน config.h ให้เลย · loop() เรียก wifiMulti.run()
      ทุก 30 วิ ตอนหลุด (ไล่ทุก SSID) → ต่อไม่ติดครบ 30 นาที → ESP.restart()
    - ⚠️ restart นี้เป็นข้อยกเว้นของการถอด runtime recovery layer (2026-07-03): จำกัดเฉพาะ WiFi
      ไม่แตะ sensor/relay, เกณฑ์ยาว 30 นาที, auto control ทำงานต่อได้ตลอดช่วงนั้น (ผู้ใช้ตัดสินใจ 2026-07-15)
    - หมายเหตุ: auto control ไม่พึ่ง WiFi อยู่แล้ว (อ่าน SHT35 + threshold ใน RAM) โรงเรือนคุมตัวเองต่อได้
      ที่หายตอนเน็ตดับคือ: สั่งจาก dashboard, push, hourly log ของชั่วโมงนั้น, NTP resync
  Changelog v2.0.0 (2026-07-15) — ปิดช่องโหว่ "เซนเซอร์เสียแล้วระบบยังสั่งรีเลย์จากค่าเก่า":
    - ⚠️ BREAKING: เซนเซอร์อากาศอ่านพลาดติดกัน ≥ SENSOR_STALE_AFTER (12 ครั้ง ≈ 6 นาที)
      → auto ปิดทั้งพัดลมและปั๊ม + ล้าง latch ทุกตัว + alert ขึ้น dashboard + buzzer
      เดิม: ค่าที่อ่านพลาดไม่เคยเข้า control buffer เลย autoControl() จึงเห็นค่าดีค่าสุดท้ายค้างอยู่
      ตลอดกาล และสั่งรีเลย์ต่อไปเรื่อยๆ บนข้อมูลที่อาจเก่าเป็นชั่วโมง — safety "humidity==0 → ปั๊มปิด"
      ที่เขียนไว้ตั้งแต่ v1.7.0 ยิงไม่ออกจริง เพราะ 0 ตัวนั้นไม่มีทางไปถึง autoControl()
    - SHT35 ให้ทั้งอุณหภูมิ+ความชื้นจากชิปเดียว → ถ้าพังคือค่าเก่าทั้งคู่ ไม่ใช่แค่ความชื้น
      จึงปิดทั้ง 2 ช่อง (ผู้ใช้เลือก) แทน logic เดิมสมัย DHT22 ที่ให้พัดลมยังตามอุณหภูมิต่อ
    - `sensorOk` เป็น input ใหม่ของ computeAutoDecisions() · เลิกใช้ `airHumidity == 0` เป็นสัญญาณ "ตาย"
    - ล้าง latch ตอนเซนเซอร์เสีย เพื่อให้ตอนฟื้นคิดใหม่จากค่าสด ไม่สานต่อสถานะเก่า
    - alert sensor_fail ต้องเช็คก่อน guard "sensor ยังไม่พร้อม" ใน checkAlerts() ไม่งั้นโดนกินทิ้ง
      (readSensors ตั้ง airTemp/airHumidity=0 ตอนพลาด = เข้าเงื่อนไข guard พอดี)
    - quiet window (ปั๊มเพิ่งสวิตช์) ไม่นับเป็น "พลาด" — มันข้ามการอ่าน ไม่ได้อ่านแล้วพัง
    - ไม่พบเซนเซอร์ตอนบูต (สายหลวมก่อนเปิดไฟ) ก็นับเป็น "พลาด" ด้วย — เดิม branch นี้ไม่แตะ counter เลย
      ทำให้ sensor_ok=true/sensor_stale=false ตลอดกาลทั้งที่ไม่เคยอ่านค่าได้ = เงียบสนิท (เคสที่เกิดง่ายที่สุด)
      + ลองสแกนหา 0x44/0x45 ใหม่เป็นระยะ → เสียบสายคืนแล้วฟื้นเอง ไม่ต้องรีบูต
    - alert น้ำ (DS18B20) แยกออกจากบล็อกเซนเซอร์อากาศ — คนละเซนเซอร์กัน SHT35 พังไม่ควรกลืน alert น้ำ
    - alert เซนเซอร์เสีย: ย้ำตอนเข้าสถานะ + ทุก SENSOR_ALERT_REPEAT_MS (10 นาที) ไม่ใช่ทุกรอบ 30 วิ
      (เซนเซอร์เสียไม่หายเอง ต่างจาก alert ร้อน/แห้ง — buzzer เป็น blocking delay ~900ms จะดังทั้งคืน)
    - แก้ log spam: "[Init] Loading control state... OK" พิมพ์ทุก 1.5 วิ (~16 บรรทัดขยะต่อ 1 บรรทัดจริง)
      ท่วม serial จน [AUTO]/[MANUAL] จมหาย · ตอนนี้พิมพ์เฉพาะตอน fail หรือ mode เปลี่ยนจริง
      · "[SHT35] ไม่พบเซนเซอร์" + "[ALERT] ข้าม" ก็ latch เหมือนกัน (เดิมสแปมทุก 30 วิ ตลอดกาล)
      · log control-load ใช้ "เหตุผลเปลี่ยน" เป็นตัวจุด ไม่ใช่ bool — กันซ่อนสาเหตุที่เปลี่ยนกลางทาง
  Changelog v1.8.0 (2026-07-15) — พัดลม+ปั๊มทำงานคู่กัน ทั้งคุมอุณหภูมิและคุมความชื้น:
    - พัดลม (CH3) เพิ่มหน้าที่ 2: เปิดตอนอากาศแห้ง (<humidity_min) ด้วย ไม่ใช่แค่ตอนร้อน
      พัดลมเป็นแบบ "ดูดเข้า" จึงดูดอากาศนอก (ความชื้นสูง) เข้ามา + กระจายละอองน้ำจากปั๊ม — ช่วยเพิ่มความชื้น ไม่ได้ไล่ทิ้ง
    - เดิม: ควบคุมอุณหภูมิ→พัดลมอย่างเดียว, ควบคุมความชื้น→ปั๊มอย่างเดียว · ใหม่: ทั้ง 2 เกณฑ์สั่งทั้ง 2 ช่องพร้อมกัน
    - รื้อ latch เป็น 3 ตัว: ร้อน (คุมพัดลม), แห้ง (คุมทั้ง 2 ช่อง), ปั๊มไล่ร้อน (เกณฑ์เดียวกับร้อน แต่ humid gate ล้างได้)
      พัดลมเป็น latch แล้ว — มี 2 hysteresis loop ต่อ 1 รีเลย์เหมือนปั๊ม จึงใช้ stateless open/close ต่อไม่ได้
    - humid gate ยังคุมเฉพาะปั๊ม: ร้อน+ชื้นเกิน (≥humidity_max) → พัดลมเปิด ปั๊มปิด (เคสเดียวที่ 2 ช่องแยกกัน)
      เพราะพัดลมยังระบายความร้อนได้ฟรี แต่ปั๊มพ่นน้ำในอากาศอิ่มตัวไม่ทำให้เย็น แค่ท่วม
    - ⚠️ gate ต้องอยู่ "ข้างใน" latch (ล้าง latch) ไม่ใช่ AND ที่ output — ไม่งั้น RH แกว่งรอบ humidity_max
      ทำปั๊มกระพริบ (วัดได้ 5 สวิตช์/6 รอบ) ซึ่งเป็นต้นเหตุ sensor latch-up ของโปรเจกต์นี้ · มีเทสต์ crossing กันไว้แล้ว
    - ไม่เพิ่ม threshold ใหม่ · เซนเซอร์ความชื้นตาย (0) → ปั๊มปิดเสมอ, พัดลมยังตามอุณหภูมิได้ (เหมือนเดิม)
  Changelog v1.7.0 (2026-07-14) — evaporative cooling: ปั๊มช่วยพัดลมลดอุณหภูมิ:
    - ปั๊ม (CH4) เพิ่มหน้าที่ 2: เมื่ออากาศร้อน (≥temp_on) เปิดปั๊มช่วยพัดลมระบายความร้อน (พ่นน้ำ+พัดลม = evaporative)
      หน้าที่เดิม (เพิ่มความชื้นเมื่ออากาศแห้ง <humidity_min) ยังอยู่ — ปั๊มเปิดถ้าหน้าที่ใดหน้าที่หนึ่งต้องการ
    - ปั๊ม cooling ข้ามถ้าอากาศชื้นแล้ว (≥humidity_max) — พ่นน้ำในอากาศอิ่มตัวไม่ทำให้เย็น แค่ท่วม
    - ใช้ threshold เดิม (temp_on/temp_off) ไม่เพิ่ม knob · พัดลมยังคุมด้วยอุณหภูมิเหมือนเดิม
    - ปั๊ม 2 หน้าที่ = 2 latch hysteresis แยกกัน (กันค้าง/กระพริบเมื่อหน้าที่หนึ่งเลิกต้องการ)
  Changelog v1.6.0 (2026-07-14) — rewrite auto-control ให้พนักงาน 10 คนเข้าใจง่าย:
    - พัดลม (CH3) คุมด้วย "อุณหภูมิอากาศ" อย่างเดียว — ถอดการผูกกับอุณหภูมิน้ำ (evaporative pad) ออก
      เพราะ 2 เซนเซอร์ต่อ 1 พัดลมทำให้พฤติกรรมเดายาก อธิบายพนักงานไม่ไหว
    - น้ำ (DS18B20) เหลือหน้าที่ แสดง/log/แจ้งเตือน อย่างเดียว ไม่คุมรีเลย์แล้ว
    - เกณฑ์ที่พนักงานตั้ง ลดจาก 9 → 6: พัดลม on/off, ปั๊ม on/off, + alert 3 ตัว (temp/hum/water)
    - เพิ่ม guard: ถ้า threshold ไม่ถูกต้อง (temp_on≤temp_off หรือ humidity_max≤humidity_min) ข้ามรอบ กันรีเลย์กระพริบ
    - water_temp_alert ตั้งค่าได้จาก Firebase แล้ว (เดิม hardcode 35°C)
    - logic บริสุทธิ์ย้ายไป auto_control_logic.h ทั้งหมด — เทสต์ครอบทุกกรณี (21 assertions, g++)
  Changelog v1.5.0 (สะสมตั้งแต่ v1.4.0 — เดิม version comment ไม่ได้ bump ตามมานาน):
    - Relay remap: CH4=ปั๊มน้ำ, CH3=พัดลม 220V, CH1=สำรอง (CH2 ไม่ใช้)
    - DS18B20 hardening: กรองค่า -127/85°C + อ่านซ้ำ (รองรับสายยาว 4m)
    - Pump safety cutoff = 10 นาที (ค่าสุดท้าย, เดิม 5) — 2026-07-07
    - Firebase Auth: ลองซ้ำ 4 ครั้งตอนบูตก่อนรีสตาร์ท กันบอร์ดวิ่งต่อแบบไม่ auth ตลอดไป — 2026-07-07
    - หมายเหตุ: ตั้งใจไม่มี runtime failsafe/recovery layer (ถอดออกแล้ว 2026-07-03 หลัง A/B test
      ยืนยันว่าความไม่เสถียรเกิดจาก noise ฮาร์ดแวร์ ไม่ใช่โค้ด) — จะพิจารณาใหม่หลังแก้ hardware noise แล้ว
    - เปลี่ยนเซนเซอร์อากาศจาก DHT22 → SHT35 (I2C) — 2026-07-11 หลังพิสูจน์แล้วว่า DHT22 ยังกลิตช์เวลา
      ปั๊ม/พัดลมสวิตช์ไม่ว่าจะแก้ firmware ยังไงก็ตาม (ดู A/B test 2026-07-03) — ลองเปลี่ยนไปใช้ I2C
      ที่มี CRC ตรวจสอบความถูกต้องของข้อมูลในตัว แทนโปรโตคอลแบบ single-wire ที่ไม่มี CRC

  Hardware:
    - ESP32 DevKit V1
    - SHT35 (I2C, address 0x44 หรือ 0x45 ตาม ADDR pin) — อุณหภูมิ + ความชื้นอากาศ (แชร์บัส I2C กับ LCD)
    - DS18B20 Waterproof (GPIO4)   — อุณหภูมิน้ำ
    - Relay 4CH Active-LOW (การเดินสายจริง 2026-06-26):
        CH1 GPIO26 — สำรอง (manual/schedule only)
        CH2 GPIO27 — ไม่ได้ใช้
        CH3 GPIO14 — พัดลม 220V AC (ดูดเข้า) — คุมด้วยอุณหภูมิ
        CH4 GPIO25 — ปั๊มน้ำ 24V DC          — คุมด้วยความชื้น + pump safety

  Libraries (Arduino IDE → Manage Libraries):
    - Firebase ESP32 Client by Mobizt
    - OneWire by Paul Stoffregen
    - DallasTemperature by Miles Burton
    - Adafruit SHT31 Library (รองรับ SHT30/31/35 — คำสั่ง I2C ชุดเดียวกัน)
    - LiquidCrystal I2C by Frank de Brabander
*/

#include <WiFi.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <Preferences.h>
#include "config.h"
#include "auto_control_logic.h"

// เผื่อ config.h เก่าไม่มี define นี้ — relay เป็น active-LOW (LOW=เปิด, HIGH=ปิด)
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW true
#endif

// เผื่อ config.h เก่าไม่มี define นี้ — buzzer module 3 ขา (S/VCC/GND) มักเป็น active-LOW เหมือน relay
// (LOW=ดัง, HIGH=เงียบ) ถ้าใช้ buzzer แบบอื่นแล้วเงียบตลอด/ดังกลับด้าน ให้เปลี่ยนเป็น false ใน config.h
#ifndef BUZZER_ACTIVE_LOW
#define BUZZER_ACTIVE_LOW true
#endif

// ── Role → Channel mapping (การเดินสายจริง 2026-06-26) ──
//   index ใน array control: 0=ch1  1=ch2  2=ch3  3=ch4
//   CH3 (GPIO14) = พัดลม (ดูดเข้า) — คุมด้วยอุณหภูมิ
//   CH4 (GPIO25) = ปั๊มน้ำ        — คุมด้วยความชื้น + pump safety
//   CH1 (GPIO26) = สำรอง — manual/schedule เท่านั้น (ไม่มี auto)
//   CH2 = ไม่ได้ใช้ (ซ่อนใน dashboard) — ค้าง OFF เสมอ
#define IDX_FAN   2   // ch3_fan_in  → พัดลม
#define IDX_PUMP  3   // ch4_spare   → ปั๊มน้ำ

// ── LCD I2C (16x2) — สร้าง object หลัง auto-detect address ใน setup() ─────
// (โมดูลส่วนใหญ่เป็น 0x27 แต่บางล็อตเป็น 0x3F — hardcode ผิด address = จอไม่ขึ้นอะไรเลยแม้ backlight ติด)
LiquidCrystal_I2C* lcd = nullptr;
uint8_t lcdPage = 0;  // หน้าปัจจุบัน (สลับทุก 5 วิ)

// ── Firebase Objects ──────────────────────────────────
FirebaseData   fbData;
FirebaseAuth   fbAuth;
FirebaseConfig fbConfig;

// ── Sensor Objects ────────────────────────────────────
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
Adafruit_SHT31 sht35 = Adafruit_SHT31();
uint8_t shtAddr = 0;   // address ที่เจอจริงตอนบูต (0x44/0x45 ตาม ADDR pin) — 0 = ไม่เจอ

// ── Sensor Values ─────────────────────────────────────
float airTemp     = 0.0;
float airHumidity = 0.0;
float waterTemp   = 0.0;

// ── Relay State (actual hardware state) ───────────────
bool ch1_pump   = false;   // CH1 = สำรอง (manual)
bool ch2_fanOut = false;   // CH2 ไม่ได้ใช้
bool ch3_fanIn  = false;   // CH3 = พัดลม (ดูดเข้า)
bool ch4_spare  = false;   // CH4 = ปั๊มน้ำ

// ── Control State จาก Firebase (volatile = RTOS-safe) ─
// ch index: 0=ch1(unused) 1=ch2(unused) 2=ch3_fan 3=ch4_pump
volatile bool  ch_isAuto[4] = {false, false, true,  true};
volatile bool  ch_manual[4] = {false, false, false, false};
// ── Auto-control thresholds (พนักงานตั้งค่าจาก dashboard) ──
// พัดลม (CH3) คุมด้วย "อุณหภูมิอากาศ" · ปั๊ม (CH4) เปิดเมื่อ "อากาศแห้ง" (เพิ่มความชื้น) หรือ
// "อากาศร้อน" (ช่วยพัดลมระบายความร้อนแบบ evaporative — ข้ามถ้าอากาศชื้นเกิน humidity_max)
// แต่ละอุปกรณ์มีเลข ON/OFF คู่กัน (ช่องว่าง = hysteresis กันรีเลย์กระพริบ)
volatile float thresh_temp_on  = TEMP_ON;       // พัดลมเปิดเมื่ออากาศ ≥ ค่านี้ (ร้อน)
volatile float thresh_temp_off = TEMP_OFF;      // พัดลมปิดเมื่ออากาศ ≤ ค่านี้ (เย็น) — ต้อง < temp_on
volatile float thresh_hum_min  = HUMIDITY_MIN;  // ปั๊มเปิดเมื่อความชื้น < ค่านี้ (แห้ง)
volatile float thresh_hum_max  = 75.0;          // ปั๊มปิดเมื่อความชื้น ≥ ค่านี้ (ชื้น) — ต้อง > humidity_min
volatile float thresh_hum_vent = 80.0;          // v2.7.0: พัดลมเปิดไล่ความชื้นเมื่อ RH ≥ ค่านี้ (เช่นฝนตก) · ≤0 = ปิดฟีเจอร์
// เกณฑ์แจ้งเตือน/buzzer (แยกจาก auto-control — ไม่สั่งรีเลย์ แค่เตือน) sync กับ dashboard
volatile float thresh_temp_alert       = 38.0;
volatile float thresh_hum_alert        = 40.0;
volatile float thresh_water_temp_alert = 35.0;  // configurable จาก Firebase
volatile bool  buzzerEnabled   = true;   // ปิด/เปิดเสียงเตือนจาก dashboard

// ── Schedule State ────────────────────────────────────
// ch index: 0=ch1(unused) 1=ch2(unused) 2=ch3_fan 3=ch4_pump
volatile bool ch_schedEnabled[4] = {false, false, false, false};
char ch_schedOn[4][6]  = {"07:00","07:00","07:00","07:00"};
char ch_schedOff[4][6] = {"18:00","18:00","18:00","18:00"};

// ── Hourly Log Accumulators (RTC memory — รอดข้าม reset) ──
// เดิมเป็น global ธรรมดา = reset ทีนึงข้อมูลสะสมทั้งชั่วโมงหายเกลี้ยง (อาการที่รายงานมา 2026-07-16)
// ⚠️ ต้องเป็น RTC_NOINIT_ATTR ไม่ใช่ RTC_DATA_ATTR — RTC_DATA_ATTR อยู่ใน segment .rtc.data ซึ่ง
//    bootloader โหลดทับจากแฟลชทุก reset ที่ไม่ใช่ deep-sleep wake = ค่าหายอยู่ดี ไม่ได้แก้อะไรเลย
//    RTC_NOINIT_ATTR ไม่ถูกแตะเลย → รอด panic/WDT/ESP.restart() · แลกกับว่าห้ามมี initializer
//    และตอนไฟดับ/brownout เนื้อในเป็นขยะ → ต้องมี magic ตรวจ (ดู setup())
// รอด: PANIC, TASK_WDT, ESP.restart() · ไม่รอด: ไฟดับ, brownout (RTC RAM ไม่การันตีตอนไฟตก)
#define RTC_MAGIC 0x5A17FA02   // เปลี่ยนเลขนี้ = บังคับให้ accumulator เริ่มใหม่หลัง flash firmware ที่ layout เปลี่ยน
RTC_NOINIT_ATTR uint32_t rtcMagic;
RTC_NOINIT_ATTR float h_sumAT, h_maxAT, h_minAT;
RTC_NOINIT_ATTR float h_sumAH, h_maxAH, h_minAH;
RTC_NOINIT_ATTR float h_sumWT, h_maxWT, h_minWT;
RTC_NOINIT_ATTR int   h_count;     // จำนวน sample อากาศ
RTC_NOINIT_ATTR int   h_countWT;   // จำนวน sample น้ำที่อ่านได้ (แยกต่างหาก — DS18B20 สายยาวอาจอ่านพลาดบางครั้ง)
// ลอง restart แก้ WiFi stack ค้างไปแล้วหรือยัง — จำข้าม reboot เพื่อไม่ให้ reboot ซ้ำทุก 30 นาทีตลอดกาล
// ตอนเราเตอร์เจ๊งจริง (ผู้ใช้เลือก "one-shot" 2026-07-16)
RTC_NOINIT_ATTR bool     rtcWifiRestartDone;
RTC_NOINIT_ATTR uint32_t rtcBootCount;   // บูตกี่ครั้งนับตั้งแต่ไฟดับล่าสุด — เลขนี้พุ่ง = กำลัง reboot loop
// path ของ "ชั่วโมงที่ accumulator กำลังสะสมอยู่" ("" = ยังไม่เริ่ม/นาฬิกายังไม่ติด) — ต้องอยู่ RTC คู่กับ accumulator
// hourly log เลิกใช้ millis (LOG_INTERVAL) แล้ว: millis รีเซ็ตตอน reboot แต่ accumulator อยู่ RTC ไม่รีเซ็ต =
// เข้ากันไม่ได้ · reboot ก่อนครบชั่วโมง → ไม่เคย flush + สะสมข้ามชั่วโมง/ข้าม reboot แล้วตกถังผิดชั่วโมง
// เปลี่ยนมา flush ตาม "ขอบชั่วโมงนาฬิกาจริง" แทน — reboot กี่ครั้งก็ยังตกถังถูกชั่วโมง (path จำไว้ใน RTC)
RTC_NOINIT_ATTR char rtcLogPath[24];     // "/logs/YYYY-MM-DD/HH" (ยาวสุด 19 ตัว +null) — 24 เผื่อเหลือ

// ── Control-loop Averaging (แยกจาก hourly-log accumulator ด้านบนโดยสิ้นเชิง) ──
// N=3 รอบ — กัน relay สั่งเปลี่ยนจากค่าเพี้ยนชั่วครู่ครั้งเดียว
// เก็บเฉพาะอากาศ (temp+humidity) — auto-control ใช้แค่ 2 ตัวนี้ · น้ำไม่คุมรีเลย์แล้ว (alert-only)
#define CTRL_AVG_N 3
float ctrlBufAT[CTRL_AVG_N] = {0};   // buffer อุณหภูมิอากาศ (คุมพัดลม)
float ctrlBufAH[CTRL_AVG_N] = {0};   // buffer ความชื้นอากาศ (คุมปั๊ม)
int   ctrlBufATCount = 0, ctrlBufIdx = 0;   // อากาศ+ความชื้น sample พร้อมกันเสมอ ใช้ index/count ร่วม

// ── Timing ────────────────────────────────────────────
unsigned long lastSensorTime = 0;
unsigned long lastNtpSync    = 0;

// ── Safety / Worst-case Protection (v1.3.0) ───────────
#define WDT_TIMEOUT_S        60                // watchdog: reboot ถ้า loop ค้างเกิน 60 วิ
#define PUMP_MAX_RUNTIME_MS  (15UL*60*1000)    // ปั๊มเดินต่อเนื่องได้สูงสุด 15 นาที (auto/schedule) — ยืดจาก 10 นาที ให้ความชื้นสะสมได้นานขึ้น 2026-07-20 (เดิม 10 นาที / ก่อนหน้า 5)
#define PUMP_COOLDOWN_MS     (5UL*60*1000)     // หลังตัด พักปั๊ม 5 นาที
// v2.8.0: พัดลมกลับมาพักคู่ปั๊ม (ยกเลิก decouple v2.6.0 ตามคำสั่งหน้างาน) — max run + cooldown เท่ากัน
#define FAN_MAX_RUNTIME_MS   PUMP_MAX_RUNTIME_MS  // พัดลมเดินต่อเนื่องได้สูงสุด 15 นาที (เท่าปั๊ม)
#define FAN_COOLDOWN_MS      PUMP_COOLDOWN_MS     // หลังตัด พักพัดลม 5 นาที (พร้อมปั๊ม)
// VENT_HYST ย้ายไปเป็น const ใน auto_control_logic.h (แหล่งความจริงเดียวฝั่ง C++)
// ห้าม #define ซ้ำที่นี่ — macro จะ shadow const แล้วจูนค่าที่ header ไม่มีผลกับ .ino
#define AIR_TEMP_MIN         -20.0              // ช่วงค่าอุณหภูมิที่สมเหตุผล (นอกช่วง = sensor เพี้ยน)
#define AIR_TEMP_MAX          70.0
// DS18B20: ช่วงอุณหภูมิน้ำสมเหตุผล — นอกช่วงนี้ = ค่าเสีย (-127 สายหลุด / 85.0 reset อ่านไม่ทัน / noise จากสายยาว)
#define DS_WATER_MIN        -20.0
#define DS_WATER_MAX         80.0              // น้ำในฟาร์มไม่เกินนี้ → 85.0 (sentinel) ถูกตัดออกอัตโนมัติ
#define DS_READ_RETRY        2                 // อ่าน DS18B20 ซ้ำได้กี่ครั้งถ้าค่าเสีย (สายยาว 4m รบกวน)
#define DS_REINIT_EVERY      3                 // อ่านน้ำไม่ได้ติดกัน N รอบ → ds18b20.begin() สแกนบัส 1-Wire ใหม่
                                               // ⚠️ จำเป็น ไม่ใช่ของแถม — DallasTemperature::getAddress() มีบรรทัด
                                               // `if (index < devices)` โดย devices ถูกตั้งจาก begin() ที่เดียว
                                               // begin() ตอนบูตไม่เจอ (สายหลวม/ยังไม่ได้ต่อ) → devices=0 ตลอดกาล
                                               // → getTempCByIndex(0) คืน -127 ทันทีโดยไม่แตะบัสเลย แม้เสียบสายคืนแล้ว
                                               // = เสียบสายตอนบอร์ดรันอยู่แล้วไม่มีวันขึ้น ต้องรีบูตเท่านั้น (แนวเดียวกับ
                                               // เคส SHT35 ที่ v2.0.0 แก้ไป — DS18B20 ตกสำรวจ) · begin() ~150ms worst case
// เดิม DHT22 อยู่ใกล้ relay/สาย pump บน expansion board มาก — noise ตอน pump switch ทำอ่านพลาด
// SHT35 ยังไม่ยืนยันว่าเจอปัญหาเดียวกันไหม (ขึ้นกับตำแหน่งที่ติดตั้งจริง) — คงกลไกนี้ไว้เป็นเซฟตี้เน็ตก่อน
#define PUMP_SWITCH_QUIET_MS  3000             // เว้น 3 วิหลังปั๊มสวิตช์ ก่อนอ่าน sensor อากาศรอบถัดไป (รอ noise transient สงบ)
#define SHT_REINIT_EVERY      3                 // ลอง sht35.begin() re-init ทุกๆ N ครั้งที่อ่านพลาด — self-heal เบาๆ ไม่ผูกกับ emergency mode ใดๆ
#define SENSOR_STALE_AFTER    12                // อ่านพลาดติดกัน N ครั้ง (~30วิ/ครั้ง = 6 นาที) → ถือว่าเซนเซอร์เชื่อไม่ได้ → auto ปิดทุกช่อง
                                                // เลือก 12 เพราะทนต่อ glitch ชั่วคราวได้ (noise/CRC พลาดเป็นครั้งคราว) แต่ค่าเก่าสุดไม่เกิน 6 นาที
                                                // หมายเหตุ: quiet window (ปั๊มเพิ่งสวิตช์) ไม่นับเป็นพลาด — มันข้ามการอ่าน ไม่ได้อ่านแล้วพัง
#define SENSOR_ALERT_REPEAT_MS (10UL*60*1000)   // เซนเซอร์เสียแล้วไม่หายเอง (ต่างจาก alert ร้อน/แห้งที่หายเองได้) — ย้ำเตือนทุก 10 นาที
                                                // ไม่ใช่ทุกรอบ 30 วิ ไม่งั้น buzzer ดังทั้งคืนและเขียน Firebase ซ้ำข้อความเดิมเป็นพันครั้ง
// WiFi — ESP32 core auto-reconnect เองได้ แต่เฉพาะ SSID "ตัวเดิม" ที่เคยต่อ ถ้าตัวนั้นหายถาวร
// (เราเตอร์เจ๊ง/เปลี่ยนชื่อ) มันจะไม่ลองตัวสำรองใน config.h ให้เลย ต้อง wifiMulti.run() เท่านั้น
#define WIFI_RETRY_EVERY_MS     (30UL*1000)     // ตอนหลุด: ลอง wifiMulti.run() (ไล่ทุก SSID) ทุก 30 วิ
#define WIFI_CONNECT_TIMEOUT_MS 15000           // timeout ที่ส่งเข้า wifiMulti.run() ตอนบูต — ให้ "มัน" รอ อย่าไปวนเรียกซ้ำ
                                                // run() ข้างในทำ scanNetworks()+disconnect()+begin()+รอ ครบชุดอยู่แล้ว
                                                // เรียกซ้ำๆ = แต่ละรอบ disconnect() ฆ่า attempt ของรอบก่อนทิ้ง → ต่อติดแบบสุ่ม
#define WIFI_OFFLINE_RESTART_MS (30UL*60*1000)  // ไม่ติดครบ 30 นาที → ESP.restart() "ครั้งเดียว" (ผู้ใช้ตัดสินใจ 2026-07-16)
                                                // เป็นข้อยกเว้นของการถอด runtime recovery layer เมื่อ 2026-07-03: จำกัดเฉพาะ WiFi
                                                // (ไม่ใช่ sensor/relay) เกณฑ์ยาว 30 นาที และ auto control ทำงานต่อได้ตลอดช่วงนั้น
                                                // reboot คือทางเดียวที่แก้ WiFi stack ค้างจริง — แต่แก้ "เราเตอร์เจ๊ง" ไม่ได้เลย
                                                // ⚠️ จึงยิงได้ครั้งเดียว (rtcWifiRestartDone) ไม่งั้นเราเตอร์ดับยาว = reboot ทุก 30 นาที
                                                // ตลอดกาล ทุกรอบ relay ดับ + hourly หาย = ทำลายความต่อเนื่องที่ตั้งใจจะปกป้อง
                                                // ต่อ WiFi ได้เมื่อไหร่ = เคลียร์ flag คืนสิทธิ์ให้เคสค้างครั้งหน้า
#define FB_AUTH_RETRY_MS        (60UL*1000)     // Firebase auth ไม่ผ่าน → retry ทุก 60 วิ (ไม่ reboot — ดู v2.2.0)
// Heartbeat LED (GPIO2 — ตรงกับ LED บนบอร์ด ESP32 DevKit V1 ส่วนใหญ่) — กระพริบ = loop() ยังรันอยู่
// ถ้าเจอ "ค้าง" ให้ดู LED นี้: กระพริบต่อ = loop() ไม่ตาย (ปัญหาอยู่ที่ฟังก์ชันใดฟังก์ชันหนึ่งค้างเงียบๆ)
// หยุดกระพริบ/ดับสนิท = loop() ตายจริง หรือชิป reset วนเร็วจนไม่เห็นจังหวะ
#define PIN_STATUS_LED        2
#define HEARTBEAT_BLINK_MS    500
#define NTP_RESYNC_MS        (6UL*3600*1000)   // sync NTP ใหม่ทุก 6 ชม. (นาฬิกาปกติดีอยู่แล้ว แค่กันเพี้ยนสะสม)
#define NTP_RETRY_INVALID_MS (60UL*1000)       // แต่ถ้านาฬิกายัง "ไม่ valid" เลย → retry ทุก 60 วิ ไม่ใช่รอ 6 ชม.
                                               // ⚠️ v2.2.0 ทำให้เคสนี้เกิดจริงเป็นครั้งแรก: setup() ข้าม syncNTP() ตอนบูตแบบ
                                               // offline (เมื่อก่อน reboot ทิ้ง) แต่ยังเซ็ต lastNtpSync=millis() → loop รอ 6 ชม.
                                               // ระหว่างนั้น timeValid()=false → schedule ไม่ทำงาน + hourly log ถูกทิ้งทุกชั่วโมง
                                               // ครอบเคส "บูต online แต่ NTP server ช้า/ไม่ตอบตอนนั้น" ด้วย (นาฬิกาไม่ติดเหมือนกัน)
#define CONTROL_POLL_MS      1500              // poll คำสั่งควบคุมทุก 1.5 วิ (เดิม 5 วิ — relay ตอบไวขึ้น)
unsigned long pumpOnSince     = 0;             // เวลาเริ่มเดินปั๊ม (0 = หยุด)
unsigned long pumpLockUntil   = 0;             // ล็อกห้ามเปิดปั๊มจนถึงเวลานี้ (cooldown)
unsigned long fanOnSince      = 0;             // v2.8.0: เวลาเริ่มเดินพัดลม (0 = หยุด) — สำหรับ max-runtime
unsigned long fanLockUntil    = 0;             // v2.8.0: ล็อกห้ามเปิดพัดลมจนถึงเวลานี้ (พักคู่ปั๊ม — กลับมาจาก v2.6.0)
Preferences   ctrlPrefs;                       // NVS (flash) เก็บ control config ให้รอด reboot + ไฟดับ (v2.4.0)
bool          controlDirty   = false;          // มี config เปลี่ยนจาก dashboard รอบนี้ → เซฟลง NVS (กันเขียนทุก poll = NVS wear)
unsigned long lastPumpSwitchTime = 0;          // เวลาที่ปั๊ม (CH4) สวิตช์ล่าสุด (0 = ยังไม่เคยสวิตช์) — ใช้เว้น quiet window ก่อนอ่าน sensor อากาศ
int  airSensorFailCount = 0;                   // นับ sensor อากาศอ่านพลาดติดกัน — ใช้ trigger re-init เป็นระยะ + โชว์ status/sensor_ok
bool waterSensorOk  = true;                    // DS18B20 อ่านได้ไหม
int  waterFailCount = 0;                       // นับรอบที่อ่านน้ำไม่ได้ติดกัน — trigger สแกนบัส 1-Wire ใหม่ (ดู DS_REINIT_EVERY)
String controlLoadError = "";                  // เหตุผลที่โหลด control พลาดล่าสุด ("" = ปกติ) — เก็บ "เหตุผล" ไม่ใช่แค่ bool
                                               // เพื่อให้ log ใหม่เมื่อ "สาเหตุเปลี่ยน" (เน็ตหลุด → auth พัง) ไม่ใช่เงียบยาวจนไล่ผิดทาง
bool shtMissingLogged  = false;                // latch: พิมพ์ "ไม่พบเซนเซอร์" ไปแล้ว (กัน log ท่วมทุก 30 วิ)
bool airNotReadyLogged = false;                // latch: พิมพ์ "sensor ยังไม่พร้อม" ไปแล้ว
bool airStaleLatched   = false;                // latch: เข้าสถานะเซนเซอร์เสียแล้ว — ใช้จับ "ขอบ" ตอนเข้า/ออก
unsigned long wifiOfflineSince = 0;            // millis() ตอน WiFi หลุด (0 = ออนไลน์อยู่) — ใช้นับครบ 30 นาทีก่อน restart
unsigned long lastPushTime = 0;                // millis() ที่ push ขึ้น Firebase ครั้งล่าสุด — loop() เว้น 2 วิก่อน poll กัน SSL ชน
                                               // ตั้งใน pushStatus() (จุดเดียว) เพราะทั้ง pushToFirebase() และ manual-change path เรียกผ่านมันหมด
bool fbNotReadyLogged = false;                 // latch: พิมพ์ "Firebase not ready" ไปแล้ว (กัน log ท่วมตอนเน็ตดับ)
bool firebaseAuthed = false;                   // signUp ผ่าน + Firebase.begin() แล้ว (2 อย่างเกิดคู่กันเสมอ ใช้ flag เดียว)
                                               // ตั้ง true ครั้งเดียวใน tryFirebaseAuth() ไม่เคยกลับเป็น false — begin() จึงถูกเรียกครั้งเดียว
esp_reset_reason_t bootResetReason = ESP_RST_UNKNOWN;   // สาเหตุ reset รอบนี้ — เก็บไว้ push ขึ้น dashboard
bool waterBadLogged   = false;                 // latch: พิมพ์ "ค่าน้ำผิดปกติ" ไปแล้ว (สายไม่ต่อ = จะขึ้นทุก 30 วิ ตลอดกาล)
bool schedNoTimeLogged = false;                // latch: พิมพ์ "นาฬิกายังไม่ sync" ไปแล้ว
// 3 latch (hysteresis คนละชุด) — autoControl() คำนวณใหม่ทุกรอบแล้วเก็บกลับที่นี่
bool airHotOn   = false;                       // latch: อากาศร้อนอยู่ (≥temp_on จนกว่าจะ ≤temp_off) — คุมพัดลม
bool airDryOn   = false;                       // latch: อากาศแห้งอยู่ (<humidity_min จนกว่าจะ ≥humidity_max) — คุมทั้ง 2 ช่อง
bool pumpHeatOn = false;                       // latch: ปั๊มไล่ร้อนอยู่ — เกณฑ์เดียวกับ airHotOn แต่ถูกล้างเมื่ออากาศอิ่มตัว (กันปั๊มกระพริบที่เส้น humidity_max)
bool airWetOn   = false;                       // v2.7.0 latch: อากาศชื้นเกิน (≥humidity_vent จนกว่าจะ ≤vent-5%) — พัดลมไล่ความชื้น

// ── Forward Declarations ──────────────────────────────
void readSensors();
void autoControl();
bool applyManualControl();
void pushStatus();
void setRelay(int pin, bool state);
void pushToFirebase();
void checkAlerts();
void pushHourlyLog(const String& path);
void resetAccumulators();
void loadControlFromFirebase();
void syncNTP();
String getHourlyPath();
void checkSchedule();
void updateLCD();
void buzzerBeep(int times, int onMs = 200, int offMs = 150);
void pumpSafetyCheck();
bool timeValid();
float avgCtrlAT();
float avgCtrlAH();
bool tryFirebaseAuth();

// ─────────────────────────────────────────────────────
// ⚠️ ใช้ตัวนี้แทน Firebase.ready() ทุกที่
// v2.2.0 เลิก reboot ตอน auth ไม่ผ่าน = เกิดสถานะใหม่ที่เมื่อก่อนเป็นไปไม่ได้เลย: บอร์ดวิ่งอยู่ทั้งที่
// Firebase.begin() ยังไม่เคยถูกเรียก (เดิม setup() การันตีว่าถ้าถึง loop() แปลว่า begin() ผ่านแล้วเสมอ
// เพราะไม่ผ่านมันก็ ESP.restart() ไปแล้ว) · ไม่พึ่งว่า lib จะเช็ค config==nullptr ให้ — เช็คเองตรงนี้
static inline bool fbReady() { return firebaseAuthed && Firebase.ready(); }

// ─────────────────────────────────────────────────────
// สาเหตุ reset รอบล่าสุด — เดิมโค้ดไม่เคยถามค่านี้เลย ทั้งที่ ESP32 เก็บให้ฟรีตั้งแต่บูต
// "บอร์ดรีสตาร์ทเอง" จึงไล่ไม่ได้ว่าเป็นอะไร ซึ่งแต่ละอย่างแก้คนละทางสิ้นเชิง:
//   BROWNOUT  → ปัญหาไฟ (relay/ปั๊มสวิตช์ดึงกระแสจนแรงดันตก) — แก้ที่ฮาร์ดแวร์ ไม่ใช่โค้ด
//   PANIC     → โค้ด crash (exception/stack overflow/heap หมด) — ดู backtrace + heap trend
//   TASK_WDT  → loop() ค้างเกิน 60 วิ (SSL/I2C ค้าง) — หาว่าค้างที่ไหน
//   SW        → ESP.restart() ของเราเอง (WiFi 30 นาที) — ไม่ใช่บั๊ก
//   POWERON   → ไฟดับจริง/กด EN — ไม่ใช่บั๊ก
static const char* resetReasonStr(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON (เสียบไฟใหม่/กดปุ่ม EN)";
    case ESP_RST_EXT:       return "EXT (reset จากขาภายนอก)";
    case ESP_RST_SW:        return "SW (ESP.restart() จากโค้ดเราเอง)";
    case ESP_RST_PANIC:     return "PANIC *** โค้ด crash (exception/stack overflow) ***";
    case ESP_RST_INT_WDT:   return "INT_WDT *** interrupt watchdog ***";
    case ESP_RST_TASK_WDT:  return "TASK_WDT *** loop() ค้างเกิน 60 วิ ***";
    case ESP_RST_WDT:       return "WDT *** watchdog อื่น ***";
    case ESP_RST_BROWNOUT:  return "BROWNOUT *** ไฟตก — สงสัย relay/ปั๊มสวิตช์ดึงกระแส ***";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP (ไม่ควรเจอ — เราไม่ได้ใช้ deep sleep)";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "UNKNOWN";
  }
}

// ─────────────────────────────────────────────────────
// Firebase Auth (Anonymous) — เรียกได้ทั้งตอน setup() และ retry จาก loop()
// เดิม: auth พลาด 4 ครั้งตอนบูต → ESP.restart() · เจตนาดี (กันบอร์ดวิ่งต่อแบบไม่ auth ตลอดไป เงียบสนิท)
// แต่ผลจริงคือ reboot loop ตอนเน็ต/Firebase มาช้ากว่าบอร์ด — ซึ่งเป็นเรื่องปกติมากตอนไฟกลับมาทั้งตึก
// ตอนนี้กันเคสเดิมด้วยการ "retry จริงทุก 60 วิ + log" แทนการ reboot · auto control ไม่ต้องใช้ Firebase อยู่แล้ว
bool tryFirebaseAuth() {
  if (!Firebase.signUp(&fbConfig, &fbAuth, "", "")) return false;
  if (!firebaseAuthed) {   // ครั้งแรกที่ auth ผ่าน → begin() ครั้งเดียว (flag ไม่เคยกลับเป็น false)
    Firebase.begin(&fbConfig, &fbAuth);
    // ⚠️ false โดยเจตนา — ตั้งแต่ v2.1.0 loop() จัดการ WiFi เองด้วย wifiMulti.run() (ไล่ทุก SSID)
    // ถ้าเปิด true ไว้ Firebase lib จะไล่ reconnect SSID เดิมของมันเองสวนกัน = 2 ตัวแย่งวิทยุตัวเดียว
    // อาการ: ต่อติดๆ หลุดๆ แบบสุ่ม อธิบายไม่ได้
    Firebase.reconnectWiFi(false);
    fbData.setBSSLBufferSize(512, 512);
    firebaseAuthed = true;
  }
  return true;
}

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Greenhouse IoT Smart Farm v2.8.0 ===");

  // ── Boot diagnostics ───────────────────────────────
  // พิมพ์ก่อนอย่างอื่นทั้งหมด — ถ้าบอร์ดค้างตอนบูต อย่างน้อยได้รู้ว่ารอบก่อนตายเพราะอะไร
  bootResetReason = esp_reset_reason();
  Serial.printf("[BOOT] สาเหตุ reset: %s\n", resetReasonStr(bootResetReason));
  Serial.printf("[BOOT] Heap ว่าง %u bytes (ก้อนต่อเนื่องใหญ่สุด %u)\n",
                ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  // ── กู้ hourly accumulator จาก RTC memory ──────────
  // magic ไม่ตรง = RTC RAM เป็นขยะ (ไฟดับ/brownout/flash firmware ใหม่) → เริ่มนับใหม่
  if (rtcMagic != RTC_MAGIC) {
    rtcMagic = RTC_MAGIC;
    resetAccumulators();
    rtcWifiRestartDone = false;
    rtcBootCount = 0;
    rtcLogPath[0] = '\0';   // ยังไม่รู้ว่าสะสมของชั่วโมงไหน — loop() ตั้งให้เองเมื่อนาฬิกาติด
    Serial.println("[BOOT] RTC memory ว่าง/เป็นขยะ (ไฟดับ, brownout หรือ flash ใหม่) — เริ่มสะสม hourly ใหม่");
  } else {
    Serial.printf("[BOOT] กู้ hourly accumulator จาก RTC ได้ — สะสมไว้แล้ว %d sample อากาศ / %d น้ำ (ไม่ต้องเริ่มนับใหม่)\n",
                  h_count, h_countWT);
  }
  rtcBootCount++;
  Serial.printf("[BOOT] บูตครั้งที่ %lu นับตั้งแต่ไฟดับล่าสุด%s\n", (unsigned long)rtcBootCount,
                rtcBootCount >= 5 ? "  *** เลขนี้พุ่ง = กำลัง reboot loop ให้ดูสาเหตุ reset ด้านบน ***" : "");

  // Heartbeat LED — เริ่มกระพริบตั้งแต่ต้น setup() เพื่อ debug ว่าติดค้างช่วงไหนของการบูต
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  // Relay: ปิดทั้งหมดก่อน (boot-safe) — active-LOW → HIGH = ปิด
  const int relayPins[] = {PIN_RELAY_CH1, PIN_RELAY_CH2, PIN_RELAY_CH3, PIN_RELAY_CH4};
  for (int p : relayPins) { pinMode(p, OUTPUT); digitalWrite(p, RELAY_ACTIVE_LOW ? HIGH : LOW); }

  // ── restore control config จาก NVS (v2.4.0) ──────────
  // ก่อนต่อ WiFi — ถ้าบูตแบบ offline (ไฟดับกลับมาแต่เน็ตยังไม่มา หรือ WiFi 30 นาที restart) จะคุมด้วย
  // ค่าล่าสุดที่คนตั้งไว้ ไม่ใช่ compile default · แล้วขับ relay ตาม manual state ที่ restore ทันที
  // (applyManualControl ปกติอยู่หลัง fbReady gate = ออฟไลน์ไม่เคยทำงาน → ต้องเรียกตรงนี้ให้สถานะ manual ติดตั้งแต่บูต)
  loadControlState();
  applyManualControl();

  // Buzzer — boot-safe: ตั้งเป็น "เงียบ" ก่อน แล้วทดสอบดังสั้นๆ 1 ครั้ง แล้วกลับไปเงียบ
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW);   // เงียบ
  buzzerBeep(1, 100, 0);                                       // ทดสอบดัง 100ms แล้วกลับเงียบ
  Serial.println("Buzzer Ready");

  // I2C scanner — หา address ของ LCD (โมดูลส่วนใหญ่ 0x27 บางล็อต 0x3F) และ SHT35 (0x44/0x45 ตาม ADDR pin)
  // ในรอบเดียว — ทั้งสองตัวแชร์บัส I2C เดียวกัน (SDA=21, SCL=22)
  // เดิม hardcode 0x27 ตายตัว — ถ้าโมดูลจริงเป็น 0x3F จะเขียนไปที่ address ที่ไม่มีใครตอบ
  // (I2C write ไป address ที่ไม่มีอุปกรณ์จะเงียบ ไม่ error) ทำให้ backlight ติด (จัมเปอร์ไฟตรง)
  // แต่ตัวอักษรไม่ขึ้นเลย — เป็นสาเหตุที่พบบ่อยที่สุดของอาการนี้
  Wire.begin();
  Wire.setClock(50000);   // ลดจาก default 100kHz — I2C ช้าลงแต่ทนต่อ noise บนบอร์ดที่มี relay/ปั๊มได้มากขึ้น
  uint8_t lcdAddr = 0, shtAddrFound = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] พบอุปกรณ์ที่ 0x%02X\n", a);
      if (lcdAddr == 0 && (a == 0x27 || a == 0x3F)) lcdAddr = a;   // เจอ address LCD ที่รู้จัก
      if (shtAddrFound == 0 && (a == 0x44 || a == 0x45)) shtAddrFound = a;   // เจอ address SHT35
    }
  }
  if (lcdAddr == 0) {
    Serial.println("[I2C] ไม่พบ LCD ที่ 0x27/0x3F — เช็คสาย SDA(21)/SCL(22)/VCC/GND หรือ contrast pot");
  } else {
    lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd->setCursor(0, 0); lcd->print("SmartFarm v2.8.0");
    lcd->setCursor(0, 1); lcd->print("Starting...");
    Serial.printf("LCD Ready (address 0x%02X)\n", lcdAddr);
  }

  // WiFi (WiFiMulti — ลองทุกเครือข่ายใน config.h อัตโนมัติ)
  // ⚠️ wifiMulti.run() ไม่ใช่ poll ราคาถูก — ข้างในมันทำ scanNetworks() (~2-4 วิ) → WiFi.disconnect()
  //    → WiFi.begin() → รอจนครบ timeout ของมันเอง ครบชุดในการเรียกครั้งเดียว
  //    เดิมวน run() 40 ครั้ง + delay(500): แต่ละรอบ disconnect() ไปฆ่า attempt ของรอบก่อนที่กำลังจะติดพอดี
  //    = churn ได้ถึง ~6 นาที และผลลัพธ์สุ่ม ("บางทีติดบางทีไม่ติด") → เรียกครั้งเดียว ให้ "มัน" รอเอง
  WiFi.mode(WIFI_STA);
  for (auto& n : wifiNetworks) wifiMulti.addAP(n.ssid, n.pass);
  Serial.print("Connecting WiFi... ");
  wifiMulti.run(WIFI_CONNECT_TIMEOUT_MS);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("OK — SSID: " + WiFi.SSID() + " IP: " + WiFi.localIP().toString());
  } else {
    // ⚠️ ห้าม ESP.restart() ตรงนี้เด็ดขาด — loop() ออกแบบมาให้ทน offline ได้ 30 นาทีและคุมโรงเรือน
    // ต่อได้ตลอดช่วงนั้น (auto control อ่าน SHT35 + threshold ใน RAM ไม่ต้องใช้เน็ตเลย)
    // setup() ที่ reboot ตอนต่อไม่ติด = ขัดกับ loop() เอง และทำให้ restart ครั้งเดียวจากสาเหตุอะไรก็ตาม
    // + เราเตอร์ยังไม่ฟื้น → reboot loop ไม่รู้จบ ทุกรอบ relay ดับ + accumulator หาย
    // = ต้นเหตุอาการ "ระบบไม่ต่อเนื่อง" ที่รายงานมา 2026-07-16 · loop() จะไล่ต่อให้เองทุก 30 วิ
    Serial.println("FAILED — วิ่งต่อแบบ offline (ไม่ reboot) · auto control ทำงานปกติ · loop() ลองใหม่ทุก 30 วิ");
  }

  // NTP (UTC+7 ประเทศไทย) — ต้องการสำหรับ path ของ hourly log
  // ข้ามถ้าไม่มีเน็ต — ไม่งั้นเสียเวลาบูตฟรี 10 วิ (syncNTP วน 20 × 500ms) ทั้งที่รู้อยู่แล้วว่าไม่ติด
  if (WiFi.status() == WL_CONNECTED) syncNTP();
  else Serial.println("NTP ข้าม — ยังไม่มี WiFi (loop() sync ให้เองตอนเน็ตกลับมา)");
  lastNtpSync = millis();

  // Firebase Auth (Anonymous) — ลองซ้ำถ้าล้มเหลว (เน็ต/Firebase สะดุดชั่วคราวตอนบูต)
  // เจตนาเดิม (v1.5.0) คือกันบอร์ดวิ่งต่อแบบไม่ auth ตลอดไป = ดูปกติทุกอย่างแต่ไม่มีข้อมูลขึ้น dashboard เลย
  // ⚠️ เจตนานั้นยังอยู่ แต่เปลี่ยนวิธี: เดิมใช้ ESP.restart() ซึ่งกลายเป็น reboot loop ตอน Firebase มาช้ากว่าบอร์ด
  // (เกิดประจำตอนไฟกลับมาทั้งตึก: ESP32 บูตเร็วกว่าเราเตอร์) · ตอนนี้ loop() retry ทุก 60 วิ + log แทน
  fbConfig.database_url = FIREBASE_HOST;
  fbConfig.api_key      = FIREBASE_API_KEY;
  if (WiFi.status() == WL_CONNECTED) {
    for (int a = 1; a <= 4 && !firebaseAuthed; a++) {
      if (tryFirebaseAuth()) {
        Serial.println("Firebase Auth OK (anonymous) — Firebase Ready");
      } else {
        Serial.printf("Firebase Auth FAILED (ครั้งที่ %d/4): %s\n", a, fbConfig.signer.signupError.message.c_str());
        if (a < 4) delay(2000);
      }
    }
  }
  if (!firebaseAuthed) {
    Serial.println("Firebase ยังไม่ auth — วิ่งต่อ (ไม่ reboot) · loop() จะลองใหม่ทุก 60 วิ จนกว่าจะผ่าน");
  }

  // DS18B20 — ไม่เจอตอนนี้ก็ไม่เป็นไรแล้ว readSensors() สแกนบัสใหม่ให้เองทุก ~90 วิ (ดู DS_REINIT_EVERY)
  ds18b20.begin();
  if (ds18b20.getDeviceCount() > 0) {
    Serial.printf("DS18B20 พบ %d ตัว\n", ds18b20.getDeviceCount());
  } else {
    Serial.println("[DS18B20] ไม่พบเซนเซอร์บนบัส 1-Wire (GPIO4) — เช็ค: pull-up 4.7k ระหว่าง data กับ 3V3,");
    Serial.println("[DS18B20] VCC ต้องเป็น 3V3 ไม่ใช่ VIN/5V, GND ร่วมกับบอร์ด · จะสแกนใหม่เรื่อยๆ ไม่ต้องรีบูต");
  }

  // SHT35 — ใช้ address ที่เจอจาก I2C scan ด้านบน (Wire.begin() เรียกไปแล้วตอนสแกน LCD ไม่ต้องเรียกซ้ำ)
  if (shtAddrFound != 0 && sht35.begin(shtAddrFound)) {
    shtAddr = shtAddrFound;
    Serial.printf("SHT35 Ready (address 0x%02X)\n", shtAddr);
  } else {
    Serial.println("[SHT35] ไม่พบเซนเซอร์ที่ 0x44/0x45 — เช็คสาย SDA(21)/SCL(22)/VCC/GND");
  }

  // โหลด control state ครั้งแรก
  loadControlFromFirebase();
  applyManualControl();

  // Watchdog — reboot อัตโนมัติถ้า loop ค้าง (worst-case: ESP32 แฮงค์/SSL ค้าง)
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  // core 3.x: TWDT ถูก init ไว้แล้วตอน boot (timeout สั้น ~5 วิ) → ปรับเป็น 60 วิ
  esp_task_wdt_config_t wdtCfg = { .timeout_ms = WDT_TIMEOUT_S * 1000, .idle_core_mask = 0, .trigger_panic = true };
  if (esp_task_wdt_init(&wdtCfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&wdtCfg);   // มีอยู่แล้ว → แค่ปรับ timeout
  }
#else
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
#endif
  esp_task_wdt_add(NULL);
  Serial.printf("Watchdog Ready (%ds)\n", WDT_TIMEOUT_S);

  Serial.println("=== Setup Complete ===\n");
}

// ─────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  esp_task_wdt_reset();   // ป้อน watchdog — ถ้า loop ไม่วน watchdog จะ reboot ให้

  // ── WiFi ─────────────────────────────────────────────
  // auto control ไม่พึ่ง WiFi (อ่าน sensor + threshold ใน RAM) → โรงเรือนคุมตัวเองต่อได้ตลอดช่วงหลุด
  // ที่หายคือ: สั่งจาก dashboard, push, hourly log · buzzer ยังดัง (checkAlerts อยู่นอก Firebase guard)
  static unsigned long lastWifiRetry = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiOfflineSince == 0) {
      wifiOfflineSince = now;
      Serial.println("[WiFi] หลุด — ลองต่อใหม่ทุก 30 วิ · auto control ยังทำงานต่อบนค่า threshold เดิม");
    }
    if (now - lastWifiRetry >= WIFI_RETRY_EVERY_MS) {
      lastWifiRetry = now;
      wifiMulti.run();   // ไล่ทุก SSID ใน config.h — core auto-reconnect ลองแค่ตัวเดิม
    }
    // restart ได้ครั้งเดียวเท่านั้น (rtcWifiRestartDone จำข้าม reboot ผ่าน RTC memory)
    // เหตุผล: reboot แก้ได้แค่ "WiFi stack ค้าง" — แก้ "เราเตอร์เจ๊ง/ไฟดับทั้งตึก" ไม่ได้เลย
    // ถ้าปล่อยให้ยิงทุก 30 นาที เราเตอร์ดับยาว 5 ชม. = reboot 10 ครั้ง ทุกรอบ relay ดับ + hourly หาย
    // = ทำลายความต่อเนื่องที่ตัวมันเองตั้งใจจะปกป้อง (ผู้ใช้เลือก one-shot 2026-07-16)
    if (!rtcWifiRestartDone && now - wifiOfflineSince >= WIFI_OFFLINE_RESTART_MS) {
      rtcWifiRestartDone = true;   // ตั้ง "ก่อน" restart — ไม่งั้นลืมว่าเคยลองแล้ว วนไม่จบ
      Serial.println("[WiFi] ต่อไม่ติดครบ 30 นาที — restart 1 ครั้ง เผื่อ WiFi stack ค้าง (ครั้งเดียว ไม่วนซ้ำ)");
      delay(300);   // ให้ Serial ส่งข้อความออกให้จบก่อน
      ESP.restart();
    }
  } else {
    if (wifiOfflineSince != 0) {
      Serial.printf("[WiFi] กลับมาแล้ว (หลุดไป %lu วินาที) — SSID: %s\n",
                    (now - wifiOfflineSince) / 1000, WiFi.SSID().c_str());
      wifiOfflineSince = 0;
    }
    // ต่อได้แล้ว = คืนสิทธิ์ restart ให้เคส stack ค้างครั้งหน้า
    // ⚠️ ต้องเคลียร์ตรงนี้ ไม่ใช่ในบล็อก "กลับมาแล้ว" ด้านบน — บล็อกนั้นยิงเฉพาะตอนมี "ขอบ" offline→online
    // ถ้า restart แล้ว setup() ต่อ WiFi ติดเลย จะไม่มีขอบให้จับ → flag ค้าง true ตลอดกาล = เคสค้างครั้งหน้าไม่ได้ restart
    if (rtcWifiRestartDone) rtcWifiRestartDone = false;

    // Firebase auth retry — แทนที่ ESP.restart() ที่เคยอยู่ใน setup()
    // เจตนาเดิมยังอยู่ (ห้ามวิ่งต่อแบบไม่ auth เงียบๆ ตลอดไป) แต่แก้ด้วยการ retry จริง ไม่ใช่ reboot
    static unsigned long lastAuthRetry = 0;
    static String lastAuthErr = "";
    if (!firebaseAuthed && now - lastAuthRetry >= FB_AUTH_RETRY_MS) {
      lastAuthRetry = now;
      if (tryFirebaseAuth()) {
        lastAuthErr = "";
        Serial.println("[Firebase] Auth สำเร็จแล้ว — เริ่ม push ขึ้น dashboard ได้");
      } else {
        // พิมพ์เมื่อ "เหตุผลเปลี่ยน" เท่านั้น — retry ทุก 60 วิ ถ้าพิมพ์ทุกรอบก็ท่วม serial (แนวเดียวกับ controlLoadError)
        // เทียบกับ const char* ตรงๆ — String lastAuthErr copy เฉพาะตอนเปลี่ยน · เดิม String(c_str()) จอง heap
        // ทุก 60 วิ ทั้งที่ค่าไม่เปลี่ยน (heap fragmentation คือผู้ต้องสงสัยหลักของอาการ restart)
        const char* e = fbConfig.signer.signupError.message.c_str();
        if (lastAuthErr != e) {
          lastAuthErr = e;
          Serial.printf("[Firebase] Auth ยังไม่ผ่าน — retry ทุก 60 วิ (auto control ทำงานปกติ): %s\n", e);
        }
      }
    }
  }

  // ทุก 30 วินาที: อ่าน sensor + auto control + push Firebase
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    readSensors();
    autoControl();
    // v2.6.0: ขับ manual channel เฉพาะตอนออฟไลน์ — ออนไลน์ poll block (ทุก 1.5 วิ) คุมให้อยู่แล้ว
    // เรียกซ้ำตอนออนไลน์ = งานซ้ำเปล่า · ออฟไลน์/หลัง reboot: การันตี relay ที่คนสั่ง manual-ON ไม่ค้างดับ
    if (!fbReady()) applyManualControl();
    // checkAlerts() อยู่นอก guard เจตนา — buzzer เป็นอุปกรณ์ local ที่ไม่ต้องใช้เน็ต
    // เดิมอยู่ในกรอบ Firebase.ready() = เน็ตดับแล้วอากาศร้อนวิกฤต คนหน้างานไม่ได้ยินอะไรเลย
    checkAlerts();
    if (fbReady()) {
      if (fbNotReadyLogged) { fbNotReadyLogged = false; Serial.println("[Firebase] กลับมา push ได้แล้ว"); }
      pushToFirebase();   // ตั้ง lastPushTime ให้เองผ่าน pushStatus()
    } else if (!fbNotReadyLogged) {
      fbNotReadyLogged = true;
      Serial.println("[Firebase] ยังไม่พร้อม — ข้าม push (auto control ยังทำงานต่อ)");
    }
  }

  // ความปลอดภัยปั๊ม — เช็คทุก loop (ตัดถ้าเดินเกิน 15 นาทีในโหมดอัตโนมัติ)
  pumpSafetyCheck();

  // poll คำสั่งควบคุมทุก CONTROL_POLL_MS (1.5 วิ) — รอ 2 วิหลัง push กัน SSL ชน
  static unsigned long lastControlPoll = 0;
  if (now - lastControlPoll >= CONTROL_POLL_MS && fbReady() && (millis() - lastPushTime >= 2000)) {
    lastControlPoll = now;
    loadControlFromFirebase();
    bool changed = applyManualControl();
    if (changed) pushStatus();   // มี relay เปลี่ยน → ยืนยันกลับ dashboard ทันที (ไม่ต้องรอรอบ 30 วิ)
  }

  // Hourly log — flush เมื่อ "ชั่วโมงตามนาฬิกาจริง" เปลี่ยน (ไม่ใช่ทุก 60 นาที millis เดิม)
  // เดิม millis-based เข้ากันไม่ได้กับ accumulator ที่ย้ายไป RTC: reboot ก่อนครบชั่วโมง = ไม่เคย flush +
  // สะสมข้าม reboot ไปเรื่อยๆ แล้วตกถังผิดชั่วโมง · ตอนนี้ผูกกับขอบชั่วโมงจริง reboot กี่ครั้งก็ตกถังถูก
  if (timeValid()) {
    String curPath = getHourlyPath();   // "/logs/YYYY-MM-DD/HH" ของชั่วโมงปัจจุบัน
    if (curPath.length()) {
      if (rtcLogPath[0] == '\0') {
        // เพิ่งมีนาฬิกาครั้งแรก — จองชั่วโมงนี้เป็นถังที่กำลังสะสม (ยังไม่ flush)
        strncpy(rtcLogPath, curPath.c_str(), sizeof(rtcLogPath) - 1);
        rtcLogPath[sizeof(rtcLogPath) - 1] = '\0';
      } else if (curPath != rtcLogPath) {
        // ข้ามขอบชั่วโมง → flush ของ "ชั่วโมงเก่า" ไปถังของชั่วโมงเก่า (ไม่ใช่ถังปัจจุบัน) แล้วเลื่อนไปชั่วโมงใหม่
        // pushHourlyLog() reset accumulator ให้เสมอทุกทางออก (รวมเคสไม่มีข้อมูล/เน็ตดับ) กันค้างข้ามชั่วโมง
        pushHourlyLog(String(rtcLogPath));
        strncpy(rtcLogPath, curPath.c_str(), sizeof(rtcLogPath) - 1);
        rtcLogPath[sizeof(rtcLogPath) - 1] = '\0';
      }
    }
  }

  // sync NTP — ปกติทุก 6 ชม. (กันนาฬิกาเพี้ยน) · แต่ถ้านาฬิกายังไม่ valid เลย retry ทุก 60 วิ จนกว่าจะติด
  // (เดิมรอ 6 ชม. เท่ากันหมด → บูต offline แล้วเน็ตกลับมา 1 นาที นาฬิกาก็ยังไม่ติดไปอีก 6 ชม.)
  // ⚠️ gate ด้วย WiFi connected ก่อน แล้วค่อยเช็ค interval — ไม่งั้น lastNtpSync ถูกรีเซ็ตตอน offline
  // ทำให้พอเน็ตกลับมาต้องนับ interval ใหม่ทั้งก้อน (เลื่อน sync ออกไปอีกโดยไม่จำเป็น)
  unsigned long ntpInterval = timeValid() ? NTP_RESYNC_MS : NTP_RETRY_INVALID_MS;
  if (WiFi.status() == WL_CONNECTED && now - lastNtpSync >= ntpInterval) {
    lastNtpSync = now;
    syncNTP();   // configTime() อยู่ข้างใน — เคสบูต offline configTime() ยังไม่เคยถูกเรียกเลย ตรงนี้เรียกให้
  }

  // ทุก 60 วินาที: ตรวจสอบตารางเวลา (schedule) — เรียกเสมอ เหตุผลเดียวกับ applyManualControl()
  static unsigned long lastSchedTime = 0;
  if (now - lastSchedTime >= 60000) {
    lastSchedTime = now;
    checkSchedule();
  }

  // ทุก 5 วินาที: สลับหน้า LCD
  static unsigned long lastLCDTime = 0;
  if (now - lastLCDTime >= 5000) {
    lastLCDTime = now;
    updateLCD();
  }

  // Heartbeat LED — กระพริบทุก HEARTBEAT_BLINK_MS พิสูจน์ว่า loop() ยังวนอยู่จริง
  // (ใช้ debug ตอนระบบ "ค้าง": กระพริบต่อ = loop ไม่ตาย, ปัญหาอยู่ที่จุดอื่น / ดับสนิท = loop ตายจริง)
  static unsigned long lastBlinkTime = 0;
  static bool ledState = false;
  if (now - lastBlinkTime >= HEARTBEAT_BLINK_MS) {
    lastBlinkTime = now;
    ledState = !ledState;
    digitalWrite(PIN_STATUS_LED, ledState);
  }
}


// ─────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────
// PERSIST control config ลง NVS (flash) — รอดทั้ง reboot และ "ไฟดับ/brownout" (ต่างจาก RTC memory ที่ไม่รอดไฟตก)
// เหตุผล v2.4.0: WiFi หลุด 30 นาที → ESP.restart() (หรือ brownout) แล้ว setup() รีเซ็ต ch_isAuto/ch_manual/
//   threshold กลับเป็นค่า compile default → อุปกรณ์ที่คนสั่ง manual-ON ดับหมด = อาการ "ออฟไลน์แล้วทุกอย่างดับ"
//   เก็บ config ลง NVS ทุกครั้งที่ dashboard เปลี่ยนค่า แล้ว restore ตอนบูตก่อนต่อ WiFi → reboot ไม่ทำลายสถานะ
// เขียนเฉพาะตอนค่าเปลี่ยนจริง (controlDirty) — กัน NVS wear (flash ~100k write cycle)
void saveControlState() {
  ctrlPrefs.begin("sf_ctrl", false);
  bool a[4], m[4], s[4];
  for (int i = 0; i < 4; i++) { a[i] = ch_isAuto[i]; m[i] = ch_manual[i]; s[i] = ch_schedEnabled[i]; }
  ctrlPrefs.putBytes("isAuto",  a, sizeof(a));
  ctrlPrefs.putBytes("manual",  m, sizeof(m));
  ctrlPrefs.putBytes("sched",   s, sizeof(s));
  ctrlPrefs.putBytes("schedOn",  ch_schedOn,  sizeof(ch_schedOn));
  ctrlPrefs.putBytes("schedOff", ch_schedOff, sizeof(ch_schedOff));
  ctrlPrefs.putFloat("tOn",    thresh_temp_on);
  ctrlPrefs.putFloat("tOff",   thresh_temp_off);
  ctrlPrefs.putFloat("hMin",   thresh_hum_min);
  ctrlPrefs.putFloat("hMax",   thresh_hum_max);
  ctrlPrefs.putFloat("hVent",  thresh_hum_vent);
  ctrlPrefs.putFloat("tAlert", thresh_temp_alert);
  ctrlPrefs.putFloat("hAlert", thresh_hum_alert);
  ctrlPrefs.putFloat("wAlert", thresh_water_temp_alert);
  ctrlPrefs.putBool ("buzzer", buzzerEnabled);
  ctrlPrefs.putBool ("valid",  true);
  ctrlPrefs.end();
  Serial.println("[NVS] เซฟ control config ลง flash แล้ว — จะ restore อัตโนมัติถ้าบอร์ด reboot/ไฟดับ");
}

// restore ตอนบูต — ก่อนต่อ WiFi · "valid"=false แปลว่ายังไม่เคยเซฟ (บูตแรก/หลัง flash ใหม่) → คงค่า compile default
void loadControlState() {
  ctrlPrefs.begin("sf_ctrl", true);   // read-only
  if (!ctrlPrefs.getBool("valid", false)) {
    ctrlPrefs.end();
    Serial.println("[NVS] ยังไม่มี control config ที่เซฟไว้ — ใช้ค่า default (บูตครั้งแรก หรือหลัง flash ใหม่)");
    return;
  }
  // ⚠️ seed locals จากค่า default ปัจจุบันก่อน — ถ้า getBytes อ่านไม่ครบ (key หาย/ขนาดเพี้ยน จาก
  //    partial write, brownout กลางเซฟ, layout เก่าข้ามเวอร์ชัน) มันจะ "ไม่แตะ buffer" คืน 0
  //    ถ้าไม่ seed = stack garbage เข้า ch_isAuto/ch_manual = โหมดรีเลย์มั่วตอนบูต · เขียนเฉพาะ array ที่อ่านครบ
  bool a[4], m[4], s[4];
  for (int i = 0; i < 4; i++) { a[i] = ch_isAuto[i]; m[i] = ch_manual[i]; s[i] = ch_schedEnabled[i]; }
  bool okAuto  = ctrlPrefs.getBytes("isAuto", a, sizeof(a)) == sizeof(a);
  bool okMan   = ctrlPrefs.getBytes("manual", m, sizeof(m)) == sizeof(m);
  bool okSched = ctrlPrefs.getBytes("sched",  s, sizeof(s)) == sizeof(s);
  for (int i = 0; i < 4; i++) {
    if (okAuto)  ch_isAuto[i]       = a[i];
    if (okMan)   ch_manual[i]       = m[i];
    if (okSched) ch_schedEnabled[i] = s[i];
  }
  // schedOn/schedOff: อ่านลง temp ก่อน เขียนทับ ch_ เฉพาะตอนอ่านครบ (ไม่ครบ = คงค่า default เดิม)
  char sOn[4][6], sOff[4][6];
  if (ctrlPrefs.getBytes("schedOn",  sOn,  sizeof(sOn))  == sizeof(sOn))  memcpy(ch_schedOn,  sOn,  sizeof(sOn));
  if (ctrlPrefs.getBytes("schedOff", sOff, sizeof(sOff)) == sizeof(sOff)) memcpy(ch_schedOff, sOff, sizeof(sOff));
  if (!okAuto || !okMan || !okSched)
    Serial.println("[NVS] ⚠️ control config บางส่วนอ่านไม่ครบ — ใช้ค่า default สำหรับส่วนที่หาย (NVS อาจเสียหาย)");
  thresh_temp_on          = ctrlPrefs.getFloat("tOn",    thresh_temp_on);
  thresh_temp_off         = ctrlPrefs.getFloat("tOff",   thresh_temp_off);
  thresh_hum_min          = ctrlPrefs.getFloat("hMin",   thresh_hum_min);
  thresh_hum_max          = ctrlPrefs.getFloat("hMax",   thresh_hum_max);
  thresh_hum_vent         = ctrlPrefs.getFloat("hVent",  thresh_hum_vent);
  thresh_temp_alert       = ctrlPrefs.getFloat("tAlert", thresh_temp_alert);
  thresh_hum_alert        = ctrlPrefs.getFloat("hAlert", thresh_hum_alert);
  thresh_water_temp_alert = ctrlPrefs.getFloat("wAlert", thresh_water_temp_alert);
  buzzerEnabled           = ctrlPrefs.getBool ("buzzer", buzzerEnabled);
  ctrlPrefs.end();
  Serial.printf("[NVS] restore control config สำเร็จ — พัดลม=%s ปั๊ม=%s · temp_on=%.1f temp_off=%.1f\n",
                ch_isAuto[IDX_FAN] ? "AUTO" : "MANUAL", ch_isAuto[IDX_PUMP] ? "AUTO" : "MANUAL",
                thresh_temp_on, thresh_temp_off);
}

// อัปเดต threshold float 1 ตัวจาก JSON — คืน true ถ้าเปลี่ยนจริง (ตั้ง controlDirty)
// ⚠️ epsilon compare (ไม่ใช่ !=): thresholds ปรับทีละ 0.1 จาก dashboard · float จาก Firebase parse
//    อาจไม่ bit-identical กับค่าใน NVS → != จะ true ทุก poll (1.5 วิ) = เขียน NVS รัว = flash wear
//    0.01 ต่ำกว่าการปรับจริงมาก แต่กัน noise ระดับ rounding ได้
static bool applyFloatThresh(FirebaseJson& json, FirebaseJsonData& d, const char* key, volatile float& var) {
  if (json.get(d, key) && fabsf((float)var - d.floatValue) > 0.01f) { var = d.floatValue; return true; }
  return false;
}

// โหลด control state ด้วย 1 call (getJSON) แทน 11 calls แยกกัน — ลด SSL reconnect
// ⚠️ ฟังก์ชันนี้ poll ทุก CONTROL_POLL_MS (1.5 วิ) ไม่ใช่ init — ห้ามพิมพ์ log ทุกรอบ
// เดิมพิมพ์ "[Init] Loading control state... OK" ทุกครั้ง = ~16 บรรทัดขยะต่อ 1 บรรทัดจริง
// ท่วม serial จน [AUTO]/[MANUAL] จมหาย · พิมพ์เฉพาะตอน fail หรือ mode เปลี่ยนจริงเท่านั้น
void loadControlFromFirebase() {
  if (!fbReady()) return;

  FirebaseJson    json;
  FirebaseJsonData d;

  if (!Firebase.getJSON(fbData, "/smartfarm/control", &json)) {
    // fail ติดกันหลายรอบ = ปัญหาจริง แต่พิมพ์ทุกรอบก็ท่วมอีก — พิมพ์เมื่อ "เหตุผลเปลี่ยน" เท่านั้น
    // (latch เป็น bool เฉยๆ จะซ่อนกรณีสาเหตุเปลี่ยนกลางทาง เช่น เน็ตหลุด → auth พัง แล้วไล่ผิดทาง)
    String reason = fbData.errorReason();
    if (reason != controlLoadError) {
      controlLoadError = reason;
      Serial.println("[Control] โหลดจาก Firebase ไม่ได้ — ใช้ค่าเดิมต่อ: " + reason);
    }
    return;
  }
  if (controlLoadError != "") {
    controlLoadError = "";
    Serial.println("[Control] โหลดจาก Firebase กลับมาได้แล้ว");
  }

  const char* chKeys[] = {"ch1_pump","ch2_fan_out","ch3_fan_in","ch4_spare"};
  for (int i = 0; i < 4; i++) {
    String b = String(chKeys[i]) + "/";
    if (json.get(d, b + "mode")) {
      bool wasAuto = ch_isAuto[i];
      ch_isAuto[i] = (d.stringValue == "auto");
      // mode เปลี่ยน = เหตุการณ์จริงที่ควรเห็นใน log (ช่วยอธิบายว่าทำไม [AUTO] หยุด/เริ่ม)
      if (ch_isAuto[i] != wasAuto) {
        Serial.printf("[Control] %s → %s\n", chKeys[i], ch_isAuto[i] ? "AUTO" : "MANUAL");
        controlDirty = true;   // v2.4.0: mode เปลี่ยน → เซฟลง NVS ให้รอด reboot
      }
    }
    // เทียบก่อนเขียน — ตั้ง controlDirty เฉพาะตอนค่าต่างจริง (กัน NVS wear จาก poll ทุก 1.5 วิ)
    if (json.get(d, b + "manual_state") && ch_manual[i] != d.boolValue) {
      ch_manual[i] = d.boolValue; controlDirty = true;
    }
    if (json.get(d, b + "schedule/enabled") && ch_schedEnabled[i] != d.boolValue) {
      ch_schedEnabled[i] = d.boolValue; controlDirty = true;
    }
    if (json.get(d, b + "schedule/on_time")  && strncmp(ch_schedOn[i],  d.stringValue.c_str(), 5) != 0) {
      strncpy(ch_schedOn[i],  d.stringValue.c_str(), 5); ch_schedOn[i][5]='\0';  controlDirty = true;
    }
    if (json.get(d, b + "schedule/off_time") && strncmp(ch_schedOff[i], d.stringValue.c_str(), 5) != 0) {
      strncpy(ch_schedOff[i], d.stringValue.c_str(), 5); ch_schedOff[i][5]='\0'; controlDirty = true;
    }
  }
  // threshold floats — helper คุมการ pair key↔var + epsilon compare ที่เดียว (กัน mis-pair + NVS thrash)
  if (applyFloatThresh(json, d, "thresholds/temp_on",          thresh_temp_on))          controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/temp_off",         thresh_temp_off))         controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/humidity_min",     thresh_hum_min))          controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/humidity_max",     thresh_hum_max))          controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/humidity_vent",    thresh_hum_vent))         controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/temp_alert",       thresh_temp_alert))       controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/humidity_alert",   thresh_hum_alert))        controlDirty = true;
  if (applyFloatThresh(json, d, "thresholds/water_temp_alert", thresh_water_temp_alert)) controlDirty = true;
  if (json.get(d, "buzzer_enabled") && buzzerEnabled != d.boolValue) { buzzerEnabled = d.boolValue; controlDirty = true; }

  // เซฟครั้งเดียวท้ายฟังก์ชัน ถ้ามีอะไรเปลี่ยน — dashboard เปลี่ยนค่า = คนกดเอง (ไม่บ่อย) NVS เขียนไหว
  if (controlDirty) { saveControlState(); controlDirty = false; }
}

// ─────────────────────────────────────────────────────
void readSensors() {
  // เดิม DHT22 อยู่ใกล้ relay/สายปั๊มบน expansion board มาก — ปั๊มสวิตช์ทำให้เกิด noise transient
  // รบกวน timing ตอนอ่านได้ทันที เว้นช่วง PUMP_SWITCH_QUIET_MS ก่อนลองอ่าน กันอ่านชนจังหวะ noise
  // (คงกลไกนี้ไว้หลังเปลี่ยนเป็น SHT35 ด้วย — ยังไม่ยืนยันว่า proximity เดิมจะกระทบ I2C เหมือนกันไหม)
  bool inPumpQuietWindow = (lastPumpSwitchTime != 0)
                         && (millis() - lastPumpSwitchTime < PUMP_SWITCH_QUIET_MS);
  if (inPumpQuietWindow) {
    Serial.println("[SHT35] ข้ามรอบนี้ — ปั๊มเพิ่งสวิตช์ รอ noise transient สงบก่อน (ใช้ค่าเดิม)");
  } else if (shtAddr == 0) {
    // ⚠️ ต้องนับเป็น "พลาด" ด้วย — ไม่งั้น airSensorFailCount ค้าง 0 → sensorOk=true, sensor_stale=false
    // → dashboard บอก "เซนเซอร์ปกติ" ทั้งที่ไม่เคยอ่านค่าได้เลย และไม่มี alert ใดๆ = เงียบสนิท
    // (สายหลวมอยู่ก่อนเปิดไฟ เกิดง่ายกว่าสายหลุดระหว่างทำงาน — เคสนี้ห้ามเงียบ)
    airSensorFailCount++;
    if (!shtMissingLogged) {
      shtMissingLogged = true;
      Serial.println("[SHT35] ไม่พบเซนเซอร์ที่ 0x44/0x45 — เช็คสาย SDA(21)/SCL(22)/VCC/GND · จะลองหาใหม่เรื่อยๆ");
    }
    // ลองหาใหม่เป็นระยะ — เสียบสายคืนทีหลังต้องฟื้นเองได้ ไม่ต้องรอคนมารีบูต
    if (airSensorFailCount % SHT_REINIT_EVERY == 0) {
      for (uint8_t a : {0x44, 0x45}) {
        if (sht35.begin(a)) {
          shtAddr = a; shtMissingLogged = false;
          Serial.printf("[SHT35] เจอเซนเซอร์แล้วที่ 0x%02X — กลับมาอ่านได้\n", a);
          break;
        }
      }
    }
  } else {
    airTemp     = sht35.readTemperature();
    airHumidity = sht35.readHumidity();
    // ถือว่าพังถ้า NaN (CRC ไม่ผ่าน/สื่อสารพลาด) หรือค่านอกช่วงสมเหตุผล
    bool airBad = isnan(airTemp) || isnan(airHumidity)
               || airTemp < AIR_TEMP_MIN || airTemp > AIR_TEMP_MAX
               || airHumidity < 0 || airHumidity > 100;
    if (airBad) {
      Serial.println("[SHT35] อ่านค่าผิดปกติ (NaN/CRC พลาด หรือ นอกช่วง)");
      airTemp = 0; airHumidity = 0;
      airSensorFailCount++;
      // ลองซ้ำทุกๆ SHT_REINIT_EVERY ครั้งที่พลาด (ไม่ใช่ครั้งเดียวแล้วยอมแพ้) เผื่อฟื้นได้เอง
      if (airSensorFailCount % SHT_REINIT_EVERY == 0) {
        Serial.printf("[SHT35] พลาดสะสม %d ครั้ง — ลอง re-init sensor\n", airSensorFailCount);
        sht35.begin(shtAddr);
      }
    } else {
      // เพิ่งฟื้นจากสถานะ stale (เคยพลาดจนเลิกเชื่อ) → ล้าง buffer ทิ้งค่าเก่า (อาจเก่าเป็นชั่วโมง)
      // ก่อนใส่ค่าสด · ไม่งั้น avg 2-3 รอบแรกหลังฟื้นจะปน 2 ค่าเก่า + 1 ค่าใหม่ = ตัดสินใจจากอากาศก่อนพัง
      // (v2.0.0 ล้าง latch ให้แล้ว แต่ยังไม่ได้ล้าง buffer ที่ป้อน avg เข้า computeAutoDecisions)
      // ⚠️ ล้างตรง "ขอบฟื้น" เท่านั้น ห้ามล้างระหว่าง stale — ระหว่าง stale ต้องคง count>0 ไว้
      // ไม่งั้น autoControl() ติด guard `ctrlBufATCount==0` return ก่อน = ไม่ได้สั่งปิดรีเลย์ตอนเซนเซอร์เสีย
      if (airSensorFailCount >= SENSOR_STALE_AFTER) { ctrlBufATCount = 0; ctrlBufIdx = 0; }
      airSensorFailCount = 0;
      // เก็บเฉพาะค่าดีเข้า control-averaging buffer (กัน autoControl() ตัดสินใจจากค่าเพี้ยน)
      ctrlBufAT[ctrlBufIdx] = airTemp;
      ctrlBufAH[ctrlBufIdx] = airHumidity;
      ctrlBufIdx = (ctrlBufIdx + 1) % CTRL_AVG_N;
      if (ctrlBufATCount < CTRL_AVG_N) ctrlBufATCount++;
    }
  }

  // DS18B20 (สาย jumper ยาว ~4m ใกล้พัดลม 220V/ปั๊ม → noise สูง) — อ่านซ้ำถ้าได้ค่าเสีย
  // ค่าเสีย: -127 (สายหลุด/ไม่เจอ device), 85.0 (reset/อ่านไม่ทัน), หรือนอกช่วงจริง
  float wt = NAN;
  for (int r = 0; r <= DS_READ_RETRY; r++) {
    ds18b20.requestTemperatures();
    wt = ds18b20.getTempCByIndex(0);
    if (wt >= DS_WATER_MIN && wt <= DS_WATER_MAX) break;   // ได้ค่าดีแล้ว
  }
  if (wt >= DS_WATER_MIN && wt <= DS_WATER_MAX) {
    waterTemp = wt;          // เก็บเฉพาะค่าที่ใช้ได้ (ถ้าอ่านพลาด คงค่าเดิมไว้ ไม่เอาค่าขยะไปแสดง/log/alert)
    waterSensorOk = true;    // น้ำใช้แค่ แสดง/log/alert — ไม่คุมรีเลย์แล้ว จึงไม่มี control buffer
    waterFailCount = 0;
    if (waterBadLogged) { waterBadLogged = false; Serial.printf("[DS18B20] กลับมาอ่านได้แล้ว (%.1f°C)\n", wt); }
  } else {
    waterSensorOk = false;
    waterFailCount++;
    // latch — ถ้ายังไม่ต่อสาย DS18B20 บรรทัดนี้จะขึ้นทุก 30 วิ ตลอดกาล กลบ log อื่นหมด
    // ค่าในวงเล็บคือตัวไล่ปัญหา: -127.0 = ไม่มีใครตอบบนบัส (สาย/pull-up/เซนเซอร์ตาย)
    //                           85.0  = เจอตัวแล้วแต่ convert ไม่จบ = ไฟเลี้ยงไม่พอ (VCC ไม่ถึง/หลวม)
    if (!waterBadLogged) {
      waterBadLogged = true;
      Serial.printf("[DS18B20] ค่าน้ำผิดปกติ (%.1f) — ข้าม · -127=ไม่มีใครตอบบนบัส · 85=ไฟเลี้ยงไม่พอ · จะไม่แจ้งซ้ำจนกว่าจะกลับมาอ่านได้\n", wt);
    }
    // สแกนบัสใหม่ — ต้องมี ไม่งั้นเสียบสายคืนตอนบอร์ดรันอยู่ไม่มีวันฟื้น (devices ค้าง 0 ตั้งแต่บูต ดู DS_REINIT_EVERY)
    if (waterFailCount % DS_REINIT_EVERY == 0) {
      ds18b20.begin();
      Serial.printf("[DS18B20] อ่านไม่ได้ %d รอบติด — สแกนบัส 1-Wire ใหม่: เจอ %d ตัว%s\n",
                    waterFailCount, ds18b20.getDeviceCount(),
                    ds18b20.getDeviceCount() == 0 ? " (0 = บัสเงียบสนิท → เช็ค pull-up 4.7k ไป 3V3 + สาย)" : " → รอบหน้าอ่านได้แล้ว");
    }
  }

  // สะสมข้อมูลสำหรับ hourly log — สะสมเฉพาะตอน "นาฬิกาติดแล้ว" เท่านั้น
  // ⚠️ ถ้าสะสมตอนนาฬิกายังไม่ติด (บูต offline) ค่าพวกนี้จะถูกเหมาไปตกถังชั่วโมงที่ NTP เพิ่ง sync พอดี
  //    = ข้อมูล 13:xx ไปโผล่ในถังชั่วโมง 14 · และถ้านาฬิกาไม่ sync เลย accumulator โตไม่หยุด (precision เพี้ยน)
  //    hourly log ต้องรู้ว่าเป็นของชั่วโมงไหน — ไม่มีนาฬิกา = ไม่มีถังให้ลง จึงข้ามการสะสมไปเลย
  //    (timeValid() ไม่บล็อกแล้ว จึงเรียกตรงนี้ทุก 30 วิ ได้ไม่มีปัญหา)
  bool canLog = timeValid();
  // อากาศ — ข้ามถ้า sensor อ่านไม่ได้ หรือรอบนี้ข้ามเพราะ pump quiet window (กันนับค่าเก่าซ้ำ)
  if (canLog && !inPumpQuietWindow && (airTemp > 0 || airHumidity > 0)) {
    h_sumAT += airTemp;     h_maxAT = max(h_maxAT, airTemp);     h_minAT = min(h_minAT, airTemp);
    h_sumAH += airHumidity; h_maxAH = max(h_maxAH, airHumidity); h_minAH = min(h_minAH, airHumidity);
    h_count++;
  }
  // น้ำ: สะสมเฉพาะตอนอ่านได้ (กันค่าขยะจากสายยาวทำ avg/max/min เพี้ยน)
  if (canLog && waterSensorOk) {
    h_sumWT += waterTemp;   h_maxWT = max(h_maxWT, waterTemp);   h_minWT = min(h_minWT, waterTemp);
    h_countWT++;
  }

  Serial.printf("[Sensor] AirT:%.1f°C RH:%.1f%% WaterT:%.1f°C\n",
    airTemp, airHumidity, waterTemp);
}

// ─────────────────────────────────────────────────────
// ค่าเฉลี่ยจาก control-averaging buffer — ใช้เฉพาะใน autoControl() (กันตัดสินใจจากค่าเพี้ยนชั่วครู่)
float avgCtrlAT() { float s=0; for (int i=0;i<ctrlBufATCount;i++) s+=ctrlBufAT[i]; return ctrlBufATCount ? s/ctrlBufATCount : 0; }
float avgCtrlAH() { float s=0; for (int i=0;i<ctrlBufATCount;i++) s+=ctrlBufAH[i]; return ctrlBufATCount ? s/ctrlBufATCount : 0; }

// ─────────────────────────────────────────────────────
// Auto Control — โมเดล v1.8.0 2026-07-15: พัดลม+ปั๊มทำงานคู่กันทั้ง 2 เกณฑ์
//   3 latch: ร้อน (≥temp_on..≤temp_off) · แห้ง (<humidity_min..≥humidity_max) · ปั๊มไล่ร้อน (ร้อน + โดน gate ล้างได้)
//   พัดลม (CH3, ดูดเข้า) = ร้อน OR แห้ง        | ร้อน=ระบายความร้อน · แห้ง=ดูดอากาศชื้นนอกเข้า+กระจายละอองปั๊ม
//   ปั๊ม  (CH4)          = ปั๊มไล่ร้อน OR แห้ง   | ร้อน+ชื้นเกิน(≥humidity_max) → ปั๊มปิด พัดลมยังเปิด
//                          (พ่นน้ำในอากาศอิ่มตัว = ไม่เย็น แค่ท่วม · แต่พัดลมระบายความร้อนได้ฟรี)
//   น้ำ = แจ้งเตือนอย่างเดียว (ไม่คุมรีเลย์)
// latch เก็บสถานะข้ามรอบ (ทั้ง 2 ช่องมี 2 hysteresis loop ต่อ 1 รีเลย์ stateless ไม่ได้)
// logic บริสุทธิ์ + เหตุผลว่าทำไม gate ต้องอยู่ในlatch อยู่ auto_control_logic.h
void autoControl() {
  float ton  = thresh_temp_on,  toff = thresh_temp_off;
  float hmin = thresh_hum_min,  hmax = thresh_hum_max;

  // config ต้องมีช่องว่าง hysteresis ที่ถูกต้อง (พัดลม on>off, ปั๊ม max>min) ไม่งั้นรีเลย์กระพริบ → ข้ามรอบ
  if (ton <= toff || hmax <= hmin) {
    Serial.println("[AUTO] ข้าม — threshold ไม่ถูกต้อง (ต้อง temp_on>temp_off และ humidity_max>humidity_min)");
    return;
  }
  // ยังไม่มี sample อากาศที่อ่านได้เลย (บูตใหม่/เซนเซอร์ยังไม่พร้อม) → อย่าเพิ่งสั่งรีเลย์
  if (ctrlBufATCount == 0) return;

  // ค่าใน buffer เก่าแค่ไหน — readSensors() เก็บเฉพาะค่าดี ถ้าเซนเซอร์พังค่าจะค้างอยู่อย่างนั้น
  // ไม่มีใครบอก computeAutoDecisions() ว่าค่ามันเก่า มันเลยสั่งรีเลย์ต่อไปเรื่อยๆ — ส่ง sensorOk เข้าไปกัน
  bool sensorOk = (airSensorFailCount < SENSOR_STALE_AFTER);

  float avgAT = avgCtrlAT();
  float avgAH = avgCtrlAH();
  // v2.7.0 vent: ส่ง threshold ดิบเข้าไป — clamp (ventOn/ventOff + เงื่อนไขใช้งานได้)
  // อยู่ใน computeAutoDecisions() แล้ว เพื่อให้ unit test กินโค้ดจริง ไม่ใช่สำเนาที่นี่
  // ที่นี่เหลือแค่ "รายงาน" ว่า config ใช้ไม่ได้ — ปิดเงียบๆ = บั๊กที่หาไม่เจอจาก log
  bool ventUsable = ventIsUsable(thresh_hum_vent, hmax);
  // จำ "ค่าที่เตือนไปแล้ว" ไม่ใช่ bool — ไม่งั้นตั้งค่าผิดค่าที่ 2 จะเงียบเพราะ flag ยังค้าง true
  static float ventWarnedFor = NAN;
  if (!ventUsable && thresh_hum_vent > 0) {
    if (isnan(ventWarnedFor) || ventWarnedFor != thresh_hum_vent) {
      Serial.printf("[AUTO] ⚠️ ปิด vent — humidity_vent %.1f ใช้ไม่ได้ (ต้อง ≥ humidity_max + VENT_HYST = %.1f)\n",
                    thresh_hum_vent, hmax + VENT_HYST);
      ventWarnedFor = thresh_hum_vent;
    }
  } else {
    ventWarnedFor = NAN;   // ใช้ได้แล้ว หรือปิดฟีเจอร์เอง → พร้อมเตือนค่าถัดไป
  }
  AutoControlDecisions dec = computeAutoDecisions(
    {sensorOk, avgAT, avgAH, ton, toff, hmin, hmax, airHotOn, airDryOn, pumpHeatOn, thresh_hum_vent, airWetOn});
  airHotOn   = dec.hotOn;    // เก็บ latch กลับไปใช้รอบหน้า
  airDryOn   = dec.dryOn;
  pumpHeatOn = dec.pumpHeatOn;
  airWetOn   = dec.wetOn;

  // เหตุผลที่พัดลมเปิด — ตาราง 8 ช่องตาม bitmask hot|dry|wet (index 0 = ไม่มีเหตุผลเลย)
  // ตารางแทน if/else ladder: ทุก combo เขียนไว้ชัด ไม่มีลำดับ else-if ให้สลับผิดโดยไม่รู้ตัว
  // index 0 เกิดได้เฉพาะตอน !dec.fanOn (เช่น sensor เสีย ล้าง latch หมด) — log ด้านล่าง gate ด้วย
  // dec.fanOn อยู่แล้ว แต่ใส่ข้อความตรงไว้ เผื่อวันหน้ามีใครเอา fanWhy ไปใช้นอก gate
  static const char* const FAN_WHY[8] = {
    /*0 ---*/ "ไม่มีเงื่อนไข",
    /*1 h--*/ "ร้อน",
    /*2 -d-*/ "แห้ง",
    /*3 hd-*/ "ร้อน+แห้ง",
    /*4 --w*/ "ชื้นเกิน-ระบาย",
    /*5 h-w*/ "ร้อน+ชื้นเกิน",
    /*6 -dw*/ "แห้ง+ชื้นเกิน",
    /*7 hdw*/ "ร้อน+แห้ง+ชื้นเกิน",
  };
  const char* fanWhy = FAN_WHY[(airHotOn ? 1 : 0) | (airDryOn ? 2 : 0) | (airWetOn ? 4 : 0)];
  // เหตุผลที่ปั๊มเปิด — ปั๊มไล่ร้อนผ่าน gate แล้ว หรือ แห้ง
  const char* pumpWhyOn = pumpHeatOn ? (airDryOn ? "ร้อน+แห้ง" : "ร้อน") : "แห้ง";

  // precedence: ถ้า channel เปิด Schedule อยู่ → ปล่อยให้ checkSchedule คุม (ข้าม auto)
  // CH3 พัดลม — เปิดตาม dec.fanOn (ร้อน หรือ แห้ง)
  if (ch_isAuto[IDX_FAN] && !ch_schedEnabled[IDX_FAN]) {
    // v2.8.0: พัดลมกลับมาผูกกับ safety cutoff — เปิดได้เฉพาะพ้น fanLockUntil (พักคู่ปั๊ม)
    // ⚠️ trade-off: พัดลมอาจพักช่วงร้อน = เสี่ยง overshoot (CSV 2026-07-20) — ยอมรับตามคำสั่งหน้างาน
    if (dec.fanOn && !ch3_fanIn && millis() >= fanLockUntil) {
      ch3_fanIn = true;  setRelay(PIN_RELAY_CH3, true);
      Serial.printf("[AUTO] พัดลมเปิด (%s) — อากาศ %.1f°C ความชื้น %.1f%%\n", fanWhy, avgAT, avgAH);
    }
    if (!dec.fanOn && ch3_fanIn) {
      ch3_fanIn = false; setRelay(PIN_RELAY_CH3, false);
      if (!sensorOk) Serial.printf("[AUTO] พัดลมปิด (เซนเซอร์เชื่อไม่ได้ — พลาด %d ครั้งติด) — ค่าล่าสุด %.1f°C %.1f%%\n", airSensorFailCount, avgAT, avgAH);
      else           Serial.printf("[AUTO] พัดลมปิด — อากาศ %.1f°C ความชื้น %.1f%%\n", avgAT, avgAH);
    }
  }
  // CH4 ปั๊มน้ำ — เปิดตาม dec.pumpOn (ปั๊มไล่ร้อน หรือ แห้ง) · เปิดได้เฉพาะพ้น safety lock (cooldown)
  if (ch_isAuto[IDX_PUMP] && !ch_schedEnabled[IDX_PUMP]) {
    if (dec.pumpOn && !ch4_spare && millis() >= pumpLockUntil) {
      ch4_spare = true;  setRelay(PIN_RELAY_CH4, true);  lastPumpSwitchTime = millis();
      Serial.printf("[AUTO] ปั๊มเปิด (%s) — อากาศ %.1f°C ความชื้น %.1f%%\n", pumpWhyOn, avgAT, avgAH);
    }
    if (!dec.pumpOn && ch4_spare) {
      ch4_spare = false; setRelay(PIN_RELAY_CH4, false); lastPumpSwitchTime = millis();
      // แยกเหตุผลให้ตรง — เซนเซอร์เสียต้องไม่ถูกรายงานว่า "อากาศชื้นเกิน"
      const char* pumpWhyOff;
      if      (!sensorOk)             pumpWhyOff = "เซนเซอร์เชื่อไม่ได้";
      else if (airHotOn && !airDryOn) pumpWhyOff = "อากาศชื้นเกิน พ่นน้ำไม่ช่วย (พัดลมยังเปิด)";
      else                            pumpWhyOff = "ไม่ร้อนไม่แห้ง";
      Serial.printf("[AUTO] ปั๊มปิด (%s) — อากาศ %.1f°C ความชื้น %.1f%%\n", pumpWhyOff, avgAT, avgAH);
    }
  }
}

// ─────────────────────────────────────────────────────
// Apply Manual Control — เรียกหลัง loadControlFromFirebase()
// คืน true ถ้ามี relay เปลี่ยนสถานะ (เพื่อให้ loop push ยืนยันกลับทันที)
bool applyManualControl() {
  bool changed = false;
  // precedence: ถ้า channel เปิด Schedule อยู่ → checkSchedule คุม (ข้าม manual)
  // CH1 = สำรอง (manual/schedule) · CH2 ไม่ได้ใช้ (เผื่อ dashboard ส่งค่ามา)
  if (!ch_isAuto[0] && !ch_schedEnabled[0] && (bool)ch_manual[0] != ch1_pump) {
    ch1_pump = ch_manual[0];
    setRelay(PIN_RELAY_CH1, ch1_pump);
    Serial.printf("[MANUAL] CH1 Spare → %s\n", ch1_pump ? "ON" : "OFF");
    changed = true;
  }
  if (!ch_isAuto[1] && !ch_schedEnabled[1] && (bool)ch_manual[1] != ch2_fanOut) {
    ch2_fanOut = ch_manual[1];
    setRelay(PIN_RELAY_CH2, ch2_fanOut);
    Serial.printf("[MANUAL] CH2 (unused) → %s\n", ch2_fanOut ? "ON" : "OFF");
    changed = true;
  }
  if (!ch_isAuto[IDX_FAN] && !ch_schedEnabled[IDX_FAN] && (bool)ch_manual[IDX_FAN] != ch3_fanIn) {
    ch3_fanIn = ch_manual[IDX_FAN];
    setRelay(PIN_RELAY_CH3, ch3_fanIn);
    Serial.printf("[MANUAL] CH3 Fan → %s\n", ch3_fanIn ? "ON" : "OFF");
    changed = true;
  }
  // ตั้งใจไม่เช็ค pumpLockUntil ตรงนี้ — คนละกรณีกับ checkSchedule() ที่เช็ค (เพราะ schedule ไม่มีคนคอยดูตอนนั้น)
  // manual = คนสั่งเองตรงหน้าจอ เห็นสถานะ/badge "พักปั๊ม" อยู่แล้วถ้าจะสั่งฝืนก็ตัดสินใจเอง สอดคล้องกับหลักการ
  // เดิมของระบบ: ผู้ใช้ต้องสั่ง manual ได้เสมอ ไม่ถูกล็อกออกจากระบบตัวเอง (ดูเหตุผลเดียวกันตอนแก้ failsafe)
  if (!ch_isAuto[IDX_PUMP] && !ch_schedEnabled[IDX_PUMP] && (bool)ch_manual[IDX_PUMP] != ch4_spare) {
    ch4_spare = ch_manual[IDX_PUMP];
    setRelay(PIN_RELAY_CH4, ch4_spare);
    lastPumpSwitchTime = millis();
    Serial.printf("[MANUAL] CH4 Pump → %s\n", ch4_spare ? "ON" : "OFF");
    changed = true;
  }
  return changed;
}

// ─────────────────────────────────────────────────────
void setRelay(int pin, bool state) {
  // state=true = เปิด relay, false = ปิด
  // active-LOW: เปิด→LOW ปิด→HIGH | active-HIGH: เปิด→HIGH ปิด→LOW
  if (RELAY_ACTIVE_LOW) digitalWrite(pin, state ? LOW : HIGH);
  else                  digitalWrite(pin, state ? HIGH : LOW);
}

// ─────────────────────────────────────────────────────
// นาฬิกาถูกต้องไหม (ปี >= 2024 = NTP sync แล้ว)
// ⚠️ getLocalTime(&t, 0) — timeout 0 = ไม่บล็อก · default คือ 5000ms ซึ่งจะ busy-wait 5 วินาทีทุกครั้งที่
// นาฬิกายัง "ไม่ติด" (tm_year < 2016) · loop() เรียก timeValid() ทุกรอบ (เลือก NTP interval + gate hourly)
// ถ้าใช้ default = บูต offline แล้วนาฬิกายังไม่ sync → loop บล็อก ~10 วิ/รอบ (2 call) ทั้งช่วง = ระบบอืดหนัก
// ระหว่างที่ควรตอบสนองไวที่สุด · ค่าเวลามีอยู่ใน RTC อยู่แล้ว อ่านทันที ไม่ต้องรอ
bool timeValid() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return false;
  return (t.tm_year + 1900) >= 2024;
}

// ─────────────────────────────────────────────────────
// SAFETY — ตัดปั๊ม+พัดลมถ้าเดินต่อเนื่องเกิน 15 นาที แล้วพักคู่กัน 5 นาที (เฉพาะ auto/schedule)
// v2.8.0: พัดลมกลับมาพักคู่ปั๊ม (ยกเลิก decouple v2.6.0 ตามคำสั่งหน้างาน)
//   - จับเวลาเดินต่อเนื่องแยกช่อง (pumpOnSince / fanOnSince) · manual = คนคุมเอง ไม่จับ ไม่ตัด
//   - ครบ 15 นาทีช่องใดช่องหนึ่ง → ตัด "ทั้งคู่" (เฉพาะช่องที่เป็น auto) + ล็อกพัก 5 นาทีพร้อมกัน
//   ⚠️ พัดลมพักช่วงร้อนได้ = เสี่ยง overshoot (CSV 2026-07-20) — เป็น trade-off ที่ยอมรับตามคำสั่ง
void pumpSafetyCheck() {
  unsigned long now = millis();
  bool pumpAuto = ch_isAuto[IDX_PUMP] || ch_schedEnabled[IDX_PUMP];
  bool fanAuto  = ch_isAuto[IDX_FAN]  || ch_schedEnabled[IDX_FAN];

  // จับเวลาเดินต่อเนื่อง — รีเซ็ตเมื่อช่องหยุด หรืออยู่โหมด manual
  if (ch4_spare && pumpAuto) { if (pumpOnSince == 0) pumpOnSince = now; } else pumpOnSince = 0;
  if (ch3_fanIn && fanAuto)  { if (fanOnSince  == 0) fanOnSince  = now; } else fanOnSince  = 0;

  bool pumpMaxed = (pumpOnSince != 0) && (now - pumpOnSince >= PUMP_MAX_RUNTIME_MS);
  bool fanMaxed  = (fanOnSince  != 0) && (now - fanOnSince  >= FAN_MAX_RUNTIME_MS);

  if (pumpMaxed || fanMaxed) {
    // ล็อกพัก "ทั้งคู่" 5 นาทีพร้อมกัน (เฉพาะช่อง auto — manual คนคุมเอง ไม่แตะ)
    // ล็อกแม้ช่องนั้นกำลังปิดอยู่ตอน cutoff — กันไม่ให้อีกช่องเริ่มเดินระหว่างพักร่วม (rest together จริง)
    if (pumpAuto) {
      if (ch4_spare) { ch4_spare = false; setRelay(PIN_RELAY_CH4, false); lastPumpSwitchTime = now; }
      pumpLockUntil = now + PUMP_COOLDOWN_MS;
    }
    if (fanAuto) {
      if (ch3_fanIn) { ch3_fanIn = false; setRelay(PIN_RELAY_CH3, false); }
      fanLockUntil = now + FAN_COOLDOWN_MS;
    }
    pumpOnSince = 0; fanOnSince = 0;
    const char* trigger = pumpMaxed ? "ปั๊ม" : "พัดลม";
    Serial.printf("[SAFETY] ตัดปั๊ม+พัดลม — %s เดินครบ 15 นาที (พักคู่กัน 5 นาที)\n", trigger);
    if (fbReady()) {
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "pump_cutoff");
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
        "ตัดปั๊ม+พัดลมอัตโนมัติ — เดินต่อเนื่องเกิน 15 นาที (พักคู่กัน 5 นาที) ตรวจสอบระดับน้ำ");
    }
    if (buzzerEnabled) buzzerBeep(2);
  }
}

// ─────────────────────────────────────────────────────
// push เฉพาะสถานะ relay + health — เบา เรียกแยกเพื่อยืนยันผลให้ dashboard ทันที
// ⚠️ ตั้ง lastPushTime ที่นี่จุดเดียว — ทั้ง pushToFirebase() และ manual-change path (loop) เรียกผ่านฟังก์ชันนี้หมด
// เดิม lastPushTime ตั้งเฉพาะหลัง pushToFirebase() ทำให้ตอนกด Manual → pushStatus() ยิง SSL 13 ครั้ง
// แต่ guard "เว้น 2 วิก่อน poll" ไม่รู้ตัว → control poll รอบถัดไป (1.5 วิ) เข้าไปชน SSL ที่เพิ่งเขียนเสร็จ
// = เคสที่ guard ตั้งใจกันพอดี แต่ครอบไม่ถึง
void pushStatus() {
  lastPushTime = millis();
  const String base = "/smartfarm/";
  Firebase.setBool  (fbData, base + "status/online",      true);
  Firebase.setBool  (fbData, base + "status/ch1_pump",    ch1_pump);
  Firebase.setBool  (fbData, base + "status/ch2_fan_out", ch2_fanOut);
  Firebase.setBool  (fbData, base + "status/ch3_fan_in",  ch3_fanIn);
  Firebase.setBool  (fbData, base + "status/ch4_spare",   ch4_spare);
  Firebase.setString(fbData, base + "status/firmware",    "2.8.0");
  // Boot diagnostics — dashboard เห็นย้อนหลังได้ว่าบอร์ดรีสตาร์ทเพราะอะไร ไม่ต้องนั่งเฝ้า Serial Monitor
  // boot_count พุ่งเร็ว = reboot loop · last_reset_reason บอกว่าโทษไฟ (BROWNOUT) หรือโทษโค้ด (PANIC/TASK_WDT)
  Firebase.setString(fbData, base + "status/last_reset_reason", resetReasonStr(bootResetReason));
  Firebase.setInt  (fbData, base + "status/boot_count",   (int)rtcBootCount);
  // heap: เฝ้า fragmentation — ถ้า max_alloc_heap หดลงเรื่อยๆ ทั้งที่ free_heap ยังเยอะ = heap แตกเป็นเสี่ยง
  // → malloc ก้อนใหญ่ (SSL buffer) พลาด → crash หลังรันไปหลายชั่วโมง = ผู้ต้องสงสัยอันดับ 1 ของอาการนี้
  Firebase.setInt  (fbData, base + "status/free_heap",      (int)ESP.getFreeHeap());
  Firebase.setInt  (fbData, base + "status/max_alloc_heap", (int)ESP.getMaxAllocHeap());
  // Health / worst-case status — ให้ dashboard เห็นสถานะระบบ
  Firebase.setBool (fbData, base + "status/sensor_ok",   (airSensorFailCount == 0));
  // แยกจาก sensor_ok: พลาด 1 ครั้ง = sensor_ok false แต่ auto ยังทำงานบนค่าล่าสุดอยู่
  // sensor_stale = พลาดจนเลิกเชื่อแล้ว → auto ปิดทุกช่อง = เรื่องใหญ่กว่ามาก dashboard ต้องแยกให้เห็น
  Firebase.setBool (fbData, base + "status/sensor_stale", (airSensorFailCount >= SENSOR_STALE_AFTER));
  Firebase.setBool (fbData, base + "status/water_ok",    waterSensorOk);
  Firebase.setBool (fbData, base + "status/failsafe",    false);   // failsafe ถูกถอดออก (rollback 2026-07-03) — คงไว้เป็น false กัน dashboard พังจาก field หาย
  Firebase.setBool (fbData, base + "status/pump_locked", (millis() < pumpLockUntil));
  Firebase.setBool (fbData, base + "status/fan_locked",  (millis() < fanLockUntil));   // v2.8.0: พัดลมพักคู่ปั๊มอีกครั้ง
  Firebase.setBool (fbData, base + "status/time_ok",     timeValid());
  Firebase.setInt  (fbData, base + "status/wifi_rssi",   WiFi.RSSI());
}

void pushToFirebase() {
  const String base = "/smartfarm/";

  Firebase.setFloat (fbData, base + "sensors/air_temp",          airTemp);
  Firebase.setFloat (fbData, base + "sensors/air_humidity",      airHumidity);
  // น้ำ: push เฉพาะตอนอ่านได้ — กันค่าขยะ 0.0/ค่าเดิม ขึ้นไปหลอกหน้าจอ (dashboard เช็ค water_ok เพื่อโชว์ "—")
  if (waterSensorOk)
    Firebase.setFloat (fbData, base + "sensors/water_temp",      waterTemp);
  Firebase.setInt   (fbData, base + "sensors/uptime_sec",        (int)(millis() / 1000));

  pushStatus();

  if (fbData.errorReason() != "") {
    Serial.println("[Firebase] Error: " + fbData.errorReason());
  } else {
    // heap ทุก push (30 วิ) — เอาไว้ไล่ว่า "รันไปสักพักแล้วรีสตาร์ท" เกิดจาก heap ค่อยๆ หมด/แตกเป็นเสี่ยงไหม
    // free ลดเรื่อยๆ = leak · free นิ่งแต่ ก้อนใหญ่สุด หด = fragmentation (อันนี้อันตรายกว่า เพราะดูเหมือนปกติ)
    Serial.printf("[Firebase] Push OK · heap ว่าง %u (ก้อนใหญ่สุด %u)\n",
                  ESP.getFreeHeap(), ESP.getMaxAllocHeap());
  }
}

// ─────────────────────────────────────────────────────
// รองรับทั้ง active-HIGH และ active-LOW buzzer module (ตั้งค่าที่ BUZZER_ACTIVE_LOW)
void buzzerBeep(int times, int onMs, int offMs) {
  const int ON  = BUZZER_ACTIVE_LOW ? LOW  : HIGH;
  const int OFF = BUZZER_ACTIVE_LOW ? HIGH : LOW;
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, ON);  delay(onMs);
    digitalWrite(PIN_BUZZER, OFF); if (i < times-1) delay(offMs);
  }
}

// ─────────────────────────────────────────────────────
// ส่ง alert 1 ตัว — serial พิมพ์เสมอ, Firebase ส่งเฉพาะตอนเน็ตมี
// แยกแบบนี้เพราะ buzzer/serial เป็น local ทำงานได้แม้เน็ตดับ ส่วน Firebase คือ "ส่งออก" เท่านั้น
static void reportAlert(const char* type, float value, const String& message) {
  Serial.println("[ALERT] " + String(type) + " — " + message);
  if (!fbReady()) return;   // เน็ตดับ = ไม่มีที่ส่ง แต่ผู้เรียกยังตั้ง hasAlert → buzzer ดังอยู่ดี
  Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    type);
  Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   value);
  Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message", message);
}

void checkAlerts() {
  bool hasAlert = false;   // ตั้ง true = ให้ buzzer ดังท้ายฟังก์ชัน (ที่เดียว ไม่ก๊อป)

  // ── เซนเซอร์อากาศ ────────────────────────────────────
  // ⚠️ ต้องเช็คก่อน guard "ยังไม่พร้อม" ด้านล่าง — readSensors() ตั้ง airTemp/airHumidity = 0 ตอนอ่านพลาด
  // ซึ่งเข้าเงื่อนไข guard พอดี ถ้าเช็คทีหลังจะโดนกินทิ้ง = เซนเซอร์เสียแล้วเงียบสนิท
  if (airSensorFailCount >= SENSOR_STALE_AFTER) {
    // เซนเซอร์เสียไม่หายเอง ต่างจาก alert ร้อน/แห้ง — ย้ำตอน "เข้าสถานะ" แล้วทุก SENSOR_ALERT_REPEAT_MS
    // ไม่ใช่ทุกรอบ 30 วิ ไม่งั้น buzzer (blocking ~900ms) ดังทั้งคืน + เขียน Firebase ซ้ำเป็นพันครั้ง
    static unsigned long lastStaleAlertMs = 0;
    bool firstTime = !airStaleLatched;
    airStaleLatched = true;
    if (firstTime || millis() - lastStaleAlertMs >= SENSOR_ALERT_REPEAT_MS) {
      lastStaleAlertMs = millis();
      reportAlert("sensor_fail", airSensorFailCount,
        "เซนเซอร์อากาศอ่านไม่ได้ " + String(airSensorFailCount) + " ครั้งติด — ระบบอัตโนมัติหยุดทำงาน (ปิดพัดลม+ปั๊ม) กรุณาตรวจสอบ");
      hasAlert = true;
    }
  } else if (airTemp == 0 && airHumidity == 0) {
    // ยังไม่พร้อม (บูตใหม่) หรือพลาดชั่วคราวแต่ยังไม่ถึงเกณฑ์ stale → ข้ามเฉพาะ alert ที่อิงค่าอากาศ
    if (airStaleLatched) { airStaleLatched = false; Serial.println("[SHT35] เซนเซอร์กลับมาอ่านได้แล้ว"); }
    if (!airNotReadyLogged) { airNotReadyLogged = true; Serial.println("[ALERT] ข้าม alert อากาศ — sensor ยังไม่พร้อม"); }
  } else {
    if (airStaleLatched) { airStaleLatched = false; Serial.println("[SHT35] เซนเซอร์กลับมาอ่านได้แล้ว"); }
    airNotReadyLogged = false;

    if (airTemp > thresh_temp_alert) {
      reportAlert("high_temp", airTemp,
        "อุณหภูมิสูงเกิน " + String(thresh_temp_alert, 0) + "°C! (" + String(airTemp, 1) + "°C)");
      hasAlert = true;
    }
    if (airHumidity > 0 && airHumidity < thresh_hum_alert) {
      reportAlert("low_humidity", airHumidity,
        "ความชื้นต่ำกว่า " + String(thresh_hum_alert, 0) + "%! (" + String(airHumidity, 1) + "%)");
      hasAlert = true;
    }
  }

  // ── น้ำ (DS18B20) ────────────────────────────────────
  // คนละเซนเซอร์กับ SHT35 — SHT35 พังไม่ได้แปลว่าค่าน้ำเชื่อไม่ได้ จึงต้องอยู่นอกบล็อกอากาศทั้งหมด
  // (เดิม early-return ในบล็อกเซนเซอร์เสียกลืน alert นี้ทิ้ง = น้ำร้อนเกินตอน SHT35 เสีย → ไม่มีเตือน)
  if (waterSensorOk && waterTemp > thresh_water_temp_alert) {
    reportAlert("high_water_temp", waterTemp,
      "อุณหภูมิน้ำสูงเกิน " + String(thresh_water_temp_alert, 0) + "°C! (" + String(waterTemp, 1) + "°C)");
    hasAlert = true;
  }

  // Buzzer: ถ้ามี alert ให้ดังสั้น 3 ครั้ง (เว้นแต่ปิดเสียงจาก dashboard)
  if (hasAlert && buzzerEnabled) buzzerBeep(3);
  else if (hasAlert) Serial.println("[ALERT] Buzzer ปิดเสียงอยู่ (dashboard)");
}

// ─────────────────────────────────────────────────────
// Hourly Log — บันทึก avg/max/min ของ "ชั่วโมงที่เพิ่งจบ" ขึ้น path ที่ผู้เรียกส่งมา (/logs/YYYY-MM-DD/HH)
// ⚠️ reset accumulator ทุกทางออกเสมอ — เพราะ accumulator อยู่ RTC (รอด reboot) ถ้าไม่ reset จะค้างข้ามชั่วโมง
//    เดิม h_count==0 return โดยไม่ reset = เซนเซอร์อากาศตายแต่ค่าน้ำยังสะสม → น้ำโตไม่หยุด ไม่ถูกเขียน ไม่ถูกล้าง
// อากาศกับน้ำแยกกัน: เขียนอันที่มี sample จริง (อากาศดีแต่น้ำสายหลุด หรือกลับกัน = เขียนเท่าที่มี)
void pushHourlyLog(const String& path) {
  bool haveAir   = (h_count   > 0);
  bool haveWater = (h_countWT > 0);

  if (!haveAir && !haveWater) {   // ชั่วโมงนี้ไม่มีข้อมูลเลย (เซนเซอร์ตายทั้งคู่/บูตกลางชั่วโมง) — เคลียร์แล้วจบ
    Serial.printf("[Log] %s ไม่มีข้อมูลสะสม — ข้าม (เคลียร์ accumulator)\n", path.c_str());
    resetAccumulators();
    return;
  }
  if (!fbReady()) {   // push ไม่ได้ (เน็ตดับตอนขอบชั่วโมง) — ทิ้งชั่วโมงนี้ ไม่งั้นข้อมูลปนข้ามชั่วโมง
    Serial.printf("[Log] %s ข้าม — Firebase ไม่พร้อม (ทิ้งชั่วโมงนี้ กันข้อมูลปนข้ามชั่วโมง)\n", path.c_str());
    resetAccumulators();
    return;
  }

  float avgAT = 0, avgAH = 0, avgWT = 0;
  if (haveAir) {
    avgAT = h_sumAT / h_count;
    avgAH = h_sumAH / h_count;
    Firebase.setFloat(fbData, path + "/air_temp_avg",     avgAT);
    Firebase.setFloat(fbData, path + "/air_temp_max",     h_maxAT);
    Firebase.setFloat(fbData, path + "/air_temp_min",     h_minAT);
    Firebase.setFloat(fbData, path + "/air_humidity_avg", avgAH);
    Firebase.setFloat(fbData, path + "/air_humidity_max", h_maxAH);
    Firebase.setFloat(fbData, path + "/air_humidity_min", h_minAH);
    Firebase.setInt  (fbData, path + "/sample_count",     h_count);
  }
  if (haveWater) {   // เขียนเฉพาะเมื่อมี sample ที่อ่านได้ (กันค่าขยะ/ช่องว่างจากสายยาว)
    avgWT = h_sumWT / h_countWT;
    Firebase.setFloat(fbData, path + "/water_temp_avg",   avgWT);
    Firebase.setFloat(fbData, path + "/water_temp_max",   h_maxWT);
    Firebase.setFloat(fbData, path + "/water_temp_min",   h_minWT);
  }

  Serial.printf("[Log] Hourly → %s | T:%.1f°C RH:%.1f%% WT:%.1f°C (n=%d, nWT=%d)\n",
    path.c_str(), avgAT, avgAH, avgWT, h_count, h_countWT);

  // burst SSL ~7-10 write เพิ่งจบ — ตั้ง lastPushTime ให้ control poll เว้น 2 วิก่อนยิง getJSON
  // ไม่งั้น poll รอบถัดไป (1.5 วิ) เข้าไปชน SSL session เดียวกัน = เคสเดียวกับที่ v2.1.1 แก้ให้ pushStatus()
  lastPushTime = millis();

  resetAccumulators();
}

void resetAccumulators() {
  h_sumAT = h_sumAH = h_sumWT = 0;
  h_maxAT = h_maxAH = h_maxWT = -999;
  h_minAT = h_minAH = h_minWT =  999;
  h_count = 0;
  h_countWT = 0;
}

// ─────────────────────────────────────────────────────
void syncNTP() {
  // UTC+7 (ประเทศไทย): offset = 7 * 3600 = 25200
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("NTP sync");
  struct tm t;
  int tries = 0;
  while (!getLocalTime(&t) && tries < 20) {
    delay(500); Serial.print("."); tries++;
  }
  if (tries < 20) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    Serial.println(" OK → " + String(buf));
  } else {
    Serial.println(" FAILED (hourly log จะข้ามจนกว่าจะ sync ได้)");
  }
}

// ─────────────────────────────────────────────────────
// Schedule Check — เรียกทุก 60 วินาทีจาก loop()
// Schedule ทำงานเฉพาะเมื่อ ch_schedEnabled[i] = true
// ถ้า on_time < off_time = ปกติ (เช่น 07:00–18:00)
// ถ้า on_time > off_time = ข้ามคืน (เช่น 22:00–06:00)
void checkSchedule() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return;   // ms=0 = ไม่บล็อก (ดู timeValid) — เดิม default 5000 บล็อก 5 วิ/นาที ตอนนาฬิกายังไม่ติด
  if (!timeValid()) {
    // latch — NTP ไม่ติด = ขึ้นทุก 60 วิ ตลอดกาล
    if (!schedNoTimeLogged) { schedNoTimeLogged = true; Serial.println("[SCHED] ข้าม — นาฬิกายังไม่ sync (schedule จะไม่ทำงานจนกว่า NTP จะติด)"); }
    return;
  }
  if (schedNoTimeLogged) { schedNoTimeLogged = false; Serial.println("[SCHED] นาฬิกา sync แล้ว — schedule กลับมาทำงาน"); }
  char nowBuf[6]; strftime(nowBuf, sizeof(nowBuf), "%H:%M", &t);
  String now = String(nowBuf);

  const int pins[4] = {PIN_RELAY_CH1, PIN_RELAY_CH2, PIN_RELAY_CH3, PIN_RELAY_CH4};
  bool* states[4]   = {&ch1_pump, &ch2_fanOut, &ch3_fanIn, &ch4_spare};

  for (int i = 0; i < 4; i++) {
    if (!ch_schedEnabled[i]) continue;

    String onT  = String(ch_schedOn[i]);
    String offT = String(ch_schedOff[i]);
    bool shouldBeOn;
    if (onT < offT) {
      // ปกติ: เช่น 07:00–18:00
      shouldBeOn = (now >= onT && now < offT);
    } else {
      // ข้ามคืน: เช่น 22:00–06:00
      shouldBeOn = (now >= onT || now < offT);
    }

    // ปั๊ม (CH4) เคารพ safety lock — ห้ามเปิดระหว่าง cooldown
    if (i == IDX_PUMP && shouldBeOn && millis() < pumpLockUntil) continue;
    if (i == IDX_FAN  && shouldBeOn && millis() < fanLockUntil)  continue;   // v2.8.0: พัดลมพักคู่ปั๊ม

    if (shouldBeOn != *states[i]) {
      *states[i] = shouldBeOn;
      setRelay(pins[i], shouldBeOn);
      if (i == IDX_PUMP) lastPumpSwitchTime = millis();
      Serial.printf("[SCHED] CH%d → %s (now:%s on:%s off:%s)\n",
        i+1, shouldBeOn?"ON":"OFF", nowBuf, ch_schedOn[i], ch_schedOff[i]);
    }
  }
}

// ─────────────────────────────────────────────────────
// LCD — สลับ 3 หน้า ทุก 5 วินาที
// หน้า 0: อุณหภูมิ + ความชื้นอากาศ
// หน้า 1: อุณหภูมิน้ำ + WiFi RSSI
// หน้า 2: สถานะ Pump (CH4) + Fan (CH3)
void updateLCD() {
  if (!lcd) return;   // ไม่เจอ LCD ตอนบูต (address ผิด/สายหลุด) — ข้ามแทนที่จะ crash

  // LCD ไม่มีทางอ่านค่ากลับมาเช็คว่าเพี้ยนไหม (ไม่เหมือน SHT35/DS18B20 ที่ validate ค่าได้)
  // ถ้า I2C โดน noise จาก relay/ปั๊มรบกวนกลางทาง ตัวควบคุมจออาจ "ค้าง" สถานะภายในเพี้ยน
  // (เช่น cursor/DDRAMผิดตำแหน่ง) จนตัวอักษรกลายเป็นขยะถาวร — re-init เป็นระยะเชิงป้องกันไว้ก่อน
  static uint8_t lcdCycles = 0;
  if (++lcdCycles >= 6) {   // ทุก ~30 วิ (updateLCD ทุก 5 วิ) — re-init ล้างสถานะเพี้ยนที่อาจสะสม
    lcdCycles = 0;
    lcd->init();
  }

  lcd->clear();
  char buf1[17], buf2[17];

  switch (lcdPage) {
    case 0:
      snprintf(buf1, sizeof(buf1), "Temp: %.1f%cC", airTemp, 0xDF);
      snprintf(buf2, sizeof(buf2), "Humidity: %.1f%%", airHumidity);
      lcd->setCursor(0, 0); lcd->print(buf1);
      lcd->setCursor(0, 1); lcd->print(buf2);
      break;

    case 1:
      if (waterSensorOk) snprintf(buf1, sizeof(buf1), "Water: %.1f%cC", waterTemp, 0xDF);
      else               snprintf(buf1, sizeof(buf1), "Water: -- (err)");
      snprintf(buf2, sizeof(buf2), "WiFi: %ddBm", WiFi.RSSI());
      lcd->setCursor(0, 0); lcd->print(buf1);
      lcd->setCursor(0, 1); lcd->print(buf2);
      break;

    case 2:
      snprintf(buf1, sizeof(buf1), "Pump: %s", ch4_spare ? "ON" : "OFF");
      snprintf(buf2, sizeof(buf2), "Fan: %s",  ch3_fanIn ? "ON" : "OFF");
      lcd->setCursor(0, 0); lcd->print(buf1);
      lcd->setCursor(0, 1); lcd->print(buf2);
      break;
  }

  lcdPage = (lcdPage + 1) % 3;  // วนหน้า 0→1→2→0
}

// คืนค่า path สำหรับ hourly log เช่น "/logs/2026-06-19/14"
String getHourlyPath() {
  struct tm t;
  if (!getLocalTime(&t, 0)) return "";   // ms=0 = ไม่บล็อก (ดู timeValid) — ถูกเรียกทุกรอบ loop ตอน timeValid()
  // ปกติ caller เรียกตอน timeValid ผ่านแล้ว (นาฬิกาติด) จึงคืนค่าทันที ไม่แตะ 5 วิ default
  char path[48];
  strftime(path, sizeof(path), "/logs/%Y-%m-%d/%H", &t);
  return String(path);
}
