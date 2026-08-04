# Pinout — Greenhouse IoT Smart Farm

**Board:** ESP32 DevKit V1 (30-pin) บน Expansion Board HW-777
**Firmware:** v2.9.2 (`smartfarm_firmware.ino` + `config.h`)
**อัปเดตล่าสุด:** 2026-08-03 — อ้างอิงจาก config.h/ino จริง (ไม่ใช่เอกสารเก่า) · เอกสาร `pinout.docx`/`pinout.pdf` เก่าถูกลบทิ้งแล้ว ไฟล์นี้คือฉบับเดียวที่ใช้อ้างอิง

> ⚠️ **ปั๊มน้ำย้ายจาก GPIO25 → GPIO27 แล้ว (2026-08-03)** — ปั๊มมีปัญหา สลับไปใช้ช่อง CH2 (เดิมว่าง) แทน
> CH4 เดิม · เป็นการสลับเลข GPIO ระหว่าง CH2/CH4 เท่านั้น ไม่กระทบ Firebase key/logic ใดๆ (`ch4_spare`
> ยังคือปั๊มเหมือนเดิม, `IDX_PUMP` ไม่เปลี่ยน) เพราะ firmware เรียกผ่าน `PIN_RELAY_CH4` เป็นสัญลักษณ์เสมอ
> ไม่มีที่ไหน hardcode เลข GPIO ตรงๆ · **ต้องย้ายสายปั๊มทางกายภาพจากช่อง CH4 ไปเสียบช่อง CH2 บนบอร์ด
> relay ด้วย** ไม่ใช่แค่แก้โค้ดแล้วจบ — ถ้าลืมย้ายสาย ปั๊มจะไม่ทำงานเพราะเปิดผิดช่อง

> ✅ **เปลี่ยนเซนเซอร์อากาศจาก DHT22 (GPIO18) → SHT35 (I2C) แล้ว (2026-07-11)** — GPIO18 ว่างแล้ว, SHT35
> ใช้บัส I2C ร่วมกับ LCD (GPIO21/22) แทน address auto-detect 0x44/0x45 — ดูตารางด้านล่าง + PROJECT_MEMORY.md §15
> สำหรับเหตุผลและ TODO (ควรย้ายตำแหน่ง SHT35 ให้ไกลกลุ่ม relay เหมือน DS18B20 ด้วย ไม่ใช่แค่เปลี่ยนชิป)
>
> ✅ **LCD I2C ทำงานปกติแล้ว (2026-07-08)** — สาเหตุที่ไม่ขึ้นจอก่อนหน้านี้คือจอตัวเดิมเสีย ไม่ใช่ปัญหาสาย/address เปลี่ยนจอใหม่ต่อสายเดิมทุกเส้นแล้วติดทันที
>
> 🔎 **หลักฐานที่นำไปสู่การเปลี่ยนเซนเซอร์ (2026-07-08):** ทดสอบเทียบสด DS18B20 (จัมป์ลง breadboard ไกล
> จากกลุ่ม relay) กับ DHT22 (ต่อตรงบน Expansion Board ใกล้กลุ่ม relay) ระหว่างเปิด/ปิดปั๊ม-พัดลม — DS18B20
> อ่านค่าปกติ แต่ DHT22 ยังอ่านค่าไม่ได้ ทั้งที่ไฟเลี้ยงบอร์ดปกติดี ยืนยันว่า **ระยะห่างจากกลุ่ม relay
> สำคัญกว่าคุณภาพจุดต่อ (breadboard vs solder) หรือโปรโตคอลที่ใช้** — ข้อนี้สำคัญมากสำหรับ SHT35 ด้วย

---

## ตารางขา GPIO ทั้งหมด

