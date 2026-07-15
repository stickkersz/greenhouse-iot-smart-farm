/*
  smartfarm_firmware.ino
  Greenhouse IoT Smart Farm — บริษัท ปุ๋ยไวกิ้ง จำกัด
  จัดทำโดย: Tonkla (IT Intern) | มิถุนายน 2569
  Version: 1.8.0
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

// ── Hourly Log Accumulators ───────────────────────────
float h_sumAT = 0, h_maxAT = -999, h_minAT = 999;
float h_sumAH = 0, h_maxAH = -999, h_minAH = 999;
float h_sumWT = 0, h_maxWT = -999, h_minWT = 999;
int   h_count   = 0;   // จำนวน sample อากาศ
int   h_countWT = 0;   // จำนวน sample น้ำที่อ่านได้ (แยกต่างหาก — DS18B20 สายยาวอาจอ่านพลาดบางครั้ง)

// ── Control-loop Averaging (แยกจาก hourly-log accumulator ด้านบนโดยสิ้นเชิง) ──
// N=3 รอบ — กัน relay สั่งเปลี่ยนจากค่าเพี้ยนชั่วครู่ครั้งเดียว
// เก็บเฉพาะอากาศ (temp+humidity) — auto-control ใช้แค่ 2 ตัวนี้ · น้ำไม่คุมรีเลย์แล้ว (alert-only)
#define CTRL_AVG_N 3
float ctrlBufAT[CTRL_AVG_N] = {0};   // buffer อุณหภูมิอากาศ (คุมพัดลม)
float ctrlBufAH[CTRL_AVG_N] = {0};   // buffer ความชื้นอากาศ (คุมปั๊ม)
int   ctrlBufATCount = 0, ctrlBufIdx = 0;   // อากาศ+ความชื้น sample พร้อมกันเสมอ ใช้ index/count ร่วม

// ── Timing ────────────────────────────────────────────
unsigned long lastSensorTime = 0;
unsigned long lastLogTime    = 0;
unsigned long lastNtpSync    = 0;

// ── Safety / Worst-case Protection (v1.3.0) ───────────
#define WDT_TIMEOUT_S        60                // watchdog: reboot ถ้า loop ค้างเกิน 60 วิ
#define PUMP_MAX_RUNTIME_MS  (10UL*60*1000)    // ปั๊มเดินต่อเนื่องได้สูงสุด 10 นาที (auto/schedule) — ค่าสุดท้าย ตัดสินใจแล้ว 2026-07-07 (เดิม 5 นาที)
#define PUMP_COOLDOWN_MS     (5UL*60*1000)     // หลังตัด พักปั๊ม 5 นาที
#define AIR_TEMP_MIN         -20.0              // ช่วงค่าอุณหภูมิที่สมเหตุผล (นอกช่วง = sensor เพี้ยน)
#define AIR_TEMP_MAX          70.0
// DS18B20: ช่วงอุณหภูมิน้ำสมเหตุผล — นอกช่วงนี้ = ค่าเสีย (-127 สายหลุด / 85.0 reset อ่านไม่ทัน / noise จากสายยาว)
#define DS_WATER_MIN        -20.0
#define DS_WATER_MAX         80.0              // น้ำในฟาร์มไม่เกินนี้ → 85.0 (sentinel) ถูกตัดออกอัตโนมัติ
#define DS_READ_RETRY        2                 // อ่าน DS18B20 ซ้ำได้กี่ครั้งถ้าค่าเสีย (สายยาว 4m รบกวน)
// เดิม DHT22 อยู่ใกล้ relay/สาย pump บน expansion board มาก — noise ตอน pump switch ทำอ่านพลาด
// SHT35 ยังไม่ยืนยันว่าเจอปัญหาเดียวกันไหม (ขึ้นกับตำแหน่งที่ติดตั้งจริง) — คงกลไกนี้ไว้เป็นเซฟตี้เน็ตก่อน
#define PUMP_SWITCH_QUIET_MS  3000             // เว้น 3 วิหลังปั๊มสวิตช์ ก่อนอ่าน sensor อากาศรอบถัดไป (รอ noise transient สงบ)
#define SHT_REINIT_EVERY      3                 // ลอง sht35.begin() re-init ทุกๆ N ครั้งที่อ่านพลาด — self-heal เบาๆ ไม่ผูกกับ emergency mode ใดๆ
// Heartbeat LED (GPIO2 — ตรงกับ LED บนบอร์ด ESP32 DevKit V1 ส่วนใหญ่) — กระพริบ = loop() ยังรันอยู่
// ถ้าเจอ "ค้าง" ให้ดู LED นี้: กระพริบต่อ = loop() ไม่ตาย (ปัญหาอยู่ที่ฟังก์ชันใดฟังก์ชันหนึ่งค้างเงียบๆ)
// หยุดกระพริบ/ดับสนิท = loop() ตายจริง หรือชิป reset วนเร็วจนไม่เห็นจังหวะ
#define PIN_STATUS_LED        2
#define HEARTBEAT_BLINK_MS    500
#define NTP_RESYNC_MS        (6UL*3600*1000)   // sync NTP ใหม่ทุก 6 ชม.
#define CONTROL_POLL_MS      1500              // poll คำสั่งควบคุมทุก 1.5 วิ (เดิม 5 วิ — relay ตอบไวขึ้น)
unsigned long pumpOnSince     = 0;             // เวลาเริ่มเดินปั๊ม (0 = หยุด)
unsigned long pumpLockUntil   = 0;             // ล็อกห้ามเปิดปั๊มจนถึงเวลานี้ (cooldown)
unsigned long lastPumpSwitchTime = 0;          // เวลาที่ปั๊ม (CH4) สวิตช์ล่าสุด (0 = ยังไม่เคยสวิตช์) — ใช้เว้น quiet window ก่อนอ่าน sensor อากาศ
int  airSensorFailCount = 0;                   // นับ sensor อากาศอ่านพลาดติดกัน — ใช้ trigger re-init เป็นระยะ + โชว์ status/sensor_ok
bool waterSensorOk  = true;                    // DS18B20 อ่านได้ไหม
// 3 latch (hysteresis คนละชุด) — autoControl() คำนวณใหม่ทุกรอบแล้วเก็บกลับที่นี่
bool airHotOn   = false;                       // latch: อากาศร้อนอยู่ (≥temp_on จนกว่าจะ ≤temp_off) — คุมพัดลม
bool airDryOn   = false;                       // latch: อากาศแห้งอยู่ (<humidity_min จนกว่าจะ ≥humidity_max) — คุมทั้ง 2 ช่อง
bool pumpHeatOn = false;                       // latch: ปั๊มไล่ร้อนอยู่ — เกณฑ์เดียวกับ airHotOn แต่ถูกล้างเมื่ออากาศอิ่มตัว (กันปั๊มกระพริบที่เส้น humidity_max)

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
void pumpSafetyCheck();
bool timeValid();
float avgCtrlAT();
float avgCtrlAH();

// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Greenhouse IoT Smart Farm v1.8.0 ===");

  // Heartbeat LED — เริ่มกระพริบตั้งแต่ต้น setup() เพื่อ debug ว่าติดค้างช่วงไหนของการบูต
  pinMode(PIN_STATUS_LED, OUTPUT);
  digitalWrite(PIN_STATUS_LED, LOW);

  // Relay: ปิดทั้งหมดก่อน (boot-safe) — active-LOW → HIGH = ปิด
  const int relayPins[] = {PIN_RELAY_CH1, PIN_RELAY_CH2, PIN_RELAY_CH3, PIN_RELAY_CH4};
  for (int p : relayPins) { pinMode(p, OUTPUT); digitalWrite(p, RELAY_ACTIVE_LOW ? HIGH : LOW); }

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
    lcd->setCursor(0, 0); lcd->print("SmartFarm v1.8.0");
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

  // Firebase Auth (Anonymous) — ลองซ้ำถ้าล้มเหลว (เน็ต/Firebase สะดุดชั่วคราวตอนบูต) แทนที่จะ
  // ปล่อยให้บอร์ดวิ่งต่อแบบไม่ auth ตลอดไป (ดูปกติทุกอย่าง แต่ไม่มีข้อมูลขึ้น dashboard เลย จนกว่าจะไฟดับ/รีสตาร์ทเอง)
  // ทำเฉพาะตอนบูต (ไม่ใช่ runtime recovery loop) — ยังไม่แตะ watchdog เพราะ esp_task_wdt_add() ยังไม่ถูกเรียก ณ จุดนี้
  fbConfig.database_url = FIREBASE_HOST;
  fbConfig.api_key      = FIREBASE_API_KEY;
  bool authOk = false;
  for (int a = 1; a <= 4 && !authOk; a++) {
    authOk = Firebase.signUp(&fbConfig, &fbAuth, "", "");
    if (authOk) {
      Serial.println("Firebase Auth OK (anonymous)");
    } else {
      Serial.printf("Firebase Auth FAILED (ครั้งที่ %d/4): %s\n", a, fbConfig.signer.signupError.message.c_str());
      if (a < 4) delay(2000);
    }
  }
  if (!authOk) {
    Serial.println("Firebase Auth ล้มเหลวติดต่อกัน 4 ครั้ง — รีสตาร์ท (กันบอร์ดวิ่งต่อแบบไม่มีข้อมูลขึ้น Firebase)");
    delay(300);
    ESP.restart();
  }
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  fbData.setBSSLBufferSize(512, 512);
  Serial.println("Firebase Ready");

  // DS18B20
  ds18b20.begin();
  Serial.printf("DS18B20 พบ %d ตัว\n", ds18b20.getDeviceCount());

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

  // ทุก 30 วินาที: อ่าน sensor + auto control + push Firebase
  static unsigned long lastPushTime = 0;
  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    readSensors();
    autoControl();
    if (Firebase.ready()) {
      pushToFirebase();
      checkAlerts();
      lastPushTime = millis();
    } else {
      Serial.println("[Firebase] Not ready — skip push");
    }
  }

  // ความปลอดภัยปั๊ม — เช็คทุก loop (ตัดถ้าเดินเกิน 10 นาทีในโหมดอัตโนมัติ)
  pumpSafetyCheck();

  // poll คำสั่งควบคุมทุก CONTROL_POLL_MS (1.5 วิ) — รอ 2 วิหลัง push กัน SSL ชน
  static unsigned long lastControlPoll = 0;
  if (now - lastControlPoll >= CONTROL_POLL_MS && Firebase.ready() && (millis() - lastPushTime >= 2000)) {
    lastControlPoll = now;
    loadControlFromFirebase();
    bool changed = applyManualControl();
    if (changed) pushStatus();   // มี relay เปลี่ยน → ยืนยันกลับ dashboard ทันที (ไม่ต้องรอรอบ 30 วิ)
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
  if (json.get(d, "thresholds/temp_alert"))        thresh_temp_alert       = d.floatValue;
  if (json.get(d, "thresholds/humidity_alert"))   thresh_hum_alert        = d.floatValue;
  if (json.get(d, "thresholds/water_temp_alert")) thresh_water_temp_alert = d.floatValue;
  if (json.get(d, "buzzer_enabled"))              buzzerEnabled           = d.boolValue;

  Serial.println(" OK");
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
    Serial.println("[SHT35] ไม่พบเซนเซอร์ตอนบูต — ข้ามการอ่าน");
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
  } else {
    waterSensorOk = false;
    Serial.printf("[DS18B20] ค่าน้ำผิดปกติ (%.1f) — ข้าม (สายยาว/รบกวน/สายหลุด)\n", wt);
  }

  // สะสมข้อมูลอากาศ สำหรับ hourly log — ข้ามถ้า sensor อ่านไม่ได้ หรือรอบนี้ข้ามไปเพราะ pump quiet window
  // (กันเอาค่าเก่าจากรอบก่อนมานับซ้ำ ทำ hourly average เพี้ยน)
  if (!inPumpQuietWindow && (airTemp > 0 || airHumidity > 0)) {
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

  float avgAT = avgCtrlAT();
  float avgAH = avgCtrlAH();
  AutoControlDecisions dec = computeAutoDecisions(
    {avgAT, avgAH, ton, toff, hmin, hmax, airHotOn, airDryOn, pumpHeatOn});
  airHotOn   = dec.hotOn;    // เก็บ latch กลับไปใช้รอบหน้า
  airDryOn   = dec.dryOn;
  pumpHeatOn = dec.pumpHeatOn;

  // เหตุผลที่พัดลมเปิด — เรียกได้เฉพาะตอน dec.fanOn (การันตีว่า ร้อน หรือ แห้ง อย่างน้อย 1)
  const char* fanWhy = airHotOn ? (airDryOn ? "ร้อน+แห้ง" : "ร้อน") : "แห้ง";
  // เหตุผลที่ปั๊มเปิด — ปั๊มไล่ร้อนผ่าน gate แล้ว หรือ แห้ง
  const char* pumpWhyOn = pumpHeatOn ? (airDryOn ? "ร้อน+แห้ง" : "ร้อน") : "แห้ง";

  // precedence: ถ้า channel เปิด Schedule อยู่ → ปล่อยให้ checkSchedule คุม (ข้าม auto)
  // CH3 พัดลม — เปิดตาม dec.fanOn (ร้อน หรือ แห้ง)
  if (ch_isAuto[IDX_FAN] && !ch_schedEnabled[IDX_FAN]) {
    if (dec.fanOn && !ch3_fanIn) {
      ch3_fanIn = true;  setRelay(PIN_RELAY_CH3, true);
      Serial.printf("[AUTO] พัดลมเปิด (%s) — อากาศ %.1f°C ความชื้น %.1f%%\n", fanWhy, avgAT, avgAH);
    }
    if (!dec.fanOn && ch3_fanIn) {
      ch3_fanIn = false; setRelay(PIN_RELAY_CH3, false);
      Serial.printf("[AUTO] พัดลมปิด — อากาศ %.1f°C ความชื้น %.1f%%\n", avgAT, avgAH);
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
      // แยกเหตุผลให้ตรง — เซนเซอร์ตายต้องไม่ถูกรายงานว่า "อากาศชื้นเกิน" (RH=0 คือแห้งสุด ไม่ใช่ชื้น)
      const char* pumpWhyOff;
      if      (avgAH == 0)            pumpWhyOff = "เซนเซอร์ความชื้นตาย";
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
bool timeValid() {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  return (t.tm_year + 1900) >= 2024;
}

// ─────────────────────────────────────────────────────
// PUMP SAFETY — ตัดปั๊มถ้าเดินต่อเนื่องเกิน 10 นาที (เฉพาะ auto/schedule)
// กันน้ำท่วม / ปั๊มไหม้แห้ง · โหมด manual = คนคุมเอง ไม่ตัดอัตโนมัติ
void pumpSafetyCheck() {
  unsigned long now = millis();
  bool automated = ch_isAuto[IDX_PUMP] || ch_schedEnabled[IDX_PUMP];

  if (ch4_spare && automated) {
    if (pumpOnSince == 0) {
      pumpOnSince = now;
    } else if (now - pumpOnSince >= PUMP_MAX_RUNTIME_MS) {
      ch4_spare = false; setRelay(PIN_RELAY_CH4, false);
      lastPumpSwitchTime = now;
      pumpOnSince   = 0;
      pumpLockUntil = now + PUMP_COOLDOWN_MS;
      Serial.println("[SAFETY] ตัดปั๊ม — เดินเกิน 10 นาที (พัก 5 นาที)");
      if (Firebase.ready()) {
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "pump_cutoff");
        Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
          "ตัดปั๊มอัตโนมัติ — ทำงานต่อเนื่องเกิน 10 นาที (พัก 5 นาที) ตรวจสอบระดับน้ำ");
      }
      if (buzzerEnabled) buzzerBeep(2);
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
  Firebase.setString(fbData, base + "status/firmware",    "1.8.0");
  // Health / worst-case status — ให้ dashboard เห็นสถานะระบบ
  Firebase.setBool (fbData, base + "status/sensor_ok",   (airSensorFailCount == 0));
  Firebase.setBool (fbData, base + "status/water_ok",    waterSensorOk);
  Firebase.setBool (fbData, base + "status/failsafe",    false);   // failsafe ถูกถอดออก (rollback 2026-07-03) — คงไว้เป็น false กัน dashboard พังจาก field หาย
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
  if (waterSensorOk && waterTemp > thresh_water_temp_alert) {
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/type",    "high_water_temp");
    Firebase.setFloat (fbData, "/smartfarm/alerts/last_alert/value",   waterTemp);
    Firebase.setString(fbData, "/smartfarm/alerts/last_alert/message",
      "อุณหภูมิน้ำสูงเกิน " + String(thresh_water_temp_alert, 0) + "°C! (" + String(waterTemp, 1) + "°C)");
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
  if (!getLocalTime(&t)) return "";
  char path[48];
  strftime(path, sizeof(path), "/logs/%Y-%m-%d/%H", &t);
  return String(path);
}
