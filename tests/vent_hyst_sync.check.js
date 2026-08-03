// ตรวจว่า VENT_HYST ตรงกันทั้ง 3 ชั้น — firmware / dashboard / database rules
// รัน: npm run test:sync   (ไม่ต้องใช้ emulator/Java — อ่านไฟล์เทียบค่าเฉยๆ เร็วมาก)
//
// ⚠️ ทำไมไม่ทำ "single source of truth" จริงๆ:
// ค่านี้ต้องอยู่ใน 3 runtime ที่ import กันไม่ได้เลย — C++ (firmware), JS (browser), JSON (Firebase rules)
// database.rules.json เป็นข้อจำกัดตัวจริง: Firebase security rules ไม่มีกลไก import/ตัวแปร/มาโครใดๆ
// ต้องเขียนตัวเลขลงไปตรงๆ เท่านั้น · ส่วนโปรเจกต์นี้ก็ไม่มี build step (ตั้งใจ — vanilla ทั้งหมด)
// ถ้าใช้ codegen สร้าง rules.json จากค่า C++ จะได้ single source จริง แต่แลกมาด้วย:
//   `firebase deploy` ส่งไฟล์ "ที่อยู่บนดิสก์" ขึ้น production — ถ้าลืม regenerate ก่อน deploy
//   จะ deploy ค่าเก่าขึ้นไปเงียบๆ = ย้ายบั๊กไปอยู่ที่จุดที่มองเห็นยากกว่าเดิม
// จึงเลือก "guard" แทน "generate": ยอมให้มี 3 ที่ แต่ทำให้การไม่ตรงกันเป็น test failure ที่เสียงดัง
// แทนที่จะเป็นความเพี้ยนเงียบๆ ที่ไม่มีใครรู้จนกว่าจะมีคนบ่นว่าพัดลมไม่ทำงาน
"use strict";

const { test, describe } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");

const root = path.join(__dirname, "..");
const read = (p) => fs.readFileSync(path.join(root, p), "utf8");

// ── แหล่งความจริง: ค่าใน auto_control_logic.h ────────
// ค่านี้คือตัวที่ถูกเทสต์จริง (sweep ใน auto_control_logic.test.cpp กินโค้ดนี้ตรงๆ)
// อีก 2 ชั้นเป็นแค่ "สำเนา" ที่ต้องตามให้ตรง
function firmwareHyst() {
  const src = read("smartfarm_firmware/auto_control_logic.h");
  const m = src.match(/static\s+const\s+float\s+VENT_HYST\s*=\s*([0-9.]+)f?\s*;/);
  assert.ok(m, "หา VENT_HYST ใน auto_control_logic.h ไม่เจอ — ถ้าเปลี่ยนชื่อ/รูปแบบ ต้องแก้เทสต์นี้ด้วย");
  return parseFloat(m[1]);
}

