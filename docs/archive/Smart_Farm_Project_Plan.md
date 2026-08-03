# 🌿 Greenhouse IoT Smart Farm — Project Plan
**โปรเจกต์ฝึกงาน | IT Intern | ระบบพ่นหมอกอัตโนมัติ + Smart Monitoring**

---

## 📋 ภาพรวมโปรเจกต์

ติดตั้งระบบ IoT บนโรงเรือนโครงเหล็กสำหรับวิจัยผักสลัดกับปุ๋ยเจลคีเลต โดยระบบต้องทำได้ครบ:
- ควบคุมอุณหภูมิ/ความชื้น **อัตโนมัติ** ผ่าน threshold
- **Dashboard real-time** ดูผ่านมือถือ/browser ได้
- **สั่งงาน remote** เปิด/ปิดปั๊ม และพัดลมจากระยะไกล
- **แจ้งเตือน Line** เมื่ออุณหภูมิเกินค่าวิกฤต
- **บันทึกข้อมูล** สภาพแวดล้อมอัตโนมัติ + Export Excel

---

## 🏗️ System Architecture

```
[DHT22 Sensor] ──── GPIO ────┐
[Relay 2CH]    ──── GPIO ────┤
                              ▼
                         [ ESP32 ]  ──── WiFi/MQTT ────► [Mosquitto Broker]
                         (Main Brain)                          │
                         + SD Card backup                      ▼
                                                     [Python FastAPI Backend]
                                                          │         │
                                              [SQLite DB]    [Line Notify]
                                                          │
                                                          ▼
                                                  [Web Dashboard]
                                            กราฟ Real-time + Remote Control
```

**ทำไม ESP32 แทน STC-3028?**
- ESP32 ทำได้ทุกอย่างที่ STC-3028 ทำ + มี WiFi ในตัว + เขียนโค้ดได้เอง
- ราคาใกล้กัน (~200-300 บาท) แต่ได้ประสบการณ์ Engineering จริงๆ
- ถ้า WiFi หลุด → ยังทำงาน standalone ต่อได้ + SD Card backup

---

## 🛒 Bill of Materials (BOM) — งบประมาณ

| # | อุปกรณ์ | สเปก | ซื้อที่ไหน | ราคา (บาท) |
|---|---------|------|------------|------------|
| 1 | ESP32 Development Board | NodeMCU ESP32 (38-pin) | Shopee/Lazada | 200–280 |
| 2 | เซนเซอร์อุณหภูมิ/ความชื้น | DHT22 หรือ SHT31 (แนะนำ SHT31 แม่นยำกว่า) | Shopee | 80–150 |
| 3 | Relay Module 2 Channel | DC 5V trigger, รองรับ AC 220V/10A | Shopee | 60–120 |
| 4 | SD Card Module + SD Card | SPI interface + MicroSD 8–16GB | Shopee | 80–120 |
| 5 | ปั๊มน้ำไดอะแฟรม | DC 12V, 8-10 Bar, มี Adapter 220V→12V | Shopee | 500–800 |
| 6 | ชุดหัวพ่นหมอก | ทองเหลือง 0.1mm (10-15 หัว) + สาย PE 15m | Shopee | 300–500 |
| 7 | พัดลมบานเกล็ด | 8-10 นิ้ว, AC 220V, Gravity Shutter | Shopee | 800–1,200 |
| 8 | ถังพักน้ำ 200L + ลูกลอย | ถังน้ำ DI เดิม + วาล์วลูกลอย 1/2" | มีแล้ว + HomePro | 200 |
| 9 | กรองน้ำทางดูด | Suction Filter สแตนเลส | Shopee | 50–80 |
| 10 | RCBO เบรกเกอร์กันดูด | 16A, 30mA (Panasonic/Safe-T-Cut) | HomePro/ร้านไฟฟ้า | 500–800 |
| 11 | กล่องกันน้ำ IP65 | ขนาดกลาง + แท่งกราวด์ทองแดง | HomePro | 400–600 |
| 12 | Power Supply 5V/2A | สำหรับ ESP32 + relay | Shopee | 80–150 |
| 13 | สายไฟ + อุปกรณ์เบ็ดเตล็ด | Terminal block, สายกราวด์, Cable tie | ร้านไฟฟ้า | 300–500 |

**รวมทั้งหมด: ประมาณ 3,550 – 5,300 บาท**

