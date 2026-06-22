# Greenhouse IoT Smart Farm — สถานะโปรเจกต์
**บริษัท ปุ๋ยไวกิ้ง จำกัด**
จัดทำโดย: Tonkla (IT Intern, CS Year 2, KMUTT) | อัปเดต: มิถุนายน 2569

---

## Hardware — สถานะอุปกรณ์

| อุปกรณ์ | สถานะ | หมายเหตุ |
|---|---|---|
| ESP32 DevKit V1 | ✅ Online | IP 192.168.59.233 |
| DHT11 | ✅ ทำงาน | อุณหภูมิ + ความชื้นอากาศ (ชั่วคราว) |
| DS18B20 Waterproof | ✅ ทำงาน | อุณหภูมิน้ำ พบ 1 ตัว |
| Relay 4CH Active-LOW | ✅ ทำงาน | โหลด config จาก Firebase ได้ |
| Capacitive Soil Moisture | ✅ ต่อแล้ว | GPIO34 |
| SHT35 | 🛒 ยังไม่ได้ซื้อ | แม่นยำกว่า DHT11 — ซื้อแล้วแทน DHT11 ได้เลย |
| Fan Shutter 10" AC 220V | 🛒 ยังไม่ได้ซื้อ | CH2/CH3 รอต่อ |
| ปั๊มน้ำ DC 24V | ⏳ รอต่อ relay | CH1 พร้อม |

---

## Firmware v1.2.0 — สถานะ Features

| Feature | สถานะ | รายละเอียด |
|---|---|---|
| อ่าน sensor ทุก 30 วิ → push Firebase | ✅ | `/smartfarm/sensors/` |
| Auto control (temp/humidity threshold) | ✅ | ปรับได้จาก dashboard |
| Poll Firebase ทุก 5 วิ → apply relay | ✅ | แทน stream (SSL memory fix) |
| Schedule ตั้งเวลาเปิด/ปิดต่อ relay | ✅ | รองรับข้ามคืน (22:00–06:00) |
| Hourly log avg/max/min | ✅ | `/logs/YYYY-MM-DD/HH/` ทุก 1 ชม. |
| NTP sync UTC+7 | ✅ | pool.ntp.org |
| Firebase Anonymous Auth | ✅ | |

**วิธี Flash:** เปิด `smartfarm_firmware/smartfarm_firmware.ino` ใน Arduino IDE → เลือก Board: ESP32 Dev Module → Upload

---

## Dashboard v1.2.0 (Blynk IoT Level) — สถานะ Features

| Feature | สถานะ | รายละเอียด |
|---|---|---|
| SVG Gauge widgets (4 sensors) | ✅ | แทน progress bar |
| Real-time sensor data | ✅ | Firebase listener |
| Relay control Manual/Auto | ✅ | ทุก channel |
| Schedule ตั้งเวลาต่อ relay | ✅ | details/summary ใน relay card |
| History chart 24 ชม. | ✅ | รอ hourly log แรก (~1 ชม.) |
| Multi-user real-time | ✅ | ทุกคนเห็นพร้อมกัน |
| Mobile bottom nav (4 tabs) | ✅ | Sensors / Control / Chart / Settings |
| PWA ติดตั้งบนมือถือ | ✅ | manifest.json + sw.js + icon.svg |
| Push notification | ✅ | Browser Notification API |
| Offline device detection | ✅ | แจ้งเตือนถ้า ESP32 หาย > 2 นาที |
| Action log (ใครสั่งอะไร) | ✅ | real-time, แสดง 10 รายการล่าสุด |
| Threshold settings | ✅ | บันทึก Firebase → ESP32 รับทันที |

---

## Firebase Project

```
Project ID:   greenhouse-iot-smart-farm
Database URL: https://greenhouse-iot-smart-farm-default-rtdb.asia-southeast1.firebasedatabase.app
Hosting URL:  https://greenhouse-iot-smart-farm.web.app
API Key:      AIzaSyBZ2ke2Bbx-Nd9zhsZa0QDZUlKRtEbnCWE
Auth:         Anonymous Authentication
```

### Database Structure
```
/smartfarm/
  sensors/        ← ESP32 push ทุก 30 วิ
  status/         ← online, relay state, firmware version
  control/        ← thresholds, mode, manual_state, schedule
  alerts/         ← last_alert
  action_log/     ← ประวัติการสั่งงาน
/logs/
  YYYY-MM-DD/
    HH/           ← avg/max/min รายชั่วโมง
```

---

## Arduino Libraries ที่ต้องติดตั้ง

| Library | by | หมายเหตุ |
|---|---|---|
| Firebase ESP32 Client | Mobizt | **ไม่ใช่** "Firebase Arduino Client Library" |
| OneWire | Paul Stoffregen | |
| DallasTemperature | Miles Burton | |
| Adafruit SHT31 Library | Adafruit | ใช้ address 0x44 |
| DHT sensor library | Adafruit | DHT11 ชั่วคราว |

---

## งานที่ต้องทำต่อ

- [ ] **Deploy Firebase Hosting** — ให้พนักงานเข้าจากนอก WiFi
  ```bash
  cd "/Users/tonklax/Documents/Greenhouse IoT Smart Farm"
  firebase deploy
  ```
  URL หลัง deploy: `https://greenhouse-iot-smart-farm.web.app`

- [ ] **รอ Hourly Log แรก** — ประมาณ 1 ชม. หลัง firmware boot → กราฟจะขึ้นเอง

- [ ] **ซื้อและต่อ SHT35** — แม่นยำกว่า DHT11 มาก (I2C: SDA=21, SCL=22, address 0x44)

- [ ] **ต่อ Fan Shutter** — CH2 GPIO27 (ระบายออก), CH3 GPIO14 (ดึงเข้า)

- [ ] **ต่อปั๊มน้ำ 24V** — CH1 GPIO26 ผ่าน Boost Converter 12V→24V

---

## ข้อควรระวัง Hardware

| ปัญหา | วิธีแก้ |
|---|---|
| GPIO12 boot fail | ห้ามใช้กับ relay — ใช้ GPIO25 แทน (CH4) |
| DS18B20 อ่านไม่ได้ | ต้องมี external pull-up 5.1kΩ ระหว่าง VCC–DAT |
| ESP32 WiFi ไม่ติด | รองรับ 2.4GHz เท่านั้น — ไม่รองรับ 5GHz |
| Relay ไม่ทำงาน | Active-LOW: LOW=เปิด, HIGH=ปิด |
| Firebase Stream SSL crash | แก้แล้วด้วย polling แทน (ทุก 5 วิ) |

---

*อัปเดตล่าสุด: 19 มิถุนายน 2569*
