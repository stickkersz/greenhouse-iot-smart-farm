/*
  smartfarm_firmware.ino
  Greenhouse IoT Smart Farm — บริษัท ปุ๋ยไวกิ้ง จำกัด
  จัดทำโดย: Tonkla (IT Intern) | มิถุนายน 2569
  Version: 1.3.0

  Hardware:
    - ESP32 DevKit V1
    - DHT22 (GPIO32) — อุณหภูมิ + ความชื้นอากาศ
    - DS18B20 Waterproof (GPIO4)   — อุณหภูมิน้ำ
    - Capacitive Soil Moisture (GPIO34) — ความชื้นดิน
    - Relay 4CH Active-LOW:
        CH1 GPIO26 — ปั๊มน้ำ 24V
        CH2 GPIO27 — พัดลม Shutter OUT (โรงเรือน)
        CH3 GPIO14 — พัดลม Shutter IN (โรงเรือน)
        CH4 GPIO25 — สำรอง

  Libraries (Arduino IDE → Manage Libraries):
    - Firebase ESP32 Client by Mobizt
    - OneWire by Paul Stoffregen
    - DallasTemperature by Miles Burton
    - DHT sensor library by Adafruit
    - LiquidCrystal I2C by Frank de Brabander
*/

#include <WiFi.h>
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#include <FirebaseESP32.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <time.h>
#include <esp_task_wdt.h>
#include "config.h"

// ── LCD I2C (16x2, address 0x27) ─────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
uint8_t lcdPage = 0;  // หน้าปัจจุบัน (สลับทุก 5 วิ)

// ── Firebase Objects ──────────────────────────────────
FirebaseData   fbData;
FirebaseAuth   fbAuth;
FirebaseConfig fbConfig;

// ── Sensor Objects ────────────────────────────────────
OneWire oneWire(PIN_DS18B20);
DallasTemperature ds18b20(&oneWire);
DHT dht22(PIN_DHT11, DHT22);

// ── Sensor Values ─────────────────────────────────────
float airTemp     = 0.0;
float airHumidity = 0.0;
float waterTemp   = 0.0;
int   soilRaw     = 0;
int   soilPct     = 0;

// ── Relay State (actual hardware state) ───────────────
bool ch1_pump   = false;
bool ch2_fanOut = false;
bool ch3_fanIn  = false;
bool ch4_spare  = false;

// ── Control State จาก Firebase (volatile = RTOS-safe) ─
// ch index: 0=ch1_pump  1=ch2_fan_out  2=ch3_fan_in  3=ch4_spare
volatile bool  ch_isAuto[4] = {true,  true,  true,  false};
volatile bool  ch_manual[4] = {false, false, false, false};
volatile float thresh_temp_on  = TEMP_ON;
volatile float thresh_temp_off = TEMP_OFF;
volatile float thresh_hum_min  = HUMIDITY_MIN;
volatile float thresh_temp_alert = 38.0;  // เกณฑ์แจ้งเตือน/buzzer (sync กับ dashboard)
volatile float thresh_hum_alert  = 40.0;
volatile bool  buzzerEnabled   = true;   // ปิด/เปิดเสียงเตือนจาก dashboard

// ── Schedule State ────────────────────────────────────
// ch index: 0=ch1_pump  1=ch2_fan_out  2=ch3_fan_in  3=ch4_spare
volatile bool ch_schedEnabled[4] = {false, false, false, false};
char ch_schedOn[4][6]  = {"07:00","07:00","07:00","07:00"};
char ch_schedOff[4][6] = {"18:00","18:00","18:00","18:00"};

// ── Hourly Log Accumulators ───────────────────────────
float h_sumAT = 0, h_maxAT = -99, h_minAT = 99;
float h_sumAH = 0, h_maxAH = -1,  h_minAH = 101;
float h_sumWT = 0, h_maxWT = -99, h_minWT = 99;
float h_sumSP = 0, h_maxSP = -1,  h_minSP = 101;
int   h_count = 0;

// ── Timing ────────────────────────────────────────────
unsigned long lastSensorTime = 0;
unsigned long lastLogTime    = 0;
unsigned long lastNtpSync    = 0;