> 💡 **Tips:** ซื้อ DHT22 + Relay ของ Elegoo หรือ HiLetgo แบรนด์ไหนก็ได้ แต่ SHT31 แม่นยำกว่า DHT22 มากถ้างบเพิ่มได้

---

## 📅 Timeline — 3 Phase

### Phase 1: Hardware + Control (สัปดาห์ 1–2)
**เป้าหมาย:** ระบบพ่นหมอกอัตโนมัติทำงานได้จริงในโรงเรือน

- [ ] ประกอบและทดสอบ wiring ESP32 + DHT22 + Relay บน breadboard
- [ ] เขียนโค้ด ESP32 firmware (อ่าน sensor → สั่ง relay อัตโนมัติ)
- [ ] ทดสอบ logic: เปิดปั๊มเมื่อ T > 35°C, ปิดเมื่อ T < 32°C
- [ ] ประกอบตู้ควบคุม (Control Box) ในกล่อง IP65
- [ ] ติดตั้ง RCBO + ระบบสายดิน
- [ ] ร้อยสายเข้าโรงเรือนผ่านท่อ PVC ระดับ 1.5m
- [ ] ติดตั้งหัวพ่นหมอก + ถังพักน้ำ + ลูกลอย
- [ ] ✅ ทดสอบ End-to-End: เปิดระบบ วัดอุณหภูมิ ปั๊มทำงาน หมอกออก

### Phase 2: IoT & Monitoring (สัปดาห์ 3–4)
**เป้าหมาย:** ดู Dashboard real-time + แจ้งเตือน Line ได้

- [ ] ติดตั้ง Mosquitto MQTT Broker บน PC/Laptop ในออฟฟิศ
- [ ] เพิ่มโค้ด ESP32: publish ข้อมูล sensor ไป MQTT ทุก 30 วินาที
- [ ] เขียน Python Backend (FastAPI):
  - Subscribe MQTT → บันทึกลง SQLite
  - REST API สำหรับ Dashboard
  - Line Notify เมื่อ T > 38°C หรือ sensor ขาด
- [ ] เพิ่ม SD Card logging เป็น offline backup
- [ ] ✅ ทดสอบ: ปล่อยระบบทำงาน 24 ชั่วโมง ข้อมูลเข้า DB ครบ

### Phase 3: Dashboard + Data Export (สัปดาห์ 5–6)
**เป้าหมาย:** หน้า web สวย + ส่งออก Excel ให้ทีมเกษตรได้

- [ ] เขียน Web Dashboard (HTML + Chart.js):
  - กราฟ real-time อุณหภูมิ/ความชื้น
  - สถานะปั๊มและพัดลม (ON/OFF)
  - ปุ่ม manual override (เปิด/ปิดจากระยะไกล)
  - ตาราง log ย้อนหลัง
- [ ] เขียน Python Data Pipeline:
  - ดึงข้อมูลจาก SQLite
  - ทำความสะอาด (กรอง outlier, เติม missing values)
  - Export Excel พร้อมกราฟ pivot (T-max, T-min, T-avg รายวัน)
- [ ] เขียน Wiring Diagram + คู่มือ 1 หน้า
- [ ] ✅ ส่ง Excel ให้น้องเกษตรรัน SPSS ได้ทันที

---

## 💻 โครงสร้างโค้ด (Code Structure)

```
smart-farm/
├── firmware/                    # โค้ดสำหรับ ESP32
│   ├── main.ino (หรือ main.py)
│   ├── sensor.h                 # อ่าน DHT22/SHT31
│   ├── relay_control.h          # ควบคุม relay
│   ├── mqtt_client.h            # ส่งข้อมูลผ่าน MQTT
│   └── sd_logger.h              # บันทึกลง SD Card
│
├── backend/                     # Python Server
│   ├── main.py                  # FastAPI entry point
│   ├── mqtt_subscriber.py       # รับข้อมูลจาก ESP32
│   ├── database.py              # SQLite operations
│   ├── line_notify.py           # ส่งแจ้งเตือน Line
│   └── data_export.py           # Export Excel
│
├── dashboard/                   # Web UI
│   ├── index.html               # หน้า Dashboard หลัก
│   ├── style.css
│   └── app.js                   # Chart.js + WebSocket/polling
│
└── data_pipeline/               # Data Cleansing
    ├── clean_data.py            # กรอง outlier
    └── export_excel.py          # สร้าง Excel สำหรับ SPSS
```

---

## ⚡ Wiring Overview (ESP32)

