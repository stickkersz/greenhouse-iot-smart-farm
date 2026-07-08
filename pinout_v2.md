# Pinout — Greenhouse IoT Smart Farm

**Board:** ESP32 DevKit V1 (30-pin) บน Expansion Board HW-777
**Firmware:** v1.4.0 (`smartfarm_firmware.ino` + `config.h`)
**อัปเดตล่าสุด:** 2026-07-08 — อ้างอิงจาก config.h/ino จริง (ไม่ใช่เอกสารเก่า), sync กับ PROJECT_INSTRUCTION.md/PROJECT_MEMORY.md แล้ว

> ✅ DHT22 อยู่ที่ **GPIO18** ตั้งแต่ 2026-07-02 (ย้ายจาก GPIO32 ให้ไกลจากกลุ่ม relay กัน noise) — PROJECT_INSTRUCTION.md และ PROJECT_MEMORY.md แก้ให้ตรงกันแล้ว (2026-07-07)
>
> ✅ **LCD I2C ทำงานปกติแล้ว (2026-07-08)** — สาเหตุที่ไม่ขึ้นจอก่อนหน้านี้คือจอตัวเดิมเสีย ไม่ใช่ปัญหาสาย/address เปลี่ยนจอใหม่ต่อสายเดิมทุกเส้นแล้วติดทันที
>
> 🔎 **หลักฐานใหม่ (2026-07-08):** ทดสอบเทียบสด DS18B20 (จัมป์ลง breadboard ไกลจากกลุ่ม relay) กับ DHT22
> (ต่อตรงบน Expansion Board ใกล้กลุ่ม relay) ระหว่างเปิด/ปิดปั๊ม-พัดลม — DS18B20 อ่านค่าปกติ แต่ DHT22
> ยังอ่านค่าไม่ได้ ทั้งที่ไฟเลี้ยงบอร์ดปกติดี ยืนยันว่า **ระยะห่างจากกลุ่ม relay สำคัญกว่าคุณภาพจุดต่อ**
> (breadboard vs solder) — แผนต่อไปคือย้ายสาย/ตัว DHT22 ออกห่างจากกลุ่ม relay เพิ่ม ดู PROJECT_MEMORY.md §15

---

## ตารางขา GPIO ทั้งหมด

| GPIO | อุปกรณ์ | หน้าที่ | หมายเหตุ |
|---|---|---|---|
| **GPIO2** | Onboard Status LED | ไฟแสดงสถานะบอร์ด | Active-HIGH, เพิ่งพบใน .ino (ไม่มีในเอกสารเดิม) |
| **GPIO4** | DS18B20 (Waterproof) | วัดอุณหภูมิน้ำ | ต้องมี Pull-up 4.7kΩ ระหว่าง VCC–DATA ที่ฝั่ง ESP32 |
| **GPIO14** | Relay CH3 | พัดลม 220V AC (ดูดอากาศเข้า) | Auto ตามอุณหภูมิ (TEMP_ON/OFF) |
| **GPIO18** | DHT22 | วัดอุณหภูมิ/ความชื้นอากาศ | ย้ายจาก GPIO32 (2026-07-02); ตัวแปรในโค้ดยังชื่อ `PIN_DHT11` (ชื่อเก่า ใช้จริงเป็น DHT22) |
| **GPIO21** | LCD I2C — SDA | จอ LCD 16x2 | ต้องใช้ไฟ 5V ไม่ใช่ 3.3V — ✅ ยืนยันทำงานแล้ว (2026-07-08 เปลี่ยนจอตัวใหม่) |
| **GPIO22** | LCD I2C — SCL | จอ LCD 16x2 | Address auto-scan 0x27/0x3F (ไม่ hardcode แล้ว), I2C clock ลดเหลือ 50kHz กัน noise |
| **GPIO25** | Relay CH4 | ปั๊มน้ำ 24V DC | Auto ตามความชื้น (HUMIDITY_MIN); เดิมเคยใช้ GPIO12 แต่ชนกับ strapping pin ทำ boot fail จึงย้ายมา |
| **GPIO26** | Relay CH1 | สำรอง (manual/schedule เท่านั้น) | Key เก่าใน Firebase: `ch1_pump` |
| **GPIO27** | Relay CH2 | ไม่ได้ใช้งาน (ซ่อนใน dashboard) | |
| **GPIO33** | Buzzer Module (Active) | เสียงแจ้งเตือน | Active-LOW (LOW=ดัง, HIGH=เงียบ), 3 ขา GND–I/O–VCC |
| **GPIO34** | ว่าง (ไม่ได้ใช้) | — | ADC1, Input-Only — เคยจองไว้สำหรับ Capacitive Soil Moisture แต่**ยกเลิกแผนถาวรแล้ว (2026-07-07)** ไม่ต้องซื้อเซนเซอร์ |

---

## ขาที่ห้ามใช้

| GPIO | เหตุผล |
|---|---|
| GPIO12 | Strapping Pin — ต่อ Relay แล้ว boot fail (เคยเจอปัญหานี้กับปั๊มน้ำ ก่อนย้ายไป GPIO25) |

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
- Pump Safety: เดินต่อเนื่องได้สูงสุด 10 นาที (ตัดสินใจสุดท้าย 2026-07-07, เดิม 5 นาที) แล้วพัก 5 นาที (ป้องกันปั๊มไหม้)

| Channel | GPIO | อุปกรณ์ | โหมด |
|---|---|---|---|
| CH1 | GPIO26 | สำรอง | Manual only |
| CH2 | GPIO27 | ไม่ใช้ | ซ่อนใน dashboard |
| CH3 | GPIO14 | พัดลม 220V AC | Auto (อุณหภูมิ) |
| CH4 | GPIO25 | ปั๊มน้ำ 24V DC | Auto (ความชื้น) |

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
- `pinout.docx` / `pinout.pdf` — เอกสารเดิม (มีอยู่แล้วในโฟลเดอร์ แต่ยังอ้าง DHT22=GPIO32 ซึ่งล้าสมัย)
