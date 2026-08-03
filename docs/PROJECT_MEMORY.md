# Greenhouse IoT Smart Farm — Project Memory
**บริษัท ปุ๋ยไวกิ้ง จำกัด**
จัดทำโดย: Nattakit Prasertsak (IT Intern, CS Year 2, KMUTT) | มิถุนายน–กรกฎาคม 2569

---

## 1. โครงสร้างโปรเจกต์

```
/Greenhouse IoT Smart Farm/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   ← Firmware หลัก (v2.2.1)
│   └── config.h                 ← Pin mapping, WiFi, Firebase
├── dashboard/
│   ├── index.html               ← Web Dashboard (Green Nature theme)
│   └── index.v1.1.0.backup.html ← Backup ก่อน redesign
├── tests/                       ← Firebase rules unit tests (npm run test:rules)
├── docs/archive/                ← เอกสารวางแผนก่อนสร้างจริง (มิ.ย. 2569) — เก็บประวัติ ไม่ใช่ reference ปัจจุบัน ดู docs/archive/README.md
├── Firebase_Database_Structure.md
└── PROJECT_MEMORY.md            ← ไฟล์นี้ — source of truth สำหรับสถานะปัจจุบัน
```

> 📁 เอกสารวางแผนช่วงก่อนสร้างระบบจริง (`STATUS.md`, `ESP32_Firmware_Plan.md`, `Hardware_Checklist.md`,
> `Smart_Farm_Project_Plan.md`, ผังอุปกรณ์ร่าง v1/v2, เอกสารเสนอโครงการร่าง) ย้ายไป `docs/archive/`
> แล้ว (2026-07-11) เพราะมีเนื้อหาไม่ตรงกับระบบจริงอีกต่อไป (soil moisture, DHT22, GPIO mapping เดิม)
> — **ไฟล์นี้ (PROJECT_MEMORY.md) คือ source of truth เดียวสำหรับสถานะปัจจุบัน**

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
| SHT35 (I2C) | ✅ ทำงาน **(เปลี่ยนมาใช้ 2026-07-11)** | Address 0x44/0x45 (auto-detect), SDA=GPIO21, SCL=GPIO22 ร่วมกับ LCD — แทน DHT22 (ดูหัวข้อ 15) |
| DS18B20 Waterproof | ✅ ทำงาน | GPIO4 — วัดอุณหภูมิน้ำ, **ต้องมี Pull-up 4.7kΩ** (DATA-VCC) |
| LCD I2C 16x2 | ✅ ทำงาน | Address 0x27, SDA=GPIO21, SCL=GPIO22, **ต้องการไฟ 5V** — แก้แล้ว 2026-07-08 (จอเดิมเสีย เปลี่ยนจอใหม่) |
| Buzzer Module (Active) | ✅ ทำงาน | GPIO33 (GND–I/O–VCC), เสียงเตือนแจ้งเตือน |
| Fan 220V AC (พัดลม ดูดเข้า) | ✅ ต่อจริงแล้ว (2026-07-01) | CH3 — evaporative cooling, ทำงานปกติ |
| Fan Module 5V | ✅ ทำงาน | ระบายความร้อนกล่อง — แก้แล้ว 2026-07-03 (JUMP jumper) |
| ปั๊มน้ำ DC 24V | ✅ ต่อจริงแล้ว (2026-07-01) | CH4 — ทำงานปกติ |
| PWM Speed Controller CW008 | ✅ มีแล้ว | IN+/IN−/OUT+/OUT− สำหรับควบคุมความเร็วปั๊ม |

