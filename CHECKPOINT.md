# Greenhouse IoT Smart Farm — Checkpoint v1.2.0

**Date:** 2026-06-22  
**Status:** ✅ Production Ready  
**Version:** 1.2.0 (Notification Panel + Mobile Optimization + Live Chart Injection)

---

## 📋 Session Summary

This session focused on completing the notification panel UI, fixing 24h chart display, and comprehensive mobile/tablet responsiveness improvements.

### Completed Tasks

#### 1. ✅ Notification Panel Implementation
- **Bell icon (🔔)** in header with red badge counter
- **Slide-in panel** from right side with alert history
- **Mark as seen** → `lastSeenAlertTs` stored in localStorage
- **Clear all** → remove from Firebase `/smartfarm/alert_history/`
- **Push notification enable button** → requests browser permission + state indicator
- Full Thai UI text

**Files Modified:**
- `dashboard/index.html` → HTML + CSS + JS functions

**JS Functions Added:**
```javascript
openNotifPanel()          // open + mark seen + render
closeNotifPanel()         // close
clearAlertHistory()       // remove all + toast
renderAlertHistory()      // populate both chart tab + notif panel
renderNotifPanel()        // populate panel body + badge count
requestNotifPermission()  // request browser permission
```

#### 2. ✅ 24h Chart Live Data Injection
**Problem:** Chart showed empty graph if ESP32 hadn't logged a full hour yet  
**Solution:** Inject current `sensorLive` values as "hour 0" data point

**Changes:**
- If current hour has no log → use `sensorLive.airTemp / airHum / waterTemp`
- Always show dots on 24h chart (even with sparse data)
- Chart remains responsive across all data ranges

#### 3. ✅ Mobile & Tablet Responsive Design

**Breakpoints Implemented:**

| Screen | Width | Layout |
|--------|-------|--------|
| Very small (iPhone SE) | < 375px | Status pill hidden, compact gauge, 2-col sensors |
| Mobile (iPhone 12 mini) | < 600px | Bottom nav, title → "SmartFarm", larger fonts, stacked toolbar |
| Tablet portrait (iPad) | 768–1023px | 2-col relay grid, 2-col settings, 2-col sensor grid |
| Desktop/iPad Pro | ≥ 1024px | Full auto-fit grid layout |

**Key Mobile Fixes:**
- ✅ `viewport-fit=cover` → nav bar stays visible above home indicator
- ✅ `safe-area-inset-bottom` → padding on iPhone notch/home bar
- ✅ Header title: "Greenhouse IoT Smart Farm" → "SmartFarm" (< 600px)
- ✅ Font sizes increased by 15–20% for readability
- ✅ Nav tab labels: .58rem → .65rem, icons: 1.25rem → 1.35rem
- ✅ Sensor cards: 2x2 grid with better spacing
- ✅ Chart toolbar: stacks vertically on small screens
- ✅ Detail sheet chart: height reduced to 155px on mobile

---

## 🏗️ Architecture Overview

### Tech Stack
- **Frontend:** HTML5 + Vanilla JS + Chart.js 4.4.0 + SVG gauges
- **Backend:** Firebase Realtime Database (asia-southeast1)
- **Auth:** Firebase Email/Password (disguised as username/password)
- **Hosting:** Firebase Hosting (Spark tier)
- **PWA:** manifest.json + service worker + icon.svg

### Database Structure
```
/smartfarm/
  sensors/              → current sensor values
  status/               → system status
  control/              → relay manual overrides
  alerts/               → current alert state
  alert_history/        → historical alerts (30 min retention)
  action_log/           → user actions (dashboard)
/logs/
  YYYY-MM-DD/
    HH/
      air_temp_avg, air_temp_min, air_temp_max
      air_humidity_avg, air_humidity_min, air_humidity_max
      water_temp_avg, water_temp_min, water_temp_max
      soil_pct_avg, soil_pct_min, soil_pct_max
      sample_count
```

