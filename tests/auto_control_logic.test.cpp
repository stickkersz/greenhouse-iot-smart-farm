// ทดสอบ auto_control_logic.h ตรงๆ ด้วย g++ ธรรมดา — ไม่ต้องใช้ Arduino toolchain
// รัน: g++ -std=c++17 -I smartfarm_firmware tests/auto_control_logic.test.cpp -o /tmp/auto_control_test && /tmp/auto_control_test
#include <cstdio>
#include <cstdlib>
#include "../smartfarm_firmware/auto_control_logic.h"

static int failures = 0;

void check(bool cond, const char* name) {
  if (cond) { printf("  PASS  %s\n", name); }
  else      { printf("  FAIL  %s\n", name); failures++; }
}

// ค่า default จริงจากระบบ: fanOnTemp=35 fanOffTemp=32 pumpOnHum=60 pumpOffHum=75
// {airTemp, airHumidity, fanOnTemp, fanOffTemp, pumpOnHum, pumpOffHum}
static AutoControlInputs base() {
  return {0, 50, 35, 32, 60, 75};   // humidity=50 (ยังไม่ตาย) กันเผลอ trigger pump safety-close ในเทสต์พัดลม
}

int main() {
  printf("== Fan: hot air opens ==\n");
  {
    auto in = base(); in.airTemp = 36; // >= fanOnTemp(35)
    auto d = computeAutoDecisions(in);
    check(d.fanOpen,  "airTemp=36 >= 35 -> fanOpen");
    check(!d.fanClose, "airTemp=36 -> fanClose false");
  }

  printf("== Fan: cool air closes ==\n");
  {
    auto in = base(); in.airTemp = 30; // <= fanOffTemp(32)
    auto d = computeAutoDecisions(in);
    check(d.fanClose,  "airTemp=30 <= 32 -> fanClose");
    check(!d.fanOpen,  "airTemp=30 -> fanOpen false");
  }

  printf("== Fan: hysteresis dead-zone holds state ==\n");
  {
    auto in = base(); in.airTemp = 33.5; // between 32 and 35
    auto d = computeAutoDecisions(in);
    check(!d.fanOpen,  "airTemp=33.5 -> fanOpen false (hold)");
    check(!d.fanClose, "airTemp=33.5 -> fanClose false (hold)");
  }

  printf("== Fan: water temp no longer affects fan ==\n");
  {
    // struct ไม่มี field น้ำแล้ว — พิสูจน์โดยตั้งอากาศเย็น: ต้องปิดเสมอ ไม่ว่าน้ำจะร้อนแค่ไหน
    auto in = base(); in.airTemp = 25;
    auto d = computeAutoDecisions(in);
    check(d.fanClose,  "cool air -> fanClose regardless of any water temp");
    check(!d.fanOpen,  "cool air -> fanOpen false");
  }

  printf("== Fan: open/close never both true (valid config) ==\n");
  {
    bool sawOverlap = false;
    for (float t = 20; t <= 45; t += 0.25f) {
      auto in = base(); in.airTemp = t;
      auto d = computeAutoDecisions(in);
      if (d.fanOpen && d.fanClose) sawOverlap = true;
    }
    check(!sawOverlap, "no airTemp triggers fan open+close simultaneously");
  }

  printf("== Fan: boundary values ==\n");
  {
    auto in = base(); in.airTemp = 35;  // exactly fanOnTemp
    check(computeAutoDecisions(in).fanOpen, "airTemp==fanOnTemp(35) -> fanOpen (>=)");
    in.airTemp = 32;                    // exactly fanOffTemp
    check(computeAutoDecisions(in).fanClose, "airTemp==fanOffTemp(32) -> fanClose (<=)");
  }

  printf("== Pump: dead sensor (humidity==0) forces close, never open ==\n");
  {
    auto in = base(); in.airHumidity = 0;
    auto d = computeAutoDecisions(in);
    check(d.pumpClose, "humidity=0 -> pumpClose true (safety)");
    check(!d.pumpOpen, "humidity=0 -> pumpOpen false (never open on dead sensor)");
  }

  printf("== Pump: dry air opens ==\n");
  {
    auto in = base(); in.airHumidity = 50; // < pumpOnHum(60)
    auto d = computeAutoDecisions(in);
    check(d.pumpOpen,  "humidity=50 < 60 -> pumpOpen");
    check(!d.pumpClose, "humidity=50 -> pumpClose false");
  }

  printf("== Pump: humid air closes ==\n");
  {
    auto in = base(); in.airHumidity = 80; // >= pumpOffHum(75)
    auto d = computeAutoDecisions(in);
    check(d.pumpClose, "humidity=80 >= 75 -> pumpClose");
    check(!d.pumpOpen, "humidity=80 -> pumpOpen false");
  }

  printf("== Pump: hysteresis dead-zone holds state ==\n");
  {
    auto in = base(); in.airHumidity = 65; // between 60 and 75
    auto d = computeAutoDecisions(in);
    check(!d.pumpOpen,  "humidity=65 -> pumpOpen false (hold)");
    check(!d.pumpClose, "humidity=65 -> pumpClose false (hold)");
  }

  printf("== Pump: open/close never both true ==\n");
  {
    bool sawOverlap = false;
    for (float h = 0; h <= 100; h += 0.25f) {
      auto in = base(); in.airHumidity = h;
      auto d = computeAutoDecisions(in);
      if (d.pumpOpen && d.pumpClose) sawOverlap = true;
    }
    check(!sawOverlap, "no humidity value triggers pump open+close simultaneously");
  }

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "SOME FAILED", failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