> ✅ **SHT35 เปลี่ยนกลับมาใช้แล้ว (2026-07-11)** — หลังพิสูจน์ว่า DHT22 ยังกลิตช์เวลาปั๊ม/พัดลมสวิตช์
> ต่อเนื่องแม้แก้ firmware ทุกทางแล้ว (ดู A/B test 2026-07-03) เปลี่ยนไปใช้ I2C ที่มี CRC ตรวจสอบ
> ข้อมูลในตัวแทน — **หมายเหตุสำคัญ:** หลักฐาน 2026-07-08 ชี้ว่าปัญหาเดิมเกิดจาก **ระยะห่างจากกลุ่ม
> relay** ไม่ใช่ตัวโปรโตคอล (DS18B20 ที่อยู่ไกล relay อ่านค่าปกติตลอด) — ถ้าติด SHT35 ตำแหน่งเดิมที่
> DHT22 เคยอยู่ (ใกล้กลุ่ม relay) ก็มีโอกาสเจอปัญหาเดิมได้ ควรย้ายสาย SHT35 ให้ไกลกลุ่ม relay เหมือน
> DS18B20 ด้วยถึงจะทดสอบได้ชัดว่าโปรโตคอลช่วยจริงไหม ดูหัวข้อ 15 สำหรับ TODO ทดสอบ

---

## 4. Pin Mapping (config.h)

```cpp
// ── Sensors ───────────────────────────────────────────
#define PIN_DS18B20       4    // DS18B20 Data (ต้องมี External Pull-up 5.1kΩ)
#define PIN_BUZZER       33    // Buzzer Module (I/O)
// SHT35 (อุณหภูมิ+ความชื้นอากาศ) ไม่มี PIN_ แยก — เป็น I2C ใช้บัสเดียวกับ LCD ด้านล่าง

// I2C bus ร่วม (LCD + SHT35): SDA=GPIO21, SCL=GPIO22 — LCD address=0x27, SHT35 address=0x44/0x45 (auto-detect)

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
> สรุปย่อ — **สัญญาข้อมูลเต็ม + field ครบชุดอยู่ที่ `Firebase_Database_Structure.md`** (อัปเดตให้ตรง firmware v2.2.1 แล้ว)
> block ด้านล่างไม่ครบทุก field (เช่น sensor_ok/sensor_stale/water_ok/pump_locked/time_ok/wifi_rssi + boot diagnostics v2.2.0)
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
    sensor_ok/ sensor_stale/ water_ok/ failsafe/ pump_locked/ time_ok  bool
    wifi_rssi         int
    firmware          string  "2.2.1"
    last_reset_reason string  ← [v2.2.0] boot diagnostics
    boot_count/ free_heap/ max_alloc_heap  int  ← [v2.2.0] ต้องมีใน database.rules.json ไม่งั้นโดน reject เงียบ
  control/
    thresholds/
      temp_on         float   (35.0)  ← เปิด auto
      temp_off        float   (32.0)  ← ปิด auto
      humidity_min    float   (60.0)  ← เปิดปั๊ม
      humidity_max    float   (75.0)  ← ปิดปั๊ม (hysteresis)
      temp_alert      float   (38.0)  ← ส่งแจ้งเตือน Buzzer
      humidity_alert  float   (40.0)  ← ส่งแจ้งเตือน Buzzer
      water_temp_alert float  (35.0)  ← alert อุณหภูมิน้ำ
    ch1_pump/ ch2_fan_out/ ch3_fan_in/ ch4_spare/
      mode            string  "auto"/"manual"
      manual_state    bool
      schedule/
        enabled       bool
        on_time       string  "HH:MM"
        off_time      string  "HH:MM"
    buzzer_enabled    bool    (true)
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

## 6. ระบบแจ้งเตือนภายนอก — ไม่ใช้แล้ว

ระบบไม่มีการแจ้งเตือนออกนอกเครือข่ายเลย · การเตือนหน้างานใช้ **buzzer** (local ไม่ต้องใช้เน็ต)
และ **dashboard** (banner + alert history) เท่านั้น

> ℹ️ credential ของระบบแจ้งเตือนเก่ายังอยู่ใน git history (repo นี้เป็น **private**) · ไม่มีอะไรในระบบ
> พึ่ง credential นั้นแล้ว และมันเข้าถึงโรงเรือน/Firebase/รีเลย์ไม่ได้เลย — ผู้ใช้รับทราบและตัดสินใจ
> ปล่อยไว้ (2026-07-30) · **ถ้าวันหนึ่ง repo นี้เปลี่ยนเป็น public หรือถูก fork ต้องรีบจัดการก่อน**

---

## 7. Libraries (Arduino IDE)

| Library | ใช้สำหรับ |
|---|---|
| Firebase ESP32 Client by Mobizt | Firebase Realtime DB |
| OneWire by Paul Stoffregen | DS18B20 |
| DallasTemperature by Miles Burton | DS18B20 |
| Adafruit SHT31 Library | SHT35 (รองรับ SHT30/31/35 — คำสั่ง I2C ชุดเดียวกัน) |
| LiquidCrystal I2C by Frank de Brabander | LCD I2C 16x2 |

> ✅ **Adafruit SHT31 Library — ต้องติดตั้งใหม่** (Arduino IDE → Manage Libraries → ค้นหา "SHT31" → เลือกของ Adafruit) — เปลี่ยนกลับมาใช้ 2026-07-11 หลังเคยลบไปตอนยกเลิก SHT35 รอบก่อน
> ❌ **DHT sensor library by Adafruit — ไม่ใช้แล้ว** (ถอด DHT22 ออกจากระบบแล้ว 2026-07-11)

---

## 8. Firmware — จุดสำคัญ (โค้ดล่าสุดใน repo — ⚠️ ยังไม่ยืนยันว่า flash ขึ้นบอร์ดจริงแล้ว ดูหัวข้อ 15)

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
  // อ่าน mode/manual_state/schedule/thresholds/buzzer_enabled
}
```