### Firebase Rules
```json
{
  "smartfarm": {
    "sensors":  { ".read": "email", ".write": "anon" },
    "status":   { ".read": "email", ".write": "anon" },
    "control":  { ".read": "anon",  ".write": "email" },
    "alerts":   { ".read": "email", ".write": "anon" },
    "alert_history": { ".read": "email", ".write": "email" },
    "action_log": { ".read": "email", ".write": "email", ".validate": "..." }
  },
  "logs": { ".read": "email", ".write": "anon" }
}
```

---

## 🔌 Hardware

**ESP32 DevKit V1**
- Sensors: DS18B20 (water temp), DHT11 (air temp+humidity), capacitive soil moisture (ADC)
- Relays: 4x relay module (GPIO 26, 27, 14, 25)
- WiFiMulti: home + company networks
- Partition: Huge APP (3MB No OTA)
- Firmware: `smartfarm_firmware.ino`

**Firebase Integration:**
- Anonymous auth (token-only, no email)
- getJSON() for batch control reads (prevents SSL heap fragmentation)
- Hourly log push (avg/min/max aggregation on device)

---

## 📱 UI Features

### Tabs (Bottom Nav on mobile)
1. **Sensors** — 4 gauge cards + uptime + status
2. **Control** — 4 relay cards (auto/manual/schedule)
3. **Chart** — 24h/7d/30d history + CSV export
4. **Settings** — control thresholds (temp on/off, humidity min)

### Dashboard Sections
- **Uptime Card** → firmware version + last update time
- **Sensor Gauges** → real-time values + trend arrows + status badges
- **Relay Cards** → toggle + mode (auto/manual) + schedule details
- **Alert History** → timestamped alert log (chart tab + notification panel)
- **Control State** → live relay status (on/off) + mode
- **History Chart** → selectable range + dual-axis (temp, humidity) + CSV export

### Notification System
- **Push Notifications** — triggered on high temp/low humidity
- **Alert History** — persistent, 5-min cooldown per alert type
- **Notification Panel** — slide-in from right, shows recent alerts + enable/disable push
- **Badge Counter** → red dot shows unseen alerts

---

## 🔐 Security Notes

**Credentials:**
- `config.h` (firmware WiFi + Firebase API key) → .gitignore ✅
- Dashboard auth → username/password (disguised as `username@smartfarm.local`)
- Session token stored in localStorage (browser only)

**Database Rules:**
- Dashboard (email users): can read sensors/alerts, write control
- ESP32 (anonymous): can write sensors/logs, read control
- Alert history: email-only read/write
- Action log: email-only + required fields validation

---

## 🚀 Deployment

**Current URL:** https://greenhouse-iot-smart-farm.web.app

**Deploy Process:**
```bash
# Stage + commit
git add dashboard/index.html manifest.json
git commit -m "feat: notification panel + mobile optimizations"

# Deploy to Firebase
firebase deploy --only hosting

# Check status
firebase hosting:channel:list
```

**Environment:**
- `.firebaserc` → project: greenhouse-iot-smart-farm
- Firebase config in `index.html` (SDK v9 compat)
- CI/CD: manual via `firebase deploy`

---

## 📊 Charts & Data

### 24h Chart
- Pulls hourly logs from `/logs/{today}/{HH}` + `/logs/{yesterday}/{HH}`
- Plots air temp (green), humidity (blue), water temp (teal)
- **Live injection:** if current hour has no log → uses `sensorLive` values
- Always shows dots on mobile (even sparse data)
- Dual-axis: left (°C, 20–45), right (%, 0–100)

### 7d / 30d Charts
- Daily average from all hourly logs in that day
- Parallel Firebase reads via `Promise.all()`
- Lower point density → no dots (cleaner UI)

### CSV Export
- UTF-8 BOM for Excel Thai support
- Columns: timestamp, air_temp, humidity, water_temp
- Filename: `smartfarm_24h_YYYY-MM-DD.csv`

---

## 🐛 Known Issues & Workarounds

### Issue: WiFi Not Connecting
- **Status:** ✅ Fixed (WiFiMulti)
- **Details:** Firmware now tries home + company network automatically
- **Config:** stored in `config.h` (excluded from git)