// ── Safety / Worst-case Protection (v1.3.0) ───────────
#define WDT_TIMEOUT_S        60                // watchdog: reboot ถ้า loop ค้างเกิน 60 วิ
#define PUMP_MAX_RUNTIME_MS  (5UL*60*1000)     // ปั๊มเดินต่อเนื่องได้สูงสุด 5 นาที (auto/schedule)
#define PUMP_COOLDOWN_MS     (5UL*60*1000)     // หลังตัด พักปั๊ม 5 นาที
#define DHT_FAIL_LIMIT       3                 // DHT อ่านพลาดติดกันกี่ครั้งถึงเข้า failsafe
#define DHT_RECOVER_LIMIT    3                 // อ่านดีติดกันกี่ครั้งถึงออกจาก failsafe (กัน flapping)
#define FAILSAFE_REALERT_MS  (10UL*60*1000)    // ใน failsafe ดัง buzzer เตือนซ้ำทุก 10 นาที
#define DHT_TEMP_MIN        -20.0              // ช่วงค่าอุณหภูมิที่สมเหตุผล (นอกช่วง = sensor เพี้ยน)
#define DHT_TEMP_MAX         70.0
#define NTP_RESYNC_MS        (6UL*3600*1000)   // sync NTP ใหม่ทุก 6 ชม.
unsigned long pumpOnSince     = 0;             // เวลาเริ่มเดินปั๊ม (0 = หยุด)
unsigned long pumpLockUntil   = 0;             // ล็อกห้ามเปิดปั๊มจนถึงเวลานี้ (cooldown)
unsigned long lastFailsafeBeep = 0;            // เวลาที่ buzzer เตือน failsafe ครั้งล่าสุด
int  dhtFailCount  = 0;                        // นับ DHT อ่านพลาดติดกัน (เข้า failsafe)
int  dhtGoodCount  = 0;                        // นับ DHT อ่านดีติดกัน (ออก failsafe)
bool failsafeActive = false;                   // โหมดฉุกเฉิน sensor อากาศพัง
bool waterSensorOk  = true;                    // DS18B20 อ่านได้ไหม

// ── Forward Declarations ──────────────────────────────
void readSensors();
void autoControl();
void applyManualControl();
void setRelay(int pin, bool state);
void pushToFirebase();
void checkAlerts();
void pushHourlyLog();
void resetAccumulators();
void loadControlFromFirebase();
void syncNTP();
String getHourlyPath();
void checkSchedule();
void updateLCD();
void buzzerBeep(int times, int onMs = 200, int offMs = 150);
void checkFailsafe();
void pumpSafetyCheck();
bool timeValid();

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Greenhouse IoT Smart Farm v1.3.0 ===");

  // Relay: ปิดทั้งหมดก่อน (active-LOW → HIGH = ปิด)
  const int relayPins[] = {PIN_RELAY_CH1, PIN_RELAY_CH2, PIN_RELAY_CH3, PIN_RELAY_CH4};
  for (int p : relayPins) { pinMode(p, OUTPUT); digitalWrite(p, HIGH); }

  // Buzzer
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_BUZZER, HIGH); delay(100); digitalWrite(PIN_BUZZER, LOW);
  Serial.println("Buzzer Ready");

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("SmartFarm v1.3.0");
  lcd.setCursor(0, 1); lcd.print("Starting...");
  Serial.println("LCD Ready");

  // WiFi (WiFiMulti — ลองทุกเครือข่ายใน config.h อัตโนมัติ)
  for (auto& n : wifiNetworks) wifiMulti.addAP(n.ssid, n.pass);
  Serial.print("Connecting WiFi");
  int retry = 0;
  while (wifiMulti.run() != WL_CONNECTED && retry < 40) {
    delay(500); Serial.print("."); retry++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK — SSID: " + WiFi.SSID() + " IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi FAILED — รีสตาร์ท");
    ESP.restart();
  }

  // NTP (UTC+7 ประเทศไทย) — ต้องการสำหรับ path ของ hourly log
  syncNTP();
  lastNtpSync = millis();

  // Firebase Auth (Anonymous)
  fbConfig.database_url = FIREBASE_HOST;
  fbConfig.api_key      = FIREBASE_API_KEY;
  if (Firebase.signUp(&fbConfig, &fbAuth, "", "")) {
    Serial.println("Firebase Auth OK (anonymous)");
  } else {
    Serial.println("Firebase Auth FAILED: " + String(fbConfig.signer.signupError.message.c_str()));
  }
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  fbData.setBSSLBufferSize(512, 512);
  Serial.println("Firebase Ready");

  // DS18B20
  ds18b20.begin();
  Serial.printf("DS18B20 พบ %d ตัว\n", ds18b20.getDeviceCount());

  // DHT22
  Wire.begin();
  dht22.begin();
  Serial.println("DHT22 Ready");

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

  // ทุก 30 วินาที: อ่าน sensor + auto control + push Firebase
  static unsigned long lastPushTime = 0;
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    readSensors();
    checkFailsafe();                        // sensor อากาศพัง → เปิดพัดลม + ปิดปั๊ม
    if (!failsafeActive) autoControl();      // auto ทำงานเฉพาะตอน sensor ปกติ
    if (Firebase.ready()) {
      pushToFirebase();
      checkAlerts();
      lastPushTime = millis();
    } else {
      Serial.println("[Firebase] Not ready — skip push");
    }
  }

  // ความปลอดภัยปั๊ม — เช็คทุก loop (ตัดถ้าเดินเกิน 5 นาทีในโหมดอัตโนมัติ)
  pumpSafetyCheck();

  // ทุก 5 วินาที: poll Firebase สำหรับ control changes — รอ 3 วิหลัง push เพื่อป้องกัน SSL ชน
  static unsigned long lastControlPoll = 0;
  if (now - lastControlPoll >= 5000 && Firebase.ready() && (millis() - lastPushTime >= 3000)) {
    lastControlPoll = now;
    loadControlFromFirebase();
    if (!failsafeActive) applyManualControl();
  }

  // ทุก 1 ชั่วโมง: push hourly log แล้ว reset accumulator
  if (now - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = now;
    if (Firebase.ready()) pushHourlyLog();
  }

  // sync NTP ใหม่ทุก 6 ชม. (กันนาฬิกาเพี้ยน → schedule ผิดเวลา)
  if (now - lastNtpSync >= NTP_RESYNC_MS) {
    lastNtpSync = now;
    if (WiFi.status() == WL_CONNECTED) syncNTP();
  }

  // ทุก 60 วินาที: ตรวจสอบตารางเวลา (schedule)
  static unsigned long lastSchedTime = 0;
  if (now - lastSchedTime >= 60000) {
    lastSchedTime = now;
    if (!failsafeActive) checkSchedule();
  }

  // ทุก 5 วินาที: สลับหน้า LCD
  static unsigned long lastLCDTime = 0;
  if (now - lastLCDTime >= 5000) {
    lastLCDTime = now;
    updateLCD();
  }
}


