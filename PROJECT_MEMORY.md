# Greenhouse IoT Smart Farm — Project Memory
**บริษัทประวิทย์กรุ๊ป ปุ๋ยไวกิ้ง จำกัด**
จัดทำโดย: Tonkla (IT Intern, CS Year 2, KMUTT) | มิถุนายน–กรกฎาคม 2569

---

## 1. โครงสร้างโปรเจกต์

```
/Greenhouse IoT Smart Farm/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   ← Firmware หลัก
│   └── config.h                 ← Pin mapping, WiFi, Firebase config
├── dashboard/
│   └── index.html               ← Web Dashboard (Green Nature theme)
├── Hardware_Checklist.md
├── ESP32_Firmware_Plan.md
├── Firebase_Database_Structure.md
└── PROJECT_MEMORY.md            ← ไฟล์นี้
```

---

## 2. Hardware

| อุปกรณ์ | สถานะ | หมายเหตุ |
|---|---|---|
| ESP32 DevKit V1 (38-pin) | ✅ ใช้งาน | WiFi 2.4GHz เท่านั้น |
| DS18B20 Waterproof | ✅ ทำงาน | วัดอุณหภูมิน้ำ, ต่อผ่าน Terminal Block |
| Relay Module 4CH (5V) | ✅ ทำงาน | Active-LOW |
| DHT11 | ✅ ทำงาน (ชั่วคราว) | รอ SHT35 มาแทน |
| SHT35 | 🛒 ต้องซื้อ × 2 | แม่นยำกว่า DHT11 มาก |
| Capacitive Soil Moisture | 🛒 ต้องซื้อ | แทน Resistive (2-pin) ที่มีอยู่ |
| Fan Shutter 10" AC 220V | 🛒 ต้องซื้อ × 2 | ระบายอากาศโรงเรือน |
| Fan Module 5V | ✅ มีแล้ว | ระบายความร้อนกล่อง IP65 — ต่อตรง XL4015 |
| ปั๊มน้ำ DC 24V Mini | ⏳ รอส่ง | ทดสอบแรงดันก่อน |
| XL4015 Step-Down | ✅ มีแล้ว | 12V → 5V |
| Boost Converter 400W | ✅ มีแล้ว | 12V → 24V (ปั๊มน้ำ) |
| S-120-12 PSU (12V 10A) | ✅ มีแล้ว | PSU หลัก |

---

## 3. Pin Mapping (config.h)

```cpp
// DS18B20
#define PIN_DS18B20       4    // Data (ใช้ External Pull-up 5.1kΩ)

// Soil Moisture
#define PIN_SOIL_MOISTURE 34   // Capacitive (ADC)

// DHT11 (ชั่วคราว แทน SHT35)
#define PIN_DHT11        32

// SHT35 — I2C: SDA=21, SCL=22 (default ESP32)

// Relay (Active-LOW)
#define PIN_RELAY_CH1    26   // ปั๊มน้ำ 24V
#define PIN_RELAY_CH2    27   // พัดลม Shutter OUT (โรงเรือน)
#define PIN_RELAY_CH3    14   // พัดลม Shutter IN (โรงเรือน)
#define PIN_RELAY_CH4    25   // สำรอง (เปลี่ยนจาก GPIO12 — strapping pin)
```

> ⚠️ **GPIO12 ห้ามใช้กับ Relay** — เป็น strapping pin ทำให้ ESP32 boot fail ("invalid header")

---

## 4. Firebase

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
    water_temp        float   (°C)
    soil_moisture_raw int
    soil_moisture_pct int     (%)
    uptime_sec        int
  status/
    online            bool
    ch1_pump          bool
    ch2_fan_out       bool
    ch3_fan_in        bool
    ch4_spare         bool
    firmware          string  "1.2.0"
  control/
    thresholds/
      temp_on         float   (35.0)
      temp_off        float   (32.0)
      humidity_min    float   (60.0)
    ch1_pump/
      mode            string  "auto"/"manual"
      manual_state    bool
      schedule/
        enabled       bool
        on_time       string  "HH:MM"
        off_time      string  "HH:MM"
    ch2_fan_out/ ... (same structure)
    ch3_fan_in/  ... (same structure)
    ch4_spare/   ... (same structure)
  alerts/
    last_alert/
      type    string
      value   float
      message string
```

---

## 5. Libraries (Arduino IDE)

| Library | ใช้สำหรับ |
|---|---|
| Firebase ESP32 Client by Mobizt | Firebase Realtime DB |
| OneWire by Paul Stoffregen | DS18B20 |
| DallasTemperature by Miles Burton | DS18B20 |
| Adafruit SHT31 Library | SHT35 (ใช้ address 0x44) |
| DHT sensor library by Adafruit | DHT11 (ชั่วคราว) |

---

## 6. Firmware v1.2.0 — จุดสำคัญ

### Firebase Auth
```cpp
fbConfig.database_url = FIREBASE_HOST;  // ต้องใช้ database_url ไม่ใช่ host
fbConfig.api_key      = FIREBASE_API_KEY;
Firebase.signUp(&fbConfig, &fbAuth, "", "");
Firebase.begin(&fbConfig, &fbAuth);
if (Firebase.ready()) { pushToFirebase(); }
```

### Firebase Stream Listener (v1.1 ใหม่)
```cpp
FirebaseData fbStream;  // object แยกจาก fbData

