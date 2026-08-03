# 🤖 ESP32 Firmware Plan
**โปรเจกต์: IoT Smart Farm | Phase 1-2 | มิถุนายน 2569**

---

## 📦 Library ที่ต้องติดตั้ง (Arduino IDE)

| Library | ใช้สำหรับ | ติดตั้งจาก |
|---------|-----------|-----------|
| `Firebase ESP32 Client` (Mobizt) | Firebase Realtime DB | Library Manager |
| `OneWire` | DS18B20 | Library Manager |
| `DallasTemperature` | DS18B20 | Library Manager |
| `Adafruit SHT4x` | SHT40 | Library Manager |
| `ArduinoJson` | JSON formatting | Library Manager |
| Built-in `WiFi.h` | WiFi connection | Built-in |

> ค้นหาใน Arduino IDE: Tools → Manage Libraries → พิมพ์ชื่อ Library

---

## 🗂️ โครงสร้าง Code (แบ่งเป็นส่วน)

```
smartfarm_firmware/
├── smartfarm_firmware.ino   ← main file (setup + loop)
├── config.h                 ← WiFi, Firebase credentials, pin mapping
├── sensors.h / sensors.cpp  ← อ่านค่า sensor ทุกตัว
├── relay.h / relay.cpp      ← ควบคุม relay + logic อัตโนมัติ
└── firebase_handler.h       ← push/pull Firebase
```

---

## 📌 config.h — ค่าที่ต้องกำหนด

```cpp
// ===== WiFi =====
#define WIFI_SSID     "ชื่อ WiFi บริษัท"
#define WIFI_PASSWORD "รหัส WiFi"

// ===== Firebase =====
#define FIREBASE_HOST "smartfarm-viking-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "DATABASE_SECRET_KEY"  // จาก Project Settings

// ===== Pin Mapping =====
#define PIN_DS18B20      4    // OneWire Data (DS18B20)
#define PIN_SOIL_MOISTURE 34  // ADC input (Capacitive Sensor)
#define PIN_RELAY_CH1    26   // ปั๊ม 24V
#define PIN_RELAY_CH2    27   // FAN OUT
#define PIN_RELAY_CH3    14   // FAN IN
#define PIN_RELAY_CH4    12   // LED Grow Light
// SHT40 ใช้ I2C: SDA=21, SCL=22 (default ESP32)

// ===== Thresholds (ค่าเริ่มต้น) =====
#define TEMP_ON       35.0   // °C — เปิดปั๊ม+พัดลม
#define TEMP_OFF      32.0   // °C — ปิดปั๊ม+พัดลม
#define HUMIDITY_MIN  60.0   // % — เปิดปั๊มถ้าความชื้นต่ำกว่านี้

// ===== Timing =====
#define SENSOR_READ_INTERVAL   30000   // ms — อ่าน sensor ทุก 30 วินาที
#define FIREBASE_PUSH_INTERVAL 30000   // ms — push Firebase ทุก 30 วินาที
#define LOG_INTERVAL        3600000    // ms — บันทึก log ทุก 1 ชั่วโมง
```

---

## 🔄 Flow การทำงาน (Loop)

```
┌─────────────────────────────────────────┐
│                 setup()                 │
│  1. Init Serial (115200 baud)           │
│  2. Connect WiFi                        │
│  3. Init Firebase                       │
│  4. Init SHT40 (I2C)                   │
│  5. Init DS18B20 (OneWire)             │
│  6. Set Relay pins OUTPUT + LOW         │
│  7. Subscribe to /control/ (listener)  │
└──────────────────┬──────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────┐
│                  loop()                 │
│                                         │
│  ทุก 30 วินาที:                          │
│  ┌─────────────────────────────────┐   │
│  │ 1. readSensors()                │   │
│  │    - SHT40 → air_temp, air_rh   │   │
│  │    - DS18B20 → water_temp       │   │
│  │    - ADC → soil_moisture        │   │
│  └──────────────┬──────────────────┘   │
│                 │                       │
│  ┌──────────────▼──────────────────┐   │
│  │ 2. autoControl()                │   │
│  │    if mode == "auto":           │   │
│  │      if T > TEMP_ON or          │   │
│  │         RH < HUMIDITY_MIN:      │   │
│  │        openPump() + openFans()  │   │
│  │      if T < TEMP_OFF:           │   │
│  │        closePump() + closeFans()│   │
│  └──────────────┬──────────────────┘   │
│                 │                       │
│  ┌──────────────▼──────────────────┐   │
│  │ 3. pushToFirebase()             │   │
│  │    - /sensors/ → live data      │   │
│  │    - /status/ → relay states    │   │
│  └──────────────┬──────────────────┘   │
│                 │                       │
│  ┌──────────────▼──────────────────┐   │
│  │ 4. checkAlerts()                │   │
│  │    if T > 38 → sendLineNotify() │   │
│  └──────────────┬──────────────────┘   │
│                 │                       │
│  ทุก 1 ชั่วโมง:                          │
│  ┌──────────────▼──────────────────┐   │
│  │ 5. pushHourlyLog()              │   │
│  │    - avg/max/min → /logs/       │   │
│  └─────────────────────────────────┘   │
│                                         │
│  Real-time (Firebase Listener):         │
│  ┌─────────────────────────────────┐   │
│  │ 6. onControlChange()            │   │
│  │    - รับคำสั่ง mode/manual       │   │
│  │    - อัปเดต threshold           │   │
│  └─────────────────────────────────┘   │
└─────────────────────────────────────────┘
```