describe("VENT_HYST ตรงกันทั้ง 3 ชั้น", () => {
  const H = firmwareHyst();

  test(`firmware: VENT_HYST อ่านค่าได้ (= ${H})`, () => {
    assert.ok(Number.isFinite(H) && H > 0, "VENT_HYST ต้องเป็นเลขบวก");
  });

  test("dashboard: VENT_HYST_PCT ตรงกับ firmware", () => {
    const src = read("dashboard/index.html");
    const m = src.match(/const\s+VENT_HYST_PCT\s*=\s*([0-9.]+)\s*;/);
    assert.ok(m, "หา VENT_HYST_PCT ใน dashboard/index.html ไม่เจอ");
    assert.equal(
      parseFloat(m[1]), H,
      `dashboard VENT_HYST_PCT = ${m[1]} แต่ firmware VENT_HYST = ${H} — แก้ dashboard/index.html ให้ตรง`
    );
  });

  // rules เขียนเลขลงไปตรงๆ 3 จุด (Firebase rules ไม่มีตัวแปร) — เทียบกับ "รูปแบบที่คาดไว้"
  // ไม่ใช่แค่ไล่หาเลข 5 ลอยๆ เพราะในนิพจน์มีเลขอื่น (0, 100) ปนอยู่ จะจับผิดตัวได้
  // ถ้าใครแก้รูปแบบนิพจน์ เทสต์จะ fail แล้วบอกให้มาอัปเดตเทสต์ = เจตนา ไม่ใช่เทสต์เปราะ
  describe("database.rules.json: ทั้ง 3 จุดต้องใช้ค่าเดียวกัน", () => {
    const rules = JSON.parse(read("database.rules.json"));
    const th = rules.rules.smartfarm.control.thresholds;

    test("humidity_max: ต้องเว้นระยะให้ humidity_vent ตามค่า VENT_HYST", () => {
      const v = th.humidity_max[".validate"];
      const want = `newData.val() + ${H} <= newData.parent().child('humidity_vent').val()`;
      assert.ok(
        v.includes(want),
        `humidity_max rule ไม่มีเงื่อนไข "${want}"\nที่เจอจริง: ${v}`
      );
    });

    test("humidity_vent: ต้อง > VENT_HYST และ ≥ humidity_max + VENT_HYST", () => {
      const v = th.humidity_vent[".validate"];
      const wantFloor = `newData.val() > ${H}`;
      const wantGap   = `newData.val() >= newData.parent().child('humidity_max').val() + ${H}`;
      assert.ok(v.includes(wantFloor), `humidity_vent rule ไม่มี "${wantFloor}"\nที่เจอจริง: ${v}`);
      assert.ok(v.includes(wantGap),   `humidity_vent rule ไม่มี "${wantGap}"\nที่เจอจริง: ${v}`);
    });

    // กันเคสลืมแก้บางจุด: นับจำนวนครั้งที่ค่านี้ปรากฏในนิพจน์ทั้ง 2 บรรทัด ต้องได้ 3 พอดี
    // (humidity_max 1 จุด + humidity_vent 2 จุด) — ถ้าเพิ่ม/ลดเงื่อนไข ต้องมาทบทวนตรงนี้ด้วย
    test("มีจุดที่ใช้ค่านี้ 3 จุดพอดี (ไม่มีจุดไหนตกหล่น)", () => {
      const both = th.humidity_max[".validate"] + " " + th.humidity_vent[".validate"];
      // นับเฉพาะที่อยู่ในรูป "+ H" หรือ "> H" ที่เป็นตัวเลขเดี่ยว (กันไปชน 100 / 0)
      const hits = both.match(new RegExp(`[+>]=?\\s*${H}(?![0-9.])`, "g")) || [];
      assert.equal(
        hits.length, 3,
        `คาดว่ามี 3 จุดที่ใช้ VENT_HYST (= ${H}) แต่เจอ ${hits.length} จุด — ` +
        `ถ้าแก้โครงสร้าง rule จริง ให้อัปเดตเทสต์นี้พร้อมกัน`
      );
    });
  });

  // preset ทั้ง 3 ตัวใน dashboard ต้องผ่านเงื่อนไข vent เอง — ไม่งั้นกดโหลด preset แล้ว rules ปฏิเสธ
  // (preset ทั้ง 3 อยู่พอดีเส้น vent == humidity_max + VENT_HYST ซึ่งเป็นเหตุผลที่เงื่อนไขเป็น ≥ ไม่ใช่ >)
  test("dashboard preset ทุกตัวยังผ่านเงื่อนไข vent ≥ humidity_max + VENT_HYST", () => {
    const src = read("dashboard/index.html");
    // จับ object ของ preset: มี humidity_max และ humidity_vent อยู่ใน object เดียวกัน
    const presets = [...src.matchAll(
      /humidity_max:\s*([0-9.]+)[^}]*?humidity_vent:\s*([0-9.]+)/g
    )];
    assert.ok(presets.length > 0, "หา preset ใน dashboard ไม่เจอ — ถ้าเปลี่ยนรูปแบบ ต้องแก้เทสต์นี้");
    for (const [, hmax, hvent] of presets) {
      const mx = parseFloat(hmax), vt = parseFloat(hvent);
      assert.ok(
        vt === 0 || (vt > H && vt >= mx + H),
        `preset humidity_max=${mx} humidity_vent=${vt} ไม่ผ่านเงื่อนไข (ต้อง ≥ ${mx + H}) — ` +
        `กดโหลด preset นี้แล้ว database rules จะปฏิเสธ`
      );
    }
  });
});