// ─────────────────────────────────────────────────────
// โหลด control state ด้วย 1 call (getJSON) แทน 11 calls แยกกัน — ลด SSL reconnect
void loadControlFromFirebase() {
  if (!Firebase.ready()) return;
  Serial.print("[Init] Loading control state...");

  FirebaseJson    json;
  FirebaseJsonData d;

  if (!Firebase.getJSON(fbData, "/smartfarm/control", &json)) {
    Serial.println(" FAIL: " + fbData.errorReason());
    return;
  }

  const char* chKeys[] = {"ch1_pump","ch2_fan_out","ch3_fan_in","ch4_spare"};
  for (int i = 0; i < 4; i++) {
    String b = String(chKeys[i]) + "/";
    if (json.get(d, b + "mode"))          ch_isAuto[i]      = (d.stringValue == "auto");
    if (json.get(d, b + "manual_state"))  ch_manual[i]      = d.boolValue;
    if (json.get(d, b + "schedule/enabled"))  ch_schedEnabled[i] = d.boolValue;
    if (json.get(d, b + "schedule/on_time"))  { strncpy(ch_schedOn[i],  d.stringValue.c_str(), 5); ch_schedOn[i][5]='\0'; }
    if (json.get(d, b + "schedule/off_time")) { strncpy(ch_schedOff[i], d.stringValue.c_str(), 5); ch_schedOff[i][5]='\0'; }
  }
  if (json.get(d, "thresholds/temp_on"))     thresh_temp_on  = d.floatValue;
  if (json.get(d, "thresholds/temp_off"))    thresh_temp_off = d.floatValue;
  if (json.get(d, "thresholds/humidity_min")) thresh_hum_min = d.floatValue;
  if (json.get(d, "thresholds/temp_alert"))     thresh_temp_alert = d.floatValue;
  if (json.get(d, "thresholds/humidity_alert")) thresh_hum_alert  = d.floatValue;
  if (json.get(d, "buzzer_enabled"))         buzzerEnabled  = d.boolValue;

  Serial.println(" OK");
}