### Per-Channel Control (index mapping)
```cpp
// index: 0=ch1(สำรอง) 1=ch2(ไม่ใช้) 2=ch3(พัดลม) 3=ch4(ปั๊ม)
volatile bool ch_isAuto[4] = {false, false, true, true};  // CH3/CH4 = auto
#define IDX_FAN  2   // ch3_fan_in → พัดลม 220V (อุณหภูมิ)
#define IDX_PUMP 3   // ch4_spare  → ปั๊มน้ำ (ความชื้น + pump safety)
```

### Thresholds (default values — ทั้งหมดปรับได้จาก Dashboard Settings, step 0.1)
```
TEMP_ON        = 35.0°C   เปิดพัดลม CH3 (auto) — หรือ water_temp_on ก็เปิดได้
TEMP_OFF       = 32.0°C   ปิดพัดลม CH3 — ต้องอากาศ "และ" น้ำเย็นพอทั้งคู่ถึงปิด
water_temp_on  = 30.0°C   พัดลมช่วยเปิดเมื่อน้ำร้อน (evaporative cooling)
water_temp_off = 27.0°C   ยกเลิกเงื่อนไขน้ำเมื่อน้ำเย็นพอ
HUMIDITY_MIN   = 60.0%    เปิดปั๊ม CH4 ถ้าต่ำกว่า (auto)
humidity_max   = 75.0%    ปิดปั๊ม CH4 (hysteresis คู่กับ humidity_min กันปั๊มกระพริบ)
thresh_temp_alert = 38.0°C   → Buzzer
thresh_hum_alert  = 40.0%    → Buzzer
```

### Auto Control v2 — Water-assisted Fan + Pump Hysteresis + Sensor Averaging (เพิ่ม 2026-07-02)
```cpp
// พัดลม (CH3): เปิดถ้าอากาศร้อน "หรือ" น้ำร้อน (worst-case wins)
//              ปิดต้องอากาศเย็น "และ" น้ำเย็นพอ (หรือไม่มีน้ำให้เช็ค — fallback อากาศอย่างเดียว)
bool fanOpen  = (avgAT >= ton)  || (haveWater && avgWT >= wton);
bool fanClose = (avgAT <= toff) && (!haveWater || avgWT <= wtoff);
// ปั๊ม (CH4): hysteresis จริง กันกระพริบใกล้ threshold เดียว
bool pumpOpen  = (avgAH > 0 && avgAH < hmin);
bool pumpClose = (avgAH == 0 || avgAH >= hmax);
// avgAT/avgAH/avgWT = ค่าเฉลี่ย 3 รอบล่าสุด (90 วิ, CTRL_AVG_N) — กัน relay สั่งจาก glitch ครั้งเดียว
// buffer แยกต่างหากจาก hourly-log accumulator (h_sumAT ฯลฯ) โดยสิ้นเชิง
```

