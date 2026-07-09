#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

// Magic numbers for powertrain, units of in/rev
#define RATIO_1 2.401
#define RATIO_2 1.947
#define RATIO_3 1.555
#define RATIO_4 1.333
#define RATIO_5 1.190

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

const unsigned int gear_up_pin = 19;
const unsigned int gear_down_pin = 8;
const unsigned int neutral_pin = 4;

void pulseISR()
{
    unsigned long now = micros();
    unsigned long interval = now - lastPulseTime;

    if (interval > 920)
    {
        pulseInterval = interval;
        lastPulseTime = now;
    }
}

const byte inputPin = 2;

void initSensors()
{
    // RPM
    pinMode(inputPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(inputPin), pulseISR, RISING);

    // Gear Up
    pinMode(gear_up_pin, INPUT);

    // Gear Down
    pinMode(gear_down_pin, INPUT);

    // Neutral
    pinMode(neutral_pin, INPUT_PULLUP);
}

int calculateRPM()
{
    if (micros() - lastPulseTime > 500000UL)
        return 0;

    // No pulse measured yet (e.g. first ~500 ms after boot); avoid divide-by-zero.
    if (pulseInterval == 0)
        return 0;

    return (60000000UL / pulseInterval) / 4;
}

enum Sensors
{
    RPM, // Revolutions Per Minute
    AFR, // Air Fuel Ratio
    TPS, // Throttle Position Sensor
    MAP  // Manifold Absolute Pressure
};

int readSensors(int sensor)
{
    switch (sensor)
    {
    case RPM:
        return calculateRPM();

    case AFR:
        return analogRead(A0);

    case TPS:
        return analogRead(A1);

    case MAP:
        return analogRead(A2);

    default:
        return -1;
    }
}

static const unsigned long SHIFT_TIMEOUT_MS = 300UL;

// Dead-reckoned gear state. File-scope so crossCheckGear() can resync it
// when the drivetrain ratio proves it wrong (issue #8).
static int currentGear = 1;
static bool inNeutral = false;
static bool shifting = false;
static bool waitingForRelease = false;
static unsigned long shiftStartMs = 0;

int senseGear()
{
    const unsigned long now = millis();

    // ── Neutral sensor ────────────────────────────────────────────────────
    // Neutral sits between 1st and 2nd (1-N-2-3-4-5-6 sequential box, 2004
    // Yamaha R6), so it is tracked as its own state: down goes to 1st, up
    // goes to 2nd.
    if (!shifting && digitalRead(neutral_pin) == LOW)
    {
        inNeutral = true;
        return 0;
    }

    // ── Both paddles LOW → invalid / ignition off ─────────────────────────
    if (digitalRead(gear_up_pin) == LOW && digitalRead(gear_down_pin) == LOW)
    {
        shifting = false;
        waitingForRelease = false;
        return inNeutral ? 0 : currentGear;
    }

    // ── Both paddles released → stroke complete OR release after timeout ───
    if (digitalRead(gear_up_pin) == HIGH && digitalRead(gear_down_pin) == HIGH)
    {
        shifting = false;
        waitingForRelease = false; // ← also clears the timeout latch
    }

    // ── Timeout guard ─────────────────────────────────────────────────────
    if (shifting && (now - shiftStartMs >= SHIFT_TIMEOUT_MS))
    {
        shifting = false;
        waitingForRelease = true; // ← latch: don't re-arm until released
    }

    // ── Up-shift ──────────────────────────────────────────────────────────
    if (!shifting && !waitingForRelease && // ← guard added
        digitalRead(gear_up_pin) == LOW && (inNeutral || currentGear < 6))
    {
        shifting = true;
        shiftStartMs = now;
        if (inNeutral)
        {
            currentGear = 2;
            inNeutral = false;
        }
        else
        {
            currentGear++;
        }
        return currentGear;
    }

    // ── Down-shift ────────────────────────────────────────────────────────
    if (!shifting && !waitingForRelease && // ← guard added
        digitalRead(gear_down_pin) == LOW && (inNeutral || currentGear > 1))
    {
        shifting = true;
        shiftStartMs = now;
        if (inNeutral)
        {
            currentGear = 1;
            inNeutral = false;
        }
        else
        {
            currentGear--;
        }
        return currentGear;
    }

    return inNeutral ? 0 : currentGear;
}

// ── Gear plausibility cross-check (issue #8) ──────────────────────────────
// senseGear() dead-reckons from paddle presses, so a missed or failed shift
// desyncs it until the car returns to neutral. While the engine is coupled
// to the wheels, the ratio of RPM to true (GPS) ground speed identifies the
// gear, so persistent disagreement means the tracked gear is wrong and we
// resync to the measured one.