```
ESP32 Pin    → Device
─────────────────────────────────
GPIO 4       → DHT22 Data (หรือ SDA ถ้าใช้ SHT31 I2C)
GPIO 5       → Relay CH1 (ปั๊มน้ำ 12V)
GPIO 18      → Relay CH2 (พัดลม 220V)
GPIO 19      → SD Card CS (SPI)
GPIO 23      → SD Card MOSI
GPIO 22      → SD Card SCK
GPIO 21      → SD Card MISO
3.3V         → DHT22 VCC + SD Card VCC
GND          → GND ทุกตัว

ตู้ควบคุมภายนอกโรงเรือน:
220V AC → RCBO → ESP32 Power Supply (5V) + Relay CH2 (พัดลม)
          RCBO → Adapter 220V→12V → Relay CH1 → ปั๊ม 12V
```

> ⚠️ **Safety:** ไฟ 220V ทั้งหมดอยู่ในกล่อง IP65 นอกโรงเรือน สายที่เข้าไปในโรงเรือนมีเฉพาะ **DC 12V** (ปั๊ม) และ **สายเซนเซอร์** เท่านั้น

---

## 🐍 ตัวอย่างโค้ดสำคัญ

### ESP32 Firmware Logic (Arduino C++)
```cpp
#define TEMP_ON  35.0   // เปิดปั๊มเมื่ออุณหภูมิถึงค่านี้
#define TEMP_OFF 32.0   // ปิดปั๊มเมื่ออุณหภูมิลงมาถึงค่านี้
#define RELAY_PUMP  5
#define RELAY_FAN   18

void controlMisting(float temp) {
    static bool pumpOn = false;
    if (!pumpOn && temp >= TEMP_ON) {
        digitalWrite(RELAY_PUMP, LOW);  // LOW = ON (relay active low)
        digitalWrite(RELAY_FAN, LOW);
        pumpOn = true;
        Serial.println("🌫️ Misting ON");
    } else if (pumpOn && temp <= TEMP_OFF) {
        digitalWrite(RELAY_PUMP, HIGH);
        digitalWrite(RELAY_FAN, HIGH);
        pumpOn = false;
        Serial.println("✅ Misting OFF");
    }
}
```

### Python Line Notify
```python
import requests

def send_line_alert(message: str, token: str):
    url = "https://notify-api.line.me/api/notify"
    headers = {"Authorization": f"Bearer {token}"}
    data = {"message": f"\n🚨 Smart Farm Alert\n{message}"}
    requests.post(url, headers=headers, data=data)

# ตัวอย่างใช้งาน
send_line_alert("🌡️ อุณหภูมิ 39.2°C เกิน threshold!\nปั๊มทำงานอยู่ กรุณาตรวจสอบ", LINE_TOKEN)
```

### Python Data Export (ส่ง Excel ให้น้องเกษตร)
```python
import pandas as pd
import sqlite3

def export_daily_summary(db_path: str, output_path: str):
    conn = sqlite3.connect(db_path)
    df = pd.read_sql("SELECT * FROM sensor_log ORDER BY timestamp", conn)
    df['timestamp'] = pd.to_datetime(df['timestamp'])
    df['date'] = df['timestamp'].dt.date

    # สรุปรายวัน
    daily = df.groupby('date').agg(
        T_avg=('temperature', 'mean'),
        T_max=('temperature', 'max'),
        T_min=('temperature', 'min'),
        RH_avg=('humidity', 'mean'),
        misting_events=('pump_on', 'sum')
    ).round(2).reset_index()

    with pd.ExcelWriter(output_path, engine='openpyxl') as writer:
        df.to_excel(writer, sheet_name='Raw Data', index=False)
        daily.to_excel(writer, sheet_name='Daily Summary', index=False)

    print(f"✅ Export สำเร็จ: {output_path}")

export_daily_summary('farm.db', 'greenhouse_data_for_SPSS.xlsx')
```

---

## 🎯 สิ่งที่จะได้ใส่ใน Resume

- **IoT Hardware Engineering:** Designed and assembled an ESP32-based greenhouse automation system with temperature-triggered misting control
- **Backend Development:** Built a Python FastAPI server with MQTT integration, SQLite database, and real-time web dashboard
- **Data Engineering:** Developed automated data pipeline for environmental sensor data, producing analysis-ready Excel reports
- **Embedded Systems:** Programmed ESP32 firmware in C++ with WiFi/MQTT connectivity and offline SD Card fallback

---

*จัดทำโดย: Nattakit Prasertsak (IT Intern) | โปรเจกต์: Greenhouse IoT Smart Farm*