| GPIO | อุปกรณ์ | หน้าที่ | หมายเหตุ |
|---|---|---|---|
| **GPIO2** | Onboard Status LED | ไฟแสดงสถานะบอร์ด | Active-HIGH, เพิ่งพบใน .ino (ไม่มีในเอกสารเดิม) |
| **GPIO4** | DS18B20 (Waterproof) | วัดอุณหภูมิน้ำ | ต้องมี Pull-up 4.7kΩ ระหว่าง DATA กับ **3V3 เท่านั้น** — ⚠️ ห้ามต่อไป VCC/VIN (5V): pull-up ไป 5V จะดัน GPIO4 เกิน abs max 3.6V ผ่าน ESD diode clamp ของขา (ดูคอมเมนต์ .ino:214-215) |
| **GPIO14** | Relay CH3 | พัดลม 220V AC (ดูดอากาศเข้า) | Auto ตามอุณหภูมิ (TEMP_ON/OFF) |
| **GPIO18** | ว่าง (ไม่ได้ใช้) | — | เคยเป็น DHT22 DATA — ถอดออกแล้ว 2026-07-11 (เปลี่ยนไปใช้ SHT35 แบบ I2C แทน ไม่มี pin แยก) |
| **GPIO21** | I2C — SDA (ร่วม LCD + SHT35) | จอ LCD 16x2 + เซนเซอร์อากาศ SHT35 | LCD ต้องใช้ไฟ 5V ไม่ใช่ 3.3V — ✅ ยืนยันทำงานแล้ว (2026-07-08 เปลี่ยนจอตัวใหม่) |
| **GPIO22** | I2C — SCL (ร่วม LCD + SHT35) | จอ LCD 16x2 + เซนเซอร์อากาศ SHT35 | Address auto-scan: LCD 0x27/0x3F, SHT35 0x44/0x45 (ไม่ hardcode), I2C clock ลดเหลือ 50kHz กัน noise |
| **GPIO25** | Relay CH2 | ไม่ได้ใช้งาน (ซ่อนใน dashboard) | สลับมาจาก GPIO27 เมื่อ 2026-08-03 (สลับกับ CH4) |
| **GPIO26** | Relay CH1 | สำรอง (manual/schedule เท่านั้น) | Key เก่าใน Firebase: `ch1_pump` |
| **GPIO27** | Relay CH4 | ปั๊มน้ำ 24V DC | Auto ตามความชื้น (HUMIDITY_MIN); ย้ายมาจาก GPIO25 เมื่อ 2026-08-03 (ปั๊มมีปัญหา) — ก่อนหน้านั้นเคยอยู่ GPIO12 แต่ชนกับ strapping pin ทำ boot fail |
| **GPIO33** | Buzzer Module (Active) | เสียงแจ้งเตือน | Active-LOW (LOW=ดัง, HIGH=เงียบ), 3 ขา GND–I/O–VCC |
| **GPIO34** | ว่าง (ไม่ได้ใช้) | — | ADC1, Input-Only — เคยจองไว้สำหรับ Capacitive Soil Moisture แต่**ยกเลิกแผนถาวรแล้ว (2026-07-07)** ไม่ต้องซื้อเซนเซอร์ |

---

## ขาที่ห้ามใช้

| GPIO | เหตุผล |
|---|---|
| GPIO12 | Strapping Pin — ต่อ Relay แล้ว boot fail (เคยเจอปัญหานี้กับปั๊มน้ำ ก่อนย้ายไป GPIO25 แล้วสลับไป GPIO27 อีกรอบ) |

---

## ระบบจ่ายไฟ

```
220V AC → S-120-12 PSU (12V/10A) ─┬─ XL4015 Step-Down → 5V → Expansion Board (ESP32 + Logic ทั้งหมด, LCD)
                                   └─ Boost Converter (XL6009) → 24V → Relay CH4 → ปั๊มน้ำ
```

**กฎความปลอดภัย:** ปิดไฟก่อนต่อ/ถอดสาย Boost Converter และปั๊มเสมอ — เคย Short ขณะมีไฟจน Boost Converter พังมาแล้ว

---

## Relay Logic

- ทุกช่องเป็น **Active-LOW**: LOW = เปิด, HIGH = ปิด
- ใช้ขา **NO (Normally Open)** เสมอ
- Max Runtime: ปั๊มและพัดลมเดินต่อเนื่องได้สูงสุด **15 นาที** แล้วพักคู่กัน **5 นาที** (`PUMP_MAX_RUNTIME_MS`/`FAN_MAX_RUNTIME_MS` — ยืดจาก 10 นาทีเป็น 15 เมื่อ 2026-07-20, พักคู่กันตั้งแต่ v2.8.0) เป็น `#define` ต้อง reflash ถึงจะเปลี่ยน ไม่ใช่ threshold ที่ปรับสดจาก dashboard ได้

| Channel | GPIO | อุปกรณ์ | โหมด |
|---|---|---|---|
| CH1 | GPIO26 | สำรอง | Manual only |
| CH2 | GPIO25 | ไม่ใช้ | ซ่อนใน dashboard — สลับมาจาก GPIO27 (2026-08-03) |
| CH3 | GPIO14 | พัดลม 220V AC | Auto (อุณหภูมิ) |
| CH4 | GPIO27 | ปั๊มน้ำ 24V DC | Auto (ความชื้น) — ย้ายมาจาก GPIO25 (2026-08-03, ปั๊มมีปัญหา) |

---

## DS18B20 — สายไฟ (Flat side หันหาตัวเอง)

| ตำแหน่ง | สาย |
|---|---|
| ซ้าย | GND |
| กลาง | DATA (GPIO4 + Pull-up 4.7kΩ ไป VCC) |
| ขวา | VCC |

> ต่อผิดด้านจะอ่านค่า -127°C — เคยเจอปัญหานี้มาก่อน

---

## รายการอ้างอิง

- `smartfarm_firmware/config.h` — ค่า pin mapping และ threshold ทั้งหมด
- `smartfarm_firmware/smartfarm_firmware.ino` — logic การอ่าน/ควบคุมจริง
