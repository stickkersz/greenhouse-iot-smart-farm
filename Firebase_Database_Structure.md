# 🔥 Firebase Realtime Database — Structure Design
**โปรเจกต์: IoT Smart Farm | อัปเดต: มิถุนายน 2569**

---

## 📌 ภาพรวม

ESP32 จะ push ข้อมูลขึ้น Firebase ทุก **30 วินาที** และรับคำสั่ง Remote Control จาก Firebase แบบ Real-time

```
Firebase Realtime Database
└── smartfarm/
    ├── sensors/          ← ข้อมูล sensor ล่าสุด (live)
    ├── logs/             ← ประวัติข้อมูลรายชั่วโมง (historical)
    ├── control/          ← คำสั่งควบคุม relay (จาก Dashboard → ESP32)
    ├── status/           ← สถานะระบบปัจจุบัน (ESP32 → Dashboard)
    └── alerts/           ← แจ้งเตือนล่าสุด
```

---

## 📂 โครงสร้างละเอียด

### `/smartfarm/sensors/` — ข้อมูล Sensor ล่าสุด

```json
{
  "smartfarm": {
    "sensors": {
      "air_temp": 32.5,
      "air_humidity": 65.2,
      "water_temp": 28.1,
      "soil_moisture_raw": 2340,
      "soil_moisture_pct": 62,
      "timestamp": 1718500000,
      "uptime_sec": 3600
    }
  }
}
```

| Field | ชนิด | หน่วย | Sensor |
|-------|------|-------|--------|
| `air_temp` | float | °C | SHT40 |
| `air_humidity` | float | % RH | SHT40 |
| `water_temp` | float | °C | DS18B20 |
| `soil_moisture_raw` | int | ADC (0-4095) | Capacitive |
| `soil_moisture_pct` | int | % (0-100) | Capacitive (แปลงแล้ว) |
| `timestamp` | int | Unix Epoch (sec) | ESP32 |
| `uptime_sec` | int | วินาที | ESP32 |

---

### `/smartfarm/logs/YYYY-MM-DD/HH/` — ประวัติรายชั่วโมง

```json
{
  "smartfarm": {
    "logs": {
      "2569-06-16": {
        "10": {
          "air_temp_avg": 33.1,
          "air_temp_max": 36.2,
          "air_temp_min": 30.5,
          "air_humidity_avg": 61.0,
          "water_temp_avg": 28.5,
          "soil_moisture_avg": 58,
          "pump_on_count": 3,
          "pump_on_total_sec": 180
        },
        "11": { }
      }
    }
  }
}
```

> ESP32 จะ aggregate ข้อมูลทุก 1 ชั่วโมงแล้ว push ขึ้น `logs/` ครั้งเดียว

---

### `/smartfarm/control/` — คำสั่งจาก Dashboard → ESP32

```json
{
  "smartfarm": {
    "control": {
      "ch1_pump": {
        "mode": "auto",
        "manual_state": false
      },
      "ch2_fan_out": {
        "mode": "auto",
        "manual_state": false
      },
      "ch3_fan_in": {
        "mode": "auto",
        "manual_state": false
      },
      "ch4_led": {
        "mode": "timer",
        "manual_state": false
      },
      "thresholds": {
        "temp_on": 35.0,
        "temp_off": 32.0,
        "humidity_min": 60.0
      }
    }
  }
}
```

| Field | ค่า | ความหมาย |
|-------|-----|---------|
| `mode` | `"auto"` | ESP32 ตัดสินใจเองตาม threshold |
| `mode` | `"manual"` | Dashboard บังคับเปิด/ปิด |
| `mode` | `"timer"` | ใช้กับ LED — เปิดตามเวลา |
| `manual_state` | `true/false` | ใช้เมื่อ mode = manual |

---

### `/smartfarm/status/` — สถานะระบบปัจจุบัน (ESP32 → Dashboard)

```json
{
  "smartfarm": {
    "status": {
      "online": true,
      "last_seen": 1718500000,
      "wifi_rssi": -65,
      "ch1_pump": false,
      "ch2_fan_out": false,
      "ch3_fan_in": false,
      "ch4_led": true,
      "firmware_version": "1.0.0"
    }
  }
}
```

> ESP32 จะ update `last_seen` ทุก 30 วินาที — ถ้า Dashboard เห็นว่าห่างกันนาน > 2 นาที = `offline`

---

### `/smartfarm/alerts/` — การแจ้งเตือนล่าสุด

```json
{
  "smartfarm": {
    "alerts": {
      "last_alert": {
        "type": "high_temp",
        "value": 38.5,
        "timestamp": 1718500000,
        "message": "อุณหภูมิสูงเกิน 38°C — เปิดปั๊มและพัดลมแล้ว",
        "sent_line": true
      }
    }
  }
}
```

| `type` | เงื่อนไข | Line Notify |
|--------|---------|-------------|
| `high_temp` | T > 38°C | ✅ ส่ง |
| `low_humidity` | RH < 40% | ✅ ส่ง |
| `sensor_error` | อ่านค่าไม่ได้ | ✅ ส่ง |
| `pump_on` | เปิดปั๊มอัตโนมัติ | ❌ ไม่ส่ง (ปกติ) |

---

## 🔒 Security Rules

```json
{
  "rules": {
    "smartfarm": {
      ".read": true,
      ".write": true
    }
  }
}
```

> ⚠️ **Test Mode** — ใช้ระหว่างพัฒนาเท่านั้น  
> ก่อน deploy จริง ต้องเปลี่ยนเป็น Auth-based rules:

```json
{
  "rules": {
    "smartfarm": {
      "sensors":  { ".read": true,  ".write": "auth != null" },
      "logs":     { ".read": true,  ".write": "auth != null" },
      "control":  { ".read": "auth != null", ".write": "auth != null" },
      "status":   { ".read": true,  ".write": "auth != null" },
      "alerts":   { ".read": true,  ".write": "auth != null" }
    }
  }
}
```

---

## 📦 Free Tier Limits (Spark Plan)

| Limit | ค่า | ระบบเราใช้ |
|-------|-----|-----------|
| Simultaneous connections | 100 | ~3 (ESP32 + Dashboard + Mobile) ✅ |
| Storage | 1 GB | ข้อมูล sensor 1 ปี ≈ 50 MB ✅ |
| Download/month | 10 GB | ✅ เกินพอ |
| Upload/month | ไม่จำกัด | ✅ |

> ✅ Free Tier เพียงพอสำหรับระบบขนาดนี้ ไม่ต้องจ่ายเงิน

---

## 🛠️ ขั้นตอน Setup Firebase

1. ไปที่ [console.firebase.google.com](https://console.firebase.google.com)
2. **Add project** → ตั้งชื่อ `smartfarm-viking` → Disable Google Analytics (ไม่จำเป็น)
3. **Build → Realtime Database → Create database** → เลือก Region: `asia-southeast1` (Singapore)
4. Start in **Test mode** (เปลี่ยน rules ทีหลัง)
5. Copy **Database URL**: `https://smartfarm-viking-default-rtdb.asia-southeast1.firebasedatabase.app`
6. **Project Settings → Service accounts** → Copy config สำหรับ ESP32:
   ```
   API Key: AIzaSy...
   Database URL: https://smartfarm-viking-...
   ```

---

*จัดทำโดย: Tonkla (IT Intern) | มิถุนายน 2569*