// ─────────────────────────────────────────────────────
void readSensors() {
  airTemp     = dht22.readTemperature();
  airHumidity = dht22.readHumidity();
  // ถือว่าพังถ้า NaN หรือค่านอกช่วงสมเหตุผล (จับ sensor ส่งค่าขยะ ไม่ใช่แค่ NaN)
  bool dhtBad = isnan(airTemp) || isnan(airHumidity)
             || airTemp < DHT_TEMP_MIN || airTemp > DHT_TEMP_MAX
             || airHumidity < 0 || airHumidity > 100;
  if (dhtBad) {
    Serial.println("[DHT22] อ่านค่าผิดปกติ (NaN หรือ นอกช่วง)");
    airTemp = 0; airHumidity = 0;
    dhtFailCount++; dhtGoodCount = 0;
  } else {
    dhtFailCount = 0;
    if (dhtGoodCount < 1000) dhtGoodCount++;
  }

  ds18b20.requestTemperatures();
  waterTemp = ds18b20.getTempCByIndex(0);
  waterSensorOk = (waterTemp > -100 && waterTemp < 100);  // -127 = disconnected

  soilRaw = analogRead(PIN_SOIL_MOISTURE);
  soilPct = map(soilRaw, 3200, 1500, 0, 100);
  soilPct = constrain(soilPct, 0, 100);

  // สะสมข้อมูลสำหรับ hourly log (ข้ามถ้า sensor ยังอ่านไม่ได้)
  if (airTemp > 0 || airHumidity > 0) {
    h_sumAT += airTemp;     h_maxAT = max(h_maxAT, airTemp);     h_minAT = min(h_minAT, airTemp);
    h_sumAH += airHumidity; h_maxAH = max(h_maxAH, airHumidity); h_minAH = min(h_minAH, airHumidity);
    h_sumWT += waterTemp;   h_maxWT = max(h_maxWT, waterTemp);   h_minWT = min(h_minWT, waterTemp);
    h_sumSP += soilPct;     h_maxSP = max(h_maxSP, (float)soilPct); h_minSP = min(h_minSP, (float)soilPct);
    h_count++;
  }

  Serial.printf("[Sensor] AirT:%.1f°C RH:%.1f%% WaterT:%.1f°C Soil:%d%%(raw:%d)\n",
    airTemp, airHumidity, waterTemp, soilPct, soilRaw);
}

// ─────────────────────────────────────────────────────
// Auto Control — ใช้ threshold แบบ dynamic จาก Firebase
void autoControl() {
  float ton  = thresh_temp_on;
  float toff = thresh_temp_off;
  float hmin = thresh_hum_min;

  // พัดลม (CH2/CH3/CH4): คุมด้วยอุณหภูมิ + hysteresis
  bool fanOpen  = (airTemp >= ton);
  bool fanClose = (airTemp <= toff);

  // ปั๊มน้ำ (CH1): คุมด้วยความชื้นเท่านั้น
  // ถ้า sensor ความชื้นพัง (อ่านได้ 0) → ปิดปั๊มเพื่อความปลอดภัย (กันปั๊มทำงานค้าง)
  bool pumpOpen  = (airHumidity > 0 && airHumidity < hmin);
  bool pumpClose = (airHumidity == 0 || airHumidity >= hmin);

  // หมายเหตุ precedence: ถ้า channel เปิด Schedule อยู่ → ปล่อยให้ checkSchedule คุม (ข้าม auto)
  // CH1 Pump (ความชื้น) — เปิดได้เฉพาะเมื่อพ้น safety lock (cooldown)
  if (ch_isAuto[0] && !ch_schedEnabled[0]) {
    if (pumpOpen  && !ch1_pump && millis() >= pumpLockUntil) { ch1_pump = true;  setRelay(PIN_RELAY_CH1, true);  }
    if (pumpClose &&  ch1_pump) { ch1_pump = false; setRelay(PIN_RELAY_CH1, false); }
  }
  // CH2 Fan Out (อุณหภูมิ)
  if (ch_isAuto[1] && !ch_schedEnabled[1]) {
    if (fanOpen  && !ch2_fanOut) { ch2_fanOut = true;  setRelay(PIN_RELAY_CH2, true);  }
    if (fanClose &&  ch2_fanOut) { ch2_fanOut = false; setRelay(PIN_RELAY_CH2, false); }
  }
  // CH3 Fan In (อุณหภูมิ)
  if (ch_isAuto[2] && !ch_schedEnabled[2]) {
    if (fanOpen  && !ch3_fanIn) { ch3_fanIn = true;  setRelay(PIN_RELAY_CH3, true);  }
    if (fanClose &&  ch3_fanIn) { ch3_fanIn = false; setRelay(PIN_RELAY_CH3, false); }
  }
  // CH4 Spare (อุณหภูมิ — เผื่อเปิด auto, default = manual)
  if (ch_isAuto[3] && !ch_schedEnabled[3]) {
    if (fanOpen  && !ch4_spare) { ch4_spare = true;  setRelay(PIN_RELAY_CH4, true);  }
    if (fanClose &&  ch4_spare) { ch4_spare = false; setRelay(PIN_RELAY_CH4, false); }
  }

  if (fanOpen)  Serial.printf("[AUTO] พัดลมเปิด — T:%.1f≥%.1f\n", airTemp, ton);
  if (pumpOpen) Serial.printf("[AUTO] ปั๊มเปิด — RH:%.1f<%.1f\n", airHumidity, hmin);
}