### Pump Safety
```cpp
#define PUMP_MAX_RUNTIME_MS  (10UL*60*1000)  // ปั๊มเดินต่อเนื่องได้สูงสุด 10 นาที (auto/schedule) — ค่าสุดท้าย ตัดสินใจแล้ว 2026-07-07
#define PUMP_COOLDOWN_MS     (5UL*60*1000)   // พักปั๊ม 5 นาที หลังตัด
```

### LCD I2C (auto-detect address, 3 Pages, สลับทุก 5 วิ) — เพิ่ม 2026-07-02
```cpp
// เดิม hardcode 0x27 ตายตัว — ถ้าโมดูลจริงเป็น 0x3F เขียนไปแล้วไม่มีใครตอบ (เงียบ ไม่ error)
// backlight ติดได้เพราะบางโมดูลจัมเปอร์ไฟตรง แต่ตัวอักษรไม่ขึ้นเลย — สาเหตุที่พบบ่อยสุด
// แก้เป็น scan หา 0x27/0x3F ตอนบูต แล้วค่อยสร้าง LiquidCrystal_I2C* lcd แบบ pointer
// updateLCD() มี guard if (!lcd) return; กัน crash ถ้าไม่เจอจอ (สายหลุด/address อื่น)
Page 0: Temp:XX.X°C  / Humidity:XX.X%
Page 1: Water:XX.X°C (หรือ "-- (err)" ถ้า DS18B20 พัง) / WiFi:XXdBm
Page 2: Pump:ON/OFF  / Fan:ON/OFF
```

### Buzzer (active-LOW — เพิ่ม 2026-07-01)
```cpp
#define BUZZER_ACTIVE_LOW true   // โมดูล 3 ขา (S/VCC/GND) ส่วนใหญ่เป็น active-LOW เหมือน relay
                                  // เดิม hardcode active-HIGH → ปลายทางทุก path จบที่ LOW = ดังค้างตลอดเวลา
void buzzerBeep(int times, int onMs=200, int offMs=150);  // ใช้ ON/OFF ตาม flag แทน hardcode
// Boot: ตั้งเงียบก่อน → ทดสอบดัง 100ms → กลับเงียบ (ไม่ใช่ดังค้างเหมือนเดิม)
// Alert: buzzerBeep(3) เมื่ออุณหภูมิ/ความชื้นผิดปกติ (ถ้า buzzerEnabled=true)
```

### DS18B20
```cpp
ds18b20.begin();
waterTemp = ds18b20.getTempCByIndex(0);
// กรองค่า -127°C และ 85°C (error values) — อ่านซ้ำอัตโนมัติ (DS_READ_RETRY=2)
// ต้องมี Pull-up 4.7-5kΩ ระหว่าง DATA-VCC (ยืนยันแล้วว่าจำเป็น — ทดสอบแล้วใช้ได้)
```

