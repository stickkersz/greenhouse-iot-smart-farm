# 🔥 Firebase Realtime Database — Structure Design
**โปรเจกต์: IoT Smart Farm | อัปเดต: 2026-08-03 (firmware v2.9.2)**

> ⚠️ เอกสารนี้สะท้อน "สัญญาข้อมูลจริง" ที่ firmware เขียน/อ่าน · แหล่งความจริง (source of truth) คือ
> `database.rules.json` (validation ตัวจริง) + ฟังก์ชัน `pushStatus()` / `pushToFirebase()` /
> `pushHourlyLog()` / `loadControlFromFirebase()` / `reportAlert()` ใน `smartfarm_firmware.ino`
> ถ้าแก้ field ต้องแก้ทั้ง firmware + `database.rules.json` (มี `"$other": { ".validate": false }`
> = field ที่ไม่ได้ประกาศไว้จะถูก reject เงียบๆ) + เพิ่มเทสต์ใน `tests/database.rules.test.js`

---

## 📌 ภาพรวม

ESP32 push ข้อมูลขึ้น Firebase ทุก **30 วินาที** และ poll คำสั่ง Remote Control ทุก **1.5 วินาที** แบบ Real-time

```
Firebase Realtime Database
├── smartfarm/
│   ├── sensors/         ← ข้อมูล sensor ล่าสุด (live)              ESP32 → Dashboard
│   ├── control/         ← คำสั่งควบคุม relay + threshold + schedule  Dashboard → ESP32
│   ├── status/          ← สถานะระบบปัจจุบัน + boot diagnostics       ESP32 → Dashboard
│   ├── alerts/          ← แจ้งเตือนล่าสุด                            ESP32 → Dashboard
│   ├── alert_history/   ← ประวัติแจ้งเตือน                           Dashboard เขียนเอง
│   └── action_log/      ← log การสั่งงานของผู้ใช้                     Dashboard เขียนเอง
└── logs/                ← ประวัติข้อมูลรายชั่วโมง (historical)         ESP32 → Dashboard
```

> หมายเหตุ: `logs/` อยู่ที่ **root** ไม่ใช่ใต้ `smartfarm/` (path จริง = `/logs/YYYY-MM-DD/HH`)

---

## 📂 โครงสร้างละเอียด

### `/smartfarm/sensors/` — ข้อมูล Sensor ล่าสุด (ESP32 เขียนทุก 30 วิ)

```json
{
  "smartfarm": {
    "sensors": {
      "air_temp": 32.5,
      "air_humidity": 65.2,
      "water_temp": 28.1,
      "uptime_sec": 3600
    }
  }
}
```

| Field | ชนิด | หน่วย | Sensor | หมายเหตุ |
|-------|------|-------|--------|----------|
| `air_temp` | float | °C | SHT35 (I2C 0x44/0x45) | |
| `air_humidity` | float | % RH | SHT35 | |
| `water_temp` | float | °C | DS18B20 (GPIO4) | เขียน**เฉพาะตอนอ่านได้** — สายหลุด/ค่าเสียจะไม่เขียนทับ (dashboard เช็ค `status/water_ok`) |
| `uptime_sec` | int | วินาที | ESP32 | |

> `air_temp`/`air_humidity` = 0 ตอนเซนเซอร์อ่านพลาด — ดู `status/sensor_ok` / `status/sensor_stale` ประกอบ

---

### `/logs/YYYY-MM-DD/HH/` — ประวัติรายชั่วโมง (ESP32 เขียนตอนขึ้นชั่วโมงใหม่)

```json
{
  "logs": {
    "2569-07-16": {
      "10": {
        "air_temp_avg": 33.1,
        "air_temp_max": 36.2,
        "air_temp_min": 30.5,
        "air_humidity_avg": 61.0,
        "air_humidity_max": 68.4,
        "air_humidity_min": 55.2,
        "sample_count": 118,
        "water_temp_avg": 28.5,
        "water_temp_max": 29.1,
        "water_temp_min": 27.8
      }
    }
  }
}
```

| Field | ชนิด | หมายเหตุ |
|-------|------|----------|
| `air_temp_avg/max/min` | float | เฉลี่ย/สูงสุด/ต่ำสุดของอากาศในชั่วโมงนั้น |
| `air_humidity_avg/max/min` | float | |
| `sample_count` | int | จำนวน sample อากาศที่ใช้เฉลี่ย |
| `water_temp_avg/max/min` | float | เขียน**เฉพาะเมื่อมี sample น้ำที่อ่านได้** (สายยาวอาจอ่านพลาดบางครั้ง) |