---

## 💻 Code เบื้องต้น — smartfarm_firmware.ino

```cpp
#include <WiFi.h>
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "Adafruit_SHT4x.h"
#include "config.h"

// ── Objects ──────────────────────────────────────────
FirebaseData   fbData;
FirebaseAuth   fbAuth;
FirebaseConfig fbConfig;

OneWire        oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
Adafruit_SHT4x sht40;

// ── Global Variables ──────────────────────────────────
float airTemp     = 0, airHumidity = 0;
float waterTemp   = 0;
int   soilRaw     = 0, soilPct = 0;
bool  ch1_pump    = false, ch2_fanOut = false;
bool  ch3_fanIn   = false, ch4_led    = false;
String modeArr[4] = {"auto","auto","auto","timer"};

unsigned long lastSensorRead = 0;
unsigned long lastLogPush    = 0;

// ── setup() ──────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Relay pins
  pinMode(PIN_RELAY_CH1, OUTPUT); digitalWrite(PIN_RELAY_CH1, LOW);
  pinMode(PIN_RELAY_CH2, OUTPUT); digitalWrite(PIN_RELAY_CH2, LOW);
  pinMode(PIN_RELAY_CH3, OUTPUT); digitalWrite(PIN_RELAY_CH3, LOW);
  pinMode(PIN_RELAY_CH4, OUTPUT); digitalWrite(PIN_RELAY_CH4, LOW);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println(" Connected! IP: " + WiFi.localIP().toString());

  // Firebase
  fbConfig.host           = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase ready");

  // SHT40
  if (!sht40.begin()) Serial.println("SHT40 not found!");
  sht40.setPrecision(SHT4X_HIGH_PRECISION);

  // DS18B20
  ds18b20.begin();
}

// ── loop() ──────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensors();
    autoControl();
    pushToFirebase();
    checkAlerts();
  }

  if (now - lastLogPush >= LOG_INTERVAL) {
    lastLogPush = now;
    pushHourlyLog();
  }
}

// ── readSensors() ─────────────────────────────────────
void readSensors() {
  // SHT40 — อากาศ
  sensors_event_t h, t;
  sht40.getEvent(&h, &t);
  airTemp     = t.temperature;
  airHumidity = h.relative_humidity;

  // DS18B20 — น้ำ
  ds18b20.requestTemperatures();
  waterTemp = ds18b20.getTempCByIndex(0);

  // Capacitive Soil Moisture (ADC)
  soilRaw = analogRead(PIN_SOIL_MOISTURE);
  // แปลง ADC → % (ปรับตามการ calibrate จริง)
  soilPct = map(soilRaw, 3200, 1500, 0, 100);
  soilPct = constrain(soilPct, 0, 100);

  Serial.printf("T:%.1f°C RH:%.1f%% WT:%.1f°C Soil:%d%%\n",
    airTemp, airHumidity, waterTemp, soilPct);
}

// ── autoControl() ─────────────────────────────────────
void autoControl() {
  if (modeArr[0] == "auto") {
    bool shouldPump = (airTemp >= TEMP_ON) || (airHumidity < HUMIDITY_MIN);
    bool shouldOff  = (airTemp <= TEMP_OFF) && (airHumidity >= HUMIDITY_MIN);

    if (shouldPump && !ch1_pump) {
      ch1_pump = true;  ch2_fanOut = true;  ch3_fanIn = true;
      digitalWrite(PIN_RELAY_CH1, HIGH);
      digitalWrite(PIN_RELAY_CH2, HIGH);
      digitalWrite(PIN_RELAY_CH3, HIGH);
      Serial.println(">> AUTO: เปิดปั๊ม + พัดลม");
    }
    if (shouldOff && ch1_pump) {
      ch1_pump = false;  ch2_fanOut = false;  ch3_fanIn = false;
      digitalWrite(PIN_RELAY_CH1, LOW);
      digitalWrite(PIN_RELAY_CH2, LOW);
      digitalWrite(PIN_RELAY_CH3, LOW);
      Serial.println(">> AUTO: ปิดปั๊ม + พัดลม");
    }
  }
}

// ── pushToFirebase() ──────────────────────────────────
void pushToFirebase() {
  // Live sensor data
  Firebase.setFloat(fbData, "/smartfarm/sensors/air_temp",     airTemp);
  Firebase.setFloat(fbData, "/smartfarm/sensors/air_humidity", airHumidity);
  Firebase.setFloat(fbData, "/smartfarm/sensors/water_temp",   waterTemp);
  Firebase.setInt  (fbData, "/smartfarm/sensors/soil_moisture_raw", soilRaw);
  Firebase.setInt  (fbData, "/smartfarm/sensors/soil_moisture_pct", soilPct);
  Firebase.setInt  (fbData, "/smartfarm/sensors/timestamp",    (int)(millis()/1000));

  // Status
  Firebase.setBool(fbData, "/smartfarm/status/online",    true);
  Firebase.setBool(fbData, "/smartfarm/status/ch1_pump",  ch1_pump);
  Firebase.setBool(fbData, "/smartfarm/status/ch2_fanOut",ch2_fanOut);
  Firebase.setBool(fbData, "/smartfarm/status/ch3_fanIn", ch3_fanIn);
  Firebase.setBool(fbData, "/smartfarm/status/ch4_led",   ch4_led);
  Firebase.setInt (fbData, "/smartfarm/status/last_seen", (int)(millis()/1000));
}

// ── checkAlerts() ─────────────────────────────────────
void checkAlerts() {
  if (airTemp > 38.0) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "high_temp");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   airTemp);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message", "อุณหภูมิสูงเกิน 38°C!");
    // sendLineNotify() — เพิ่มใน Phase 2
  }
}

// ── pushHourlyLog() ───────────────────────────────────
void pushHourlyLog() {
  // TODO: คำนวณ avg/max/min แล้ว push ขึ้น /logs/YYYY-MM-DD/HH/
  // เพิ่มใน Phase 2
}
```

