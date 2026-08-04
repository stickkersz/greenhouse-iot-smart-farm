# Project Instruction — Greenhouse IoT Smart Farm

## บริบทโปรเจกต์
คุณกำลังช่วย **Nattakit Prasertsak** (IT Intern, CS Year 2, KMUTT) พัฒนาระบบ Greenhouse IoT Smart Farm ที่ **บริษัท ปุ๋ยไวกิ้ง จำกัด** ระหว่างฝึกงาน มิถุนายน–กรกฎาคม 2569 โปรเจกต์นี้ควบคุมโรงเรือน 460×240×270 cm (3 แถวปลูกผัก) ผ่าน ESP32 + Firebase + Web Dashboard

---

## Hardware ที่ใช้งานอยู่

| อุปกรณ์ | GPIO / รายละเอียด |
|---|---|
| ESP32 DevKit V1 **30-pin** บน Expansion Board HW-777 | WiFi 2.4GHz เท่านั้น |
| DS18B20 Waterproof | GPIO4, Pull-up 4.7kΩ ระหว่าง DATA กับ **3V3 เท่านั้น** — ห้ามต่อไป VCC/5V (เกิน abs max ของขา ผ่าน ESD clamp ดู `pinout.md`) |
| SHT35 (อุณหภูมิ+ความชื้นอากาศ) | I2C — SDA=GPIO21, SCL=GPIO22 ร่วมบัสกับ LCD, address auto-scan 0x44/0x45 (ไม่ hardcode) — แทน DHT22 ตั้งแต่ 2026-07-11 (GPIO18 ว่างแล้ว) |
| Buzzer Module (Active) | GPIO33 (GND–I/O–VCC), active-LOW |
| LCD I2C 16x2 | SDA=GPIO21, SCL=GPIO22, Address auto-detect 0x27/0x3F ตอนบูต, **ต้องการ 5V** |
| Relay CH1 | GPIO26 = สำรอง (manual only) |
| Relay CH2 | GPIO27 = ไม่ใช้ (ซ่อนใน dashboard) |
| Relay CH3 | GPIO14 = พัดลม 220V AC (auto ตามอุณหภูมิ + ความชื้น + vent) |
| Relay CH4 | GPIO25 = ปั๊มน้ำ 24V DC (auto ตามความชื้น) · ⚠️ 2026-08-03 เคยแก้เฟิร์มแวร์เป็น GPIO27 (สลับกับ CH2) เพราะช่อง CH4 บนบอร์ดรีเลย์เสีย แต่ไม่เคยแฟลชลงบอร์ด · พี่เลี้ยงแก้ที่ฝั่งฮาร์ดแวร์แทนและปั๊มกลับมาทำงานโดยบอร์ดยังใช้ GPIO25 อยู่ · ยืนยัน 2026-08-05 ว่าไม่ได้แฟลชซ้ำ จึงถอยกลับเป็น GPIO25 ให้ตรงสายจริง |
| Status LED | GPIO2 (onboard), active-HIGH |

**ระบบจ่ายไฟ:**
```
220V AC → S-120-12 PSU (12V/10A) ─┬─ XL4015 Step-Down → 5V → Expansion Board (ESP32/Logic ทั้งหมด)
                                   └─ Boost Converter (XL6009) → 24V → Relay CH4 → ปั๊มน้ำ
```

**ข้อห้ามสำคัญ:**
- GPIO12 ห้ามใช้กับ Relay — เป็น Strapping Pin ทำให้ boot fail
- LCD I2C ต้องการ 5V ไม่ใช่ 3.3V
- Relay เป็น Active-LOW: LOW=เปิด, HIGH=ปิด, ใช้ขา NO เสมอ
- ปิดไฟก่อนต่อ/ถอดสาย Boost Converter และปั๊มเสมอ
- GPIO34 (ADC1, Input-Only) ว่างอยู่ — เคยจองไว้สำหรับ Capacitive Soil Moisture แต่ยกเลิกแผนถาวรแล้ว (ดู "สิ่งที่ยกเลิกแล้ว")
- DS18B20 pull-up ต้องไป 3V3 ไม่ใช่ VCC/5V (ดูแถวด้านบน)