> flush ตาม "ขอบชั่วโมงนาฬิกาจริง" (ไม่ใช่ทุก 60 นาที millis) · accumulator เก็บใน RTC memory รอด reboot
> · สะสมเฉพาะตอนนาฬิกา (NTP) ติดแล้วเท่านั้น — กันข้อมูลตกถังผิดชั่วโมง · ตั้งแต่ v2.9.2 เขียนเป็น
> `updateNodeSilent` (PATCH) ก้อนเดียวแทนการยิง `setFloat`/`setInt` แยก 7-10 ครั้งต่อชั่วโมง

---

### `/smartfarm/control/` — คำสั่งจาก Dashboard → ESP32 (ESP32 poll ทุก 1.5 วิ)

```json
{
  "smartfarm": {
    "control": {
      "ch1_pump":    { "mode": "manual", "manual_state": false,
                       "schedule": { "enabled": false, "on_time": "07:00", "off_time": "18:00" } },
      "ch2_fan_out": { "mode": "manual", "manual_state": false,
                       "schedule": { "enabled": false, "on_time": "07:00", "off_time": "18:00" } },
      "ch3_fan_in":  { "mode": "auto",   "manual_state": false,
                       "schedule": { "enabled": false, "on_time": "07:00", "off_time": "18:00" } },
      "ch4_spare":   { "mode": "auto",   "manual_state": false,
                       "schedule": { "enabled": false, "on_time": "07:00", "off_time": "18:00" } },
      "thresholds": {
        "temp_on": 35.0,
        "temp_off": 32.0,
        "humidity_min": 60.0,
        "humidity_max": 75.0,
        "humidity_vent": 80.0,
        "temp_alert": 38.0,
        "humidity_alert": 40.0,
        "water_temp_alert": 35.0
      },
      "buzzer_enabled": true
    }
  }
}
```

**Channel mapping (การเดินสายจริง 2026-06-26):**

| Key | Relay/GPIO | อุปกรณ์ | คุมด้วย |
|-----|-----------|---------|---------|
| `ch1_pump` | CH1 / GPIO26 | สำรอง | manual / schedule เท่านั้น (ไม่มี auto) |
| `ch2_fan_out` | CH2 / GPIO27 | ไม่ได้ใช้ | ค้าง OFF (ซ่อนใน dashboard) |
| `ch3_fan_in` | CH3 / GPIO14 | **พัดลม 220V (ดูดเข้า)** | อุณหภูมิ + ความชื้น + vent (3 latch OR กัน — ดูตาราง thresholds ด้านล่าง) |
| `ch4_spare` | CH4 / GPIO25 | **ปั๊มน้ำ 24V** | ความชื้น + evaporative cooling + pump safety |

> ⚠️ ชื่อ key เป็นชื่อ "ตำแหน่งเดิม" ไม่ตรงกับหน้าที่จริง (`ch4_spare` = ปั๊ม, `ch1_pump` = สำรอง) —
> คงชื่อไว้เพื่อไม่ให้ dashboard/rules/firmware หลุด sync · ดู `IDX_FAN`/`IDX_PUMP` ใน firmware

| Field | ค่า | ความหมาย |
|-------|-----|---------|
| `mode` | `"auto"` \| `"manual"` | auto = ESP32 คุมเองตาม threshold · manual = dashboard บังคับ (ไม่มี "timer") |
| `manual_state` | `true/false` | ใช้เมื่อ mode = manual |
| `schedule.enabled` | `true/false` | เปิด = คุมตามเวลา (แทน auto/manual) |
| `schedule.on_time` / `off_time` | `"HH:MM"` | 24 ชม. · on > off = ข้ามคืน (เช่น 22:00–06:00) |

**Thresholds** (dashboard ตั้ง · rules บังคับความสัมพันธ์ให้ hysteresis ถูกต้อง):

| Field | เงื่อนไข rules | หน้าที่ |
|-------|---------------|--------|
| `temp_on` | > `temp_off`, ในช่วง -20..70 | พัดลมเปิดเมื่ออากาศ ≥ ค่านี้ |
| `temp_off` | -20..70 | พัดลมปิดเมื่ออากาศ ≤ ค่านี้ |
| `humidity_min` | 0..100 | ปั๊มเปิดเมื่อความชื้น < ค่านี้ (แห้ง) |
| `humidity_max` | > `humidity_min`, 0..100 · ต้อง `+ VENT_HYST(5) ≤ humidity_vent` (เว้นแต่ vent ปิด = 0) | ปั๊มปิดเมื่อความชื้น ≥ ค่านี้ (ชื้น) — ปั๊มบล็อกไปด้วย ไม่ใช่แค่ปิด |
| `humidity_vent` | 0 (ปิด vent) หรือ `> 5` และ `≥ humidity_max + 5`, 0..100 | พัดลมเปิด (ไม่พึ่งปั๊ม) เมื่อความชื้น ≥ ค่านี้ — ระบายความชื้นเกินออก ปิดเมื่อลดลงมาถึง `humidity_vent − VENT_HYST` (5%) — constraint นี้บังคับด้วย `npm run test:sync` กันไม่ให้ firmware/rules/dashboard ไม่ตรงกัน |
| `temp_alert` | > `temp_on`, -20..70 | buzzer/alert เมื่ออุณหภูมิเกิน |
| `humidity_alert` | < `humidity_min`, 0..100 | buzzer/alert เมื่อความชื้นต่ำ |
| `water_temp_alert` | -20..80 | alert เมื่ออุณหภูมิน้ำเกิน |

