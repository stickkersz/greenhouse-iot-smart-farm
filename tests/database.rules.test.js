// ทดสอบ database.rules.json ผ่าน Firebase local emulator — ไม่แตะ production เลย
// รัน: npm run test:rules  (ต้องมี firebase-tools ติดตั้งไว้แล้ว — ใช้ firebase emulators:exec คลุมให้อัตโนมัติ)
// Database emulator ต้องการ Java runtime — ถ้ายังไม่มี: brew install openjdk
// แล้วรันแบบนี้ (ไม่ต้อง sudo/แก้ .zshrc ถาวร):
//   PATH="/opt/homebrew/opt/openjdk/bin:$PATH" npm run test:rules
"use strict";

const { test, before, after, beforeEach, describe } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const {
  initializeTestEnvironment,
  assertSucceeds,
  assertFails,
} = require("@firebase/rules-unit-testing");

const RULES_PATH = path.join(__dirname, "..", "database.rules.json");
const EMULATOR_PORT = 9000; // ต้องตรงกับ firebase.json → emulators.database.port

// ── อ่าน PRESETS สดจาก dashboard/index.html ──────────
// เหตุผลเดียวกับ tests/vent_hyst_sync.check.js: ค่าที่ก๊อปมาแปะจะ drift เงียบๆ เมื่อมีคนแก้ต้นทาง
// แล้วเทสต์จะเฝ้าค่าเก่าที่ไม่มีใครใช้ต่อไปโดยยังเขียวอยู่ · parse แบบหยาบๆ พอ (ไม่ต้อง JS parser เต็ม)
// เพราะรูปแบบใน index.html เป็น object literal บรรทัดเดียวต่อ preset — ถ้าวันหลังเปลี่ยนรูปแบบ
// regex จะหาไม่เจอแล้ว throw ทันที (ดังกว่าการเงียบแล้วเทสต์ค่าผิด)
function readDashboardPresets() {
  const src = fs.readFileSync(path.join(__dirname, "..", "dashboard", "index.html"), "utf8");
  const block = src.match(/const\s+PRESETS\s*=\s*\{([\s\S]*?)\n\};/);
  assert.ok(block, "หา const PRESETS ใน dashboard/index.html ไม่เจอ — ถ้าย้าย/เปลี่ยนรูปแบบ ต้องแก้เทสต์นี้");

  const out = {};
  for (const line of block[1].split("\n")) {
    const m = line.match(/^\s*(\w+)\s*:\s*\{(.+)\}\s*,?\s*$/);
    if (!m) continue;
    const fields = {};
    for (const kv of m[2].split(",")) {
      const f = kv.match(/^\s*(\w+)\s*:\s*(-?[\d.]+)\s*$/);   // เอาเฉพาะ field ที่เป็นตัวเลข (ข้าม label)
      if (f) fields[f[1]] = parseFloat(f[2]);
    }
    if (Object.keys(fields).length) out[m[1]] = fields;
  }
  assert.ok(Object.keys(out).length > 0, "parse PRESETS ไม่ได้เลย — รูปแบบใน index.html เปลี่ยนไปแล้ว");
  return out;
}

let testEnv;

before(async () => {
  testEnv = await initializeTestEnvironment({
    projectId: "greenhouse-iot-smart-farm-test",
    database: {
      rules: fs.readFileSync(RULES_PATH, "utf8"),
      host: "127.0.0.1",
      port: EMULATOR_PORT,
    },
  });
});

after(async () => {
  await testEnv.cleanup();
});

beforeEach(async () => {
  await testEnv.clearDatabase();
});

// dashboard user จริง — email claim ต้องมี (rules เช็คแบบนี้เป๊ะ)
function emailUser() {
  return testEnv.authenticatedContext("staff-1", { email: "somchai@smartfarm.local" });
}
// ESP32 ใช้ anonymous auth — มี uid แต่ "ไม่มี" email claim เลย (จำลองของจริง)
function anonUser() {
  return testEnv.authenticatedContext("esp32-anon-1", {});
}
function noAuth() {
  return testEnv.unauthenticatedContext();
}

