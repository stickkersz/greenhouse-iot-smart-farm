# Greenhouse IoT Smart Farm — Checkpoint v1.4.0

**Date:** 2026-06-29 (อัปเดต v1.4.0)  
**Status:** ✅ Production Ready — Tested at Company  
**Version:** 1.4.0  
**GitHub:** https://github.com/stickkersz/greenhouse-iot-smart-farm (private)  
**Deployed:** https://greenhouse-iot-smart-farm.web.app  
**Last Commit:** `f98160e feat: redesign settings + dynamic alert thresholds + presets`

---

## ✅ Test Results (ทดสอบที่บริษัท วันนี้)

| Feature | Status | หมายเหตุ |
|---------|--------|---------|
| ESP32 connect WiFi (floor-1-2-2.4G) | ✅ | WiFiMulti auto-connect |
| Sensor data live (30s interval) | ✅ | air_temp, humidity, water_temp |
| Relay control — Manual mode | ✅ | CH1–CH4 click ได้ยินเสียง relay |
| Dashboard action log | ✅ | บันทึก user + time ทุก action |
| Chart 24h มีข้อมูล | ✅ | Live injection + hourly logs |
| Chart X-axis รายชั่วโมง | ✅ | แสดงครบ 24 ชั่วโมง |
| Alert trigger + notification panel | ✅ | Dynamic threshold ทำงานได้ |
| Settings presets | ✅ | ☀️ / 🌤️ / 🌧️ / ✏️ |
| Export CSV | ✅ | UTF-8 BOM เปิด Excel ถูก encoding |
| Mobile responsive (iPhone 12 mini) | ✅ | Bottom nav + fonts OK |

---

## 📋 Session วันนี้ (2026-06-23)

### งานที่ทำ

#### 1. ✅ แก้ชื่อบริษัทครบทุกไฟล์
- เปลี่ยนจาก **บริษัทประวิทย์กรุ๊ป ปุ๋ยไวกิ้ง** → **บริษัท ปุ๋ยไวกิ้ง จำกัด**
- ไฟล์ที่แก้: `dashboard/index.html`, `smartfarm_firmware/smartfarm_firmware.ino`, `STATUS.md`, `PROJECT_MEMORY.md`

#### 2. ✅ Chart 24h X-axis รายชั่วโมง
- เพิ่ม `maxTicks = labels.length` สำหรับ 24h mode
- 7d/30d ยังใช้ limit 10 ticks (readable)

#### 3. ✅ Mobile UX Overhaul
- `viewport-fit=cover` → แก้ bottom nav ซ่อนใต้ Safari bar
- `safe-area-inset-bottom` → bottom nav ลอยเหนือ home indicator
- Header title: "Greenhouse IoT Smart Farm" → **"SmartFarm"** (< 600px)
- Font sizes เพิ่มขึ้น 15–20% ทั่ว mobile
- Alert threshold ปรับได้จาก Settings

#### 4. ✅ Settings Tab Redesign (สำคัญมาก)

**ก่อน:** 3 input fields เท่านั้น (temp_on, temp_off, humidity_min)

**หลัง:**
- **⚡ Quick Preset** — ☀️ ฤดูร้อน / 🌤️ ปกติ / 🌧️ ฤดูฝน / ✏️ กำหนดเอง
- **🔌 Channel Assignment** — แสดงว่า CH ไหนควบคุมอะไร
- **🌡️ Temperature group** — Temp ON + Temp OFF + 🚨 Alert threshold
- **💧 Humidity group** — Humidity MIN + 🚨 Alert threshold

#### 5. ✅ Dynamic Alert Thresholds
- Alert threshold เดิม: hardcode `t > 38` และ `rh < 40`
- ตอนนี้: โหลดจาก Firebase `/smartfarm/control/thresholds/temp_alert` + `humidity_alert`
- `checkAlerts()` ใช้ `alertThresholds` variable ที่ sync กับ Firebase real-time
- ทดสอบ: ตั้ง Alert Temp = 28°C → แจ้งเตือนขึ้นทันที ✅

#### 6. ✅ Notification Panel (จาก session ก่อน)
- Bell icon 🔔 + badge แดงแสดงจำนวน unseen
- Slide-in panel จากขวา พร้อม alert history
- Clear all + Enable Push Notifications

---

## 🏗️ Architecture

### Tech Stack
- **Frontend:** HTML5 + Vanilla JS + Chart.js 4.4.0 + SVG gauges
- **Backend:** Firebase Realtime Database (asia-southeast1)
- **Auth:** Firebase Email/Password (disguised username/password → `user@smartfarm.local`)
- **Hosting:** Firebase Hosting (Spark tier — free)
- **PWA:** manifest.json + sw.js

### Database Structure
```
/smartfarm/
  sensors/
    air_temp, air_humidity, water_temp
    uptime_sec, firmware_ver
  status/
    online, ch1_pump, ch2_fan_out, ch3_fan_in, ch4_spare, firmware
  control/
    thresholds/
      temp_on, temp_off, humidity_min        ← ESP32 ใช้ (auto relay)
      temp_alert, humidity_alert             ← Dashboard ใช้ (alert)
    ch1_pump/   {mode, state, schedule}
    ch2_fan_out/{mode, state, schedule}
    ch3_fan_in/ {mode, state, schedule}
    ch4_spare/  {mode, state, schedule}
  alerts/
  alert_history/   ← 30 รายการล่าสุด, 5min cooldown/type
  action_log/      ← 10 รายการล่าสุด, required fields validated
/logs/
  YYYY-MM-DD/HH/
    air_temp_avg/min/max
    air_humidity_avg/min/max
    water_temp_avg/min/max
    sample_count
```