### SHT35 (I2C, เปลี่ยนจาก DHT22 แล้ว 2026-07-11 — ดูหัวข้อ 15) — self-heal เบา ไม่ผูกกับ emergency mode ใดๆ
```cpp
Adafruit_SHT31 sht35 = Adafruit_SHT31();
uint8_t shtAddr = 0;   // เจอจาก I2C scan ตอนบูต (0x44 หรือ 0x45 ตาม ADDR pin) — 0 = ไม่เจอ, ข้ามอ่านทุกรอบ
#define SHT_REINIT_EVERY 3   // อ่านพลาดครบ 3 ครั้ง → ลอง sht35.begin(shtAddr) re-init เฉยๆ (ไม่ restart ไม่สั่ง relay)
// อ่าน temp/humidity แล้วเช็ค isnan() (NaN = CRC ไม่ผ่านหรือสื่อสารพลาด) เหมือน pattern เดิมของ DHT22 ทุกจุด
// ระบบ Failsafe (auto เปิดพัดลม/ปิดปั๊มเมื่อ sensor พัง) + Recovery layer (auto ESP.restart() เมื่อ
// failsafe/Firebase ค้างนาน) ถูก "ถอดออกทั้งหมด" ใน rollback 2026-07-03 — ดูหัวข้อ 15
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

## 10. Web Dashboard v1.5.0

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
| SHT35 | กล่องพลาสติกเล็กเจาะรู (ให้อากาศผ่านได้ แต่กันน้ำกระเซ็น) |
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
| LCD I2C ไม่ขึ้นจอเลย (I2C scan 1-127 ไม่เจออุปกรณ์) | จอ LCD ตัวเดิมเสีย (hardware defect ในตัวจอเอง ไม่ใช่สาย) | เปลี่ยนจอใหม่ ต่อสายเดิมทุกเส้น → ติดปกติทันที — ยืนยันแล้ว 2026-07-08 |
| "invalid header" boot fail | GPIO12 strapping pin ถูกดึง HIGH | ย้าย CH4 จาก GPIO12 → GPIO25 |
| Buzzer ดังตลอด | Sensor อากาศไม่ได้ต่อ → humidity=0 < 40% | ต่อเซนเซอร์ให้ถูกต้อง หรือ check `if (airTemp==0 && airHumidity==0) return;` |
| Boost Converter พัง | จิ้มสายปั๊มขณะมีไฟ → Short circuit | ซื้อใหม่ XL6009/XL4016, ปิดไฟก่อนต่อ/ถอดสายเสมอ |
| Port ไม่ขึ้น Arduino IDE (Mac M3) | ไม่มี CH340 driver | ติดตั้ง driver จาก wch-ic.com + อนุญาตใน Privacy & Security + restart |
| พัดลม 5V ไม่หมุน | JUMP jumper บน Expansion Board | แก้แล้ว ✅ (ยืนยัน 2026-07-03) |
| ESP32 38-pin เสียบ Expansion Board ไม่พอดี | Board รองรับ 30-pin | เปลี่ยนใช้ ESP32 30-pin |

---

## 15. สิ่งที่ต้องทำต่อ (TODO — ตรวจสอบล่าสุด 2026-07-07)

### ✅ Stability Hardening Round (2026-07-07) — เตรียมระบบสำหรับใช้งานทุกวัน/ทุกคนในทีม

ผู้ใช้ยืนยันชัดเจนว่า**ไม่ต้องการ**เพิ่ม runtime failsafe/recovery layer กลับเข้ามาตอนนี้ (รอแก้
hardware noise ก่อน — ดูหัวข้อ 🔴 ด้านล่าง) และ**ไม่ต้องการ**ระบบ role/สิทธิ์แยกระดับใน dashboard
(ทีมงานขนาดเล็ก account ทุกตัวสิทธิ์เท่ากันพอแล้ว) ดังนั้นรอบนี้เน้นความเสถียร/ใช้งานง่ายเฉพาะจุดที่
ไม่ขัดกับสองข้อนี้:
- [x] Firmware: เพิ่ม retry การ auth กับ Firebase ตอนบูต (สูงสุด 4 ครั้ง ก่อน `ESP.restart()`) กันบอร์ด
  วิ่งต่อแบบไม่ auth ตลอดไปเงียบๆ ถ้าเน็ต/Firebase สะดุดชั่วคราวตอนเปิดเครื่อง — เป็น boot-time check
  ครั้งเดียว ไม่ใช่ runtime heuristic loop จึงไม่ขัดกับการตัดสินใจ "ไม่เพิ่ม recovery layer"
- [x] Dashboard: แปล error code จาก Firebase listener (`permission-denied`, `network-error` ฯลฯ)
  เป็นข้อความไทยที่พนักงานทั่วไปเข้าใจได้ (เดิมโชว์ error code ดิบ)
- [x] เพิ่ม `TEAM_ONBOARDING.md` — วิธีเพิ่ม/ลบ account พนักงานผ่าน Firebase Console (dashboard ไม่มี
  ปุ่มสมัครเอง โดยตั้งใจ)
- [x] เพิ่ม `DAILY_OPERATIONS_CHECKLIST.md` — เช็คลิสต์ประจำวัน/สัปดาห์ + ความหมาย error ที่เจอบ่อย
  + เมื่อไหร่ควรแจ้ง Nattakit Prasertsak ทันที เขียนให้คนไม่มีพื้นฐาน IT เข้าใจได้

### 🔴 ด่วนที่สุด — ผล A/B test ยืนยันแล้ว: ปัญหาคือ Hardware ไม่ใช่ Firmware

**สรุปสำคัญ:** ผู้ใช้ flash เทียบทั้งเวอร์ชันเก่า (มี failsafe/recovery layer เต็ม) และเวอร์ชัน rollback
(ถอด failsafe/recovery ออกหมดแล้ว — ดูล่างนี้) ได้ผลลัพธ์ความไม่เสถียร **เหมือนกันทุกประการ** — สรุปได้
ชัดเจนว่า noise จากปั๊มที่รบกวน DHT22/ระบบไม่ใช่เรื่องโค้ด (ไม่ว่า simple หรือซับซ้อนแค่ไหนก็เจอเหมือนกัน)
เป็นปัญหาไฟฟ้า/hardware ล้วนๆ **priority ตอนนี้คือแก้ hardware ไม่ใช่แก้ firmware เพิ่ม**:
- [ ] Cap (ตัวเก็บประจุ) คร่อมขั้วมอเตอร์ปั๊ม (กัน arcing จาก commutator)
- [ ] RC snubber ที่ relay CH4 (ปั๊ม)
- [ ] แยกสายไฟกำลัง (ปั๊ม/มอเตอร์) ออกจากสายสัญญาณ (sensor อากาศ/DS18B20/I2C) ให้ห่างที่สุด
  - 🔎 **หลักฐานสนับสนุน (2026-07-08):** ทดสอบเทียบสด — DS18B20 ที่จัมป์สายลง breadboard (ไกลจากกลุ่ม
    relay) อ่านค่าได้ปกติแม้ตอนเปิด/ปิดปั๊ม-พัดลม ในขณะที่ DHT22 ที่ต่อตรงบน Expansion Board (ใกล้กลุ่ม
    relay) ยังอ่านค่าไม่ได้ตอนสวิตช์ ทั้งที่ไฟเลี้ยงบอร์ดปกติดี — ยืนยันว่า **ระยะห่างจากจุดสวิตช์สำคัญกว่า
    คุณภาพจุดต่อ (breadboard vs solder)** สรุป: ควรย้ายสาย/ตัวเซนเซอร์ออกไปไกลจากกลุ่ม relay เหมือนที่
    DS18B20 ทำ ไม่ใช่แค่เปลี่ยนวิธีต่อ — **ยังใช้ได้กับ SHT35 ด้วย** ดูหัวข้อ SHT35 ด้านล่าง
  - 🔎 **ทดลองเพิ่ม (2026-07-08):** ย้ายสาย VCC ของ Relay ไปจัมป์ผ่าน rail+ บน breadboard แทนการต่อ VCC
    ตรงจาก Expansion Board — ผลเบื้องต้นทำงานปกติดีขึ้น (ยังไม่ผ่านการทดสอบระยะยาว) เข้าใจได้ว่าเป็นการ
    แยก path จ่ายไฟของ relay ออกจาก rail ร่วมที่ sensor อากาศใช้ ลด common-impedance coupling — แนวทาง
    เดียวกับ star-grounding แต่ทำฝั่งจ่ายไฟ (+) แทนฝั่ง ground ยังไม่ควรถือว่าแก้ถาวร จนกว่าจะติด
    cap+snubber ที่ต้นตอด้วย

### 🔵 SHT35 แทน DHT22 (2026-07-11) — ยังต้องทดสอบ

ผู้ใช้เปลี่ยนเซนเซอร์อากาศจาก DHT22 → SHT35 (I2C) เพราะ DHT22 ยังกลิตช์ตอนปั๊ม/พัดลมสวิตช์ต่อเนื่อง
แม้แก้ firmware ทุกทางแล้ว (ดู A/B test ด้านบน — ยืนยันแล้วว่าไม่ใช่โค้ด) ความหวังคือ I2C ที่มี CRC
ตรวจสอบข้อมูลในตัวจะทนต่อ noise ได้ดีกว่าโปรโตคอล single-wire — **แต่หลักฐาน proximity ด้านบนชี้ว่า
ระยะห่างจากกลุ่ม relay น่าจะเป็นตัวแปรจริงมากกว่าโปรโตคอล** ถ้าติด SHT35 ตำแหน่งเดิมที่ DHT22 เคยอยู่
ก็มีโอกาสเจอปัญหาเดิม

- [x] Firmware: เปลี่ยน `#include <DHT.h>` → `#include <Adafruit_SHT31.h>`, `dht22` → `sht35` object,
  I2C address auto-detect (0x44/0x45) ร่วมกับ scan ของ LCD ในรอบเดียว, เปลี่ยนชื่อตัวแปร/log prefix
  จาก DHT-specific เป็น sensor-agnostic (`dhtFailCount`→`airSensorFailCount` เป็นต้น)
