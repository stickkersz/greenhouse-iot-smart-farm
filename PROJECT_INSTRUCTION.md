# Project Instruction — Greenhouse IoT Smart Farm

## บริบทโปรเจกต์
คุณกำลังช่วย **Tonkla** (IT Intern, CS Year 2, KMUTT) พัฒนาระบบ Greenhouse IoT Smart Farm ที่ **บริษัท ปุ๋ยไวกิ้ง จำกัด** ระหว่างฝึกงาน มิถุนายน–กรกฎาคม 2569 โปรเจกต์นี้ควบคุมโรงเรือน 460×240×270 cm (3 แถวปลูกผัก) ผ่าน ESP32 + Firebase + Web Dashboard

---

## Hardware ที่ใช้งานอยู่

| อุปกรณ์ | GPIO / รายละเอียด |
|---|---|
| ESP32 DevKit V1 **30-pin** บน Expansion Board HW-777 | WiFi 2.4GHz เท่านั้น |
| DS18B20 Waterproof | GPIO4, Pull-up 5.1kΩ (VCC–DATA) ที่ฝั่ง ESP32 |
| DHT22 | GPIO18 (Pin1=VCC, Pin2=DATA, Pin3=NC, Pin4=GND) — ย้ายจาก GPIO32 เมื่อ 2026-07-02 ให้ไกลกลุ่ม relay กัน noise (ตัวแปรในโค้ดยังชื่อ `PIN_DHT11` ตามชื่อเดิม แต่ต่อกับ DHT22 จริง) |
| Buzzer Module (Active) | GPIO33 (GND–I/O–VCC) |
| LCD I2C 16x2 | SDA=GPIO21, SCL=GPIO22, Address auto-detect 0x27/0x3F ตอนบูต, **ต้องการ 5V** |
| Relay CH1 | GPIO26 = สำรอง (manual only) |
| Relay CH2 | GPIO27 = ไม่ใช้ (ซ่อนใน dashboard) |
| Relay CH3 | GPIO14 = พัดลม 220V AC (auto ตามอุณหภูมิ) |
| Relay CH4 | GPIO25 = ปั๊มน้ำ 24V DC (auto ตามความชื้น) |

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

---

## Firmware v2.2.1

**ไฟล์:** `smartfarm_firmware/smartfarm_firmware.ino` + `config.h`

**Libraries ที่ต้องติดตั้ง:**
- Firebase ESP32 Client by Mobizt
- OneWire + DallasTemperature (DS18B20)
- Adafruit SHT31 Library (รองรับ SHT30/31/35 — เซนเซอร์อากาศ SHT35 I2C) + Adafruit BusIO
- LiquidCrystal I2C by Frank de Brabander

**สิ่งที่ยกเลิกแล้ว:** DHT22 (เปลี่ยนไปใช้ SHT35 I2C แล้ว 2026-07-11), ระบบแจ้งเตือนออกนอกเครือข่ายทุกรูปแบบ (ไม่ใช้แล้ว — เตือนด้วย buzzer + dashboard เท่านั้น), Capacitive Soil Moisture (ยกเลิกถาวร) — ถูกลบออกจาก firmware/dashboard ทั้งหมดแล้ว

**Auto-Control Thresholds (default, ปรับได้จาก Dashboard Settings):**
- TEMP_ON = 35°C / TEMP_OFF = 32°C → พัดลม CH3 (คุมด้วย "อุณหภูมิอากาศ" อย่างเดียว — ถอดการผูกกับอุณหภูมิน้ำออกแล้ว v1.6.0 · น้ำเหลือหน้าที่ แสดง/log/alert)
- HUMIDITY_MIN = 60% / humidity_max = 75% → ปั๊ม CH4 (hysteresis กันปั๊มกระพริบ)
- thresh_temp_alert = 38°C, thresh_hum_alert = 40% → เกณฑ์แจ้งเตือน (buzzer + banner บน dashboard)
- Pump Safety: เดินต่อเนื่องได้สูงสุด **10 นาที** (ตัดสินใจสุดท้าย 2026-07-07, เดิม 5 นาที), พัก 5 นาที