// setup():
Firebase.setStreamCallback(fbStream, streamCallback, streamTimeoutCallback);
Firebase.beginStream(fbStream, "/smartfarm/control");

// callback รันใน RTOS task แยก — ใช้ volatile variables เท่านั้น
void streamCallback(FirebaseStream data) {
  String dp = data.dataPath();   // e.g. "/ch1_pump/manual_state"
  String type = data.dataType(); // "boolean", "string", "float", "json"
  if (type == "json") { /* initial load */ }
  else if (type == "boolean") { ch_manual[0] = data.boolData(); }
  // ...
  controlChanged = true;  // flag ให้ loop() apply ใน main task
}
```

### Per-Channel Control State (v1.1 ใหม่)
```cpp
// volatile = RTOS-safe (เขียนใน stream task, อ่านใน main task)
volatile bool  ch_isAuto[4] = {true, true, true, false}; // ch1–ch4
volatile bool  ch_manual[4] = {false, false, false, false};
volatile float thresh_temp_on  = TEMP_ON;
volatile float thresh_temp_off = TEMP_OFF;
volatile float thresh_hum_min  = HUMIDITY_MIN;
volatile bool  controlChanged  = false;

// loop(): apply relay ทันทีเมื่อ flag ถูก set
if (controlChanged) { controlChanged = false; applyManualControl(); }
```

### DS18B20
```cpp
ds18b20.begin();
waterTemp = ds18b20.getTempCByIndex(0);
// External Pull-up 5.1kΩ ระหว่าง VCC กับ DAT
```

### SHT35 / DHT11 Auto-Switch
```cpp
Wire.begin();
if (sht35.begin(0x44)) { useSHT35 = true; }
else { dht11.begin(); }
```

### NTP Time (v1.1 ใหม่)
```cpp
#include <time.h>
configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // UTC+7
struct tm t;
getLocalTime(&t);  // ใช้ใน getHourlyPath()

String getHourlyPath() {
  char path[48];
  strftime(path, sizeof(path), "/logs/%Y-%m-%d/%H", &t);
  return String(path);
}
```

### Hourly Log Accumulators (v1.1 ใหม่)
```cpp
// สะสมใน readSensors() ทุก 30 วิ
float h_sumAT, h_maxAT, h_minAT;  // air temp
float h_sumAH, h_maxAH, h_minAH;  // air humidity
float h_sumWT, h_maxWT, h_minWT;  // water temp
float h_sumSP, h_maxSP, h_minSP;  // soil pct
int   h_count;

