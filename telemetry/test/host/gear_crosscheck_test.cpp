// Host-side scenario tests for the gear plausibility cross-check in
// telemetry/include/sensors.hpp (issue #8). Run with ./run.sh — no hardware
// or PlatformIO needed.
//
// sensors.hpp is header-only and includes nothing itself, so the Arduino
// surface it touches is stubbed below and the header is included unmodified.
// Its file-scope statics (currentGear, inNeutral, shifting, ...) land in this
// translation unit, so scenarios can set up and inspect state directly.

#include <math.h>
#include <stdio.h>

// ── Arduino surface stubs ─────────────────────────────────────────────────
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LOW 0
#define HIGH 1
#define RISING 3
#define A0 100
#define A1 101
#define A2 102
#define A3 103
#define A4 104
#define digitalPinToInterrupt(p) (p)
typedef unsigned char byte;

static unsigned long fakeMillisNow = 0;
static unsigned long fakeMicrosNow = 0;
static int pinLevels[32]; // indexed by digital pin number, default LOW

unsigned long millis() { return fakeMillisNow; }
unsigned long micros() { return fakeMicrosNow; }
void pinMode(unsigned int, int) {}
int digitalRead(unsigned int pin) { return pinLevels[pin]; }
int analogRead(int) { return 0; }
void attachInterrupt(unsigned int, void (*)(), int) {}

#include "../../include/sensors.hpp"

// ── Harness ───────────────────────────────────────────────────────────────
static int failures = 0;

#define CHECK(cond)                                                       \
    do                                                                    \
    {                                                                     \
        if (!(cond))                                                      \
        {                                                                 \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
            failures++;                                                   \
        }                                                                 \
    } while (0)

// Engine RPM at a given ground speed in a given gear (1-6).
static int rpmFor(int gear, double mph)
{
    return (int)(RPM_PER_MPH(GEARBOX_RATIOS[gear - 1]) * mph + 0.5);
}

// Reset the dead-reckoning state sensors.hpp tracks between scenarios.
static void resetState(int gear)
{
    fakeMillisNow = 0;
    currentGear = gear;
    inNeutral = false;
    shifting = false;
    waitingForRelease = false;
    gearUpInput = {HIGH, HIGH, 0};
    gearDownInput = {HIGH, HIGH, 0};
    neutralInput = {HIGH, HIGH, 0};
    pinLevels[neutral_pin] = HIGH;
    pinLevels[gear_up_pin] = HIGH;
    pinLevels[gear_down_pin] = HIGH;
}

// Feed one deliberately gated fix (rpm 0): resets crossCheckGear's internal
// candidate/agreement counters and seats its last-speed memory at `mph` so
// the next real fix isn't skipped by the acceleration guard.
static void settleAt(double mph)
{
    crossCheckGear(0, mph);
}

