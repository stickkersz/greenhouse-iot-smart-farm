# คู่มืออัปโหลดเฟิร์มแวร์ (Windows + macOS)

**Firmware:** v2.9.7 · **Board:** ESP32 DevKit V1 (30-pin)
**อัปเดต:** 2026-08-31

คู่มือนี้สำหรับ "แฟลชเฟิร์มแวร์ลงบอร์ด ESP32" อย่างเดียว — ไม่ต้องรู้โค้ดก็ทำตามได้
มี 2 ทาง เลือกทางใดทางหนึ่ง:

- **ทาง A — Arduino IDE** (แนะนำถ้าไม่คุ้น command line) → ข้อ 3A
- **ทาง B — arduino-cli** (เร็วกว่า ถ้าเคยใช้ terminal) → ข้อ 3B

---

## ⚠️ อ่านก่อนเริ่ม 3 ข้อ

1. **ลำดับสำคัญ** — ถ้ารุ่นนี้เพิ่ม field ใหม่ใน Firebase ต้อง deploy rules **ก่อน** แฟลช
   v2.9.7 เข้าข่ายนี้ (เพิ่ม `status/fan_channel`, `status/pump_channel`)
   ถ้าแฟลชก่อน rules → บอร์ดทำงานปกติแต่ **dashboard ขึ้นว่า offline ตลอด** (ดูข้อ 6)
   → คนที่ดูแล Firebase รัน `firebase deploy --only database` ให้ก่อน
   *(ถ้ารอบนี้ผู้ส่งบอกว่า deploy rules ไปแล้ว ก็ข้ามได้เลย)*

2. **ยืนยันก่อนแฟลชว่ากำลังจะทับของอะไร** — บอร์ดนี้คุมพัดลม/ปั๊มของโรงเรือนจริง
   ระหว่างแฟลช (~1-2 นาที) รีเลย์จะถูกรีเซ็ต = พัดลม/ปั๊มดับชั่วคราว
   **อย่าแฟลชตอนบ่ายที่อากาศร้อนจัด** ถ้าเลี่ยงได้ · แฟลชเสร็จ auto control กลับมาเองใน ~30 วิ

3. **ต้องมีไฟล์ `config.h`** ถึงจะ build ผ่าน (มี WiFi/Firebase ของจริง)
   ในไฟล์ ZIP ที่ได้รับมี `config.h` มาให้แล้ว — **ห้ามอัปโหลดขึ้นที่สาธารณะ** (มีรหัส WiFi)

---

## 1. ต่อสาย + ติดตั้งไดรเวอร์ USB

ต่อ ESP32 เข้าคอมด้วยสาย USB ที่ **ส่งข้อมูลได้** (สายชาร์จบางเส้นมีแต่ไฟ ไม่มีข้อมูล — ถ้าคอมไม่เห็นพอร์ตเลย ให้เปลี่ยนสายก่อนอย่างอื่น)

บอร์ด DevKit V1 ส่วนใหญ่ใช้ชิป **CP2102** บางล็อตเป็น **CH340**

| ระบบ | ต้องลงไดรเวอร์ไหม | ลิงก์ |
|---|---|---|
| **Windows 10/11** | ต้องลง | CP210x: silabs.com → "CP210x VCP Drivers" · CH340: wch.cn → "CH341SER" |
| **macOS 11+** | ปกติ**ไม่ต้อง** (มีมาให้แล้ว) | ถ้าไม่เห็นพอร์ต ค่อยลง CP210x VCP ของ Silicon Labs |

**เช็คว่าคอมเห็นบอร์ดหรือยัง**

- **Windows:** Device Manager → Ports (COM & LPT) → ต้องเห็น `Silicon Labs CP210x (COM3)` หรือ `USB-SERIAL CH340 (COM3)`
  *(เลข COM เป็นเท่าไหร่ก็ได้ จำไว้ใช้ตอนแฟลช)*
