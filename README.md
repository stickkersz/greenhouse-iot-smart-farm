# 🌿 Greenhouse IoT Smart Farm

ระบบควบคุมโรงเรือนอัตโนมัติด้วย ESP32 + Firebase — พัดลมและปั๊มพ่นหมอกทำงานเองตามอุณหภูมิ/ความชื้น พร้อม dashboard สั่งงานและดูข้อมูลย้อนหลังแบบ real-time.

**บริษัท ปุ๋ยไวกิ้ง จำกัด** · Firmware **v2.9.2** · ESP32 DevKit V1

---

## Table of Contents

- [What it does](#what-it-does)
- [Architecture](#architecture)
- [Hardware](#hardware)
- [Auto-control logic](#auto-control-logic)
- [Repository layout](#repository-layout)
- [Setup — firmware](#setup--firmware)
- [Setup — Firebase & dashboard](#setup--firebase--dashboard)
- [Configuration reference](#configuration-reference)
- [Firebase data contract](#firebase-data-contract)
- [Testing](#testing)
- [Deployment checklist](#deployment-checklist)
- [Troubleshooting](#troubleshooting)
- [Design notes & history](#design-notes--history)

---

## What it does

An ESP32 reads air temperature/humidity (SHT35) and water temperature (DS18B20), then drives a **fan** and a **water pump** to keep a greenhouse in range. It works **fully offline** — the control loop reads sensors and thresholds from RAM/NVS, so a Wi-Fi or Firebase outage does not stop climate control. When online, it pushes live data to Firebase every 30 s and polls remote commands every 1.5 s.

A browser dashboard (Firebase Hosting) shows live readings, historical charts, and lets staff switch each channel between **AUTO / MANUAL / SCHEDULE**, tune thresholds, and load seasonal presets.

---

## Architecture

```
┌────────────┐   30s push    ┌──────────────────────┐   live    ┌─────────────┐
│  ESP32     │ ────────────▶ │ Firebase Realtime DB │ ────────▶ │  Dashboard  │
│  firmware  │ ◀──────────── │  (asia-southeast1)   │ ◀──────── │  (browser)  │
└────────────┘  1.5s poll    └──────────────────────┘  commands └─────────────┘
     │                                  ▲
     │ reads                            │ rules validate every write
     ▼                          database.rules.json
 SHT35 / DS18B20
 relays (fan/pump)
```

Three independently deployable layers:

| Layer | Source | Deployed by | Runs on |
|---|---|---|---|
| **Firmware** | `smartfarm_firmware/` | `arduino-cli upload` (flash) | ESP32 |
| **Rules** | `database.rules.json` | `firebase deploy --only database` | Firebase cloud |
| **Dashboard** | `dashboard/` | `firebase deploy --only hosting` | Browser via Firebase Hosting |

> ⚠️ A firmware behavior change only takes effect after **flashing** the ESP32. Rules and dashboard only govern what can be *saved* — they do not change what a running board does with its current config.

---

## Hardware

**Board:** ESP32 DevKit V1

### Sensors

| Sensor | Interface | Measures | Role |
|---|---|---|---|
| **SHT35** | I2C `0x44`/`0x45` (SDA=GPIO21, SCL=GPIO22) | air temp + humidity | drives all auto control |
| **DS18B20** (waterproof) | 1-Wire GPIO4 (needs 4.7 kΩ pull-up) | water temp | **alert/display only** — does not drive relays |
| **LCD 16×2** | I2C `0x27`/`0x3F` (shared bus) | — | local status display |

> I2C addresses are auto-detected at boot by a bus scanner. SHT35 address depends on its ADDR pin.

### Relays (active-LOW: LOW = ON)

| Channel | GPIO | Load | Controlled by |
|---|---|---|---|
| **CH1** | 26 | สำรอง (spare) | manual / schedule only — no auto |
| **CH2** | 27 | ไม่ได้ใช้ (unused) | hidden in dashboard, forced OFF |
| **CH3** | 14 | พัดลม 220 V AC (ดูดเข้า) | **temperature + humidity** (auto) |
| **CH4** | 25 | ปั๊มน้ำ 24 V DC | **humidity + pump safety** (auto) |

### Other pins

| Pin | GPIO | Note |
|---|---|---|
| Buzzer | 33 | 3-pin module, active-LOW |
| Status LED | 2 | onboard |

> CH4 pump moved from GPIO12 → GPIO25 (GPIO12 is a strapping pin that caused boot failure). Do not move it back.

---

## Auto-control logic

Pure decision math lives in **`smartfarm_firmware/auto_control_logic.h`** (`computeAutoDecisions()`), separated from Arduino I/O so it can be unit-tested with plain `g++`.

Four hysteresis **latches** are computed from air sensor readings:

| Latch | ON when | OFF when | Drives |
|---|---|---|---|
| **hot** | temp ≥ `temp_on` | temp ≤ `temp_off` | fan |
| **dry** | humidity < `humidity_min` | humidity ≥ `humidity_max` | fan + pump |
| **pumpHeat** | temp ≥ `temp_on` | temp ≤ `temp_off` **or** humidity ≥ `humidity_max` (humid gate) | pump only |
| **wet** (vent) | humidity ≥ `humidity_vent` | humidity ≤ `humidity_vent − VENT_HYST` | fan only |

**Outputs:**

```
fanOn  = hot OR dry OR wet
pumpOn = (hot AND pumpHeat) OR dry
```

**Key invariants (all covered by the test sweep):**

- `pumpOn → fanOn` — the pump never sprays with the fan off (would flood, not evaporate).
- Pump is blocked while air is saturated (`humidity ≥ humidity_max`) — spraying into saturated air just floods.
- **`humidity_vent ≥ humidity_max + VENT_HYST`** is *required*. Otherwise there is a humidity band where the fan vents moisture out while the pump sprays it back in. This is enforced in firmware, rules, and dashboard.
- Sensor unreliable (`sensor_ok = false`, or humidity ≤ 0) → **everything off, all latches cleared**. SHT35 gives temp + humidity from one chip, so a failure means both readings are stale.

**Why latches instead of stateless comparisons:** two hysteresis loops OR'd statelessly either stick open or chatter at the deadband edge. Pump relay switching is the confirmed cause of this project's air-sensor latch-up, so the pump must never chatter. See the header comments and `docs/PROJECT_MEMORY.md`.

### Default thresholds (`config.h`)

| Threshold | Default | Meaning |
|---|---|---|
| `temp_on` | 35 °C | fan/pump engage |
| `temp_off` | 32 °C | fan/pump release |
| `humidity_min` | 60 % | pump engages (too dry) |
| `humidity_max` | 75 % | pump releases (saturated) |
| `humidity_vent` | 80 % | fan vents excess humidity (must be ≥ `humidity_max` + 5) |
| `VENT_HYST` | 5 % | vent latch deadband (single source of truth in `auto_control_logic.h`) |

### Seasonal presets (dashboard)

| Preset | temp_on/off | hum_min/max | vent |
|---|---|---|---|
| ☀️ ฤดูร้อน (summer) | 33 / 30 | 70 / 85 | 90 |
| 🌤️ ปกติ (normal) | 35 / 32 | 60 / 75 | 80 |
| 🌧️ ฤดูฝน (rainy) | 38 / 35 | 50 / 65 | 85 |

All three sit exactly on the `vent = humidity_max + VENT_HYST` boundary — which is why the constraint is `≥`, not `>`.

---

## Repository layout

```
.
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino   # main firmware (setup/loop, I/O, Firebase, LCD)
│   ├── auto_control_logic.h     # pure decision math (unit-tested)
│   ├── config.h                 # secrets + pins + thresholds (GITIGNORED)
│   ├── config.h.example         # template — copy to config.h
│   └── sketch.yaml              # arduino-cli build profile (fqbn, libs, versions)
├── dashboard/
│   ├── index.html               # single-file dashboard (Firebase compat SDK + Chart.js via CDN)
│   ├── sw.js, manifest.json     # PWA
│   └── icon.svg
├── database.rules.json          # Firebase RTDB security + validation rules
├── tests/
│   ├── auto_control_logic.test.cpp   # g++ unit tests for the decision logic
│   └── database.rules.test.js        # emulator tests for the rules
├── firebase.json                # hosting + database + emulator config
├── .firebaserc                  # project alias
├── package.json                 # test scripts (no build step)
├── docs/                        # project docs + archive/ (superseded) + reports/ (deliverables)
└── media/                       # site photos, board photos (not referenced by code)
```

Reference docs worth reading: `docs/Firebase_Database_Structure.md`, `docs/pinout_v2.md`, `docs/PROJECT_INSTRUCTION.md`, `docs/DAILY_OPERATIONS_CHECKLIST.md`, `docs/PROJECT_MEMORY.md`. Taking over the project? Start with `docs/HANDOFF_GUIDE.md`.

---

## Setup — firmware

**Prerequisites:** [`arduino-cli`](https://arduino.github.io/arduino-cli/) with the `esp32:esp32` core (3.3.10).

1. **Create `config.h`** (required — build fails without it; it's gitignored because it holds secrets):

   ```bash
   cp smartfarm_firmware/config.h.example smartfarm_firmware/config.h
   ```

   Then edit `config.h`: set Wi-Fi SSID/password(s), `FIREBASE_HOST`, `FIREBASE_API_KEY`. Pins and thresholds already have working defaults.

2. **Compile** (the `sketch.yaml` profile pins the board, partition, and library versions):

   ```bash
   arduino-cli compile smartfarm_firmware
   ```

   > ⚠️ `PartitionScheme=huge_app` is **required** — the firmware is ~1.4 MB, over the "default" partition's 1.31 MB limit. It's baked into `sketch.yaml`. In Arduino IDE: Tools → Partition Scheme → "Huge APP (3MB No OTA/1MB SPIFFS)".

3. **Find the port and upload:**

   ```bash
   arduino-cli board list
   arduino-cli upload -p /dev/cu.usbserial-XXXX smartfarm_firmware
   ```

4. **Watch serial** (115200 baud) — banner should read `=== Greenhouse IoT Smart Farm v2.9.2 ===`.

---

## Setup — Firebase & dashboard

**Prerequisites:** [`firebase-tools`](https://firebase.google.com/docs/cli), a Firebase project with Realtime Database (region `asia-southeast1`). Project alias is `greenhouse-iot-smart-farm` in `.firebaserc`.

```bash
firebase login
firebase deploy --only database    # push security + validation rules
firebase deploy --only hosting     # push the dashboard
```

The dashboard is a single self-contained `dashboard/index.html` (loads Firebase compat SDK + Chart.js from CDN). Firmware version shown in the header reads live from `status/firmware`.

---

## Configuration reference

Everything device-specific lives in **`smartfarm_firmware/config.h`** (gitignored). Sections:

- **Wi-Fi** — `wifiNetworks[]`, auto-connects to whichever is in range (WiFiMulti).
- **Firebase** — `FIREBASE_HOST`, `FIREBASE_API_KEY`.
- **Pins** — see [Hardware](#hardware). `RELAY_ACTIVE_LOW` / `BUZZER_ACTIVE_LOW` flip polarity if your modules are wired the other way.
- **Thresholds** — boot defaults; overridden live from Firebase `control/thresholds` and persisted to NVS.
- **Timing** — `SENSOR_INTERVAL` 30 s, `LOG_INTERVAL` 1 h.

> **Never commit `config.h`.** It contains Wi-Fi passwords and the Firebase key. Only `config.h.example` (placeholders) is tracked.

---

## Firebase data contract

`database.rules.json` is the source of truth for validation (`"$other": {".validate": false}` rejects any undeclared field silently). Full detail in `docs/Firebase_Database_Structure.md`.

```
smartfarm/
├── sensors/         air_temp, air_humidity, water_temp, uptime_sec   (ESP32 → dashboard, 30s)
├── control/         relays, modes, thresholds, schedule              (dashboard → ESP32, 1.5s poll)
├── status/          sensor_ok, water_ok, firmware, boot diagnostics  (ESP32 → dashboard)
├── alerts/          latest alert                                     (ESP32 → dashboard)
├── alert_history/   alert log                                        (dashboard writes)
└── action_log/      user action log                                  (dashboard writes)
logs/YYYY-MM-DD/HH/  hourly avg/min/max + sample_count                (ESP32 → dashboard, hourly)
```

> `logs/` is at the **root**, not under `smartfarm/`.

Auth model: ESP32 uses **anonymous auth** (can write sensors/logs/status/alerts, cannot write thresholds). Dashboard users are **email-authenticated** (can write control + read everything). Editing a field requires changing firmware **and** `database.rules.json` **and** adding a test in `tests/database.rules.test.js`.

---

## Testing

No build step — tests match the project's vanilla approach.

```bash
npm test              # runs all three suites
npm run test:sync     # cross-layer constant check (no emulator, no compiler — instant)
npm run test:rules    # Firebase rules via emulator (requires firebase-tools + Java)
npm run test:logic    # auto-control logic via g++ (C++17)
```

- **`test:sync`** asserts `VENT_HYST` is identical in all three layers that need it, and that every dashboard preset still satisfies the vent constraint — so a tuned constant can't leave firmware and rules disagreeing silently (rules rejecting a value firmware accepts shows up as "the dashboard won't save" with no stated reason). Reads files only; needs neither Java nor a compiler.
- **`test:logic`** compiles `tests/auto_control_logic.test.cpp` against `auto_control_logic.h` and runs crossing/hysteresis/sensor-fail cases plus exhaustive invariant sweeps. Because the vent clamp lives *inside* the pure function, the sweep exercises the real shipped code (mutating `VENT_HYST` makes tests fail). Also covers the safety timers (max runtime, shared cooldown, `millis()` rollover).
- **`test:rules`** spins up the RTDB emulator (port 9000) and asserts every accept/reject path, including the vent/max/min cross-field constraints and partial-update guards.

Run all three green before flashing or deploying.

---

## Deployment checklist

**Order:** firmware first (or simultaneously). No order breaks the current live config, but only flashing changes what a running board does.

1. `npm test` → both suites green.
2. `arduino-cli compile smartfarm_firmware` → clean (~44% flash on huge_app).
3. Flash the board, watch serial for `v2.9.2`.
4. `firebase deploy --only database` then `--only hosting`.
5. **Verify on hardware** (per this project's incremental-testing practice — host tests are not a substitute for a real SHT35):
   - Banner reads `v2.9.2`.
   - On a humid test, vent arms at RH ≥ `humidity_vent` (`[AUTO] พัดลมเปิด (...ชื้นเกิน...)`).
   - **No** `[AUTO] ⚠️ ปิด vent` warning on a valid config (that means firmware thinks the config is unusable).
   - Pump and fan never run in a way that violates `pumpOn → fanOn`.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `config.h: No such file` at compile | didn't create config.h | `cp config.h.example config.h` and fill in |
| "Sketch too big" / text section exceeds | wrong partition | ensure `huge_app` (it's in `sketch.yaml`; IDE users set Partition Scheme) |
| Board reboots at boot | pump on a strapping pin | CH4 must be GPIO25, not GPIO12 |
| Relays inverted (ON = OFF) | module polarity | flip `RELAY_ACTIVE_LOW` in config.h |
| Buzzer always sounds | active-HIGH module | set `BUZZER_ACTIVE_LOW false` |
| `[AUTO] ⚠️ ปิด vent` in serial | `humidity_vent < humidity_max + 5` | raise vent or lower humidity_max |
| Dashboard "save" does nothing | rules rejected the write | a toast now reports it; check values against constraints |
| Sensor badge shows stale | SHT35 read failing | check I2C wiring; firmware self-heals via periodic re-init |
| `test:rules` fails to start | emulator needs Java | install a JDK; ensure firebase-tools installed |

---

## Design notes & history

- **Auto control is offline-first** — never depends on Wi-Fi/Firebase. Config persists in NVS across reboots.
- **Air sensor is single point of failure by design** — SHT35 gives temp + humidity from one chip; on failure everything shuts off safely rather than acting on stale data (chosen 2026-07-15).
- **`VENT_HYST` lives in three layers** (`auto_control_logic.h`, dashboard `VENT_HYST_PCT`, rules literal `5`) — unavoidably, since C++, browser JS, and Firebase rules JSON cannot import from each other, and rules have no variables at all. `auto_control_logic.h` is the source of truth; tuning it means editing all three, and **`npm run test:sync` fails if they diverge** (`tests/vent_hyst_sync.check.js`). A guard rather than codegen on purpose: `firebase deploy` ships whatever rules file is on disk, so a forgotten regeneration step would deploy a stale value silently — worse than the duplication.
- Full changelog is at the top of `smartfarm_firmware.ino`. Deeper rationale, incident history, and hardware gotchas are in `docs/PROJECT_MEMORY.md`.

**v2.9.2** (latest) — fixed the reboot loop that was masquerading as Wi-Fi trouble, plus review follow-ups:

1. **Root cause: `syncNTP()` blocked past the watchdog.** Its wait loop called `getLocalTime(&t)` without the second argument, taking the library's blocking 5000 ms default — worst case 20 × (5000 + 500) = 110 s inside a single `loop()` pass, against `WDT_TIMEOUT_S` 60. TASK_WDT rebooted the board, which came back with an unset clock and did it again (`boot_count` 11 → 19 in ~1 day, `last_reset_reason` = `TASK_WDT`). Now uses the non-blocking `readLocalTime()`, bounding the wait at 10 s. Deliberately **does not** feed the watchdog inside that loop — the 10 s bound is the real guard, and the WDT stays armed as the last-resort detector for exactly this class of bug.
2. **Hourly log batched** into one `updateNodeSilent` — it was still issuing 7-10 sequential SSL round trips at each hour edge, the same stall v2.9.1 fixed on the 30 s paths.
3. **Wi-Fi diagnostics now mean what they say.** `wifi_drop_count` increments once per offline episode; previously it counted every `STA_DISCONNECTED` event, so a single 10-minute outage read as dozens and latched the dashboard chip amber permanently. The board's own escalation disconnect no longer overwrites `wifi_drop_reason` with reason 8 `ASSOC_LEAVE`, which the legend reads as "the AP kicked us — fix the router".
4. **`setup()` clears AP fail flags** on a failed boot connect, so the first reconnect attempt isn't a dead scan round (first real attempt ~75-80 s → ~45 s after boot), and `loop()` re-reads `millis()` after a blocking reconnect so the sensor/poll/schedule/NTP timers aren't back-dated ~25 s.

The firmware version string lives in **six** places — sketch header, serial banner, LCD splash, `pushStatus()`, dashboard footer, and this README (the badge at the top plus the post-flash verification steps). There's a checklist at the top of the sketch listing all six. It had drifted, with the dashboard reporting 2.9.2 while serial and the LCD still said 2.9.0, and the README telling you to check serial for a version it no longer prints. That last one is the quiet hazard: a stale README sends whoever flashed the board looking for the wrong number, and they conclude they flashed the wrong build. Bump all six together.

**v2.9.1** — fixed "dashboard shows data stale 90-120 s while the board is perfectly healthy". `pushStatus()` + `pushToFirebase()` were issuing 24 separate `Firebase.setX()` calls per 30 s cycle; at RSSI -76 each SSL round trip costs seconds, so one push cycle overran `SENSOR_INTERVAL` entirely. Both now send a single `updateNodeSilent` PATCH per node (PATCH, so an absent key — e.g. `water_temp` when the DS18B20 is unreadable — leaves the stored value alone rather than deleting it). Also replaced `getLocalTime(&t, 0)` with a `time()`/`localtime_r()` read that cannot spuriously return false, and fixed a cross-listener race that kept water temperature off the dashboard.

**v2.9.0** — fixed "Wi-Fi drops repeatedly and never reconnects, while phones/PCs on the same AP are fine". Four compounding bugs, all on the reconnect path (the boot path was fine, which is why a reboot appeared to fix it):

1. **Modem sleep was on** (ESP32 STA default `WIFI_PS_MIN_MODEM`) — the radio naps between beacons, misses them, and drops with reason 200 `BEACON_TIMEOUT`. Phones don't power-save this way, hence the asymmetry. Now `WiFi.setSleep(false)`. ⚠️ Costs ~20-40 mA average; if `last_reset_reason` starts showing `BROWNOUT` more often after flashing, suspect the power rail first.
2. **Core auto-reconnect raced `wifiMulti`** — `run()` calls `WiFi.disconnect()` mid-attempt, killing the core's in-flight reconnect. Same class of bug as the old `Firebase.reconnectWiFi(false)` fix, which counted only two radio managers and missed that the ESP32 core is a third. Now a 45 s grace window lets the core retry the same SSID alone (fast, no scan); only after that does `wifiMulti` take over and sweep all SSIDs.
3. **`run()` was called with no argument** in `loop()`, taking the library's 5000 ms default while boot used 15000 — the reconnect path was 3× more impatient than boot. Busy APs routinely need more than 5 s for assoc + DHCP. Now 20000 ms.
4. **Every other retry was a dead round** — a failed `run()` calls `markAsFailed()`, so the next round skips that AP as a candidate, ends with `bestIndex == -1`, and never calls `begin()` at all, while still paying the 3-5 s blocking scan. Now the fail flags are cleared after each failed attempt via `APlistClean()` + re-add (`resetFails()` is private).

Plus: radio power-cycle (`WIFI_OFF` → `WIFI_STA`) after 3 consecutive failures, ahead of the existing one-shot 30-minute reboot; and Wi-Fi disconnect diagnostics (`status/wifi_drop_count`, `status/wifi_drop_reason`) pushed to the dashboard, in the same spirit as `last_reset_reason` from v2.2.0 — previously the logs said only "dropped" / "back", with no way to tell why.

**v2.8.0** — fan now rests with the pump: 15 min max runtime, 5 min shared cooldown (reverses the v2.6.0 decouple, per on-site instruction; accepts the temperature-overshoot trade-off). Follow-up review fixes: no false water alarm when the fan alone triggers cutoff, `status/fan_locked` reports real state again, and both cooldown locks are now set regardless of channel mode so flipping a channel manual→auto mid-cooldown can't let the pump run with the fan locked off.

**v2.7.1** — fixed a pump-vs-vent conflict window (vent must now be ≥ `humidity_max` + `VENT_HYST`), moved the vent clamp into the testable pure logic, added reciprocal guards on `temp_off`/`humidity_min`, and hardened dashboard validation/feedback.

---

*จัดทำโดย Nattakit Prasertsak (IT Intern) · บริษัท ปุ๋ยไวกิ้ง จำกัด*