---

## Firmware v2.9.2

**ไฟล์:** `smartfarm_firmware/smartfarm_firmware.ino` + `config.h`

**Libraries ที่ต้องติดตั้ง:**
- Firebase ESP32 Client by Mobizt
- OneWire + DallasTemperature (DS18B20)
- Adafruit SHT31 Library (รองรับ SHT30/31/35 — เซนเซอร์อากาศ SHT35 I2C) + Adafruit BusIO
- LiquidCrystal I2C by Frank de Brabander

**สิ่งที่ยกเลิกแล้ว:** DHT22 (เปลี่ยนไปใช้ SHT35 I2C แล้ว 2026-07-11), ระบบแจ้งเตือนออกนอกเครือข่ายทุกรูปแบบ (ไม่ใช้แล้ว — เตือนด้วย buzzer + dashboard เท่านั้น), Capacitive Soil Moisture (ยกเลิกถาวร) — ถูกลบออกจาก firmware/dashboard ทั้งหมดแล้ว

**Auto-Control — 4 hysteresis latch จากค่า SHT35 (`auto_control_logic.h`, unit-tested แยกจาก I/O):**
- **hot**: temp ≥ `temp_on` (default 35°C) จน temp ≤ `temp_off` (32°C) → พัดลม
- **dry**: humidity < `humidity_min` (60%) จน humidity ≥ `humidity_max` (75%) → พัดลม + ปั๊ม
- **pumpHeat**: temp ≥ `temp_on` จน temp ≤ `temp_off` **หรือ** humidity ≥ `humidity_max` → ปั๊มอย่างเดียว
- **wet (vent)**: humidity ≥ `humidity_vent` (default 80%) จน humidity ≤ `humidity_vent − VENT_HYST` (5%) → พัดลมอย่างเดียว (ระบายความชื้นเกิน)
- Output: `fanOn = hot OR dry OR wet` · `pumpOn = (hot AND pumpHeat) OR dry` — **`pumpOn → fanOn` เสมอ** (ปั๊มห้ามทำงานลำพัง กันน้ำท่วมไม่ระเหย)
- `humidity_vent ≥ humidity_max + VENT_HYST` เป็น constraint บังคับ ต้องตรงกันทั้ง firmware/rules/dashboard — เช็คอัตโนมัติด้วย `npm run test:sync`
- Pump Safety: เดินต่อเนื่องได้สูงสุด **15 นาที** (ยืดจาก 10 นาทีเมื่อ 2026-07-20) แล้วพัก **5 นาที** — พัดลมพักคู่ปั๊มเสมอตั้งแต่ v2.8.0 (`PUMP_MAX_RUNTIME_MS`/`FAN_MAX_RUNTIME_MS` เป็น `#define` ต้อง reflash ถึงจะเปลี่ยน)
- thresh_temp_alert = 38°C, thresh_hum_alert = 40% → เกณฑ์แจ้งเตือน (buzzer + banner บน dashboard, แยกจาก auto control)

**LCD:** 3 หน้า สลับทุก 5 วิ (หน้า 0: Air Temp/Hum → หน้า 1: Water Temp/WiFi RSSI → หน้า 2: Pump/Fan Status)

**Control:** Polling Firebase `/smartfarm/control` ทุก 1.5 วิ (ไม่ใช้ Stream) · Push status/sensors ทุก 30 วิ แบบ batched PATCH เดียว (`updateNodeSilent`) ตั้งแต่ v2.9.1 — ไม่ใช่ยิง `setX()` แยกทีละฟิลด์แบบเดิม

---

## Firebase

```
URL: https://greenhouse-iot-smart-farm-default-rtdb.asia-southeast1.firebasedatabase.app
API Key: AIzaSyBZ2ke2Bbx-Nd9zhsZa0QDZUlKRtEbnCWE
Auth: Anonymous Authentication
```