**LCD:** 3 หน้า สลับทุก 5 วิ (หน้า 0: Air Temp/Hum → หน้า 1: Water Temp/WiFi RSSI → หน้า 2: Pump/Fan Status) — ✅ ทำงานปกติ (2026-07-08: จอตัวเดิมเสีย ไม่ใช่ปัญหาสาย/โค้ดตามที่สงสัยไว้ก่อนหน้า เปลี่ยนจอใหม่ต่อสายเดิมแล้วติดทันที)

**Control:** Polling Firebase `/smartfarm/control` ทุก 1.5 วิ (ไม่ใช้ Stream)

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
| Buzzer ดังตลอด | DHT22 ไม่ต่อ → humidity=0 < 40% | ต่อ DHT22 หรือ check null guard |
| LCD ไม่ทำงาน | VCC ต่อ 3.3V หรือ wiring ผิด | ต้องใช้ 5V |
| Port ไม่ขึ้น Arduino IDE (Mac M3) | ไม่มี CH340 driver | ติดตั้งจาก wch-ic.com |
| Boost Converter พัง | Short ขณะมีไฟ | ปิดไฟก่อนต่อสายเสมอ |

---

## TODO ที่ยังค้างอยู่

ดู PROJECT_MEMORY.md หัวข้อ 15 สำหรับรายการล่าสุด/ละเอียด — สรุปสั้นๆ ณ 2026-07-08:
- 🔴 ด่วนสุด (hardware): แก้ noise มอเตอร์ปั๊มรบกวนเซนเซอร์ — เพิ่ม cap คร่อมมอเตอร์ปั๊ม + RC snubber ที่ relay CH4 + **ย้ายสาย/ตัวเซนเซอร์ SHT35 ออกห่างจากกลุ่ม relay** (หลักฐานยืนยันว่าระยะห่างสำคัญกว่าคุณภาพจุดต่อ — เดิมเจอกับ DHT22, SHT35 ที่ต่อใกล้ relay ก็เสี่ยงเจอแบบเดียวกัน — ดู PROJECT_MEMORY §15)
- (moot แล้ว) เคยมีปัญหาสาย DHT22 VCC ต่อผิดช่อง D19 — DHT22 ถอดออกแล้ว 2026-07-11 เปลี่ยนเป็น SHT35 (I2C)
- ✅ LCD I2C แก้แล้ว (2026-07-08) — จอตัวเดิมเสีย เปลี่ยนจอใหม่ทำงานปกติ
- 🔴 Revoke bot token เก่าของระบบแจ้งเตือนผ่าน @BotFather (/revoke) — ยังอยู่ใน git history, ลบไฟล์ไม่ช่วย
- `firebase deploy` ทุกครั้งที่แก้ `dashboard/index.html` เพื่ออัปเดตเว็บที่ deploy ไว้ (`greenhouse-iot-smart-farm.web.app`)

---

## โครงสร้างไฟล์

```
/Greenhouse IoT Smart Farm/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   (v2.2.1)
│   └── config.h
├── dashboard/
│   ├── index.html
│   └── index.v1.1.0.backup.html
├── pinout_v2.md                 ← สรุปการต่อสายล่าสุด (pinout.docx/pdf เป็นเวอร์ชันเก่า ล้าสมัยบางจุด อย่าอ้างอิง)
├── TEAM_ONBOARDING.md           ← วิธีเพิ่ม/ลบ account พนักงานผ่าน Firebase Console
├── DAILY_OPERATIONS_CHECKLIST.md ← เช็คลิสต์ประจำวัน/สัปดาห์ สำหรับทีมที่ไม่ใช่สาย IT
├── PROJECT_MEMORY.md            ← ข้อมูลเทคนิคละเอียด
└── PROJECT_INSTRUCTION.md       ← ไฟล์นี้
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
- **Dashboard ไม่ต้องมีระบบ role/สิทธิ์แยกระดับ** — ทีมงานขนาดเล็ก ทุก account ที่ Tonkla สร้างให้มี
  สิทธิ์เท่ากันหมด (ดู `TEAM_ONBOARDING.md`) ไม่ต้องเสนอทำ RBAC เพิ่มเว้นแต่ผู้ใช้ขอเอง