---

### `/smartfarm/status/` — สถานะระบบปัจจุบัน (ESP32 เขียนทุก 30 วิ + ทุกครั้งที่ relay เปลี่ยน)

```json
{
  "smartfarm": {
    "status": {
      "online": true,
      "ch1_pump": false,
      "ch2_fan_out": false,
      "ch3_fan_in": true,
      "ch4_spare": false,
      "firmware": "2.9.2",
      "sensor_ok": true,
      "sensor_stale": false,
      "water_ok": true,
      "failsafe": false,
      "pump_locked": false,
      "fan_locked": false,
      "time_ok": true,
      "wifi_rssi": -65,
      "wifi_drop_count": 0,
      "wifi_drop_reason": "ยังไม่เคยหลุดตั้งแต่บูต",
      "last_reset_reason": "SW (ESP.restart() จากโค้ดเราเอง)",
      "boot_count": 3,
      "free_heap": 210344,
      "max_alloc_heap": 110580
    }
  }
}
```

> ตั้งแต่ v2.9.1 ทั้งก้อนนี้ส่งเป็น **`updateNodeSilent` (PATCH) ครั้งเดียว** ไม่ใช่ `setX()` แยกทีละฟิลด์แบบเดิม
> (24 SSL round trip → 1) — ผลคือถ้า field ไหนถูก "งด" ในรอบ push หนึ่งๆ ค่าเก่าจะยังอยู่ ไม่ถูกลบ

| Field | ชนิด | ความหมาย |
|-------|------|---------|
| `online` | bool | ESP32 กำลังทำงาน |
| `ch1_pump`..`ch4_spare` | bool | สถานะ relay จริง (ตรงกับ key ใน control) |
| `firmware` | string (<16) | เวอร์ชัน firmware ปัจจุบัน = `"2.9.2"` — อยู่ 6 ที่ในโค้ด/repo ต้องขยับพร้อมกัน (checklist หัวไฟล์ `.ino`) |
| `sensor_ok` | bool | อ่านเซนเซอร์อากาศรอบล่าสุดสำเร็จ (พลาด 1 ครั้ง = false) |
| `sensor_stale` | bool | พลาดจนเลิกเชื่อแล้ว (≥12 ครั้งติด) → **auto ปิดทุกช่อง** — เรื่องใหญ่ ต้องเด้งเตือน |
| `water_ok` | bool | DS18B20 อ่านได้ (false → dashboard โชว์ "—") |
| `failsafe` | bool | คงไว้ = false เสมอ (failsafe layer ถอดออก 2026-07-03 — คง field กัน dashboard พัง) |
| `pump_locked` | bool | ปั๊มอยู่ช่วง cooldown (พักหลังเดินครบ **15 นาที**) |
| `fan_locked` | bool | **[v2.8.0]** พัดลมอยู่ช่วง cooldown — พักคู่กับปั๊มเสมอ (deadline เดียวกัน) ตั้งแต่ v2.8.0 |
| `time_ok` | bool | นาฬิกา (NTP) sync แล้ว — false = schedule ไม่ทำงาน |
| `wifi_rssi` | int | ความแรงสัญญาณ (dBm) |
| `wifi_drop_count` | int | **[v2.9.0]** จำนวนครั้งที่ WiFi หลุดตั้งแต่บูต นับ **1 ครั้งต่อ 1 ช่วงออฟไลน์** (ไม่ใช่ต่อ retry event — แก้ไปใน v2.9.2 กันตัวเลขพุ่งเกินจริง) |
| `wifi_drop_reason` | string (<64) | **[v2.9.0]** เหตุผลการหลุดครั้งล่าสุดแบบอ่านออก (เช่น "200 BEACON_TIMEOUT") — v2.9.2 กันไม่ให้ escalation ที่บอร์ดสั่งเองไปทับเป็น "AP เตะเรา" ผิดๆ |
| `last_reset_reason` | string (<128) | **[v2.2.0]** สาเหตุ reboot รอบล่าสุด — BROWNOUT/PANIC/TASK_WDT/SW/POWERON |
| `boot_count` | int | **[v2.2.0]** บูตกี่ครั้งนับจากไฟดับล่าสุด — พุ่งเร็ว = reboot loop |
| `free_heap` | int | **[v2.2.0]** heap ว่าง (byte) — ลดเรื่อยๆ = memory leak |
| `max_alloc_heap` | int | **[v2.2.0]** ก้อน heap ต่อเนื่องใหญ่สุด — หดทั้งที่ free เยอะ = fragmentation |

