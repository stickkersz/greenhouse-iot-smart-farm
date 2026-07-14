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

// default จริง: fanOnTemp=35 fanOffTemp=32 pumpOnHum=60 pumpOffHum=75
// {airTemp, airHumidity, fanOnTemp, fanOffTemp, pumpOnHum, pumpOffHum, humidifyLatch, coolLatch}
static AutoControlInputs base() {
  return {30, 50, 35, 32, 60, 75, false, false};   // อากาศ 30°C ชื้น 50% latch ปิดทั้งคู่
}

int main() {
  printf("== Fan: hot opens / cool closes / dead-zone holds ==\n");
  {
    auto in = base(); in.airTemp = 36;
    check(computeAutoDecisions(in).fanOpen,  "airTemp=36>=35 -> fanOpen");
    in.airTemp = 30;
    check(computeAutoDecisions(in).fanClose, "airTemp=30<=32 -> fanClose");
    in.airTemp = 33.5f;
    auto d = computeAutoDecisions(in);
    check(!d.fanOpen && !d.fanClose, "airTemp=33.5 -> fan hold (neither)");
  }

  printf("== Pump humidify: dry opens, humid closes (hysteresis) ==\n");
  {
    auto in = base(); in.airTemp = 30; in.airHumidity = 50; // dry (<60), cool
    auto d = computeAutoDecisions(in);
    check(d.pumpOn && d.pumpHumidifyOn, "humidity=50<60 -> pump on (humidify)");
    check(!d.pumpCoolAssistOn, "cool air -> no cooling-assist");
  }
  {
    // latch on from humidify, humidity rises into dead-zone (60..75) -> hold on
    auto in = base(); in.airHumidity = 65; in.pumpHumidifyOn = true;
    auto d = computeAutoDecisions(in);
    check(d.pumpOn && d.pumpHumidifyOn, "humidity=65 dead-zone, latch on -> stays on");
  }
  {
    // humidity reaches hmax -> humidify releases
    auto in = base(); in.airHumidity = 78; in.pumpHumidifyOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.pumpHumidifyOn && !d.pumpOn, "humidity=78>=75 -> humidify off, pump off");
  }

  printf("== Pump cooling-assist: hot opens pump even when humidity comfortable ==\n");
  {
    auto in = base(); in.airTemp = 37; in.airHumidity = 65; // hot, humidity comfortable (60..75, no humidify)
    auto d = computeAutoDecisions(in);
    check(d.fanOpen, "hot -> fan open");
    check(d.pumpOn && d.pumpCoolAssistOn && !d.pumpHumidifyOn, "hot & humidity 65 -> pump on (cooling only, not humidify)");
  }
  {
    // hot but already humid (>=hmax) -> cooling skipped, fan still on
    auto in = base(); in.airTemp = 37; in.airHumidity = 80;
    auto d = computeAutoDecisions(in);
    check(d.fanOpen, "hot -> fan open (regardless of humidity)");
    check(!d.pumpCoolAssistOn, "hot BUT humidity=80>=75 -> no cooling assist (humid gate)");
    check(!d.pumpOn, "humid+hot -> pump off (humidify not needed either)");
  }

  printf("== Pump cooling-assist hysteresis (temp on/off), humidity comfortable ==\n");
  {
    // cool latch on, temp drops into dead-zone (32..35) -> hold on · humidity 65 = no humidify
    auto in = base(); in.airTemp = 33; in.airHumidity = 65; in.pumpCoolAssistOn = true;
    auto d = computeAutoDecisions(in);
    check(d.pumpOn && d.pumpCoolAssistOn, "cool-latch on, temp=33 dead-zone -> stays on");
  }
  {
    // temp drops to <=fanOffTemp -> cooling releases (humidity 65 so humidify does not hold it on)
    auto in = base(); in.airTemp = 31; in.airHumidity = 65; in.pumpCoolAssistOn = true;
    auto d = computeAutoDecisions(in);
    check(!d.pumpCoolAssistOn && !d.pumpOn, "temp=31<=32 -> cooling off, pump off");
  }

  printf("== No stuck-on: cooling done + mid humidity -> pump releases ==\n");
  {
    // the exact case a stateless OR would get wrong: pump was on for cooling,
    // temp now cool, humidity comfortable (mid, not >=hmax) -> must turn OFF
    auto in = base(); in.airTemp = 30; in.airHumidity = 65;
    in.pumpCoolAssistOn = true; in.pumpHumidifyOn = false;
    auto d = computeAutoDecisions(in);
    check(!d.pumpOn, "cool + humidity=65 mid, cool-latch was on -> pump OFF (no stuck-on)");
  }

  printf("== Pump combined: dry AND hot -> both reasons ==\n");
  {
    auto in = base(); in.airTemp = 37; in.airHumidity = 45; // hot + dry
    auto d = computeAutoDecisions(in);
    check(d.pumpHumidifyOn && d.pumpCoolAssistOn && d.pumpOn, "hot+dry -> both latches on");
  }

  printf("== Safety: dead humidity sensor (0) forces pump fully off ==\n");
  {
    auto in = base(); in.airTemp = 40; in.airHumidity = 0; // very hot but sensor dead
    in.pumpHumidifyOn = true; in.pumpCoolAssistOn = true;   // even if latched on
    auto d = computeAutoDecisions(in);
    check(!d.pumpHumidifyOn && !d.pumpCoolAssistOn && !d.pumpOn, "humidity=0 -> pump forced off (safety), never on");
    check(d.fanOpen, "fan still follows temperature (40>=35 -> open)");
  }

  printf("== Pump never chatters: sweep temp x humidity, latch stable ==\n");
  {
    // feed each state back as latch; after settling, on/off must be consistent (no oscillation at a fixed point)
    bool unstable = false;
    for (float t = 20; t <= 45; t += 1.0f) {
      for (float h = 10; h <= 100; h += 5.0f) {
        AutoControlInputs in = {t, h, 35, 32, 60, 75, false, false};
        auto a = computeAutoDecisions(in);
        in.pumpHumidifyOn = a.pumpHumidifyOn; in.pumpCoolAssistOn = a.pumpCoolAssistOn;
        auto b = computeAutoDecisions(in);   // second pass with settled latch
        if (b.pumpOn != a.pumpOn) unstable = true;   // fixed point must not flip
      }
    }
    check(!unstable, "pump reaches a stable fixed point for every (temp,humidity)");
  }

  printf("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASS" : "SOME FAILED", failures, failures == 1 ? "" : "s");
  return failures == 0 ? 0 : 1;
}