- [x] config.h / config.h.example: ลบ `PIN_DHT11` (ไม่ใช้แล้ว — SHT35 ไม่มี pin แยก เป็น I2C)
- [ ] **ติดตั้ง Adafruit SHT31 Library ใหม่** ผ่าน Arduino IDE (เคยลบไปตอนยกเลิก SHT35 รอบก่อน)
- [ ] **ย้ายสาย SHT35 ให้ไกลจากกลุ่ม relay เหมือน DS18B20** — ไม่ใช่แค่เปลี่ยนชิป แต่ควรย้ายตำแหน่งด้วย
  ถึงจะทดสอบได้ชัดว่า I2C protocol ช่วยจริงไหม หรือเป็นแค่เรื่องระยะห่างเหมือนที่หลักฐานชี้ไว้
- [ ] Flash + ทดสอบเปิด/ปิดปั๊ม-พัดลมซ้ำๆ เหมือนที่เคยทำกับ DHT22 ดูว่า SHT35 กลิตช์ไหม (โดยเฉพาะถ้ายัง
  ไม่ได้ย้ายตำแหน่งไกลจาก relay ก่อน — ผลลัพธ์ตรงนี้จะบอกว่า proximity หรือ protocol คือตัวแปรจริง)

### ✅ Rollback Failsafe/Recovery layer — เสร็จแล้ว ยืนยันผลแล้ว (2026-07-03)

