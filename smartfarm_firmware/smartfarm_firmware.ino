/*
  smartfarm_firmware.ino
  Greenhouse IoT Smart Farm — บริษัท ปุ๋ยไวกิ้ง จำกัด
  จัดทำโดย: Tonkla (IT Intern) | มิถุนายน 2569
  Version: 1.4.0
  Changelog v1.4.0:
    - Relay remap: CH4=ปั๊มน้ำ, CH3=พัดลม 220V, CH1=สำรอง (CH2 ไม่ใช้)
    - DS18B20 hardening: กรองค่า -127/85°C + อ่านซ้ำ (รองรับสายยาว 4m)
    - Telegram Alert: ESP32 ยิง Bot API ตรง + cooldown 5 นาที/ชนิด

  Hardware:
    - ESP32 DevKit V1
    - DHT22 (GPIO32) — อุณหภูมิ + ความชื้นอากาศ
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
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"

// เผื่อ config.h เก่าไม่มี define นี้ — relay เป็น active-LOW (LOW=เปิด, HIGH=ปิด)
#ifndef RELAY_ACTIVE_LOW
#define RELAY_ACTIVE_LOW true
#endif

// เผื่อ config.h เก่าไม่มี define นี้ — buzzer module 3 ขา (S/VCC/GND) มักเป็น active-LOW เหมือน relay
// (LOW=ดัง, HIGH=เงียบ) ถ้าใช้ buzzer แบบอื่นแล้วเงียบตลอด/ดังกลับด้าน ให้เปลี่ยนเป็น false ใน config.h
#ifndef BUZZER_ACTIVE_LOW
#define BUZZER_ACTIVE_LOW true
#endif

// เผื่อ config.h เก่าไม่มี Telegram — ปล่อยว่าง = ฟีเจอร์ปิด (ไม่ส่ง)
#ifndef TELEGRAM_BOT_TOKEN
#define TELEGRAM_BOT_TOKEN ""
#endif
#ifndef TELEGRAM_CHAT_ID
#define TELEGRAM_CHAT_ID ""
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
DHT dht22(PIN_DHT11, DHT22);

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
volatile float thresh_temp_on  = TEMP_ON;
volatile float thresh_temp_off = TEMP_OFF;
volatile float thresh_hum_min  = HUMIDITY_MIN;
volatile float thresh_hum_max  = 75.0;    // ปิดปั๊มเมื่อความชื้น ≥ นี้ (hysteresis คู่กับ humidity_min กันปั๊มกระพริบ)
volatile float thresh_water_temp_on  = 30.0;  // พัดลมช่วยเปิดเมื่อน้ำร้อน (evaporative pad)
volatile float thresh_water_temp_off = 27.0;  // ยกเลิกเงื่อนไขน้ำเมื่อน้ำเย็นพอ
volatile float thresh_temp_alert = 38.0;  // เกณฑ์แจ้งเตือน/buzzer (sync กับ dashboard)
volatile float thresh_hum_alert  = 40.0;
volatile bool  buzzerEnabled   = true;   // ปิด/เปิดเสียงเตือนจาก dashboard
volatile bool  telegramEnabled = false;  // ปิด/เปิดแจ้งเตือน Telegram จาก dashboard
bool telegramWasEnabled = false;         // จับ transition ปิด→เปิด (ส่งข้อความยืนยัน)

// ── Schedule State ────────────────────────────────────
// ch index: 0=ch1(unused) 1=ch2(unused) 2=ch3_fan 3=ch4_pump
volatile bool ch_schedEnabled[4] = {false, false, false, false};
char ch_schedOn[4][6]  = {"07:00","07:00","07:00","07:00"};
char ch_schedOff[4][6] = {"18:00","18:00","18:00","18:00"};

// ── Hourly Log Accumulators ───────────────────────────
float h_sumAT = 0, h_maxAT = -99, h_minAT = 99;
float h_sumAH = 0, h_maxAH = -1,  h_minAH = 101;
float h_sumWT = 0, h_maxWT = -99, h_minWT = 99;
int   h_count   = 0;   // จำนวน sample อากาศ
int   h_countWT = 0;   // จำนวน sample น้ำที่อ่านได้ (แยกต่างหาก — DS18B20 สายยาวอาจอ่านพลาดบางครั้ง)

// ── Control-loop Averaging (แยกจาก hourly-log accumulator ด้านบนโดยสิ้นเชิง) ──
// N=3 ตาม convention เดียวกับ DHT_FAIL_LIMIT/DHT_RECOVER_LIMIT — กัน relay สั่งเปลี่ยนจากค่าเพี้ยนชั่วครู่ครั้งเดียว
#define CTRL_AVG_N 3
float ctrlBufAT[CTRL_AVG_N] = {0};   // buffer อุณหภูมิอากาศ (สำหรับตัดสินใจ auto control เท่านั้น)
float ctrlBufAH[CTRL_AVG_N] = {0};   // buffer ความชื้นอากาศ
float ctrlBufWT[CTRL_AVG_N] = {0};   // buffer อุณหภูมิน้ำ (เก็บเฉพาะตอน waterSensorOk)
int   ctrlBufATCount = 0, ctrlBufIdx   = 0;   // อากาศ+ความชื้น sample พร้อมกันเสมอ ใช้ index/count ร่วม
int   ctrlBufWTCount = 0, ctrlBufWTIdx = 0;   // น้ำแยกต่างหาก เพราะอาจอ่านพลาดบางรอบ

// ── Timing ────────────────────────────────────────────
unsigned long lastSensorTime = 0;
unsigned long lastLogTime    = 0;
unsigned long lastNtpSync    = 0;

// ── Safety / Worst-case Protection (v1.3.0) ───────────
#define WDT_TIMEOUT_S        60                // watchdog: reboot ถ้า loop ค้างเกิน 60 วิ
#define PUMP_MAX_RUNTIME_MS  (10UL*60*1000)    // ปั๊มเดินต่อเนื่องได้สูงสุด 10 นาที (auto/schedule) — ชั่วคราวเพื่อทดสอบว่า pump cutoff เป็นตัวการ noise/DHT22 failsafe หรือไม่ (เดิม 5 นาที)
#define PUMP_COOLDOWN_MS     (5UL*60*1000)     // หลังตัด พักปั๊ม 5 นาที
#define DHT_FAIL_LIMIT       3                 // DHT อ่านพลาดติดกันกี่ครั้งถึงเข้า failsafe
#define DHT_RECOVER_LIMIT    3                 // อ่านดีติดกันกี่ครั้งถึงออกจาก failsafe (กัน flapping)
#define FAILSAFE_REALERT_MS  (10UL*60*1000)    // ใน failsafe ดัง buzzer เตือนซ้ำทุก 10 นาที
#define DHT_TEMP_MIN        -20.0              // ช่วงค่าอุณหภูมิที่สมเหตุผล (นอกช่วง = sensor เพี้ยน)
#define DHT_TEMP_MAX         70.0
// DS18B20: ช่วงอุณหภูมิน้ำสมเหตุผล — นอกช่วงนี้ = ค่าเสีย (-127 สายหลุด / 85.0 reset อ่านไม่ทัน / noise จากสายยาว)
#define DS_WATER_MIN        -20.0
#define DS_WATER_MAX         80.0              // น้ำในฟาร์มไม่เกินนี้ → 85.0 (sentinel) ถูกตัดออกอัตโนมัติ
#define DS_READ_RETRY        2                 // อ่าน DS18B20 ซ้ำได้กี่ครั้งถ้าค่าเสีย (สายยาว 4m รบกวน)
#define NTP_RESYNC_MS        (6UL*3600*1000)   // sync NTP ใหม่ทุก 6 ชม.
#define CONTROL_POLL_MS      1500              // poll คำสั่งควบคุมทุก 1.5 วิ (เดิม 5 วิ — relay ตอบไวขึ้น)
#define TG_COOLDOWN_MS       (5UL*60*1000)     // กันสแปม — แจ้ง Telegram ต่อชนิดได้ทุก 5 นาที
enum { TG_HIGH_TEMP=0, TG_LOW_HUM, TG_HIGH_WATER, TG_SENSOR_FAULT, TG_PUMP_CUTOFF, TG_TYPES };
unsigned long tgCooldown[TG_TYPES] = {0};      // เวลาพ้น cooldown ของแต่ละชนิด
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
bool applyManualControl();
void pushStatus();
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
void sendTelegram(const String& msg);
void notifyTelegram(int type, const String& msg);
float avgCtrlAT();
float avgCtrlAH();
float avgCtrlWT();

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Greenhouse IoT Smart Farm v1.4.0 ===");

  // Relay: ปิดทั้งหมดก่อน (boot-safe) — active-LOW → HIGH = ปิด
  const int relayPins[] = {PIN_RELAY_CH1, PIN_RELAY_CH2, PIN_RELAY_CH3, PIN_RELAY_CH4};
  for (int p : relayPins) { pinMode(p, OUTPUT); digitalWrite(p, RELAY_ACTIVE_LOW ? HIGH : LOW); }

  // Buzzer — boot-safe: ตั้งเป็น "เงียบ" ก่อน แล้วทดสอบดังสั้นๆ 1 ครั้ง แล้วกลับไปเงียบ
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW);   // เงียบ
  buzzerBeep(1, 100, 0);                                       // ทดสอบดัง 100ms แล้วกลับเงียบ
  Serial.println("Buzzer Ready");

  // I2C scanner — หา address ของ LCD จริง (โมดูลส่วนใหญ่ 0x27 บางล็อต 0x3F)
  // เดิม hardcode 0x27 ตายตัว — ถ้าโมดูลจริงเป็น 0x3F จะเขียนไปที่ address ที่ไม่มีใครตอบ
  // (I2C write ไป address ที่ไม่มีอุปกรณ์จะเงียบ ไม่ error) ทำให้ backlight ติด (จัมเปอร์ไฟตรง)
  // แต่ตัวอักษรไม่ขึ้นเลย — เป็นสาเหตุที่พบบ่อยที่สุดของอาการนี้
  Wire.begin();
  uint8_t lcdAddr = 0;
  for (byte a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] พบอุปกรณ์ที่ 0x%02X\n", a);
      if (lcdAddr == 0 && (a == 0x27 || a == 0x3F)) lcdAddr = a;   // เจอ address LCD ที่รู้จัก
    }
  }
  if (lcdAddr == 0) {
    Serial.println("[I2C] ไม่พบ LCD ที่ 0x27/0x3F — เช็คสาย SDA(21)/SCL(22)/VCC/GND หรือ contrast pot");
  } else {
    lcd = new LiquidCrystal_I2C(lcdAddr, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd->setCursor(0, 0); lcd->print("SmartFarm v1.4.0");
    lcd->setCursor(0, 1); lcd->print("Starting...");
    Serial.printf("LCD Ready (address 0x%02X)\n", lcdAddr);
  }

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

  // DHT22 (single-wire, ไม่ใช้ I2C — ไม่ต้องเรียก Wire.begin() ซ้ำ)
  dht22.begin();
  Serial.println("DHT22 Ready");

  // โหลด control state ครั้งแรก
  loadControlFromFirebase();
  applyManualControl();
  telegramWasEnabled = telegramEnabled;   // กันส่งข้อความ "เปิดแล้ว" ทุกครั้งที่ ESP32 รีบูต

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

  // poll คำสั่งควบคุมทุก CONTROL_POLL_MS (1.5 วิ) — รอ 2 วิหลัง push กัน SSL ชน
  static unsigned long lastControlPoll = 0;
  if (now - lastControlPoll >= CONTROL_POLL_MS && Firebase.ready() && (millis() - lastPushTime >= 2000)) {
    lastControlPoll = now;
    loadControlFromFirebase();
    // เพิ่งกดเปิด Telegram จาก dashboard → ส่งข้อความยืนยันให้รู้ว่าเชื่อมต่อได้
    if (telegramEnabled && !telegramWasEnabled) {
      sendTelegram("✅ <b>SmartFarm ปุ๋ยไวกิ้ง</b>\nเปิดการแจ้งเตือน Telegram แล้ว\nระบบพร้อมส่งเตือนเมื่อมีเหตุผิดปกติ");
    }
    telegramWasEnabled = telegramEnabled;
    if (!failsafeActive) {
      bool changed = applyManualControl();
      if (changed) pushStatus();   // มี relay เปลี่ยน → ยืนยันกลับ dashboard ทันที (ไม่ต้องรอรอบ 30 วิ)
    }
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
  if (json.get(d, "thresholds/humidity_max")) thresh_hum_max = d.floatValue;
  if (json.get(d, "thresholds/water_temp_on"))  thresh_water_temp_on  = d.floatValue;
  if (json.get(d, "thresholds/water_temp_off")) thresh_water_temp_off = d.floatValue;
  if (json.get(d, "thresholds/temp_alert"))     thresh_temp_alert = d.floatValue;
  if (json.get(d, "thresholds/humidity_alert")) thresh_hum_alert  = d.floatValue;
  if (json.get(d, "buzzer_enabled"))         buzzerEnabled  = d.boolValue;
  if (json.get(d, "telegram_enabled"))       telegramEnabled = d.boolValue;

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
    // DHT22 ไวต่อ noise บนไฟเลี้ยงมาก (เช่น ตอน relay ตัดโหลดมอเตอร์/พัดลม) — พอโดน noise
    // มักจะ "ค้าง" อ่านไม่ได้ตลอดจนกว่าจะรีเซ็ตไฟ ลอง re-init driver ให้เองตรงนี้
    // แทนที่จะรอ full power cycle จากคน (พลาดครบ limit พอดี = จังหวะเดียวกับเข้า failsafe)
    if (dhtFailCount == DHT_FAIL_LIMIT) {
      Serial.println("[DHT22] พลาดติดกันครบ limit — ลอง re-init sensor");
      dht22.begin();
    }
  } else {
    dhtFailCount = 0;
    if (dhtGoodCount < 1000) dhtGoodCount++;
    // เก็บเฉพาะค่าดีเข้า control-averaging buffer (กัน autoControl() ตัดสินใจจากค่าเพี้ยน)
    ctrlBufAT[ctrlBufIdx] = airTemp;
    ctrlBufAH[ctrlBufIdx] = airHumidity;
    ctrlBufIdx = (ctrlBufIdx + 1) % CTRL_AVG_N;
    if (ctrlBufATCount < CTRL_AVG_N) ctrlBufATCount++;
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
    waterSensorOk = true;
    // เก็บเข้า control-averaging buffer แยกจากอากาศ (น้ำอาจอ่านพลาดบางรอบ ไม่ sync กับ air buffer)
    ctrlBufWT[ctrlBufWTIdx] = waterTemp;
    ctrlBufWTIdx = (ctrlBufWTIdx + 1) % CTRL_AVG_N;
    if (ctrlBufWTCount < CTRL_AVG_N) ctrlBufWTCount++;
  } else {
    waterSensorOk = false;
    Serial.printf("[DS18B20] ค่าน้ำผิดปกติ (%.1f) — ข้าม (สายยาว/รบกวน/สายหลุด)\n", wt);
  }

  // สะสมข้อมูลอากาศ สำหรับ hourly log (ข้ามถ้า sensor อากาศยังอ่านไม่ได้)
  if (airTemp > 0 || airHumidity > 0) {
    h_sumAT += airTemp;     h_maxAT = max(h_maxAT, airTemp);     h_minAT = min(h_minAT, airTemp);
    h_sumAH += airHumidity; h_maxAH = max(h_maxAH, airHumidity); h_minAH = min(h_minAH, airHumidity);
    h_count++;
  }
  // น้ำ: สะสมเฉพาะตอนอ่านได้ (กันค่าขยะจากสายยาวทำ avg/max/min เพี้ยน)
  if (waterSensorOk) {
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
float avgCtrlWT() { float s=0; for (int i=0;i<ctrlBufWTCount;i++) s+=ctrlBufWT[i]; return ctrlBufWTCount ? s/ctrlBufWTCount : 0; }

// ─────────────────────────────────────────────────────
// Auto Control — ใช้ threshold แบบ dynamic จาก Firebase + ค่าเฉลี่ย N=3 รอบ (กัน relay สั่งจากค่าเพี้ยนครั้งเดียว)
void autoControl() {
  float ton   = thresh_temp_on,        toff  = thresh_temp_off;
  float wton  = thresh_water_temp_on,  wtoff = thresh_water_temp_off;
  float hmin  = thresh_hum_min,        hmax  = thresh_hum_max;

  float avgAT = avgCtrlAT();
  float avgAH = avgCtrlAH();
  bool  haveWater = (ctrlBufWTCount > 0) && waterSensorOk;
  float avgWT = haveWater ? avgCtrlWT() : 0;

  // พัดลม (CH3): ระบบระบายความร้อนแบบ evaporative — คุมด้วยอากาศ + น้ำร่วมกัน
  // เปิด: อากาศร้อน "หรือ" น้ำร้อน (worst-case wins — สัญญาณไหนบอกร้อนก็เปิด ไม่พลาดโอกาสระบาย)
  // ปิด: อากาศเย็นพอ "และ" น้ำเย็นพอ (หรือไม่มีน้ำให้เช็ค) — ต้องเย็นพร้อมกันถึงปิด
  bool fanOpen  = (avgAT >= ton)  || (haveWater && avgWT >= wton);
  bool fanClose = (avgAT <= toff) && (!haveWater || avgWT <= wtoff);

  // ปั๊มน้ำ (CH4): คุมด้วยความชื้น + hysteresis (เปิดต่ำกว่า hmin, ปิดสูงกว่า hmax) กันปั๊มกระพริบใกล้ threshold เดียว
  // ถ้า sensor ความชื้นพัง (อ่านได้ 0) → ปิดปั๊มเพื่อความปลอดภัย (กันปั๊มทำงานค้าง)
  bool pumpOpen  = (avgAH > 0 && avgAH < hmin);
  bool pumpClose = (avgAH == 0 || avgAH >= hmax);

  // หมายเหตุ precedence: ถ้า channel เปิด Schedule อยู่ → ปล่อยให้ checkSchedule คุม (ข้าม auto)
  // ทำงานเฉพาะตอนมี sample เฉลี่ยจริง (ctrlBufATCount>0) — กัน glitch ตอนบูตก่อน buffer เต็ม
  // CH3 พัดลม
  if (ch_isAuto[IDX_FAN] && !ch_schedEnabled[IDX_FAN] && ctrlBufATCount > 0) {
    if (fanOpen  && !ch3_fanIn) { ch3_fanIn = true;  setRelay(PIN_RELAY_CH3, true);  }
    if (fanClose &&  ch3_fanIn) { ch3_fanIn = false; setRelay(PIN_RELAY_CH3, false); }
  }
  // CH4 ปั๊มน้ำ — เปิดได้เฉพาะเมื่อพ้น safety lock (cooldown)
  if (ch_isAuto[IDX_PUMP] && !ch_schedEnabled[IDX_PUMP] && ctrlBufATCount > 0) {
    if (pumpOpen  && !ch4_spare && millis() >= pumpLockUntil) { ch4_spare = true;  setRelay(PIN_RELAY_CH4, true);  }
    if (pumpClose &&  ch4_spare) { ch4_spare = false; setRelay(PIN_RELAY_CH4, false); }
  }

  if (fanOpen)  Serial.printf("[AUTO] พัดลมเปิด — AvgT:%.1f≥%.1f หรือ AvgWT:%.1f≥%.1f\n", avgAT, ton, avgWT, wton);
  if (pumpOpen) Serial.printf("[AUTO] ปั๊มเปิด — AvgRH:%.1f<%.1f\n", avgAH, hmin);
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
  if (!ch_isAuto[IDX_PUMP] && !ch_schedEnabled[IDX_PUMP] && (bool)ch_manual[IDX_PUMP] != ch4_spare) {
    ch4_spare = ch_manual[IDX_PUMP];
    setRelay(PIN_RELAY_CH4, ch4_spare);
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
    ch3_fanIn  = true;  setRelay(PIN_RELAY_CH3, true);   // เปิดพัดลม (CH3)
    ch4_spare  = false; setRelay(PIN_RELAY_CH4, false);  // ปิดปั๊ม (CH4)
    pumpOnSince = 0;
    Serial.println("[FAILSAFE] เข้าโหมดฉุกเฉิน — sensor อากาศพัง → เปิดพัดลม + ปิดปั๊ม");
    if (Firebase.ready()) {
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "sensor_fault");
      Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
        "Sensor อากาศอ่านค่าไม่ได้ — เข้าโหมดฉุกเฉิน (เปิดพัดลม/ปิดปั๊ม) ตรวจสอบ DHT22");
    }
    if (buzzerEnabled) buzzerBeep(5);
    notifyTelegram(TG_SENSOR_FAULT, "🚨 <b>โหมดฉุกเฉิน (Failsafe)</b>\nSensor อากาศ (DHT22) อ่านค่าไม่ได้\n"
      "ระบบเปิดพัดลม + ปิดปั๊มอัตโนมัติ\nกรุณาตรวจสอบเซ็นเซอร์ด่วน — SmartFarm ปุ๋ยไวกิ้ง");
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
    if (!ch3_fanIn) { ch3_fanIn = true;  setRelay(PIN_RELAY_CH3, true);  }
    if ( ch4_spare) { ch4_spare = false; setRelay(PIN_RELAY_CH4, false); }
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
  bool automated = ch_isAuto[IDX_PUMP] || ch_schedEnabled[IDX_PUMP];

  if (ch4_spare && automated) {
    if (pumpOnSince == 0) {
      pumpOnSince = now;
    } else if (now - pumpOnSince >= PUMP_MAX_RUNTIME_MS) {
      ch4_spare = false; setRelay(PIN_RELAY_CH4, false);
      pumpOnSince   = 0;
      pumpLockUntil = now + PUMP_COOLDOWN_MS;
      Serial.println("[SAFETY] ตัดปั๊ม — เดินเกิน 5 นาที (พัก 5 นาที)");
      if (Firebase.ready()) {
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "pump_cutoff");
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
          "ตัดปั๊มอัตโนมัติ — ทำงานต่อเนื่องเกิน 5 นาที (พัก 5 นาที) ตรวจสอบระดับน้ำ");
      }
      if (buzzerEnabled) buzzerBeep(2);
      notifyTelegram(TG_PUMP_CUTOFF, "💧 <b>ตัดปั๊มอัตโนมัติ</b>\nปั๊มทำงานต่อเนื่องเกิน 5 นาที — พัก 5 นาที\n"
        "กรุณาตรวจสอบระดับน้ำ — SmartFarm ปุ๋ยไวกิ้ง");
    }
  } else {
    pumpOnSince = 0;   // ปั๊มหยุด หรืออยู่โหมด manual → รีเซ็ตตัวจับเวลา
  }
}

// ─────────────────────────────────────────────────────
// push เฉพาะสถานะ relay + health — เบา เรียกแยกเพื่อยืนยันผลให้ dashboard ทันที
void pushStatus() {
  const String base = "/smartfarm/";
  Firebase.setBool  (fbData, base + "status/online",      true);
  Firebase.setBool  (fbData, base + "status/ch1_pump",    ch1_pump);
  Firebase.setBool  (fbData, base + "status/ch2_fan_out", ch2_fanOut);
  Firebase.setBool  (fbData, base + "status/ch3_fan_in",  ch3_fanIn);
  Firebase.setBool  (fbData, base + "status/ch4_spare",   ch4_spare);
  Firebase.setString(fbData, base + "status/firmware",    "1.4.0");
  // Health / worst-case status — ให้ dashboard เห็นสถานะระบบ
  Firebase.setBool (fbData, base + "status/sensor_ok",   (dhtFailCount == 0));
  Firebase.setBool (fbData, base + "status/water_ok",    waterSensorOk);
  Firebase.setBool (fbData, base + "status/failsafe",    failsafeActive);
  Firebase.setBool (fbData, base + "status/pump_locked", (millis() < pumpLockUntil));
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
    Serial.println("[Firebase] Push OK");
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
// Telegram — ส่งข้อความเข้า Bot API ตรงผ่าน HTTPS (ไม่ต้องใช้ Cloud Functions → ใช้ได้บน Spark free)
void sendTelegram(const String& msg) {
  if (strlen(TELEGRAM_BOT_TOKEN) == 0 || strlen(TELEGRAM_CHAT_ID) == 0) {
    Serial.println("[TG] ยังไม่ตั้งค่า BOT_TOKEN/CHAT_ID ใน config.h — ข้าม");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) { Serial.println("[TG] ไม่มี WiFi — ข้าม"); return; }

  esp_task_wdt_reset();   // ป้อน watchdog ก่อน — TLS handshake อาจกินเวลาหลายวินาที
  WiFiClientSecure client;
  client.setInsecure();                       // ข้าม cert validation (จัดการ root CA บน ESP32 ยุ่งยาก)
  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(TELEGRAM_BOT_TOKEN) + "/sendMessage";
  if (!https.begin(client, url)) { Serial.println("[TG] begin() fail"); return; }
  https.addHeader("Content-Type", "application/json");
  https.setTimeout(8000);

  // escape เฉพาะอักขระที่ทำ JSON พัง — ภาษาไทย (UTF-8) ส่งดิบได้เลย
  String text = msg;
  text.replace("\\", "\\\\"); text.replace("\"", "\\\""); text.replace("\n", "\\n");
  String body = "{\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) +
                "\",\"text\":\"" + text + "\",\"parse_mode\":\"HTML\"}";

  int code = https.POST(body);
  if (code == 200) {
    Serial.println("[TG] ส่งสำเร็จ");
    // บันทึกเวลาส่งล่าสุด (epoch ms) ให้ dashboard แสดง — เฉพาะเมื่อนาฬิกา sync แล้ว
    if (Firebase.ready() && timeValid()) {
      Firebase.setDouble(fbData, "/smartfarm/alerts/telegram/last_sent", (double)time(nullptr) * 1000.0);
      Firebase.setBool  (fbData, "/smartfarm/alerts/telegram/enabled",   telegramEnabled);
    }
  } else {
    Serial.printf("[TG] ส่งไม่สำเร็จ — HTTP %d\n", code);
  }
  https.end();
}

// แจ้งเตือน Telegram แบบมี cooldown ต่อชนิด (กันสแปมตอนค่าแกว่งรอบ threshold)
void notifyTelegram(int type, const String& msg) {
  if (!telegramEnabled) return;
  if (type >= 0 && type < TG_TYPES) {
    if (millis() < tgCooldown[type]) return;          // ยังอยู่ใน cooldown ของชนิดนี้
    tgCooldown[type] = millis() + TG_COOLDOWN_MS;
  }
  sendTelegram(msg);
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
    notifyTelegram(TG_HIGH_TEMP, "🌡️ <b>อุณหภูมิสูงเกินกำหนด</b>\nวัดได้ " + String(airTemp, 1)
      + "°C (เกณฑ์ " + String(thresh_temp_alert, 0) + "°C)\n— SmartFarm ปุ๋ยไวกิ้ง");
    hasAlert = true;
  }
  if (airHumidity > 0 && airHumidity < thresh_hum_alert) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "low_humidity");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   airHumidity);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "ความชื้นต่ำกว่า " + String(thresh_hum_alert, 0) + "%! (" + String(airHumidity, 1) + "%)");
    Serial.println("[ALERT] Low Humidity: " + String(airHumidity, 1) + "%");
    notifyTelegram(TG_LOW_HUM, "💧 <b>ความชื้นอากาศต่ำ</b>\nวัดได้ " + String(airHumidity, 1)
      + "% (เกณฑ์ " + String(thresh_hum_alert, 0) + "%)\n— SmartFarm ปุ๋ยไวกิ้ง");
    hasAlert = true;
  }
  if (waterSensorOk && waterTemp > 35.0) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "high_water_temp");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   waterTemp);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "อุณหภูมิน้ำสูงเกิน 35°C! (" + String(waterTemp, 1) + "°C)");
    Serial.println("[ALERT] High Water Temp: " + String(waterTemp, 1) + "°C");
    notifyTelegram(TG_HIGH_WATER, "🌊 <b>อุณหภูมิน้ำสูง</b>\nวัดได้ " + String(waterTemp, 1)
      + "°C (เกณฑ์ 35°C)\n— SmartFarm ปุ๋ยไวกิ้ง");
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

  Firebase.setFloat(fbData, path + "/air_temp_avg",     avgAT);
  Firebase.setFloat(fbData, path + "/air_temp_max",     h_maxAT);
  Firebase.setFloat(fbData, path + "/air_temp_min",     h_minAT);
  Firebase.setFloat(fbData, path + "/air_humidity_avg", avgAH);
  Firebase.setFloat(fbData, path + "/air_humidity_max", h_maxAH);
  Firebase.setFloat(fbData, path + "/air_humidity_min", h_minAH);
  Firebase.setInt  (fbData, path + "/sample_count",     h_count);

  // น้ำ: เขียนเฉพาะเมื่อมี sample ที่อ่านได้ (กันค่าขยะ/ช่องว่างจากสายยาว)
  float avgWT = (h_countWT > 0) ? (h_sumWT / h_countWT) : 0;
  if (h_countWT > 0) {
    Firebase.setFloat(fbData, path + "/water_temp_avg",   avgWT);
    Firebase.setFloat(fbData, path + "/water_temp_max",   h_maxWT);
    Firebase.setFloat(fbData, path + "/water_temp_min",   h_minWT);
  }

  Serial.printf("[Log] Hourly → %s | T:%.1f°C RH:%.1f%% WT:%.1f°C (n=%d, nWT=%d)\n",
    path.c_str(), avgAT, avgAH, avgWT, h_count, h_countWT);

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

    // ปั๊ม (CH4) เคารพ safety lock — ห้ามเปิดระหว่าง cooldown
    if (i == IDX_PUMP && shouldBeOn && millis() < pumpLockUntil) continue;

    if (shouldBeOn != *states[i]) {
      *states[i] = shouldBeOn;
      setRelay(pins[i], shouldBeOn);
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
  if (!getLocalTime(&t)) return "";
  char path[48];
  strftime(path, sizeof(path), "/logs/%Y-%m-%d/%H", &t);
  return String(path);
}