// ─────────────────────────────────────────────────────
// Apply Manual Control — เรียกหลัง loadControlFromFirebase()
void applyManualControl() {
  // precedence: ถ้า channel เปิด Schedule อยู่ → checkSchedule คุม (ข้าม manual)
  if (!ch_isAuto[0] && !ch_schedEnabled[0] && (bool)ch_manual[0] != ch1_pump) {
    ch1_pump = ch_manual[0];
    setRelay(PIN_RELAY_CH1, ch1_pump);
    Serial.printf("[MANUAL] CH1 Pump → %s\n", ch1_pump ? "ON" : "OFF");
  }
  if (!ch_isAuto[1] && !ch_schedEnabled[1] && (bool)ch_manual[1] != ch2_fanOut) {
    ch2_fanOut = ch_manual[1];
    setRelay(PIN_RELAY_CH2, ch2_fanOut);
    Serial.printf("[MANUAL] CH2 Fan OUT → %s\n", ch2_fanOut ? "ON" : "OFF");
  }
  if (!ch_isAuto[2] && !ch_schedEnabled[2] && (bool)ch_manual[2] != ch3_fanIn) {
    ch3_fanIn = ch_manual[2];
    setRelay(PIN_RELAY_CH3, ch3_fanIn);
    Serial.printf("[MANUAL] CH3 Fan IN → %s\n", ch3_fanIn ? "ON" : "OFF");
  }
  if (!ch_isAuto[3] && !ch_schedEnabled[3] && (bool)ch_manual[3] != ch4_spare) {
    ch4_spare = ch_manual[3];
    setRelay(PIN_RELAY_CH4, ch4_spare);
    Serial.printf("[MANUAL] CH4 Spare → %s\n", ch4_spare ? "ON" : "OFF");
  }
}

// ─────────────────────────────────────────────────────
void setRelay(int pin, bool state) {
  // Active-LOW: state=true → LOW (relay เปิด), state=false → HIGH (relay ปิด)
  digitalWrite(pin, state ? LOW : HIGH);
}

// ─────────────────────────────────────────────────────
// นาฬิกาถูกต้องไหม (ปี >= 2024 = NTP sync แล้ว)
bool timeValid() {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  return (t.tm_year + 1900) >= 2024;
}