### Issue: Firebase SSL Errors
- **Status:** ✅ Fixed (getJSON batching)
- **Root cause:** 11 sequential Firebase calls → heap fragmentation
- **Solution:** single `Firebase.getJSON()` call + local JSON parsing
- **Impact:** reduced SSL connections from 11 to 1 per poll cycle

### Issue: Chart Empty on Fresh Boot
- **Status:** ✅ Fixed (live injection)
- **Details:** no logs until 1 hour has passed → now shows live sensor data
- **Impact:** chart always has ≥1 data point if ESP32 is online

### Issue: Bottom Nav Hidden on iPhone
- **Status:** ✅ Fixed (safe-area-inset)
- **Details:** 60px nav bar was behind home indicator
- **Solution:** `safe-area-inset-bottom` + `viewport-fit=cover`
- **Impact:** nav bar now floats above home indicator

---

## 📝 Files & Structure

```
/Users/tonklax/Documents/Greenhouse IoT Smart Farm/
├── dashboard/
│   ├── index.html            (↑ main app, 1600+ lines)
│   ├── manifest.json         (PWA manifest)
│   ├── icon.svg              (PWA icon)
│   └── sw.js                 (service worker)
├── smartfarm_firmware/
│   ├── smartfarm_firmware.ino (↑ main firmware)
│   ├── config.h.example      (credentials template)
│   └── config.h              (↑ .gitignore, real credentials)
├── database.rules.json       (↑ Firebase rules)
├── .firebaserc               (Firebase project config)
├── .gitignore                (excludes config.h, .firebase)
├── CLAUDE.md                 (legacy docs)
└── CHECKPOINT.md             (this file)
```

---

## ✨ Latest Changes (This Session)

### Merged Features
1. **Notification Panel** (HTML + CSS + JS)
   - Slide-in from right, header+body+footer
   - Alert items with icon/message/timestamp
   - Clear all button + enable push notifications
   - Badge counter on bell icon
   - Responsive width: `min(380px, 100vw)`

2. **Live Chart Injection**
   - 24h chart now shows current hour's live data if no log exists
   - Always displays dots on 24h chart
   - Prevents empty/blank graph on fresh boot

3. **Mobile UX Overhaul**
   - Added `viewport-fit=cover` + `safe-area-inset`
   - Bottom nav: 60px + safe area padding
   - Header title: "SmartFarm" on < 600px
   - Font size increases across all mobile breakpoints
   - Responsive grid layouts: 2-col (mobile), 2-col (tablet), auto (desktop)
   - Chart toolbar stacks vertically on small screens
   - Status text hidden on mobile (keep colored dot)
   - Nav tabs: larger text + icons

### Testing Checklist
- ✅ Desktop (1024px+) — all features visible, layout stable
- ✅ Tablet (768–1023px) — 2-col grids, readable fonts
- ✅ Mobile (375–599px) — bottom nav accessible, no truncation
- ✅ Very small (< 375px) — compact but usable

---

## 🎯 Next Steps (Optional)

### Phase 2 Ideas
1. **Dark Mode Toggle** — theme switcher in settings
2. **Email Alerts** — send summary to user email (Firebase Cloud Functions)
3. **Historical Reports** — downloadable daily/weekly PDFs
4. **Multiple Rooms** — support for 2+ growing zones
5. **Advanced Scheduling** — recurring schedules, conditional triggers
6. **Analytics Dashboard** — trends, predictive models (TBD)

### Maintenance
- Monitor Firebase costs (Spark tier free limits)
- Regular firmware backups to GitHub (private repo)
- Update npm dependencies (if adding build tools)
- Test WiFi failover on real hardware

---

## 🔗 References

**GitHub:** https://github.com/stickkersz/greenhouse-iot-smart-farm (private)  
**Firebase Console:** https://console.firebase.google.com/project/greenhouse-iot-smart-farm  
**Deployed App:** https://greenhouse-iot-smart-farm.web.app  
**Firmware Docs:** ESP32 + Arduino IDE  

---

**Last Updated:** 2026-06-22 (Sonnet 4.6)  
**Commit:** `f090dd9` (v1.2.0)