describe("control/thresholds — hysteresis + type validation", () => {
  const VALID = {
    temp_on: 35, temp_off: 32,
    humidity_min: 60, humidity_max: 75,
    humidity_vent: 80,
    temp_alert: 40, humidity_alert: 40,
    water_temp_alert: 35,
  };

  test("email user CAN write valid ordered thresholds", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));
  });

  test("REJECTS temp_on <= temp_off (the exact bug the code review found)", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, temp_on: 30, temp_off: 32 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS humidity_max <= humidity_min", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, humidity_min: 80, humidity_max: 75 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  // ── humidity_vent (v2.7.0 vent) ────────────────────────────────────────
  // firmware ปิดฟีเจอร์เงียบๆ ถ้า vent <= VENT_HYST(5) หรือ vent <= humidity_max
  // rules ต้องกันไว้ก่อน ไม่งั้น dashboard โชว์ว่าตั้งได้ แต่ ESP32 ไม่ทำอะไรเลย

  test("ACCEPTS humidity_vent = 0 (feature disabled)", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_vent: 0 }));
  });

  test("REJECTS humidity_vent <= humidity_max (fan would vent while pump still sprays)", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_vent: 70 }));
  });

  // wet latch ค้างได้ทั้งช่วง [vent-5, vent) แต่ปั๊มหยุดที่ humidity_max
  // ถ้า vent - humidity_max < 5 จะมีช่อง RH ที่พัดลมไล่ชื้นออกพร้อมปั๊มพ่นเข้า (พิสูจน์ใน logic test)
  test("REJECTS humidity_vent within VENT_HYST of humidity_max (pump-vs-vent conflict window)", async () => {
    const db = emailUser().database();
    // hmax 79 + vent 80: vent > hmax แต่เว้นแค่ 1% -> ที่ RH 76 ปั๊มกับพัดลมตีกัน
    await assertFails(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_max: 79, humidity_vent: 80 }));
  });

  test("ACCEPTS humidity_vent exactly humidity_max + VENT_HYST (boundary all presets sit on)", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_max: 75, humidity_vent: 80 }));
  });

  test("REJECTS humidity_vent <= VENT_HYST(5) — firmware would silently disable it", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, humidity_min: 1, humidity_max: 3, humidity_vent: 4, humidity_alert: 0 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS humidity_vent > 100", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_vent: 150 }));
  });

  test("REJECTS non-numeric humidity_vent", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/thresholds").set({ ...VALID, humidity_vent: "high" }));
  });

  // reciprocal guard: ยกแค่ humidity_max ทีหลังต้องไม่แซง humidity_vent ที่เก็บไว้แล้ว
  test("REJECTS partial update raising humidity_max above stored humidity_vent", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // vent 80 / max 75
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ humidity_max: 95 }));
  });

  test("REJECTS partial update pulling humidity_max within VENT_HYST of humidity_vent", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // vent 80 / max 75
    // 78 < 80 แต่เว้นแค่ 2% — เคยเขียนเทสต์นี้เป็น assertSucceeds ตอนคิดว่าเงื่อนไขคือ "max < vent"
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ humidity_max: 78 }));
  });

  test("ACCEPTS partial update keeping humidity_max at least VENT_HYST below humidity_vent", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").update({ humidity_max: 70 }));
  });

  // ทั้ง 3 preset ในหน้า dashboard ต้องเขียนผ่าน rules ได้ — preset ที่ rules ปัดคือบั๊กที่ผู้ใช้เจอทันที
  //
  // ⚠️ v2.9.3: อ่านค่าจาก dashboard/index.html ตอนรันเทสต์ ไม่ก๊อปตัวเลขมาแปะ
  //    เดิมเขียนซ้ำไว้ที่นี่ → แก้ preset ในหน้า dashboard แล้วลืมแก้ที่นี่ เทสต์ยัง "เขียว" ทั้งที่
  //    ของจริงอาจโดน rules ปัดตกแล้ว = เทสต์เฝ้าค่าที่ไม่มีใครใช้ (ปัญหาเดียวกับที่ VENT_HYST เคยเจอ
  //    จน tests/vent_hyst_sync.check.js ต้องอ่านไฟล์เอาค่าจริงมาเทียบ — ที่นี่ใช้วิธีเดียวกัน)
  const DASHBOARD_PRESETS = readDashboardPresets();
  for (const [name, preset] of Object.entries(DASHBOARD_PRESETS)) {
    test(`ACCEPTS dashboard preset '${name}' verbatim (อ่านสดจาก index.html)`, async () => {
      const db = emailUser().database();
      await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(preset));
    });
  }

  test("sanity: อ่าน preset จาก dashboard ได้ครบ 3 ตัวและมี field ครบ", () => {
    const names = Object.keys(DASHBOARD_PRESETS);
    assert.deepEqual(names.sort(), ["normal", "rainy", "summer"],
      "ถ้าเพิ่ม/ลบ preset ในหน้า dashboard ให้แก้เทสต์นี้ด้วย (กันอ่านพลาดแล้วเงียบ)");
    for (const [n, p] of Object.entries(DASHBOARD_PRESETS)) {
      for (const f of ["temp_on","temp_off","humidity_min","humidity_max",
                       "humidity_vent","temp_alert","humidity_alert","water_temp_alert"]) {
        assert.equal(typeof p[f], "number", `preset '${n}' ขาด field ${f} หรือไม่ใช่ตัวเลข`);
      }
    }
  });

  // ช่องเดียวกับที่ vent/max เคยมี — partial update ทำให้ ton<=toff หรือ hmax<=hmin ได้
  // แล้ว autoControl() ติด guard `if (ton <= toff || hmax <= hmin) return;` ทุกรอบ = รีเลย์ค้างถาวร
  test("REJECTS partial update making temp_off >= temp_on (would freeze autoControl)", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // on 35 / off 32
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ temp_off: 40 }));
  });

  test("REJECTS partial update making humidity_min >= humidity_max (would freeze autoControl)", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // min 60 / max 75
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ humidity_min: 90 }));
  });

  test("ACCEPTS valid partial update of temp_off", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").update({ temp_off: 30 }));
  });

  // ── v2.9.3: ช่องเดียวกันอีกคู่ที่ตกสำรวจ — เกณฑ์ "เริ่มทำงาน" ต้องไม่แซงเกณฑ์ "แจ้งเตือน" ──
  // temp_alert เช็ค > temp_on อยู่แล้ว แต่ขาขึ้น temp_on ไม่เคยเช็คกลับ → ยก temp_on ทีหลังแซงได้
  // ผลจริง: แตร/แจ้งเตือนดังก่อนพัดลมเริ่มทำงาน = เตือนว่า "วิกฤต" ทั้งที่ระบบยังไม่ได้เริ่มแก้ด้วยซ้ำ
  test("REJECTS partial update raising temp_on above stored temp_alert", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // on 35 / alert 38
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ temp_on: 41 }));
  });

  test("REJECTS partial update lowering humidity_min below stored humidity_alert", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));   // min 60 / alert 40
    await assertFails(db.ref("/smartfarm/control/thresholds").update({ humidity_min: 35 }));
  });

  test("ACCEPTS partial update keeping temp_on below temp_alert", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").update({ temp_on: 37 }));
  });

  test("ACCEPTS partial update keeping humidity_min above humidity_alert", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(VALID));
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").update({ humidity_min: 55 }));
  });

  test("ACCEPTS water_temp_alert as a standalone number (no ordering constraint)", async () => {
    const db = emailUser().database();
    const ok = { ...VALID, water_temp_alert: 42 };
    await assertSucceeds(db.ref("/smartfarm/control/thresholds").set(ok));
  });

  test("REJECTS out-of-range water_temp_alert", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, water_temp_alert: 999 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS non-numeric threshold value", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, temp_on: "hot" };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS out-of-range threshold value", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, temp_on: 9999, temp_off: 32 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS unrecognized field under thresholds ($other:false)", async () => {
    const db = emailUser().database();
    const bad = { ...VALID, some_typo_field: 1 };
    await assertFails(db.ref("/smartfarm/control/thresholds").set(bad));
  });

  test("REJECTS anonymous (ESP32-style) auth writing thresholds", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/control/thresholds").set(VALID));
  });

  test("REJECTS unauthenticated write", async () => {
    const db = noAuth().database();
    await assertFails(db.ref("/smartfarm/control/thresholds").set(VALID));
  });
});