// push ขึ้น /logs/YYYY-MM-DD/HH/ ทุก 1 ชั่วโมง
// fields: air_temp_avg/max/min, air_humidity_avg/max/min,
//         water_temp_avg/max/min, soil_pct_avg/max/min, sample_count
```

### Relay (Active-LOW)
```cpp
void setRelay(int pin, bool state) {
  digitalWrite(pin, state ? LOW : HIGH);
}
```

### Auto Control Logic (ใช้ dynamic threshold จาก Firebase)
```cpp
bool shouldOpen  = (airTemp >= thresh_temp_on)  || (airHumidity < thresh_hum_min);
bool shouldClose = (airTemp <= thresh_temp_off) && (airHumidity >= thresh_hum_min);
// แต่ละ channel ตรวจ ch_isAuto[i] ก่อน — ถ้า false = manual mode
```

---

## 7. WiFi

```
SSID:     floor-1-2-2.4G
Password: pvg4239500
```
> ⚠️ ESP32 รองรับ **2.4GHz เท่านั้น** — 5GHz จะ connect ไม่ได้

---

## 8. Web Dashboard v1.1.0

**ไฟล์:** `dashboard/index.html`
**เปิดได้:** double-click ไฟล์ หรือ deploy ขึ้น Firebase Hosting

**Features:**
- Real-time sensor cards (Air Temp, Air Humidity, Water Temp, Soil Moisture)
- Progress bar + Status badge (ปกติ / เตือน / วิกฤต)
- Alert banner อัตโนมัติเมื่อค่าผิดปกติ
- Toggle relay แต่ละ CH + AUTO/MANUAL mode (Toggle disable ใน AUTO mode)
- ตั้งค่า Threshold แล้ว save ขึ้น Firebase → ESP32 รับค่าทันที
- Toast notification
- Uptime + Firmware version display
- **History Chart (v1.1 ใหม่):** กราฟ 24 ชั่วโมงย้อนหลัง (Chart.js)
  - Air Temp avg (เส้นเขียว, แกนซ้าย °C)
  - Air Humidity avg (เส้นน้ำเงิน, แกนขวา %)
  - Water Temp avg (เส้นเขียวอมฟ้า, แกนซ้าย °C)
  - ดึงจาก `/logs/YYYY-MM-DD/HH` (2 reads ต่อครั้ง)
  - ปุ่ม Refresh + Empty state เมื่อยังไม่มี log

**Firebase Listeners ที่ dashboard ใช้:**
- `/smartfarm/sensors` — sensor real-time
- `/smartfarm/status` — relay actual state
- `/smartfarm/control` — mode + thresholds (รวมใน 1 listener)

**Action Log (v1.2 ใหม่):**
- พนักงานตั้งชื่อตัวเองผ่าน user chip บน header (เก็บใน localStorage)
- ทุก action (toggle relay / เปลี่ยน mode / บันทึก threshold) → push ขึ้น `/smartfarm/action_log`
- Dashboard แสดง 10 รายการล่าสุด real-time (ทุกคนเห็นพร้อมกัน)

**⚠️ สิ่งที่ยังขาด:**
- Line Notify แจ้งเตือน (Phase 2)
- Firebase Hosting deploy (ไฟล์พร้อมแล้ว — รันคำสั่งใน Deploy section)

---

## 9. สิ่งที่ต้องทำต่อ (TODO)

### Phase 1 — Hardware
- [ ] ซื้อ SHT35 × 2 (~300–500฿) → เปลี่ยนแทน DHT11
- [ ] ซื้อ Capacitive Soil Moisture × 1 (~50–80฿)
- [ ] ซื้อ Fan Shutter 10" × 2 (~1,000–1,600฿)
- [ ] ทดสอบปั๊มน้ำ 24V เมื่อของมาส่ง

### Phase 1 — Firmware
- [x] ✅ เพิ่ม Firebase Listener ใน ESP32 (Stream บน /smartfarm/control)
- [x] ✅ Per-channel AUTO/MANUAL mode + dynamic threshold
- [x] ✅ NTP time sync (UTC+7)
- [ ] Calibrate Capacitive Soil Moisture sensor

### Phase 2
- [x] ✅ pushHourlyLog() — บันทึก avg/max/min รายชั่วโมงขึ้น `/logs/YYYY-MM-DD/HH/`
- [x] ✅ History Chart บน Dashboard (Chart.js, 24h)
- [ ] Line Notify แจ้งเตือน

### Deploy (ไฟล์พร้อมแล้ว — รันคำสั่งด้านล่างได้เลย)
- [ ] `npm install -g firebase-tools` (ครั้งแรกครั้งเดียว)
- [ ] `firebase login`
- [ ] `cd "/Users/tonklax/Documents/Greenhouse IoT Smart Farm"`
- [ ] `firebase deploy` → ได้ URL เช่น `greenhouse-iot-smart-farm.web.app`
- [ ] แชร์ URL ให้พนักงานทุกคน

### Firebase DB Structure เพิ่มเติม (action_log)
```
/smartfarm/action_log/
  (push-id)/
    action    string   "ปั๊มน้ำ (CH1) → เปิด ON"
    user      string   "น้องตั้ม"
    timestamp int      (Unix ms)
```

---

## 10. ปัญหาที่เคยเจอและวิธีแก้

| ปัญหา | สาเหตุ | วิธีแก้ |
|---|---|---|
| `fbConfig.host` deprecated | API เปลี่ยน | ใช้ `fbConfig.database_url` |
| Firebase Auth FAILED: CONFIGURATION_NOT_FOUND | ยังไม่เปิด Anonymous Auth | Firebase Console → Authentication → Anonymous → Enable |
| Token not ready / revoked | Push ก่อน ready | เช็ค `Firebase.ready()` ก่อนทุกครั้ง |
| DS18B20 ได้ 127°C | Pull-up อ่อนเกิน / wiring ผิด | ใช้ External R 5.1kΩ ระหว่าง VCC-DAT |
| WiFi connect ไม่ได้ | ESP32 ไม่รองรับ 5GHz | เปลี่ยน SSID เป็น 2.4GHz |
| Serial Monitor garbage | Baud rate ผิด | ตั้งเป็น 115200 |
| "invalid header" boot fail | GPIO12 strapping pin ถูกดึง HIGH | เปลี่ยน CH4 จาก GPIO12 → GPIO25 |
| SHT35 ได้ NaN | ยังไม่ต่อ / `Wire.begin()` หายไป | เพิ่ม `Wire.begin()` ก่อน `sht35.begin()` |

---

## 11. Firebase Database — เพิ่มเติม v1.1

```
/logs/
  YYYY-MM-DD/
    HH/
      air_temp_avg      float
      air_temp_max      float
      air_temp_min      float
      air_humidity_avg  float
      air_humidity_max  float
      air_humidity_min  float
      water_temp_avg    float
      water_temp_max    float
      water_temp_min    float
      soil_pct_avg      int
      soil_pct_max      int
      soil_pct_min      int
      sample_count      int    (ปกติ ~120 ต่อชั่วโมง ที่ SENSOR_INTERVAL=30s)
```

> ⚠️ Dashboard History Chart จะ empty จนกว่า ESP32 จะทำงานครบ 1 ชั่วโมงแรก

---

*อัปเดตล่าสุด: มิถุนายน 2569 (v1.1.0)*
