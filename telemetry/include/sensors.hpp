#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

#include <math.h>

// 2004 Yamaha YZF-R6 drivetrain, spec-sheet values (dimensionless reductions)
#define PRIMARY_RATIO 1.955 // 86/44, crank -> clutch
#define RATIO_1 2.846       // 37/13
#define RATIO_2 1.947       // 37/19
#define RATIO_3 1.556       // 28/18
#define RATIO_4 1.333       // 32/24
#define RATIO_5 1.190       // 25/21
#define RATIO_6 1.083       // 26/24

// MR19 drivetrain, measured on-car 2026-07-18.
#define FINAL_DRIVE_RATIO 3.48     // measured MR19 sprockets
#define TIRE_CIRCUMFERENCE_IN 64.4 // measured 20.5 in OD tire × pi

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

const unsigned int gear_up_pin = 19;
const unsigned int gear_down_pin = 8;
const unsigned int neutral_pin = 4;
const unsigned int kill_switch_pin = 6;

// Coolant temperature sender on A3. Defaults target a 0.5-4.5 V linear
// automotive sender; update these calibration values if the car uses a
// different sensor.
#define COOLANT_SENSOR_PIN A3
#define COOLANT_ADC_MAX 1023.0f
#define COOLANT_SENSOR_REF_VOLTAGE 5.0f
#define COOLANT_SENSOR_MIN_VOLTAGE 0.5f
#define COOLANT_SENSOR_MAX_VOLTAGE 4.5f
#define COOLANT_SENSOR_MIN_TEMP_F -40.0f
#define COOLANT_SENSOR_MAX_TEMP_F 300.0f

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

    // Kill switch sense. Wire an isolated switch contact from D6 to ground;
    // INPUT_PULLUP makes LOW mean kill switch active.
    pinMode(kill_switch_pin, INPUT_PULLUP);
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
    MAP, // Manifold Absolute Pressure
    COOLANT_TEMP_F
};

float adcToCoolantTempF(float adc)
{
    float voltage = (adc / COOLANT_ADC_MAX) * COOLANT_SENSOR_REF_VOLTAGE;

    if (voltage < COOLANT_SENSOR_MIN_VOLTAGE)
        voltage = COOLANT_SENSOR_MIN_VOLTAGE;
    else if (voltage > COOLANT_SENSOR_MAX_VOLTAGE)
        voltage = COOLANT_SENSOR_MAX_VOLTAGE;

    const float sensorSpan = COOLANT_SENSOR_MAX_VOLTAGE - COOLANT_SENSOR_MIN_VOLTAGE;
    const float tempSpan = COOLANT_SENSOR_MAX_TEMP_F - COOLANT_SENSOR_MIN_TEMP_F;
    return COOLANT_SENSOR_MIN_TEMP_F +
           ((voltage - COOLANT_SENSOR_MIN_VOLTAGE) / sensorSpan) * tempSpan;
}

float getCoolantTempF()
{
    long sum = 0;

    analogRead(COOLANT_SENSOR_PIN);
    for (int i = 0; i < 5; i++)
    {
        sum += analogRead(COOLANT_SENSOR_PIN);
    }

    return adcToCoolantTempF(sum / 5.0f);
}