describe("real dashboard write patterns — multi-path .update(), not just leaf .set()", () => {
  // setGlobalMode() ในหน้า dashboard เขียนแบบนี้เป๊ะ: .update() บน control root ด้วย dot-path keys
  test("setGlobalMode()-style multi-path update to 2 channels at once succeeds", async () => {
    const db = emailUser().database();
    await assertSucceeds(
      db.ref("/smartfarm/control").update({ "ch3_fan_in/mode": "auto", "ch4_spare/mode": "auto" })
    );
  });

  test("setGlobalMode()-style update REJECTS if one of the two values is invalid", async () => {
    const db = emailUser().database();
    await assertFails(
      db.ref("/smartfarm/control").update({ "ch3_fan_in/mode": "auto", "ch4_spare/mode": "bogus" })
    );
  });

  // setRelay() ในหน้า dashboard เขียนแบบนี้เป๊ะ: .update({manual_state}) บน channel root ไม่ใช่ .set() ตรงๆที่ leaf
  test("setRelay()-style .update({manual_state}) on channel root succeeds", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/ch4_spare").update({ manual_state: true }));
  });

  test("setRelay()-style update REJECTS a non-boolean manual_state", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/ch4_spare").update({ manual_state: "true" }));
  });
});

describe("control/ch4_spare — channel mode/manual_state/schedule validation", () => {
  test("email user CAN set mode to 'auto' or 'manual'", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/ch4_spare/mode").set("auto"));
    await assertSucceeds(db.ref("/smartfarm/control/ch4_spare/mode").set("manual"));
  });

  test("REJECTS mode outside the auto/manual enum", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/ch4_spare/mode").set("bogus"));
  });

  test("REJECTS manual_state as a non-boolean", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/ch4_spare/manual_state").set("true"));
  });

  test("email user CAN write a valid HH:MM schedule", async () => {
    const db = emailUser().database();
    await assertSucceeds(
      db.ref("/smartfarm/control/ch4_spare/schedule").set({
        enabled: true, on_time: "07:00", off_time: "18:00",
      })
    );
  });

  test("REJECTS malformed schedule time strings", async () => {
    const db = emailUser().database();
    await assertFails(
      db.ref("/smartfarm/control/ch4_spare/schedule").set({
        enabled: true, on_time: "25:99", off_time: "18:00",
      })
    );
  });

  test("REJECTS unrecognized field under a channel ($other:false)", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/ch4_spare/typo_field").set(true));
  });

  // ── v2.9.3: on_time == off_time = "เปิดตลอด" ใน firmware ──
  // checkSchedule() แตกเป็น 2 กิ่ง on<off (ในวัน) กับ on>off (ข้ามคืน) · พอ on==off กิ่งข้ามคืน
  // กลายเป็น `now>=onT || now<offT` ซึ่งเป็นจริงเสมอ = ช่องนั้นเดินตลอด 24 ชม. ไม่ใช่ตามตารางที่ตั้ง
  // dashboard กันไว้ฝั่ง JS แล้ว แต่ rules คือด่านจริง — เขียนตรงผ่าน REST/console ยังหลุดได้
  test("REJECTS schedule with on_time === off_time (firmware reads it as always-ON)", async () => {
    const db = emailUser().database();
    await assertFails(
      db.ref("/smartfarm/control/ch4_spare/schedule").set({
        enabled: true, on_time: "07:00", off_time: "07:00",
      })
    );
  });

  test("REJECTS partial update making off_time equal stored on_time", async () => {
    const db = emailUser().database();
    await assertSucceeds(
      db.ref("/smartfarm/control/ch4_spare/schedule").set({
        enabled: true, on_time: "07:00", off_time: "18:00",
      })
    );
    await assertFails(db.ref("/smartfarm/control/ch4_spare/schedule").update({ off_time: "07:00" }));
  });

  test("ACCEPTS overnight schedule (on_time > off_time) — ยังต้องตั้งข้ามคืนได้", async () => {
    const db = emailUser().database();
    await assertSucceeds(
      db.ref("/smartfarm/control/ch4_spare/schedule").set({
        enabled: true, on_time: "22:00", off_time: "06:00",
      })
    );
  });
});