int main()
{
    // A brief LOW pulse is electrical noise, not an upshift.
    resetState(3);
    pinLevels[gear_up_pin] = LOW;
    CHECK(senseGear() == 3);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS - 1;
    CHECK(senseGear() == 3);
    pinLevels[gear_up_pin] = HIGH;
    fakeMillisNow++;
    CHECK(senseGear() == 3);

    // The downshift input gets the same noise rejection.
    resetState(4);
    pinLevels[gear_down_pin] = LOW;
    CHECK(senseGear() == 4);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS - 1;
    CHECK(senseGear() == 4);
    pinLevels[gear_down_pin] = HIGH;
    fakeMillisNow++;
    CHECK(senseGear() == 4);

    // A sustained LOW is accepted once and requires a debounced release
    // before another shift can be counted.
    resetState(3);
    pinLevels[gear_up_pin] = LOW;
    CHECK(senseGear() == 3);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 4);
    fakeMillisNow += SHIFT_TIMEOUT_MS;
    CHECK(senseGear() == 4);
    pinLevels[gear_up_pin] = HIGH;
    CHECK(senseGear() == 4);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 4);

    // A brief neutral-switch glitch must not report or latch neutral.
    resetState(3);
    pinLevels[neutral_pin] = LOW;
    CHECK(senseGear() == 3);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS - 1;
    CHECK(senseGear() == 3);
    pinLevels[neutral_pin] = HIGH;
    fakeMillisNow++;
    CHECK(senseGear() == 3);
    CHECK(!inNeutral);

    // Neutral is reported only while its active-low signal is present.
    resetState(1);
    pinLevels[neutral_pin] = LOW;
    CHECK(senseGear() == 1);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 0);
    CHECK(inNeutral);
    pinLevels[neutral_pin] = HIGH;
    CHECK(senseGear() == 0);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 1);
    CHECK(inNeutral);

    // The remembered neutral state still identifies a subsequent upshift as
    // neutral -> 2nd, even though the switch released before the stroke.
    pinLevels[gear_up_pin] = LOW;
    CHECK(senseGear() == 1);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 2);
    CHECK(!inNeutral);

    // Invalid simultaneous paddle inputs must not preserve a stale neutral
    // claim after the neutral signal disappears.
    resetState(3);
    inNeutral = true;
    pinLevels[gear_up_pin] = LOW;
    pinLevels[gear_down_pin] = LOW;
    CHECK(senseGear() == 3);
    fakeMillisNow += GEAR_INPUT_DEBOUNCE_MS;
    CHECK(senseGear() == 3);
    CHECK(inNeutral);

    // ── gearFromRatio identifies every gear from a clean ratio ────────────
    for (int g = 1; g <= 6; g++)
        CHECK(gearFromRatio(rpmFor(g, 30.0), 30.0) == g);

    // ── gearFromRatio rejects ratios that match no gear ───────────────────
    // Clutch pulled, engine near idle at speed: far below 6th's ratio.
    CHECK(gearFromRatio(2000, 40.0) == 0);
    // Halfway (geometrically) between 3rd and 4th: ~8% from both, > ±6%.
    {
        double between = sqrt(RPM_PER_MPH(RATIO_3) * RPM_PER_MPH(RATIO_4));
        CHECK(gearFromRatio((int)(between * 30.0), 30.0) == 0);
    }

    // ── Missed upshift resyncs after exactly 3 agreeing fixes ─────────────
    resetState(3); // tracked 3rd, car actually in 4th
    settleAt(40.0);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 3);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 3);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 4);

    // ── A gated fix between disagreements restarts the count ──────────────
    resetState(3);
    settleAt(40.0);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    pinLevels[neutral_pin] = LOW; // blip of (spurious) neutral: gated fix
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    pinLevels[neutral_pin] = HIGH;
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 3); // only 2 consecutive since the blip
    crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 4);

    // ── Neutral switch is ground truth: never resync while it reads LOW ───
    resetState(2);
    pinLevels[neutral_pin] = LOW;
    settleAt(25.0);
    for (int i = 0; i < 5; i++)
        crossCheckGear(rpmFor(3, 25.0), 25.0);
    CHECK(currentGear == 2);

    // ── Mid-shift fixes are ignored ───────────────────────────────────────
    resetState(3);
    settleAt(40.0);
    shifting = true;
    for (int i = 0; i < 5; i++)
        crossCheckGear(rpmFor(4, 40.0), 40.0);
    CHECK(currentGear == 3);

    // ── Below the speed floor nothing resyncs ─────────────────────────────
    resetState(2);
    settleAt(8.0);
    for (int i = 0; i < 5; i++)
        crossCheckGear(rpmFor(1, 8.0), 8.0);
    CHECK(currentGear == 2);

    // ── Acceleration guard: fast-changing speed is skipped, steady resyncs ─
    resetState(3); // car actually in 4th throughout
    settleAt(30.0);
    for (int i = 1; i <= 3; i++) // +8 mph per fix: every sample gated
    {
        double mph = 30.0 + 8.0 * i;
        crossCheckGear(rpmFor(4, mph), mph);
    }
    CHECK(currentGear == 3);
    for (int i = 0; i < 3; i++) // speed settles: resync proceeds
        crossCheckGear(rpmFor(4, 54.0), 54.0);
    CHECK(currentGear == 4);

    // ── Stale tracked neutral while moving in gear gets corrected ─────────
    resetState(1);
    inNeutral = true; // tracker thinks neutral; switch (HIGH) says otherwise
    settleAt(25.0);
    for (int i = 0; i < 3; i++)
        crossCheckGear(rpmFor(2, 25.0), 25.0);
    CHECK(currentGear == 2);
    CHECK(!inNeutral);

    // ── Agreeing fixes leave the tracked gear alone ───────────────────────
    resetState(5);
    settleAt(60.0);
    for (int i = 0; i < 5; i++)
        crossCheckGear(rpmFor(5, 60.0), 60.0);
    CHECK(currentGear == 5);

    if (failures == 0)
        printf("PASS: all gear cross-check scenarios\n");
    else
        printf("%d FAILURE(S)\n", failures);
    return failures;
}
