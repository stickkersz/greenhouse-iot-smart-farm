# Greenhouse IoT Smart Farm — Project Memory
**บริษัทประวิทย์กรุ๊ป ปุ๋ยไวกิ้ง จำกัด**
จัดทำโดย: Tonkla (IT Intern, CS Year 2, KMUTT) | มิถุนายน–กรกฎาคม 2569

---

## 1. โครงสร้างโปรเจกต์

```
/Greenhouse IoT Smart Farm/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   ← Firmware หลัก (v1.4.0)
│   └── config.h                 ← Pin mapping, WiFi, Firebase, Telegram
├── dashboard/
│   ├── index.html               ← Web Dashboard (Green Nature theme)
│   └── index.v1.1.0.backup.html ← Backup ก่อน redesign
├── Hardware_Checklist.md
├── ESP32_Firmware_Plan.md
├── Firebase_Database_Structure.md
└── PROJECT_MEMORY.md            ← ไฟล์นี้
```

---

## 2. ขนาดโรงเรือน

| มิติ | ค่า |
|---|---|
| ความยาว | 460 cm |
| ความกว้าง | 240 cm |
| ความสูง | 270 cm |
| พื้นที่ | ~11 m² |
| โครงสร้าง | หลังคาเอียง (Side view: สูงด้านหนึ่งกว่าอีกด้าน) |
| แถวปลูก | 3 แถว (ชั้นวางผัก ปลูกพืชขั้นขั้นใด) |
| ประตู | 1 ประตู ด้านหน้า |

**ปั๊มน้ำที่แนะนำ:** 24V DC ไดอะแฟรม, **2–3 bar**, **5–10 L/min**
- ใช้ได้กับทั้ง Drip irrigation และ Mini sprinkler
- Self-priming (ดูดน้ำเองได้ ไม่ต้องจุ่มน้ำ)
- ควบคุม on/off ผ่าน Relay ได้ตรงๆ

---

## 3. Hardware

