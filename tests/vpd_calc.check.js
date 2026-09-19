// ตรวจฟังก์ชันคำนวณ VPD ในหน้า dashboard — รัน: npm run test:vpd
// (ไม่ต้องใช้ emulator/Java — อ่าน dashboard/index.html แล้วรันเฉพาะบล็อกฟังก์ชันล้วนที่คั่นด้วย VPD_BEGIN/VPD_END)
//
// ทำไมดึงบล็อกจาก index.html แทนที่จะแยกเป็นไฟล์ .js: โปรเจกต์นี้ไม่มี build step และ dashboard เป็นไฟล์เดียวโดยตั้งใจ
// ตัวเทสต์จึงอ่านโค้ดจริงที่จะถูก deploy ขึ้นไป ไม่ใช่สำเนา
//
// ค่าอ้างอิง: ความดันไอน้ำอิ่มตัวมาตรฐาน 25 °C ≈ 3.17 kPa, 30 °C ≈ 4.24 kPa (ตาราง psychrometric ทั่วไป)
// ⚠️ ช่วงเป้าหมาย VPD_BAND (0.8–1.2 kPa) ยังเป็น Unverified — เทสต์นี้เช็คแค่ว่า "ขอบเขตทำงานตามที่เขียนไว้"
//    ไม่ได้ยืนยันว่าช่วงนี้เหมาะกับพืชชนิดไหน
"use strict";

const { test, describe } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const vm = require("node:vm");

const html = fs.readFileSync(path.join(__dirname, "..", "dashboard", "index.html"), "utf8");

function loadVpdBlock() {
  const begin = html.split("// VPD_BEGIN").length - 1;
  const end = html.split("// VPD_END").length - 1;
  assert.equal(begin, 1, "ต้องมี // VPD_BEGIN เพียงหนึ่งจุดใน dashboard/index.html");
  assert.equal(end, 1, "ต้องมี // VPD_END เพียงหนึ่งจุดใน dashboard/index.html");
  const src = html.slice(html.indexOf("// VPD_BEGIN"), html.indexOf("// VPD_END"));
  const ctx = {};
  vm.runInNewContext(`${src}\nresult = { calcVPD, vpdStatus, vpdBandNote, VPD_BAND };`, ctx);
  return ctx.result;
}

const { calcVPD, vpdStatus, vpdBandNote, VPD_BAND } = loadVpdBlock();
const near = (actual, expected, tol = 0.02) =>
  assert.ok(Math.abs(actual - expected) <= tol, `expected ${expected} ±${tol}, got ${actual}`);

describe("calcVPD", () => {
  test("25 °C / 50% RH ≈ 1.58 kPa", () => near(calcVPD(25, 50), 1.585));
  test("30 °C / 70% RH ≈ 1.27 kPa", () => near(calcVPD(30, 70), 1.273));
  test("RH 0% = ความดันไอน้ำอิ่มตัวเต็ม (25 °C ≈ 3.17 kPa)", () => near(calcVPD(25, 0), 3.169));
  test("RH 100% = 0 kPa", () => near(calcVPD(25, 100), 0, 0.001));
  test("อุณหภูมิสูงขึ้น ความชื้นเท่าเดิม → VPD สูงขึ้น", () => {
    assert.ok(calcVPD(32, 60) > calcVPD(26, 60));
  });
  test("ความชื้นสูงขึ้น อุณหภูมิเท่าเดิม → VPD ต่ำลง", () => {
    assert.ok(calcVPD(28, 80) < calcVPD(28, 50));
  });
  test("ค่าไม่ครบหรือไม่ใช่ตัวเลข → null (ไม่ใช่ NaN/0)", () => {
    for (const [t, rh] of [[null, 50], [25, null], [undefined, 50], [25, undefined], [NaN, 50], [25, NaN], ["25", 50], [25, "50"]]) {
      assert.equal(calcVPD(t, rh), null, `calcVPD(${t}, ${rh})`);
    }
  });
  test("RH นอกช่วง 0–100 → null (กันค่า sensor เพี้ยนแล้วโชว์ VPD ติดลบ)", () => {
    assert.equal(calcVPD(25, -1), null);
    assert.equal(calcVPD(25, 100.5), null);
  });
});

describe("vpdStatus", () => {
  test("ค่าคงที่ช่วงอ้างอิงเป็นตัวเลขและ min < max", () => {
    assert.equal(typeof VPD_BAND.min, "number");
    assert.equal(typeof VPD_BAND.max, "number");
    assert.ok(VPD_BAND.min < VPD_BAND.max);
  });
  test("ต่ำกว่าช่วง → low", () => assert.equal(vpdStatus(VPD_BAND.min - 0.01), "low"));
  test("ขอบล่างและขอบบนอยู่ในช่วง → ok", () => {
    assert.equal(vpdStatus(VPD_BAND.min), "ok");
    assert.equal(vpdStatus(VPD_BAND.max), "ok");
  });
  test("สูงกว่าช่วง → high", () => assert.equal(vpdStatus(VPD_BAND.max + 0.01), "high"));
  test("null → null (ไม่ตัดสินเมื่อไม่มีค่า)", () => assert.equal(vpdStatus(null), null));
  test("ข้อความบนการ์ดอ้างช่วงเดียวกับค่าคงที่ และบอกว่ายังไม่ยืนยัน (กันตัวเลขกับป้ายไม่ตรงกัน)", () => {
    const note = vpdBandNote();
    assert.ok(note.includes(String(VPD_BAND.min)) && note.includes(String(VPD_BAND.max)));
    assert.ok(note.includes("ยังไม่ยืนยัน"));
  });
});