// ─────────────────────────────────────────────────────
// FAILSAFE — sensor อากาศพัง (อ่านพลาดติดกัน DHT_FAIL_LIMIT ครั้ง)
// worst-case: ตัดสินใจ auto ไม่ได้ → ระบายอากาศไว้ก่อน + ปิดปั๊มกันน้ำท่วม
void checkFailsafe() {
  // เข้า failsafe: อ่านพลาดติดกัน DHT_FAIL_LIMIT ครั้ง
  if (!failsafeActive && dhtFailCount >= DHT_FAIL_LIMIT) {
    failsafeActive = true;
    lastFailsafeBeep = millis();
    ch2_fanOut = true;  setRelay(PIN_RELAY_CH2, true);   // เปิดพัดลม OUT
    ch3_fanIn  = true;  setRelay(PIN_RELAY_CH3, true);   // เปิดพัดลม IN
    ch1_pump   = false; setRelay(PIN_RELAY_CH1, false);  // ปิดปั๊ม
    pumpOnSince = 0;
    Serial.println("[FAILSAFE] เข้าโหมดฉุกเฉิน — sensor อากาศพัง → เปิดพัดลม + ปิดปั๊ม");
    if (Firebase.ready()) {
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "sensor_fault");
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
        "Sensor อากาศอ่านค่าไม่ได้ — เข้าโหมดฉุกเฉิน (เปิดพัดลม/ปิดปั๊ม) ตรวจสอบ DHT22");
    }
    if (buzzerEnabled) buzzerBeep(5);
    return;
  }

  // ออกจาก failsafe: ต้องอ่านดีติดกัน DHT_RECOVER_LIMIT ครั้ง (hysteresis — กัน flapping จากสายหลวม)
  if (failsafeActive && dhtGoodCount >= DHT_RECOVER_LIMIT) {
    failsafeActive = false;
    Serial.println("[FAILSAFE] sensor กลับมาปกติ (อ่านดีติดกัน) → คืนการควบคุมอัตโนมัติ");
    return;
  }

  // ยังอยู่ใน failsafe: ย้ำสถานะปลอดภัย + ดัง buzzer เตือนซ้ำทุก 10 นาที
  if (failsafeActive) {
    if (!ch2_fanOut) { ch2_fanOut = true;  setRelay(PIN_RELAY_CH2, true);  }
    if (!ch3_fanIn)  { ch3_fanIn  = true;  setRelay(PIN_RELAY_CH3, true);  }
    if ( ch1_pump)   { ch1_pump   = false; setRelay(PIN_RELAY_CH1, false); }
    if (buzzerEnabled && millis() - lastFailsafeBeep >= FAILSAFE_REALERT_MS) {
      lastFailsafeBeep = millis();
      buzzerBeep(5);
      Serial.println("[FAILSAFE] ยังฉุกเฉินอยู่ — เตือนซ้ำ (sensor ยังไม่กลับมา)");
    }
  }
}

// ─────────────────────────────────────────────────────
// PUMP SAFETY — ตัดปั๊มถ้าเดินต่อเนื่องเกิน 5 นาที (เฉพาะ auto/schedule)
// กันน้ำท่วม / ปั๊มไหม้แห้ง · โหมด manual = คนคุมเอง ไม่ตัดอัตโนมัติ
void pumpSafetyCheck() {
  unsigned long now = millis();
  bool automated = ch_isAuto[0] || ch_schedEnabled[0];

  if (ch1_pump && automated) {
    if (pumpOnSince == 0) {
      pumpOnSince = now;
    } else if (now - pumpOnSince >= PUMP_MAX_RUNTIME_MS) {
      ch1_pump = false; setRelay(PIN_RELAY_CH1, false);
      pumpOnSince   = 0;
      pumpLockUntil = now + PUMP_COOLDOWN_MS;
      Serial.println("[SAFETY] ตัดปั๊ม — เดินเกิน 5 นาที (พัก 5 นาที)");
      if (Firebase.ready()) {
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "pump_cutoff");
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
          "ตัดปั๊มอัตโนมัติ — ทำงานต่อเนื่องเกิน 5 นาที (พัก 5 นาที) ตรวจสอบระดับน้ำ");
      }
      if (buzzerEnabled) buzzerBeep(2);
    }
  } else {
    pumpOnSince = 0;   // ปั๊มหยุด หรืออยู่โหมด manual → รีเซ็ตตัวจับเวลา
  }
}

