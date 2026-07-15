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