หลังจากต่อกัน 3 รอบ (auto control v2 → recovery layer v1 → recovery layer v2) ในเวลาไล่เลี่ยกัน
ระบบเริ่มรวน/ค้างบ่อยขึ้น ไม่ลดลง — rollback กลับไปเวอร์ชันไม่มี failsafe/recovery เลย แล้ว flash
เทียบกับเวอร์ชันเก่า **ผลออกมาเหมือนกัน** (ดูสรุปด้านบน) — เก็บเวอร์ชันเรียบง่ายนี้ไว้เป็น baseline
ต่อไปไม่ต้องเพิ่ม recovery/failsafe กลับเข้ามาจนกว่าจะแก้ hardware noise ที่ต้นตอก่อน

- [x] Backup โค้ดเวอร์ชันเต็ม (recovery layer v2 + DHT power-cycle + push embargo + NVS + LCD
  widen) ไว้ที่ branch `experimental/recovery-layers-v2-untested` — ไม่ได้ลบทิ้ง ดึงกลับมาอ้างอิง/
  cherry-pick ได้เสมอ (แต่ไม่ควรดึงกลับมาจนกว่าจะแก้ hardware noise แล้ว เพราะพิสูจน์แล้วว่าไม่ช่วย)
- [x] ถอด `checkFailsafe()`, `failsafeActive`, recovery-layer restart (failsafe timeout restart,
  Firebase connectivity watchdog restart), DHT22 GPIO19 power-cycle, push embargo ออกจาก `main`
  ทั้งหมด — **เก็บไว้**: relay channel mapping, DS18B20 hardening, buzzer active-LOW fix,
  heartbeat LED (GPIO2), auto control v2 (hysteresis + water-temp assist)
- [x] Flash เทียบเวอร์ชันเก่า vs rollback — ผลเหมือนกัน ยืนยันเป็น hardware issue (ดูสรุปด้านบน)
- [x] ~~ย้ายสาย VCC ของ DHT22 กลับจากช่อง S (D19) ไปช่อง V~~ — **moot แล้ว** DHT22 ถูกถอดออกจากระบบ
  ทั้งหมดแล้ว (เปลี่ยนไปใช้ SHT35 แทน 2026-07-11 — ดูหัวข้อ SHT35 ด้านบน) ไม่ต้องสนใจสาย DHT22 อีก