---

## 🔌 Wiring — ESP32 Pin Connection

| ESP32 Pin | ต่อกับ | หมายเหตุ |
|-----------|--------|---------|
| GPIO 4 | DS18B20 Data | ต้องมี Pull-up 4.7kΩ ระหว่าง Data และ 3.3V |
| GPIO 34 | Soil Moisture OUT | ADC input (ห้ามเกิน 3.3V) |
| GPIO 21 (SDA) | SHT40 SDA | I2C |
| GPIO 22 (SCL) | SHT40 SCL | I2C |
| GPIO 26 | Relay CH1 (IN1) | ปั๊ม |
| GPIO 27 | Relay CH2 (IN2) | FAN OUT |
| GPIO 14 | Relay CH3 (IN3) | FAN IN |
| GPIO 12 | Relay CH4 (IN4) | LED |
| 3.3V | SHT40 VCC | |
| 5V (VIN) | Relay VCC | Relay ต้องการ 5V |
| GND | ทุก GND | ต่อ GND ร่วม |

> ⚠️ **XL4015**: จ่าย 5V ให้ Relay และ ESP32 VIN — ตั้ง output เป็น 5.0V ก่อนต่อ

---

## 📋 ลำดับการทดสอบ (ก่อน Firebase)

- [ ] **Step 1**: ทดสอบ DS18B20 อย่างเดียวก่อน — ขึ้น Serial Monitor ได้ค่าไหม?
- [ ] **Step 2**: ทดสอบ Relay 4CH — แต่ละช่องเปิด/ปิดได้ไหม? (ฟังเสียง click)
- [ ] **Step 3**: ทดสอบ Soil Moisture — อ่านค่า ADC ขึ้น Serial ได้ไหม?
- [ ] **Step 4**: เพิ่ม WiFi Connect — เชื่อมได้ไหม? IP address ขึ้นไหม?
- [ ] **Step 5**: เพิ่ม Firebase Push — ขึ้น Database Console ได้ไหม?
- [ ] **Step 6**: ทดสอบ autoControl() — ปรับค่า threshold ต่ำๆ แล้วดูว่า Relay ทำงานไหม
- [ ] **Step 7**: เพิ่ม SHT40 (เมื่อได้รับของ)
- [ ] **Step 8**: Line Notify integration

---

## 🐛 Common Errors

| Error | สาเหตุ | วิธีแก้ |
|-------|--------|--------|
| DS18B20 ได้ -127°C | ไม่มี Pull-up resistor | ใส่ 4.7kΩ ระหว่าง Data และ 3.3V |
| Firebase connection fail | Host/Auth ผิด | ตรวจ config.h ให้ตรงกับ Console |
| Relay ไม่ทำงาน | ESP32 GPIO output voltage ต่ำกว่า 5V | ใช้ active-low relay หรือ transistor driver |
| Soil moisture ได้ค่าเกิน range | ยังไม่ calibrate | ปรับค่า map() ตาม calibration จริง |
| WiFi หลุดบ่อย | สัญญาณอ่อน | เพิ่ม `WiFi.setTxPower(WIFI_POWER_19_5dBm)` |

---

*จัดทำโดย: Nattakit Prasertsak (IT Intern) | มิถุนายน 2569*