> ⚠️ field ที่มีแท็กเวอร์ชัน `[vX.Y.Z]` ทั้งหมดต้องมีใน `database.rules.json` (`status` block) ไม่งั้นโดน
> `$other:false` reject เงียบๆ — เพราะ `pushStatus()` จบด้วย field ที่ valid ทำให้ `errorReason()` ว่าง
> = ดูเหมือน push สำเร็จ

---

### `/smartfarm/alerts/last_alert/` — การแจ้งเตือนล่าสุด (ESP32 เขียน)

```json
{
  "smartfarm": {
    "alerts": {
      "last_alert": {
        "type": "high_temp",
        "value": 38.5,
        "message": "อุณหภูมิสูงเกิน 38°C! (38.5°C)"
      }
    }
  }
}
```

| `type` | เงื่อนไข | หมายเหตุ |
|--------|---------|----------|
| `high_temp` | `air_temp > temp_alert` | |
| `low_humidity` | `air_humidity < humidity_alert` | |
| `high_water_temp` | `water_temp > water_temp_alert` | เฉพาะตอน `water_ok` |
| `sensor_fail` | เซนเซอร์อากาศพลาด ≥12 ครั้ง | auto หยุด — ย้ำทุก 10 นาที ไม่ใช่ทุก 30 วิ |
| `pump_cutoff` | ปั๊มเดินเกิน **15 นาที** (ตัว trigger) | ตัดปั๊ม + พัดลมพักคู่กัน 5 นาที — ไม่ตั้ง `value` (เขียนตรงผ่าน `setString`, ไม่ผ่าน `reportAlert()`) |
| `fan_cutoff` | **[v2.8.0]** พัดลมเดินเกิน 15 นาที (ตัว trigger แทน ไม่ใช่ปั๊ม) | ตัดพัดลม + ปั๊มพักคู่กัน 5 นาที — ไม่ตั้ง `value` เหมือนกัน (`pumpMaxed=false` กันแตร water alarm หลอก) |

> buzzer ดัง local เสมอเมื่อมี alert (ถ้า `buzzer_enabled`) — ไม่พึ่งเน็ต · Firebase เขียนเฉพาะตอนออนไลน์
> ยังไม่มี Line Notify (ไม่ได้ทำ) · ไม่มี field `sent_line` / `timestamp` ใน `last_alert`

---

### `/smartfarm/alert_history/` และ `/smartfarm/action_log/` — Dashboard เขียนเอง

```json
{
  "alert_history": { "$entry": { "msg": "...", "timestamp": 1718500000 } },
  "action_log":    { "$entry": { "action": "...", "user": "...", "timestamp": 1718500000 } }
}
```

> ESP32 **ไม่เขียน** 2 node นี้ — dashboard บันทึกเองตอนผู้ใช้สั่งงาน/ตอนเห็น alert · rules บังคับ field ครบชุด

---

## 🔒 Security Rules

Rules จริงอยู่ที่ **`database.rules.json`** (deploy ด้วย `firebase deploy --only database`) — **ไม่ใช่ test mode แล้ว**

หลักการ:
- **อ่าน** (`sensors`/`status`/`alerts`/`logs`/`alert_history`/`action_log`): ต้อง `auth != null && auth.token.email != null`
  (= ผู้ใช้ล็อกอินอีเมล · ESP32 anonymous อ่านไม่ได้ แต่ไม่จำเป็นต้องอ่าน)
- **เขียน sensors/status/alerts/logs**: `auth != null` (ESP32 anonymous เขียนได้)
- **เขียน control/alert_history/action_log**: ต้องมีอีเมล (เฉพาะผู้ใช้ dashboard — ESP32 อ่าน control อย่างเดียว)
- ทุก field มี `.validate` ตรวจชนิด/ช่วง + `"$other": { ".validate": false }` กัน field แปลกปลอม
- มีเทสต์ครอบใน `tests/database.rules.test.js` (`npm run test:rules` — ต้องมี Java + firebase emulator)

---

## 📦 Free Tier Limits (Spark Plan)

| Limit | ค่า | ระบบเราใช้ |
|-------|-----|-----------|
| Simultaneous connections | 100 | ~3 (ESP32 + Dashboard + Mobile) ✅ |
| Storage | 1 GB | ข้อมูล sensor 1 ปี ≈ 50 MB ✅ |
| Download/month | 10 GB | ✅ เกินพอ |

> ✅ Free Tier เพียงพอสำหรับระบบขนาดนี้

---

*จัดทำโดย: Nattakit Prasertsak (IT Intern) | อัปเดตให้ตรง firmware v2.9.2 — 2026-08-03*