describe("control/buzzer_enabled", () => {
  test("CAN write boolean", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/control/buzzer_enabled").set(false));
  });
  test("REJECTS non-boolean", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/control/buzzer_enabled").set("off"));
  });
});

describe("sensors/status — ESP32 (anonymous) write path stays intact", () => {
  test("anonymous auth CAN write sensor readings (matches real ESP32 auth)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/sensors/air_temp").set(28.5));
  });

  test("unauthenticated CANNOT write sensor readings", async () => {
    const db = noAuth().database();
    await assertFails(db.ref("/smartfarm/sensors/air_temp").set(28.5));
  });

  test("anonymous auth CANNOT read sensors (read requires email claim)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/sensors/air_temp").once("value"));
  });

  test("email user CAN read sensors", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/sensors/air_temp").once("value"));
  });
});

// v2.9.1: firmware เลิกยิง Firebase.setX() แยก 24 ครั้งต่อรอบ แล้วรวมเป็น updateNodeSilent (PATCH)
// ก้อนเดียวต่อ node — เพราะ 24 SSL round trip ที่ RSSI -76 กินเวลาเกิน SENSOR_INTERVAL (30 วิ)
// = dashboard ขึ้น "ข้อมูลค้าง 90-120 วิ" ทั้งที่บอร์ดปกติดี
// ⚠️ เทสต์ชุดเดิมยิงทีละ leaf (.set()) = คนละรูปทรงกับที่ firmware ส่งจริงตอนนี้
//    ถ้า rules ไม่รับ PATCH ก้อนใหญ่ = dashboard ดับสนิท ซึ่งคือบั๊กที่ v2.9.1 ตั้งใจแก้พอดี
//    ชุดนี้จึงล็อก "รูปทรงจริง" ไว้ ไม่ใช่แค่ล็อกว่าแต่ละ field ผ่าน
describe("v2.9.1 batched PATCH — รูปทรงที่ firmware ส่งจริง (updateNodeSilent)", () => {
  // ตรงกับ pushStatus() ใน smartfarm_firmware.ino — ครบทุก key
  const STATUS_BATCH = {
    online: true,
    ch1_pump: false,
    ch2_fan_out: false,
    ch3_fan_in: true,
    ch4_spare: true,
    firmware: "2.9.3",
    last_reset_reason: "POWERON (เสียบไฟใหม่/กดปุ่ม EN)",
    boot_count: 3,
    free_heap: 201528,
    max_alloc_heap: 110580,
    sensor_ok: true,
    sensor_stale: false,
    water_ok: true,
    failsafe: false,
    pump_locked: false,
    fan_locked: false,
    time_ok: true,
    wifi_rssi: -76,
    wifi_drop_count: 0,
    wifi_drop_reason: "ยังไม่เคยหลุดตั้งแต่บูต",
  };

  // ตรงกับ pushToFirebase() — water_temp มีเฉพาะตอน waterSensorOk
  const SENSORS_BATCH = {
    air_temp: 34.1,
    air_humidity: 62.3,
    water_temp: 31.6,
    uptime_sec: 934,
  };

  test("ESP32 (anon) ยิง status ก้อนเดียวครบทุก field ได้", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/status").update(STATUS_BATCH));
  });

  test("ESP32 (anon) ยิง sensors ก้อนเดียวได้", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/sensors").update(SENSORS_BATCH));
  });

  test("$other ยังกันอยู่แม้ส่งมาแบบ PATCH ก้อนใหญ่ (key แปลกปน 1 ตัว = ตกทั้งก้อน)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status").update({ ...STATUS_BATCH, hacked: true }));
    await assertFails(db.ref("/smartfarm/sensors").update({ ...SENSORS_BATCH, soil_moisture: 42 }));
  });

  test("type validation ยังทำงานใน PATCH (wifi_rssi เป็น string = ตกทั้งก้อน)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status").update({ ...STATUS_BATCH, wifi_rssi: "-76" }));
  });

  // นี่คือ semantics ที่ v2.9.1 พึ่งพา: DS18B20 อ่านไม่ได้ → ไม่ใส่ water_temp ลงก้อน
  // PATCH merge เฉพาะ key ที่ส่ง → ค่าเดิมคาไว้ ไม่ถูกลบ (เหมือน setFloat เดิมที่ข้ามการเขียนไป)
  test("ไม่ใส่ water_temp (เซนเซอร์น้ำพัง) → ค่าเดิมใน DB ต้องคาอยู่ ไม่ถูกลบ", async () => {
    await testEnv.withSecurityRulesDisabled(async (ctx) => {
      await ctx.database().ref("/smartfarm/sensors/water_temp").set(31.6);
    });

    const db = anonUser().database();
    const noWater = { air_temp: 34.1, air_humidity: 62.3, uptime_sec: 964 };
    await assertSucceeds(db.ref("/smartfarm/sensors").update(noWater));

    await testEnv.withSecurityRulesDisabled(async (ctx) => {
      const snap = await ctx.database().ref("/smartfarm/sensors").once("value");
      assert.equal(snap.val().water_temp, 31.6, "water_temp หายไปจาก PATCH = จะไปลบค่าน้ำใน production");
      assert.equal(snap.val().uptime_sec, 964, "key ที่ส่งไปต้องอัปเดตจริง");
    });
  });
});