**Database paths หลัก:**
- `/smartfarm/sensors` — ข้อมูล sensor real-time
- `/smartfarm/status` — สถานะ relay + firmware + health (sensor_ok, water_ok, pump_locked ฯลฯ)
- `/smartfarm/control` — threshold + mode + schedule
- `/smartfarm/alerts/last_alert` — alert ล่าสุดจาก firmware (type/value/message)
- `/smartfarm/alert_history` — ประวัติ alert ฝั่ง dashboard สร้างเอง (client-side check, ไม่ได้มาจาก firmware โดยตรง)
- `/smartfarm/action_log` — log การสั่งงาน
- `/logs/YYYY-MM-DD/HH` — hourly avg/max/min

**หมายเหตุ Firebase key ชื่อเก่า:** `ch1_pump`=สำรอง, `ch4_spare`=ปั๊มน้ำ (ชื่อไม่ตรงหน้าที่จริง)

---

## WiFi

```
- floor-1-2-2.4G   (ชั้น 1-2)    — รหัสใน config.h
- Lab_F2-2.4G      (Lab ชั้น 2)  — รหัสใน config.h
```
ESP32 รองรับ **2.4GHz เท่านั้น**

---

## Web Dashboard

**ไฟล์:** `dashboard/index.html`
- Real-time sensor + relay toggle + AUTO/MANUAL mode
- History Chart 24h (Chart.js) จาก `/logs/YYYY-MM-DD/HH`
- Action Log + ตั้งค่า threshold ผ่าน dashboard
- Deploy: `firebase deploy` → `greenhouse-iot-smart-farm.web.app`

---

## ปัญหาที่เคยเจอ (สำคัญ)

| อาการ | สาเหตุ | วิธีแก้ |
|---|---|---|
| DS18B20 อ่าน -127°C | ต่อสายผิดด้าน | Flat side หันหาตัวเอง: ซ้าย=GND, กลาง=DATA, ขวา=VCC |
| Boot fail "invalid header" | GPIO12 strapping pin | เปลี่ยน pin Relay |
| Buzzer ดังตลอด | SHT35 อ่านไม่ได้ → humidity=0 < 40% | เช็คสาย I2C (GPIO21/22) และ address auto-scan |
| LCD ไม่ทำงาน | VCC ต่อ 3.3V หรือ wiring ผิด | ต้องใช้ 5V |
| Port ไม่ขึ้น Arduino IDE (Mac M3) | ไม่มี CH340 driver | ติดตั้งจาก wch-ic.com |
| Boost Converter พัง | Short ขณะมีไฟ | ปิดไฟก่อนต่อสายเสมอ |

---

## TODO ที่ยังค้างอยู่

ดู PROJECT_MEMORY.md หัวข้อ 15 สำหรับรายการล่าสุด/ละเอียด — สรุปสั้นๆ ณ 2026-07-08:
- 🔴 ด่วนสุด (hardware): แก้ noise มอเตอร์ปั๊มรบกวนเซนเซอร์ — เพิ่ม cap คร่อมมอเตอร์ปั๊ม + RC snubber ที่ relay CH4 + **ย้ายสาย/ตัวเซนเซอร์ SHT35 ออกห่างจากกลุ่ม relay** (หลักฐานยืนยันว่าระยะห่างสำคัญกว่าคุณภาพจุดต่อ — เดิมเจอกับ DHT22, SHT35 ที่ต่อใกล้ relay ก็เสี่ยงเจอแบบเดียวกัน — ดู PROJECT_MEMORY §15)
- (moot แล้ว) เคยมีปัญหาสาย DHT22 VCC ต่อผิดช่อง D19 — DHT22 ถอดออกแล้ว 2026-07-11 เปลี่ยนเป็น SHT35 (I2C)
- ✅ LCD I2C แก้แล้ว (2026-07-08) — จอตัวเดิมเสีย เปลี่ยนจอใหม่ทำงานปกติ
- `firebase deploy` ทุกครั้งที่แก้ `dashboard/index.html` เพื่ออัปเดตเว็บที่ deploy ไว้ (`greenhouse-iot-smart-farm.web.app`)