bool isKillSwitchActive()
{
    return digitalRead(kill_switch_pin) == LOW;
}

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

    case COOLANT_TEMP_F:
    {
        float temp = getCoolantTempF();
        return temp >= 0.0f ? (int)(temp + 0.5f) : (int)(temp - 0.5f);
    }

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
    const int neutralLevel = digitalRead(neutral_pin);
    const int gearUpLevel = digitalRead(gear_up_pin);
    const int gearDownLevel = digitalRead(gear_down_pin);

    // ── Neutral sensor ────────────────────────────────────────────────────
    // Neutral sits between 1st and 2nd (1-N-2-3-4-5-6 sequential box, 2004
    // Yamaha R6), so it is tracked as its own state: down goes to 1st, up
    // goes to 2nd.
    if (!shifting && neutralLevel == LOW)
    {
        inNeutral = true;
        return 0;
    }

    // ── Both paddles LOW → invalid / ignition off ─────────────────────────
    if (gearUpLevel == LOW && gearDownLevel == LOW)
    {
        shifting = false;
        waitingForRelease = false;
        return inNeutral ? 0 : currentGear;
    }

    // ── Both paddles released → stroke complete OR release after timeout ───
    if (gearUpLevel == HIGH && gearDownLevel == HIGH)
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
        gearUpLevel == LOW && (inNeutral || currentGear < 6))
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
        gearDownLevel == LOW && (inNeutral || currentGear > 1))
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

// Spec gearbox ratios 1st–6th, indexed by gear - 1. Same constants as
// getSpeed() so a calibration edit happens in one place (top of file).
static const double GEARBOX_RATIOS[6] = {RATIO_1, RATIO_2, RATIO_3,
                                         RATIO_4, RATIO_5, RATIO_6};
static const double SPEED_MPH_PER_RPM[6] = {
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_1 * FINAL_DRIVE_RATIO * 1056.0),
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_2 * FINAL_DRIVE_RATIO * 1056.0),
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_3 * FINAL_DRIVE_RATIO * 1056.0),
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_4 * FINAL_DRIVE_RATIO * 1056.0),
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_5 * FINAL_DRIVE_RATIO * 1056.0),
    TIRE_CIRCUMFERENCE_IN / (PRIMARY_RATIO * RATIO_6 * FINAL_DRIVE_RATIO * 1056.0),
};

// Engine RPM per mph of ground speed in a given gear (1 mph = 1056 in/min).
#define RPM_PER_MPH(gearboxRatio) \
    (PRIMARY_RATIO * (gearboxRatio) * FINAL_DRIVE_RATIO * 1056.0 / TIRE_CIRCUMFERENCE_IN)

#define GEAR_CHECK_MIN_SPEED_MPH 10.0 // below this, launch clutch slip and GPS noise dominate
#define GEAR_CHECK_TOLERANCE 0.06     // ±6% outer bound. 5th/6th differ by only
                                      // 9.9%, so their bands overlap slightly;
                                      // nearest match decides. Don't widen this.
#define GEAR_CHECK_AGREE_FIXES 3      // consecutive GPS fixes (~1/s) before resync
#define GEAR_CHECK_MAX_DELTA_MPH 3.0  // speed change per fix above which GPS lag
                                      // skews the ratio toward the wrong gear

// Gear implied by the observed RPM/speed ratio, or 0 if no gear is within
// tolerance (clutch in, mid-shift, or coasting in neutral).
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
    static double lastSpeedMph = -1.0;

    const double deltaMph =
        (lastSpeedMph < 0) ? 2 * GEAR_CHECK_MAX_DELTA_MPH : fabs(speedMph - lastSpeedMph);
    lastSpeedMph = speedMph;

    // The neutral switch is ground truth; mid-shift or low-speed ratios don't
    // identify a gear. Under hard acceleration or braking the GPS speed lags
    // true speed, which biases the observed ratio toward an adjacent gear, so
    // those samples are skipped too.
    if (shifting || digitalRead(neutral_pin) == LOW ||
        rpm <= 0 || speedMph < GEAR_CHECK_MIN_SPEED_MPH ||
        deltaMph > GEAR_CHECK_MAX_DELTA_MPH)
    {
        candidate = 0;
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

double getSpeed(int gear, int rpm)
{
    if (gear == 0)
        return 0.0;

    if (gear < 1 || gear > 6)
        return -0.5;

    // mph = engine rev/min × precomputed per-gear drivetrain conversion.
    return rpm * SPEED_MPH_PER_RPM[gear - 1];
}

double getSpeed(int gear)
{
    return getSpeed(gear, readSensors(RPM));
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