describe("status — field validation + $other rejection", () => {
  test("anonymous auth CAN write valid status fields (matches ESP32)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/status/online").set(true));
    await assertSucceeds(db.ref("/smartfarm/status/firmware").set("1.4.0"));
  });

  test("REJECTS non-boolean for a boolean status field", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status/online").set("yes"));
  });

  test("REJECTS firmware string >= 16 chars", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status/firmware").set("1.4.0-this-is-way-too-long"));
  });

  test("REJECTS unrecognized field under status ($other:false)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status/typo_field").set(true));
  });

  // fw 2.0.0 — $other:false เงียบๆ reject field ใหม่ทุกตัว ถ้าลืมมาเพิ่มที่ rules
  // sensor_stale = สัญญาณว่า auto ปิดทุกช่องแล้ว ถ้ามันไม่ผ่าน dashboard จะไม่มีวันบอกคนว่าโรงเรือนหยุดคุมเอง
  test("anonymous auth CAN write sensor_stale (fw 2.0.0 — auto shutdown signal)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/status/sensor_stale").set(true));
    await assertSucceeds(db.ref("/smartfarm/status/sensor_stale").set(false));
  });

  // fw 2.2.0 — boot diagnostics · $other:false เคย reject 4 field นี้เงียบๆ (pushStatus จบด้วย wifi_rssi ที่ valid
  // → errorReason ว่าง → "Push OK" ทั้งที่ 4 ตัวนี้ตกไป) = อาการที่ทำให้ field ไล่สาเหตุ restart ไม่เคยขึ้น dashboard
  test("anonymous auth CAN write boot diagnostics (fw 2.2.0 — restart forensics)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/status/last_reset_reason").set("BROWNOUT *** ไฟตก ***"));
    await assertSucceeds(db.ref("/smartfarm/status/boot_count").set(7));
    await assertSucceeds(db.ref("/smartfarm/status/free_heap").set(210000));
    await assertSucceeds(db.ref("/smartfarm/status/max_alloc_heap").set(110000));
  });

  test("REJECTS non-boolean sensor_stale", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/status/sensor_stale").set("yes"));
  });

  test("REJECTS unauthenticated write to status", async () => {
    const db = noAuth().database();
    await assertFails(db.ref("/smartfarm/status/online").set(true));
  });
});