- **macOS:** เปิด Terminal พิมพ์ `ls /dev/cu.*` → ต้องเห็น `/dev/cu.usbserial-XXXX` หรือ `/dev/cu.SLAB_USBtoUART`

> ❌ **ไม่เห็นพอร์ต** = ยังแฟลชไม่ได้ อย่าเพิ่งไปขั้นต่อไป → ดูข้อ 6

---

## 2. แตกไฟล์ ZIP

แตกไว้ที่ไหนก็ได้ที่ path **ไม่มีภาษาไทยและไม่มีเว้นวรรค** (ทั้ง Arduino IDE และ arduino-cli
เคยมีปัญหากับ path ที่มีอักขระพิเศษ)

- ✅ `C:\smartfarm\` หรือ `~/smartfarm/`
- ❌ `C:\Users\สมชาย\Desktop\ของใหม่ (1)\`

ข้างในจะมีโครงแบบนี้:

```
smartfarm-firmware-v2.9.7/
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   ← ไฟล์หลัก เปิดไฟล์นี้
│   ├── auto_control_logic.h
│   ├── config.h                 ← มีรหัส WiFi จริง (ห้ามแชร์ออกนอกบริษัท)
│   ├── config.h.example
│   └── sketch.yaml
├── UPLOAD_GUIDE.md              ← ไฟล์นี้
└── PINOUT.md                    ← ผังขา + วิธีสลับช่องรีเลย์สำรอง
```

> ⚠️ **โฟลเดอร์ `smartfarm_firmware` กับไฟล์ `smartfarm_firmware.ino` ต้องชื่อตรงกัน** — เป็นกฎของ Arduino
> ถ้าเปลี่ยนชื่อโฟลเดอร์ จะเปิดไม่ขึ้น

---

## 3A. ทาง A — Arduino IDE

### ติดตั้ง (ครั้งแรกครั้งเดียว)

1. โหลด **Arduino IDE 2.x** จาก arduino.cc/en/software
2. เปิด IDE → **File → Preferences** → ช่อง *Additional boards manager URLs* ใส่:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
3. **Tools → Board → Boards Manager** → ค้น `esp32` → ติดตั้ง **esp32 by Espressif Systems** เวอร์ชัน **3.3.10**
   *(เวอร์ชันอื่นอาจ build ได้แต่ไม่ตรงกับที่ทดสอบไว้ — ถ้าเลือกได้ให้ใช้ 3.3.10)*
4. ติดตั้ง library ทั้ง 6 ตัว — **Tools → Manage Libraries** ค้นชื่อแล้วเลือกเวอร์ชันให้ตรง:

   | Library | เวอร์ชัน |
   |---|---|
   | Firebase ESP32 Client (โดย Mobizt) | 4.4.17 |
   | OneWire | 2.3.8 |
   | DallasTemperature | 4.0.6 |
   | Adafruit SHT31 Library | 2.2.2 |
   | Adafruit BusIO | 1.17.4 |
   | LiquidCrystal I2C (โดย Frank de Brabander) | 1.1.2 |

### ตั้งค่าบอร์ด (สำคัญมาก)

เปิด `smartfarm_firmware/smartfarm_firmware.ino` แล้วตั้งที่เมนู **Tools**:

| หัวข้อ | ต้องเลือก |
|---|---|
| Board | **ESP32 Dev Module** |
| **Partition Scheme** | **Huge APP (3MB No OTA/1MB SPIFFS)** ← **ถ้าไม่ตั้ง จะ build ไม่ผ่าน** |
| Upload Speed | 921600 *(ถ้าแฟลชหลุดบ่อย ลดเป็น 115200)* |
| Port | Windows: `COM3` · macOS: `/dev/cu.usbserial-XXXX` |

> **Partition Scheme คือข้อที่พลาดกันบ่อยที่สุด** — เฟิร์มแวร์ตัวนี้ใหญ่เกิน partition แบบ default
> อาการถ้าลืม: `Sketch too big` / `text section exceeds available space`

### แฟลช

กดปุ่ม **→ (Upload)** แล้วรอ ~1-2 นาที

- ถ้าค้างที่ `Connecting........_____` → **กดปุ่ม BOOT บนบอร์ดค้างไว้** จนเห็น `Writing at 0x...` แล้วปล่อย
- เสร็จแล้วจะขึ้น `Hard resetting via RTS pin...`

---

## 3B. ทาง B — arduino-cli

### ติดตั้ง (ครั้งแรกครั้งเดียว)

```bash
# macOS
brew install arduino-cli
```

**Windows:** โหลดไฟล์ `.zip` ของ Windows จาก <https://arduino.github.io/arduino-cli/latest/installation/>
แล้วแตกไฟล์ `arduino-cli.exe` ไว้ในโฟลเดอร์เดียวกับที่แตก ZIP ของโปรเจกต์
(หรือเพิ่ม path นั้นเข้า PATH ถ้าอยากเรียกจากที่ไหนก็ได้) แล้วรันคำสั่งข้างล่างใน PowerShell
โดยเติม `.\` ข้างหน้า เช่น `.\arduino-cli board list`

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.10
```