- [x] **LCD ไม่ขึ้นจอเลย — แก้แล้ว ✅ (2026-07-08)** สาเหตุจริงคือจอ LCD ตัวเดิมเสีย ไม่ใช่ปัญหาสาย/โค้ด
  ตามที่สงสัยไว้ — เปลี่ยนจอใหม่ต่อสายเดิมทุกเส้น ติดปกติทันที ดูหัวข้อ 14

### 🟡 Hardware ที่ยังค้าง
- [x] พัดลม 5V (ระบายความร้อนกล่อง IP65) ไม่หมุน — แก้แล้ว (JUMP jumper) ยืนยัน 2026-07-03 ✅
- [x] ปั๊มน้ำ (CH4) + พัดลม 220V (CH3) ต่อจริงแล้ว ทำงานปกติ (ยืนยัน 2026-07-01) ✅
- [ ] Noise มอเตอร์ปั๊มรบกวน sensor อากาศ/ระบบ — ดูรายการ 3 ข้อในหัวข้อ 🔴 ด้านบน (ตอนนี้เป็น priority หลัก) + หัวข้อ SHT35 สำหรับแผนทดสอบล่าสุด

### 🟢 Software ที่ทำเสร็จแล้วรอบนี้ (2026-06-29 – 2026-07-03)
- [x] Relay channel remap ตามสายจริง (CH4=ปั๊ม, CH3=พัดลม, CH1=สำรอง)
- [x] DS18B20 hardening (retry, กรองค่าขยะ) + ยืนยัน pull-up 4.7-5kΩ จำเป็นจริง (ทดสอบแล้ว)
- [x] ลบระบบ Soil Moisture ทั้งหมด (ไม่ใช้แล้ว)
- [x] แก้บั๊กกราฟย้อนหลัง (ค่าเฉลี่ยน้ำเพี้ยน + timezone UTC+7 ตายตัว)
- [x] Buzzer active-LOW fix
- [x] LCD auto-detect I2C address + DHT22 self-heal
- [x] Auto Control v2 (น้ำช่วยคุมพัดลม, ปั๊ม hysteresis, sensor averaging)
- [x] Dashboard redesign (เสร็จไปหลาย session ก่อนแล้ว)
- [x] Rollback ถอด failsafe/recovery layer — ยืนยันด้วย A/B test ว่าเป็น hardware issue

### 🔵 ค้างไว้พิจารณา (ไม่เร่งด่วน)
- [ ] Login: ยังไม่มีปุ่มสมัคร account — แนะนำไม่ทำ (control write เปิดให้ทุก account ที่ login ได้
  คุมฮาร์ดแวร์จริง) ถ้าต้องการ ควรทำ invite-only ไม่ใช่ signup สาธารณะ
- [ ] Redact WiFi password จาก git history เก่า (ยังอยู่ใน commit history แม้ redact จากไฟล์ปัจจุบันแล้ว)
- [x] Firmware version bump — v1.4.0 → v1.5.0 (2026-07-11) สะท้อนงานสะสม (SHT35, rules hardening, boot-time auth retry ฯลฯ) — อัปเดตครบทุกจุด (.ino ×4, dashboard footer, PROJECT_MEMORY.md ×3)

### 🚀 Deploy
```bash
npm install -g firebase-tools   # ครั้งแรกครั้งเดียว
firebase login
cd "/Users/tonklax/Documents/Greenhouse IoT Smart Farm"
firebase deploy
# → URL: greenhouse-iot-smart-farm.web.app
```

---

*อัปเดตล่าสุด: 2026-07-11 — เปลี่ยนเซนเซอร์อากาศจาก DHT22 → SHT35 (I2C) หลัง DHT22 พิสูจน์แล้วว่ายังกลิตช์
ต่อเนื่องแม้แก้ firmware ทุกทาง — ยังไม่ได้ทดสอบว่า SHT35 ช่วยจริงไหม (ควรย้ายตำแหน่งให้ไกล relay ด้วย
ไม่ใช่แค่เปลี่ยนชิป — ดูหัวข้อ SHT35)*