// ── การ์ด VPD: รัน renderVpdCard() จริงกับ DOM ปลอม ───────────────────────────
// เปิด dashboard จริงในเครื่องนี้ไม่ได้ (ต้อง login Firebase) จึงดึงฟังก์ชันจริงจาก index.html ไปรันกับ element ปลอม
// เพื่อเช็คทุกสถานะที่การ์ดแสดงได้: รอข้อมูล / ต่ำ / เหมาะสม / สูง / เซ็นเซอร์เสีย / ขัดข้อง
function extractFn(name) {
  const head = `function ${name}(`;
  const start = html.indexOf(head);
  assert.ok(start >= 0, `ไม่พบ ${head} ใน dashboard/index.html`);
  const lineEnd = html.indexOf("\n", start);
  const firstLine = html.slice(start, lineEnd);
  if (/\}\s*$/.test(firstLine) && !firstLine.trim().endsWith("{")) return firstLine;   // ฟังก์ชันบรรทัดเดียว
  const close = html.indexOf("\n}\n", start);
  assert.ok(close > start, `หาปิดวงเล็บของ ${name} ไม่เจอ`);
  return html.slice(start, close + 2);
}

function makeCard() {
  const els = {};
  const get = (id) => (els[id] ??= {
    innerHTML: "", className: "", textContent: "",
    classes: new Set(),
    classList: { toggle(c, on) { on ? els[id].classes.add(c) : els[id].classes.delete(c); } },
  });
  const code = [
    html.slice(html.indexOf("// VPD_BEGIN"), html.indexOf("// VPD_END")),
    extractFn("buildGauge"), extractFn("setBadge"), extractFn("setText"), extractFn("renderVpdCard"),
    "var sysStatus = {}; var sensorLive = {};",
    "function __run(sys, live) { sysStatus = sys; sensorLive = live; renderVpdCard(); }",
  ].join("\n");
  const ctx = { document: { getElementById: get } };
  vm.runInNewContext(code, ctx);
  return { els, get, run: ctx.__run };
}

describe("renderVpdCard", () => {
  test("ยังไม่มีข้อมูล → ไม่วาดเกจ คงสถานะรอข้อมูล", () => {
    const { get, run } = makeCard();
    run({}, {});
    assert.equal(get("vpdGauge").innerHTML, "");
    assert.equal(get("vpdBadge").textContent, "");
  });
  test("อากาศแห้ง (25 °C/50%) → สูง 🟡 และเกจโชว์ 2 ทศนิยม", () => {
    const { get, run } = makeCard();
    run({}, { airTemp: 25, airHum: 50 });
    assert.equal(get("vpdBadge").textContent, "สูง");
    assert.ok(get("vpdStatus").textContent.includes("🟡"));
    assert.ok(get("vpdGauge").innerHTML.includes(">1.58<"));
    assert.equal(get("vpdGauge").classes.has("stale"), false);
  });
  test("ในช่วง (25 °C/70%) → เหมาะสม 🟢 badge-ok", () => {
    const { get, run } = makeCard();
    run({}, { airTemp: 25, airHum: 70 });
    assert.equal(get("vpdBadge").textContent, "เหมาะสม");
    assert.ok(get("vpdBadge").className.includes("badge-ok"));
    assert.ok(get("vpdStatus").textContent.includes("🟢"));
  });
  test("อากาศชื้น (25 °C/90%) → ต่ำ 🔵", () => {
    const { get, run } = makeCard();
    run({}, { airTemp: 25, airHum: 90 });
    assert.equal(get("vpdBadge").textContent, "ต่ำ");
    assert.ok(get("vpdStatus").textContent.includes("🔵"));
  });
  test("ทุกสถานะมีข้อความกำกับ ไม่พึ่งสีอย่างเดียว", () => {
    for (const rh of [50, 70, 90]) {
      const { get, run } = makeCard();
      run({}, { airTemp: 25, airHum: rh });
      assert.ok(get("vpdBadge").textContent.length > 0);
      assert.ok(get("vpdStatus").textContent.length > 0);
    }
  });
  test("เซ็นเซอร์เสีย (sensor_stale) → ไม่คำนวณจากค่าเก่า โชว์ — และหรี่เกจ", () => {
    const { get, run } = makeCard();
    run({ sensor_stale: true }, { airTemp: 25, airHum: 70 });
    assert.ok(get("vpdGauge").innerHTML.includes(">—<"));
    assert.equal(get("vpdGauge").classes.has("stale"), true);
    assert.equal(get("vpdBadge").textContent, "เสีย");
  });
  test("อ่านค่าไม่ได้ชั่วคราว (sensor_ok=false) → ขัดข้อง ไม่แสดง VPD", () => {
    const { get, run } = makeCard();
    run({ sensor_ok: false }, { airTemp: 25, airHum: 70 });
    assert.equal(get("vpdBadge").textContent, "ขัดข้อง");
    assert.ok(get("vpdGauge").innerHTML.includes(">—<"));
  });
  test("กลับมาปกติหลังเสีย → เลิกหรี่เกจและคำนวณใหม่", () => {
    const { get, run } = makeCard();
    run({ sensor_stale: true }, { airTemp: 25, airHum: 70 });
    run({}, { airTemp: 25, airHum: 70 });
    assert.equal(get("vpdGauge").classes.has("stale"), false);
    assert.equal(get("vpdBadge").textContent, "เหมาะสม");
  });
  test("ป้ายช่วงอ้างอิงชั่วคราวขึ้นเสมอ", () => {
    const { get, run } = makeCard();
    run({}, {});
    assert.ok(get("vpdNote").textContent.includes("ยังไม่ยืนยัน"));
  });
});