| อุปกรณ์ | สถานะ | หมายเหตุ |
|---|---|---|
| ESP32 DevKit V1 **(30-pin)** | ✅ ใช้งาน | WiFi 2.4GHz เท่านั้น, เสียบ Expansion Board ได้พอดี |
| ESP32 Expansion Board HW-777 | ✅ ใช้งาน | SVG = Signal-VCC-GND ต่อ pin ตรงๆ ไม่ต้องสาย Jumper |
| S-25-5 PSU (5V / 5A) | ❌ ไม่ได้ใช้แล้ว | เปลี่ยนมาใช้ XL4015 Step-Down แทน |
| S-120-12 PSU (12V / 10A) | ✅ ใช้งาน | จ่ายไฟทั้งระบบ DC: XL4015 + Boost Converter |
| XL4015 Step-Down | ✅ ใช้งาน **(หลัก)** | 12V → 5V จ่าย ESP32 + Relay + Fan 5V + Buzzer + LCD |
| Boost Converter (XL6009/XL4016) | ✅ มีแล้ว (ตัวใหม่) | 12V → 24V สำหรับปั๊มน้ำ (ตัวเก่าพังเพราะ Short) |
| Relay Module 4CH (5V) | ✅ ทำงาน | Active-LOW (LOW=เปิด, HIGH=ปิด) |
| DHT22 | ✅ ทำงาน **(ถาวร)** | GPIO32 — วัดอุณหภูมิ + ความชื้นอากาศ |
| DS18B20 Waterproof | ✅ ทำงาน | GPIO4 — วัดอุณหภูมิน้ำ, **ต้องมี Pull-up 4.7kΩ** (DATA-VCC) |
| LCD I2C 16x2 | ✅ มีแล้ว | Address 0x27, SDA=GPIO21, SCL=GPIO22, **ต้องการไฟ 5V** |
| Buzzer Module (Active) | ✅ ทำงาน | GPIO33 (GND–I/O–VCC), เสียงเตือนแจ้งเตือน |
| Fan 220V AC (พัดลม Shutter 10") | 🛒 ต้องซื้อ × 2 | ระบายอากาศโรงเรือน — ช่างไฟเดินสาย |
| Fan Module 5V | ⚠️ มีปัญหา | ระบายความร้อนกล่อง — ยังไม่หมุน (ตรวจ JUMP jumper) |
| ปั๊มน้ำ DC 24V | 🛒 ต้องซื้อ | แนะนำ: ไดอะแฟรม 24V, 2–3 bar, 5–10 L/min |
| PWM Speed Controller CW008 | ✅ มีแล้ว | IN+/IN−/OUT+/OUT− สำหรับควบคุมความเร็วปั๊ม |

> ❌ **SHT35 ยกเลิกแล้ว** — ไม่ซื้อ ใช้ DHT22 ถาวร (SHT35 ถูกลบออกจาก firmware ทั้งหมดแล้ว)

---

## 4. Pin Mapping (config.h)

```cpp
// ── Sensors ───────────────────────────────────────────
#define PIN_DS18B20       4    // DS18B20 Data (ต้องมี External Pull-up 5.1kΩ)
#define PIN_DHT11        32    // DHT22 — อุณหภูมิ + ความชื้นอากาศ (ถาวร)
#define PIN_BUZZER       33    // Buzzer Module (I/O)

// LCD I2C: SDA=GPIO21, SCL=GPIO22 (ESP32 default I2C), Address=0x27

// ── Relay (Active-LOW: LOW=เปิด, HIGH=ปิด) ─────────────
#define PIN_RELAY_CH1    26   // CH1 = สำรอง (manual/schedule เท่านั้น)
#define PIN_RELAY_CH2    27   // CH2 = ไม่ได้ใช้ (ซ่อนใน dashboard)
#define PIN_RELAY_CH3    14   // CH3 = พัดลม 220V AC (ดูดเข้า) — auto ตามอุณหภูมิ
#define PIN_RELAY_CH4    25   // CH4 = ปั๊มน้ำ 24V DC — auto ตามความชื้น + pump safety
```

> ⚠️ **GPIO12 ห้ามใช้กับ Relay** — เป็น strapping pin ทำให้ ESP32 boot fail ("invalid header")
> ⚠️ **LCD I2C ต้องการไฟ 5V** (ไม่ใช่ 3.3V) — ต่อ VCC เข้า 5V rail ของ Expansion Board

---

## 5. Firebase

```
Project ID:   greenhouse-iot-smart-farm
Database URL: https://greenhouse-iot-smart-farm-default-rtdb.asia-southeast1.firebasedatabase.app
API Key:      AIzaSyBZ2ke2Bbx-Nd9zhsZa0QDZUlKRtEbnCWE
Auth:         Anonymous Authentication (เปิดแล้ว)
```

### Database Structure
```
/smartfarm/
  sensors/
    air_temp          float   (°C)
    air_humidity      float   (% RH)
    water_temp        float   (°C) — push เฉพาะตอน water_ok
    uptime_sec        int
  status/
    online            bool
    ch1_pump          bool    ← สำรอง (ชื่อ key เก่า ไม่ตรงกับหน้าที่จริง)
    ch2_fan_out       bool    ← ไม่ได้ใช้
    ch3_fan_in        bool    ← พัดลม 220V
    ch4_spare         bool    ← ปั๊มน้ำ (ชื่อ key เก่า)
    firmware          string  "1.4.0"
  control/
    thresholds/
      temp_on         float   (35.0)  ← เปิด auto
      temp_off        float   (32.0)  ← ปิด auto
      humidity_min    float   (60.0)  ← เปิดปั๊ม
      temp_alert      float   (38.0)  ← ส่งแจ้งเตือน Telegram + Buzzer
      hum_alert       float   (40.0)  ← ส่งแจ้งเตือน Telegram + Buzzer
    ch1_pump/ ch2_fan_out/ ch3_fan_in/ ch4_spare/
      mode            string  "auto"/"manual"
      manual_state    bool
      schedule/
        enabled       bool
        on_time       string  "HH:MM"
        off_time      string  "HH:MM"
    buzzer_enabled    bool    (true)
    telegram_enabled  bool    (false — เปิดได้จาก dashboard)
  alerts/
    last_alert/
      type    string
      value   float
      message string
  action_log/
    (push-id)/
      action    string   "ปั๊มน้ำ (CH4) → เปิด ON"
      user      string   "น้องตั้ม"
      timestamp int      (Unix ms)
/logs/
  YYYY-MM-DD/
    HH/
      air_temp_avg/max/min      float
      air_humidity_avg/max/min  float
      water_temp_avg/max/min    float
      sample_count              int    (~120/ชั่วโมง ที่ interval=30s)
```

---

## 6. Telegram Bot

| | ค่า |
|---|---|
| Bot Username | @pvglab_bot |
| Bot Token | `8604786415:AAE9lve4DSVI4JiwlrqCm-TcgDBV5Fpuc0k` |
| Chat ID | `6849829706` |
| ใส่ใน config.h | `TELEGRAM_BOT_TOKEN` และ `TELEGRAM_CHAT_ID` |

> ⚠️ **TODO:** ยังใส่ค่าจริงใน config.h ไม่ได้ (ยังเป็น placeholder "ใส่_BOT_TOKEN_ที่นี่")
> ✅ Firmware รองรับแล้ว — แค่ใส่ค่าใน config.h แล้ว upload ใหม่
> Cooldown: แจ้งเตือนซ้ำต่อชนิดได้ทุก 5 นาที

> ❌ **Line Notify ยกเลิกแล้ว** — Discontinued ตั้งแต่ 31 มีนาคม 2568 (ใช้ Telegram แทน)

---

## 7. Libraries (Arduino IDE)

| Library | ใช้สำหรับ |
|---|---|
| Firebase ESP32 Client by Mobizt | Firebase Realtime DB |
| OneWire by Paul Stoffregen | DS18B20 |
| DallasTemperature by Miles Burton | DS18B20 |
| DHT sensor library by Adafruit | DHT22 |
| LiquidCrystal I2C by Frank de Brabander | LCD I2C 16x2 |

> ❌ **Adafruit SHT31 Library — ลบออกแล้ว** (ไม่ต้องติดตั้ง)

---

## 8. Firmware v1.4.0 — จุดสำคัญ

### Firebase Auth
```cpp
fbConfig.database_url = FIREBASE_HOST;  // ต้องใช้ database_url ไม่ใช่ host
fbConfig.api_key      = FIREBASE_API_KEY;
Firebase.signUp(&fbConfig, &fbAuth, "", "");
Firebase.begin(&fbConfig, &fbAuth);
if (Firebase.ready()) { pushToFirebase(); }
```

### Control Polling (เลิกใช้ Stream แล้ว)
```cpp
// ใช้ polling ด้วย getJSON() 1 call แทน stream callback (เสถียรกว่า)
// loop(): poll ทุก CONTROL_POLL_MS = 1500ms
void loadControlFromFirebase() {
  FirebaseJson json; FirebaseJsonData d;
  Firebase.getJSON(fbData, "/smartfarm/control", &json);
  // อ่าน mode/manual_state/schedule/thresholds/buzzer_enabled/telegram_enabled
}
```

### Per-Channel Control (index mapping)
```cpp
// index: 0=ch1(สำรอง) 1=ch2(ไม่ใช้) 2=ch3(พัดลม) 3=ch4(ปั๊ม)
volatile bool ch_isAuto[4] = {false, false, true, true};  // CH3/CH4 = auto
#define IDX_FAN  2   // ch3_fan_in → พัดลม 220V (อุณหภูมิ)
#define IDX_PUMP 3   // ch4_spare  → ปั๊มน้ำ (ความชื้น + pump safety)
```

### Thresholds (default values)
```
TEMP_ON       = 35.0°C   เปิดพัดลม+ปั๊ม (auto)
TEMP_OFF      = 32.0°C   ปิดพัดลม+ปั๊ม (auto)
HUMIDITY_MIN  = 60.0%    เปิดปั๊มถ้าต่ำกว่า (auto)
thresh_temp_alert = 38.0°C   → Buzzer + Telegram
thresh_hum_alert  = 40.0%    → Buzzer + Telegram
```

### Pump Safety
```cpp
#define PUMP_MAX_RUNTIME_MS  (5UL*60*1000)  // เดินต่อเนื่องได้สูงสุด 5 นาที
#define PUMP_COOLDOWN_MS     (5UL*60*1000)  // พักปั๊ม 5 นาที หลังตัด
```

### LCD I2C (3 Pages, สลับทุก 5 วิ)
```
Page 0: Air:XX.X°C   / Hum:XX.X%
Page 1: Wat:XX.X°C   / WiFi:XXdBm
Page 2: P:ON Fo:OFF  / Fi:ON Sp:OFF  (สถานะ Relay)
```

### Buzzer
```cpp
void buzzerBeep(int times, int onMs=200, int offMs=150);
// Boot: beep 1 ครั้ง
// Alert: buzzerBeep(3) เมื่ออุณหภูมิ/ความชื้นผิดปกติ (ถ้า buzzerEnabled=true)
```

### DS18B20
```cpp
ds18b20.begin();
waterTemp = ds18b20.getTempCByIndex(0);
// กรองค่า -127°C และ 85°C (error values) — อ่านซ้ำอัตโนมัติ
// External Pull-up 5.1kΩ ระหว่าง VCC กับ DAT (ที่ฝั่ง ESP32)
```

### Relay (Active-LOW)
```cpp
#define RELAY_ACTIVE_LOW true
void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);
}
```

### NTP Time (UTC+7)
```cpp
configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
```

---

## 9. WiFi

```
- floor-1-2-2.4G   (บริษัท ชั้น 1-2)   รหัส: ใน config.h
- Lab_F2-2.4G      (บริษัท Lab ชั้น 2)  รหัส: ใน config.h
```
> ⚠️ ESP32 รองรับ **2.4GHz เท่านั้น** — 5GHz จะ connect ไม่ได้
> 🔒 รหัสผ่านเก็บใน `config.h` เท่านั้น (อย่า commit ขึ้น Git)

---

## 10. Web Dashboard v1.4.0

**ไฟล์:** `dashboard/index.html`
**เปิดได้:** double-click หรือ deploy ขึ้น Firebase Hosting

**Features:**
- Real-time sensor cards (Air Temp, Humidity, Water Temp)
- Progress bar + Status badge (ปกติ / เตือน / วิกฤต)
- Alert banner อัตโนมัติเมื่อค่าผิดปกติ
- Toggle relay แต่ละ CH + AUTO/MANUAL mode
- ตั้งค่า Threshold แล้ว save ขึ้น Firebase → ESP32 รับทันที
- History Chart 24h (Chart.js) — ดึงจาก `/logs/YYYY-MM-DD/HH`
- Action Log 10 รายการล่าสุด real-time (ทุกคนเห็นพร้อมกัน)
- User chip บน header (เก็บชื่อใน localStorage)

---

## 11. Power Distribution

```
220V AC ──── S-120-12 (12V/10A) ─┬─ XL4015 Step-Down → 5V ─── Expansion Board → ESP32, Relay, Fan 5V, LCD, Buzzer
                                  └─ Boost Converter → 24V ──── ปั๊มน้ำ DC
```

**Wire Gauge:**
- AC 220V: 1.5 mm²
- DC 12V/24V (ปั๊ม): 1.5 mm²
- DC 5V (logic/sensor): 0.5–1.0 mm²

> ⚠️ **Common GND** — ทุก component ต้องต่อ GND ร่วมกัน
> ⚠️ **Expansion Board SVG pin** = Signal–VCC–GND (เรียงซ้าย→ขวา)

---

## 12. IP65 Box Layout (255 × 300 × 144 mm)

```
┌─────────────────────────┐
│  [Fan ระบายอากาศ]        │ ← ผนังด้านข้าง
│                          │
│  S-25-5 │ S-120-12      │ ← ล่าง: โซน AC + PSU
│  ────────────────────── │
│  Relay │ ESP32+Board    │ ← บน: โซน DC Logic
│  Boost │ LCD (นอกกล่อง?)│
└─────────────────────────┘
```
- Terminal Block: กลางกล่อง (แบ่งระหว่าง AC และ DC)
- ช่องเดินสาย: ด้านล่างกล่อง + ปะเก็นยาง
- LCD: ควรติดฝาด้านนอกหรือกล่องแยก (ให้ดูได้)

---

## 13. Waterproofing

| อุปกรณ์ | วิธีกันน้ำ |
|---|---|
| PCB ทั่วไป | Conformal Coating (สเปรย์เคลือบ) |
| DS18B20 5m cable | PVC Conduit (ท่อร้อยสาย) + กาวปิดปลาย |
| DHT22 | กล่องพลาสติกเล็กเจาะรู (ให้อากาศผ่านได้ แต่กันน้ำกระเซ็น) |
| ESP32/Expansion Board | อยู่ในกล่อง IP65 |

### DS18B20 สายยาว 5 เมตร
- ใช้สาย CAT5 หรือ Speaker Wire (2 เส้น)
- Pull-up 5.1kΩ **ที่ฝั่ง ESP32** (ไม่ใช่ฝั่ง sensor)
- เดินสายในท่อ PVC Conduit ตลอดความยาว
- Firmware กรองค่า -127°C และ 85°C อัตโนมัติ

---

## 14. ปัญหาที่เคยเจอและวิธีแก้

| ปัญหา | สาเหตุ | วิธีแก้ |
|---|---|---|
| `fbConfig.host` deprecated | API เปลี่ยน | ใช้ `fbConfig.database_url` |
| Firebase Auth FAILED: CONFIGURATION_NOT_FOUND | ยังไม่เปิด Anonymous Auth | Firebase Console → Authentication → Anonymous → Enable |
| DS18B20 ได้ -127°C | วางสายผิดด้าน | จับ flat side หันหาตัวเอง: ซ้าย=GND, กลาง=DAT, ขวา=VCC |
| DS18B20 ได้ 85°C | อ่านค่าก่อน conversion เสร็จ | Firmware v1.4 กรองแล้ว |
| WiFi connect ไม่ได้ | ESP32 ไม่รองรับ 5GHz | เปลี่ยน SSID เป็น 2.4GHz |
| Serial Monitor garbage | Baud rate ผิด | ตั้งเป็น 115200 |
| LCD แสดงตัวอักษรแปลกๆ ตอน boot | ESP32 bootloader output ที่ 74880 baud | ปกติ ไม่ใช่ bug |
| LCD I2C ไม่ทำงาน | VCC ต่อกับ 3.3V / wiring ผิด | ต้องใช้ไฟ 5V, ตรวจสาย SDA/SCL |
| "invalid header" boot fail | GPIO12 strapping pin ถูกดึง HIGH | ย้าย CH4 จาก GPIO12 → GPIO25 |
| Buzzer ดังตลอด | DHT22 ไม่ได้ต่อ → humidity=0 < 40% | ต่อ DHT22 หรือ check `if (airTemp==0 && airHumidity==0) return;` |
| Boost Converter พัง | จิ้มสายปั๊มขณะมีไฟ → Short circuit | ซื้อใหม่ XL6009/XL4016, ปิดไฟก่อนต่อ/ถอดสายเสมอ |
| Port ไม่ขึ้น Arduino IDE (Mac M3) | ไม่มี CH340 driver | ติดตั้ง driver จาก wch-ic.com + อนุญาตใน Privacy & Security + restart |
| พัดลม 5V ไม่หมุน | ยังไม่แน่ใจ — JUMP jumper? | ตรวจ JUMP jumper บน Expansion Board + วัด Multimeter |
| ESP32 38-pin เสียบ Expansion Board ไม่พอดี | Board รองรับ 30-pin | เปลี่ยนใช้ ESP32 30-pin |

---

## 15. สิ่งที่ต้องทำต่อ (TODO)

### 🔴 ด่วน — Hardware
- [ ] ใส่ Telegram Token + Chat ID ใน `config.h` แล้ว upload firmware ใหม่
- [ ] แก้ปัญหาพัดลม 5V ไม่หมุน (ตรวจ JUMP jumper)
- [ ] ซื้อ ปั๊มน้ำ DC 24V ไดอะแฟรม 2–3 bar 5–10 L/min (~300–700฿)
- [ ] ซื้อ Fan Shutter 10" AC 220V × 2 (~1,000–1,600฿) + ให้ช่างไฟเดินสาย

### 🟡 Firmware
- [ ] ทดสอบ Telegram Alert (หลังใส่ token ใน config.h)

### 🟢 Dashboard
- [ ] Dashboard Redesign (Prompt พร้อมแล้ว — รอส่งให้ Claude Code)

### 🚀 Deploy
```bash
npm install -g firebase-tools   # ครั้งแรกครั้งเดียว
firebase login
cd "/Users/tonklax/Documents/Greenhouse IoT Smart Farm"
firebase deploy
# → URL: greenhouse-iot-smart-farm.web.app
```

---

*อัปเดตล่าสุด: 2026-06-30 (Firmware v1.4.0)*