---

## โครงสร้างไฟล์

```
/Greenhouse IoT Smart Farm/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   (v2.9.2)
│   ├── auto_control_logic.h     ← decision math ล้วน แยกจาก I/O เพื่อ unit test
│   └── config.h                 (gitignored)
├── dashboard/
│   └── index.html
├── docs/
│   ├── pinout.md                 ← สรุปการต่อสายล่าสุด (ฉบับเดียว — pinout.docx/pdf เก่าถูกลบทิ้งแล้ว)
│   ├── TEAM_ONBOARDING.md        ← วิธีเพิ่ม/ลบ account พนักงานผ่าน Firebase Console
│   ├── DAILY_OPERATIONS_CHECKLIST.md ← เช็คลิสต์ประจำวัน/สัปดาห์ สำหรับทีมที่ไม่ใช่สาย IT
│   ├── HANDOFF_GUIDE.md          ← สำหรับใครก็ตามที่รับช่วงดูแลระบบต่อ
│   ├── PROJECT_MEMORY.md         ← ข้อมูลเทคนิคละเอียด
│   ├── PROJECT_INSTRUCTION.md    ← ไฟล์นี้
│   └── archive/                  ← เอกสารก่อนสร้างระบบจริง (ประวัติเท่านั้น)
└── media/                        ← รูปภาพ ไม่เกี่ยวกับโค้ด
```

---

## แนวทางในการช่วยเหลือ

- ตอบเป็น**ภาษาไทย**เสมอ (ยกเว้น code และ technical term)
- เมื่อเขียนหรือแก้ไข firmware ให้อ้างอิง pin mapping ข้างต้น
- ถ้าต้องแก้ไข config.h ให้ระวัง WiFi password (sensitive)
- เมื่อแนะนำ hardware ให้คำนึงว่าเป็นโรงเรือนขนาดเล็ก ~11m²
- Relay เป็น Active-LOW เสมอ — ตรวจสอบ logic ให้ถูกต้องทุกครั้ง

**การตัดสินใจเรื่อง Stability (2026-07-07 — อย่าเสนอย้อนกลับโดยไม่ถามก่อน):**
- **ห้ามเพิ่ม runtime failsafe/recovery layer กลับเข้า firmware** (เช่น checkFailsafe บังคับ relay,
  ESP.restart() อัตโนมัติเมื่อ sensor/Firebase ค้าง) จนกว่าจะแก้ hardware noise (cap+snubber ที่ปั๊ม)
  เสร็จก่อน — เคยเพิ่มแล้วถอดออกเพราะ A/B test พิสูจน์ว่าปัญหาคือ noise ฮาร์ดแวร์ ไม่ใช่โค้ด และการมี
  recovery layer ซับซ้อนขึ้นไม่ได้ช่วยอะไร ยกเว้นผู้ใช้ขอเปลี่ยนใจเอง
  - ข้อยกเว้นที่ทำได้: boot-time retry แบบมีขอบเขต (เช่น WiFi connect retry, Firebase Auth retry
    ก่อน ESP.restart()) เพราะเป็น deterministic check ตอนบูตครั้งเดียว ไม่ใช่ heuristic ที่รันตลอดและ
    เคยไปชนกับ noise — ปัจจุบันมีทั้งสองแบบแล้ว
- **Dashboard ไม่ต้องมีระบบ role/สิทธิ์แยกระดับ** — ทีมงานขนาดเล็ก ทุก account ที่ Nattakit Prasertsak สร้างให้มี
  สิทธิ์เท่ากันหมด (ดู `TEAM_ONBOARDING.md`) ไม่ต้องเสนอทำ RBAC เพิ่มเว้นแต่ผู้ใช้ขอเอง