### แฟลช

`sketch.yaml` ล็อกเวอร์ชัน core + library ไว้ให้แล้ว — `arduino-cli` จะดึงเองอัตโนมัติ
ไม่ต้องลง library ทีละตัวเหมือนทาง A

```bash
cd smartfarm-firmware-v2.9.7      # โฟลเดอร์ที่แตก ZIP ไว้

arduino-cli board list            # หาพอร์ต
# Windows ได้ COM3 · macOS ได้ /dev/cu.usbserial-XXXX

arduino-cli compile smartfarm_firmware        # ต้องขึ้น ~41% flash
arduino-cli upload -p COM3 smartfarm_firmware                  # Windows
arduino-cli upload -p /dev/cu.usbserial-XXXX smartfarm_firmware # macOS
```

---

## 4. ตรวจว่าแฟลชสำเร็จ (Serial Monitor)

เปิด Serial Monitor ตั้ง **115200 baud**
- IDE: Tools → Serial Monitor (มุมขวาล่างเลือก 115200)
- CLI: `arduino-cli monitor -p <พอร์ต> -c baudrate=115200`

**ต้องเห็นครบ 3 บรรทัดนี้:**

```
=== Greenhouse IoT Smart Farm v2.9.7 ===
...
WiFi OK — SSID: ASUS_2.4G_LAB2  IP: 192.168.x.x
Firebase Auth OK (anonymous) — Firebase Ready
```

| เห็นอะไร | แปลว่า |
|---|---|
| `v2.9.7` | ✅ แฟลชรุ่นถูกแล้ว — ถ้าขึ้นเลขอื่น = แฟลชไม่ติด ของเก่ายังอยู่ |
| `WiFi OK` | ✅ ต่อ WiFi ได้ |
| `WiFi FAILED` | ❌ SSID/รหัสผิด หรือ AP ไม่อยู่ในระยะ → ดูข้อ 6 |
| `[AUTO] พัดลมเปิด/ปิด ...` | ✅ auto control ทำงาน (จะขึ้นทุก 30 วิเมื่อมีการเปลี่ยนสถานะ) |

จากนั้นเปิด dashboard เช็คว่าบอร์ดขึ้น **online** ภายใน ~30 วินาที

---

## 5. ✅ เช็กลิสต์หลังแฟลช

- [ ] Serial banner = `v2.9.7`
- [ ] `WiFi OK` + มี IP
- [ ] `Firebase Auth OK`
- [ ] Dashboard ขึ้น online (ไม่เกิน 30 วิ)
- [ ] จอ LCD ขึ้น `SmartFarm v2.9.7` ตอนบูต
- [ ] ค่าอุณหภูมิ/ความชื้นบน dashboard ขยับ (ไม่ค้างที่ 0)
- [ ] ผ่านไป 2-3 ชม. เช็ค `boot_count` ไม่พุ่ง และ `last_reset_reason` **ไม่ใช่** `TASK_WDT`