// ─────────────────────────────────────────────────────
void pushToFirebase() {
  const String base = "/smartfarm/";

  Firebase.setFloat (fbData, base + "sensors/air_temp",          airTemp);
  Firebase.setFloat (fbData, base + "sensors/air_humidity",      airHumidity);
  Firebase.setFloat (fbData, base + "sensors/water_temp",        waterTemp);
  Firebase.setInt   (fbData, base + "sensors/soil_moisture_raw", soilRaw);
  Firebase.setInt   (fbData, base + "sensors/soil_moisture_pct", soilPct);
  Firebase.setInt   (fbData, base + "sensors/uptime_sec",        (int)(millis() / 1000));

  Firebase.setBool  (fbData, base + "status/online",      true);
  Firebase.setBool  (fbData, base + "status/ch1_pump",    ch1_pump);
  Firebase.setBool  (fbData, base + "status/ch2_fan_out", ch2_fanOut);
  Firebase.setBool  (fbData, base + "status/ch3_fan_in",  ch3_fanIn);
  Firebase.setBool  (fbData, base + "status/ch4_spare",   ch4_spare);
  Firebase.setString(fbData, base + "status/firmware",    "1.3.0");

  // Health / worst-case status — ให้ dashboard เห็นสถานะระบบ
  Firebase.setBool (fbData, base + "status/sensor_ok",   (dhtFailCount == 0));
  Firebase.setBool (fbData, base + "status/water_ok",    waterSensorOk);
  Firebase.setBool (fbData, base + "status/failsafe",    failsafeActive);
  Firebase.setBool (fbData, base + "status/pump_locked", (millis() < pumpLockUntil));
  Firebase.setBool (fbData, base + "status/time_ok",     timeValid());
  Firebase.setInt  (fbData, base + "status/wifi_rssi",   WiFi.RSSI());

  if (fbData.errorReason() != "") {
    Serial.println("[Firebase] Error: " + fbData.errorReason());
  } else {
    Serial.println("[Firebase] Push OK");
  }
}

// ─────────────────────────────────────────────────────
void buzzerBeep(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(PIN_BUZZER, HIGH); delay(onMs);
    digitalWrite(PIN_BUZZER, LOW);  if (i < times-1) delay(offMs);
  }
}

void checkAlerts() {
  // ข้าม alert ถ้า sensor ยังอ่านไม่ได้
  if (airTemp == 0 && airHumidity == 0) {
    Serial.println("[ALERT] ข้าม — sensor ยังไม่พร้อม");
    return;
  }

  bool hasAlert = false;

  if (airTemp > thresh_temp_alert) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "high_temp");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   airTemp);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "อุณหภูมิสูงเกิน " + String(thresh_temp_alert, 0) + "°C! (" + String(airTemp, 1) + "°C)");
    Serial.println("[ALERT] High Temp: " + String(airTemp, 1) + "°C");
    hasAlert = true;
  }
  if (airHumidity > 0 && airHumidity < thresh_hum_alert) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "low_humidity");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   airHumidity);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "ความชื้นต่ำกว่า " + String(thresh_hum_alert, 0) + "%! (" + String(airHumidity, 1) + "%)");
    Serial.println("[ALERT] Low Humidity: " + String(airHumidity, 1) + "%");
    hasAlert = true;
  }
  if (waterTemp > 35.0) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "high_water_temp");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   waterTemp);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "อุณหภูมิน้ำสูงเกิน 35°C! (" + String(waterTemp, 1) + "°C)");
    Serial.println("[ALERT] High Water Temp: " + String(waterTemp, 1) + "°C");
    hasAlert = true;
  }

  // Buzzer: ถ้ามี alert ให้ดังสั้น 3 ครั้ง (เว้นแต่ปิดเสียงจาก dashboard)
  if (hasAlert && buzzerEnabled) buzzerBeep(3);
  else if (hasAlert) Serial.println("[ALERT] Buzzer ปิดเสียงอยู่ (dashboard)");
}

// ─────────────────────────────────────────────────────
// Hourly Log — บันทึก avg/max/min ขึ้น /logs/YYYY-MM-DD/HH
void pushHourlyLog() {
  if (h_count == 0) {
    Serial.println("[Log] ไม่มีข้อมูลสะสม — ข้าม");
    return;
  }

  String path = getHourlyPath();
  if (path == "") {
    Serial.println("[Log] NTP ยังไม่ sync — ข้ามบันทึก hourly log");
    resetAccumulators();
    return;
  }

  float avgAT = h_sumAT / h_count;
  float avgAH = h_sumAH / h_count;
  float avgWT = h_sumWT / h_count;
  int   avgSP = (int)(h_sumSP / h_count);

  Firebase.setFloat(fbData, path + "/air_temp_avg",     avgAT);
  Firebase.setFloat(fbData, path + "/air_temp_max",     h_maxAT);
  Firebase.setFloat(fbData, path + "/air_temp_min",     h_minAT);
  Firebase.setFloat(fbData, path + "/air_humidity_avg", avgAH);
  Firebase.setFloat(fbData, path + "/air_humidity_max", h_maxAH);
  Firebase.setFloat(fbData, path + "/air_humidity_min", h_minAH);
  Firebase.setFloat(fbData, path + "/water_temp_avg",   avgWT);
  Firebase.setFloat(fbData, path + "/water_temp_max",   h_maxWT);
  Firebase.setFloat(fbData, path + "/water_temp_min",   h_minWT);
  Firebase.setInt  (fbData, path + "/soil_pct_avg",     avgSP);
  Firebase.setInt  (fbData, path + "/soil_pct_max",     (int)h_maxSP);
  Firebase.setInt  (fbData, path + "/soil_pct_min",     (int)h_minSP);
  Firebase.setInt  (fbData, path + "/sample_count",     h_count);

  Serial.printf("[Log] Hourly → %s | T:%.1f°C RH:%.1f%% WT:%.1f°C Soil:%d%% (n=%d)\n",
    path.c_str(), avgAT, avgAH, avgWT, avgSP, h_count);

  resetAccumulators();
}