describe("alerts — anon write allowed (ESP32), email-only read", () => {
  test("anonymous auth CAN write an alert (matches ESP32 pushing alerts)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/smartfarm/alerts/last_alert/type").set("high_temp"));
  });

  test("REJECTS unauthenticated write to alerts", async () => {
    const db = noAuth().database();
    await assertFails(db.ref("/smartfarm/alerts/last_alert/type").set("high_temp"));
  });

  test("email user CAN read alerts", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/alerts").once("value"));
  });

  test("anonymous auth CANNOT read alerts (read requires email claim)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/alerts").once("value"));
  });
});

describe("alert_history — required-fields validation, email-only", () => {
  test("email user CAN push an entry with msg + timestamp", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/smartfarm/alert_history").push({ msg: "test alert", timestamp: Date.now() }));
  });

  test("REJECTS entry missing the required 'timestamp' field", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/alert_history").push({ msg: "test alert" }));
  });

  test("REJECTS anonymous (ESP32-style) write to alert_history", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/alert_history").push({ msg: "test alert", timestamp: Date.now() }));
  });
});

describe("action_log — required-fields validation, email-only", () => {
  test("email user CAN push an entry with action + user + timestamp", async () => {
    const db = emailUser().database();
    await assertSucceeds(
      db.ref("/smartfarm/action_log").push({ action: "ปั๊มน้ำ → เปิด", user: "somchai", timestamp: Date.now() })
    );
  });

  test("REJECTS entry missing the required 'user' field", async () => {
    const db = emailUser().database();
    await assertFails(db.ref("/smartfarm/action_log").push({ action: "ปั๊มน้ำ → เปิด", timestamp: Date.now() }));
  });

  test("REJECTS anonymous (ESP32-style) write to action_log", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/smartfarm/action_log").push({ action: "x", user: "y", timestamp: Date.now() }));
  });
});

describe("logs (hourly) — anon write (ESP32), email-only read", () => {
  test("anonymous auth CAN write hourly logs (matches ESP32)", async () => {
    const db = anonUser().database();
    await assertSucceeds(db.ref("/logs/2026-07-10/14/air_temp_avg").set(28.4));
  });

  test("REJECTS unauthenticated write to logs", async () => {
    const db = noAuth().database();
    await assertFails(db.ref("/logs/2026-07-10/14/air_temp_avg").set(28.4));
  });

  test("email user CAN read logs", async () => {
    const db = emailUser().database();
    await assertSucceeds(db.ref("/logs").once("value"));
  });

  test("anonymous auth CANNOT read logs (read requires email claim)", async () => {
    const db = anonUser().database();
    await assertFails(db.ref("/logs").once("value"));
  });
});