### Firebase Rules Summary
```
sensors:       read=email, write=anon (ESP32)
status:        read=email, write=anon
control:       read=anon,  write=email (dashboard only)
alerts:        read=email, write=anon
alert_history: read=email, write=email
action_log:    read=email, write=email + validate fields
logs:          read=email, write=anon
```

---

## ⚙️ Settings Presets

| Preset | Temp ON | Temp OFF | Pump ON | Alert Temp | Alert Hum |
|--------|---------|----------|---------|------------|-----------|
| ☀️ ฤดูร้อน | 33°C | 30°C | ≤70% | 38°C | 50% |
| 🌤️ ปกติ (default) | 35°C | 32°C | ≤60% | 40°C | 40% |
| 🌧️ ฤดูฝน | 38°C | 35°C | ≤50% | 42°C | 35% |
| ✏️ กำหนดเอง | user input | - | - | - | - |

---

## 🔌 Hardware

**ESP32 DevKit V1**

| Channel | GPIO | อุปกรณ์ | ควบคุมด้วย |
|---------|------|---------|-----------|
| CH1 | 26 | สำรอง | Manual / Schedule only |
| CH2 | 27 | — ไม่ได้ใช้ | - |
| CH3 | 14 | พัดลม 220V (ดูดเข้า) | Temperature (auto) |
| CH4 | 25 | ปั๊มน้ำ 24V | Humidity (auto) |

**Sensors:**
- DHT22 (GPIO32) — อุณหภูมิ + ความชื้นอากาศ (ชั่วคราว รอ SHT35)
- DS18B20 (GPIO4) — อุณหภูมิน้ำ (ต้องมี Pull-up 4.7kΩ ที่ DATA-VCC)

**WiFiMulti:**
- `floor-1-2-2.4G` — บริษัท (ชั้น 1-2)
- `Lab_F2-2.4G` — บริษัท (Lab ชั้น 2)
- 🔒 รหัสผ่านเก็บใน `config.h` เท่านั้น (ห้าม commit)

**Partition:** Huge APP (3MB No OTA) — เพราะ sketch ใหญ่

---

## 📱 UI / UX

### Tabs (Bottom Nav mobile / Full desktop)
1. **📊 Sensors** — gauge cards + uptime + status
2. **🔌 Control** — relay toggle + mode + schedule
3. **📈 Chart** — 24h/7d/30d + CSV export + alert history
4. **⚙️ Settings** — presets + thresholds + action log

### Responsive Breakpoints
| Width | Layout |
|-------|--------|
| < 375px | iPhone SE — compact, ซ่อน status pill |
| < 600px | Mobile — "SmartFarm" title, bottom nav, larger fonts |
| 768–1023px | Tablet — 2-col relay/settings, 2-col sensors |
| ≥ 1024px | Desktop — full auto-fit grid |

### Alert System
- **checkAlerts()** — เรียกทุกครั้งที่ sensor data update
- Threshold โหลดจาก Firebase → dynamic (ผู้ใช้ปรับได้)
- Banner แดงบน dashboard + badge 🔔 + push notification
- 5-minute cooldown per alert type (sessionStorage)
- บันทึก history → `/smartfarm/alert_history`

---

## 🐛 Known Issues & Fixes (ประวัติ)

| ปัญหา | สถานะ | วิธีแก้ |
|-------|--------|---------|
| WiFi connect ไม่ได้ 2 network | ✅ Fixed | WiFiMulti library |
| Firebase SSL errors | ✅ Fixed | getJSON() batching (1 call แทน 11) |
| Arduino flash 106%+ | ✅ Fixed | Partition: Huge APP 3MB |
| Chart 24h ว่างเปล่า | ✅ Fixed | Live sensor injection ชั่วโมงปัจจุบัน |
| Bottom nav ซ่อนใต้ Safari | ✅ Fixed | viewport-fit=cover + safe-area-inset |
| Alert hardcode ปรับไม่ได้ | ✅ Fixed | Dynamic threshold จาก Firebase |
| Chart X-axis ไม่ครบ 24h | ✅ Fixed | maxTicksLimit = 24 |
| ชื่อบริษัทผิด | ✅ Fixed | แก้ครบทุกไฟล์ |

---

## 📂 Files

```
/Users/tonklax/Documents/Greenhouse IoT Smart Farm/
├── dashboard/
│   ├── index.html          ← Main app (~1700+ lines)
│   ├── manifest.json       ← PWA
│   ├── icon.svg
│   └── sw.js
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino
│   ├── config.h            ← .gitignore (WiFi + Firebase credentials)
│   └── config.h.example    ← Template
├── database.rules.json     ← Firebase security rules
├── .firebaserc
├── .gitignore
├── STATUS.md
├── PROJECT_MEMORY.md
└── CHECKPOINT.md           ← This file
```

---

## 🎯 Next Steps (Phase 2)

### Hardware
- [ ] ซื้อ SHT35 แทน DHT22 (แม่นยำกว่า)
- [ ] ต่อปั๊มน้ำ CH1 จริง
- [ ] ต่อพัดลม CH2/CH3 จริง
- [ ] ทดสอบ Auto mode ครบ loop

### Software
- [ ] Dark Mode toggle
- [x] Telegram alerts (ESP32 ยิง Bot API ตรง — ใช้ได้บน Spark free) ✅
- [ ] Email alerts (Firebase Cloud Functions — ต้อง Blaze)
- [ ] Multiple zones support
- [ ] Weekly/monthly report PDF

### Maintenance
- [ ] Monitor Firebase free tier usage
- [ ] Regular git backup
- [ ] Test WiFi failover home ↔ company

---

**Last Updated:** 2026-06-23  
**Commits:** `f090dd9` → `f98160e`  
**Tested by:** Tonkla (IT Intern, KMUTT CS Year 2)