void resetAccumulators() {
  h_sumAT = h_sumAH = h_sumWT = h_sumSP = 0;
  h_maxAT = h_maxAH = h_maxWT = h_maxSP = -999;
  h_minAT = h_minAH = h_minWT = h_minSP =  999;
  h_count = 0;
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
  if (!getLocalTime(&t)) return;
  if (!timeValid()) { Serial.println("[SCHED] ข้าม — นาฬิกายังไม่ sync"); return; }
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

    // ปั๊ม (CH1) เคารพ safety lock — ห้ามเปิดระหว่าง cooldown
    if (i == 0 && shouldBeOn && millis() < pumpLockUntil) continue;

    if (shouldBeOn != *states[i]) {
      *states[i] = shouldBeOn;
      setRelay(pins[i], shouldBeOn);
      Serial.printf("[SCHED] CH%d → %s (now:%s on:%s off:%s)\n",
        i+1, shouldBeOn?"ON":"OFF", nowBuf, ch_schedOn[i], ch_schedOff[i]);
    }
  }
}

// ─────────────────────────────────────────────────────
// LCD — สลับ 4 หน้า ทุก 5 วินาที
// หน้า 0: อุณหภูมิ + ความชื้นอากาศ
// หน้า 1: อุณหภูมิน้ำ + ความชื้นดิน
// หน้า 2: สถานะ Pump + Fan Out
// หน้า 3: สถานะ Fan In + Spare
void updateLCD() {
  lcd.clear();
  char buf1[17], buf2[17];

  switch (lcdPage) {
    case 0:
      snprintf(buf1, sizeof(buf1), "Temp: %.1f%cC", airTemp, 0xDF);
      snprintf(buf2, sizeof(buf2), "Humidity: %.1f%%", airHumidity);
      lcd.setCursor(0, 0); lcd.print(buf1);
      lcd.setCursor(0, 1); lcd.print(buf2);
      break;

    case 1:
      snprintf(buf1, sizeof(buf1), "Water: %.1f%cC", waterTemp, 0xDF);
      snprintf(buf2, sizeof(buf2), "Soil: %d%%", soilPct);
      lcd.setCursor(0, 0); lcd.print(buf1);
      lcd.setCursor(0, 1); lcd.print(buf2);
      break;

    case 2:
      snprintf(buf1, sizeof(buf1), "Pump: %s", ch1_pump   ? "ON" : "OFF");
      snprintf(buf2, sizeof(buf2), "Fan Out: %s", ch2_fanOut ? "ON" : "OFF");
      lcd.setCursor(0, 0); lcd.print(buf1);
      lcd.setCursor(0, 1); lcd.print(buf2);
      break;

    case 3:
      snprintf(buf1, sizeof(buf1), "Fan In: %s", ch3_fanIn ? "ON" : "OFF");
      snprintf(buf2, sizeof(buf2), "Spare: %s",  ch4_spare ? "ON" : "OFF");
      lcd.setCursor(0, 0); lcd.print(buf1);
      lcd.setCursor(0, 1); lcd.print(buf2);
      break;
  }

  lcdPage = (lcdPage + 1) % 4;  // วนหน้า 0→1→2→3→0
}

// คืนค่า path สำหรับ hourly log เช่น "/logs/2026-06-19/14"
String getHourlyPath() {
  struct tm t;
  if (!getLocalTime(&t)) return "";
  char path[48];
  strftime(path, sizeof(path), "/logs/%Y-%m-%d/%H", &t);
  return String(path);
}