---

## 6. ปัญหาที่เจอบ่อย

| อาการ | สาเหตุ | วิธีแก้ |
|---|---|---|
| คอมไม่เห็นพอร์ตเลย | สาย USB ไม่มีเส้นข้อมูล / ไม่ได้ลงไดรเวอร์ | เปลี่ยนสายก่อน แล้วค่อยลงไดรเวอร์ (ข้อ 1) |
| `Sketch too big` / `text section exceeds` | ลืมตั้ง Partition Scheme | Tools → Partition Scheme → **Huge APP** |
| `config.h: No such file` | ไม่มี config.h | ก๊อป `config.h.example` เป็น `config.h` แล้วใส่ค่าจริง (ปกติ ZIP มีให้แล้ว) |
| ค้างที่ `Connecting......____` | บอร์ดไม่เข้าโหมด flash | กดปุ่ม **BOOT** ค้างไว้ตอนที่ขึ้น Connecting แล้วปล่อยเมื่อเริ่ม Writing |
| แฟลชหลุดกลางคัน | Upload Speed สูงไป / สายยาว | ลด Upload Speed เป็น 115200 |
| `Serial port busy` / `Access denied` | มีโปรแกรมอื่นเปิดพอร์ตค้าง | ปิด Serial Monitor / โปรแกรมอื่นที่เปิดพอร์ตนั้น แล้วลองใหม่ |
| Serial ขึ้นภาษาต่างดาว | baud rate ผิด | ตั้งเป็น **115200** |
| `WiFi FAILED` | SSID/รหัสผิด, หรือ AP เป็น 5GHz | ESP32 ต่อได้เฉพาะ **2.4GHz** · เช็ค `WIFI_SSID`/`WIFI_PASS` ใน config.h |
| **Serial ขึ้น `WiFi OK` แต่ dashboard บอก offline** | แฟลชก่อน deploy rules (ดูคำเตือนข้อ 1) | ให้คนดูแล Firebase รัน `firebase deploy --only database` · บอร์ดกลับมาเองในรอบ push ถัดไป **ไม่ต้องแฟลชซ้ำ** |
| บอร์ด reboot วนไม่หยุด | ไฟไม่พอ / brownout | ใช้ไฟเลี้ยงที่จ่ายกระแสพอ อย่าจ่ายผ่านพอร์ต USB คอมอย่างเดียวตอนรีเลย์ทำงาน |

---

## 7. ถ้าต้องย้อนกลับรุ่นเก่า (rollback)

เก็บ ZIP รุ่นเก่าไว้เสมอ · การย้อนกลับ = แฟลช ZIP รุ่นก่อนหน้าทับด้วยขั้นตอนเดียวกันทุกอย่าง

> ⚠️ ถ้าย้อนกลับไปรุ่น **ก่อน v2.9.5** ให้กลับไปใช้ช่องหลัก (พัดลม CH3 / ปั๊ม CH4) ก่อน
> เพราะรุ่นเก่าไม่รู้จักการสลับช่องสำรอง — ถ้าสายยังอยู่ที่ CH1/CH2 อุปกรณ์จะไม่ทำงาน

---

## ต้องการความช่วยเหลือ

แนบ 3 อย่างนี้ไปด้วยจะแก้ได้เร็วที่สุด:
1. ข้อความ Serial Monitor ตั้งแต่บรรทัด `=== Greenhouse IoT Smart Farm ... ===` ลงมา ~20 บรรทัด
2. ค่า `boot_count` / `last_reset_reason` / `wifi_drop_count` จาก dashboard
3. ระบบที่ใช้ (Windows / macOS) + วิธีที่ใช้ (Arduino IDE / arduino-cli)