// 2004 Yamaha YZF-R6 spec internal gearbox ratios, 1st–6th. Kept separate
// from RATIO_1..5 above, which encode the current (known-suspect) speed
// calibration rather than the spec drivetrain.
static const double GEARBOX_RATIOS[6] = {2.846, 1.947, 1.556, 1.333, 1.190, 1.083};

#define PRIMARY_RATIO 1.955 // 86/44, crank → clutch

// TODO(team): placeholders — measure MR19's actual sprockets and tire
// rolling circumference. If these are far off, no gear ever falls inside
// GEAR_CHECK_TOLERANCE and the cross-check never resyncs (fails safe).
#define FINAL_DRIVE_RATIO 3.000    // stock R6 48/16; MR19 sprockets unconfirmed
#define TIRE_CIRCUMFERENCE_IN 56.5 // ~18 in OD tire; unconfirmed

// Engine RPM per mph of ground speed in a given gear (1 mph = 1056 in/min)
#define RPM_PER_MPH(gearboxRatio) \
    (PRIMARY_RATIO * (gearboxRatio) * FINAL_DRIVE_RATIO * 1056.0 / TIRE_CIRCUMFERENCE_IN)

#define GEAR_CHECK_MIN_SPEED_MPH 10.0 // below this, launch clutch slip and GPS noise dominate
#define GEAR_CHECK_TOLERANCE 0.06     // ±6%; adjacent gears differ by ≥10%
#define GEAR_CHECK_AGREE_FIXES 3      // consecutive GPS fixes (~1/s) before resync

// Gear implied by the observed RPM/speed ratio, or 0 if no gear is within
// tolerance (clutch in, mid-shift, coasting in neutral, or uncalibrated
// drivetrain constants).
int gearFromRatio(int rpm, double speedMph)
{
    const double observed = rpm / speedMph;
    int best = 0;
    double bestError = GEAR_CHECK_TOLERANCE;

    for (int g = 0; g < 6; g++)
    {
        double error = fabs(observed / RPM_PER_MPH(GEARBOX_RATIOS[g]) - 1.0);
        if (error < bestError)
        {
            bestError = error;
            best = g + 1;
        }
    }
    return best;
}

// Call once per fresh GPS speed fix. Resyncs the dead-reckoned gear when the
// drivetrain ratio names the same different gear GEAR_CHECK_AGREE_FIXES
// fixes in a row.
void crossCheckGear(int rpm, double speedMph)
{
    static int candidate = 0;
    static int agreeCount = 0;

    // The neutral switch is ground truth, and mid-shift or low-speed ratios
    // don't identify a gear.
    if (shifting || digitalRead(neutral_pin) == LOW ||
        rpm <= 0 || speedMph < GEAR_CHECK_MIN_SPEED_MPH)
    {
        agreeCount = 0;
        return;
    }

    const int measured = gearFromRatio(rpm, speedMph);

    // No confident match, or it agrees with the tracked gear (a stale
    // tracked neutral is still a disagreement — the car is moving in gear).
    if (measured == 0 || (measured == currentGear && !inNeutral))
    {
        candidate = 0;
        agreeCount = 0;
        return;
    }

    if (measured == candidate)
    {
        agreeCount++;
    }
    else
    {
        candidate = measured;
        agreeCount = 1;
    }

    if (agreeCount >= GEAR_CHECK_AGREE_FIXES)
    {
        currentGear = candidate;
        inNeutral = false;
        candidate = 0;
        agreeCount = 0;
    }
}

double getSpeed(int gear)
{

    switch (gear)
    {

    case 0:
        return 0.001;
        break;

    case 1:
        return readSensors(RPM) * RATIO_1 / 1056.0;
        break;

    case 2:
        return readSensors(RPM) * RATIO_2 / 1056.0;
        break;

    case 3:
        return readSensors(RPM) * RATIO_3 / 1056.0;
        break;

    case 4:
        return readSensors(RPM) * RATIO_4 / 1056.0;
        break;

    case 5:
        return readSensors(RPM) * RATIO_5 / 1056.0;
        break;

    default:
        return -0.5;
        break;
    }
}

float getBatteryVoltage()
{
    long sum = 0;
  
    analogRead(A4);
    for (int i = 0; i < 5; i++) {
        sum += analogRead(A4);
    }

    return (((sum / 1023.0)) * 3.43);
}
#endif