#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

// 2004 Yamaha YZF-R6 drivetrain, spec-sheet values (dimensionless reductions)
#define PRIMARY_RATIO 1.955 // 86/44, crank -> clutch
#define RATIO_1 2.846       // 37/13
#define RATIO_2 1.947       // 37/19
#define RATIO_3 1.556       // 28/18
#define RATIO_4 1.333       // 32/24
#define RATIO_5 1.190       // 25/21
#define RATIO_6 1.083       // 26/24

// TODO(team): placeholders — measure MR19's actual sprockets and tire rolling
// circumference; displayed speed is only correct once these are confirmed.
#define FINAL_DRIVE_RATIO 3.000    // stock R6 48/16; MR19 sprockets unconfirmed
#define TIRE_CIRCUMFERENCE_IN 56.5 // ~18 in OD tire; unconfirmed

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

int senseGear()
{
    static int gear = 1;
    static bool inNeutral = false;
    static bool shifting = false;
    static bool waitingForRelease = false;
    static unsigned long shiftStartMs = 0;

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
        return inNeutral ? 0 : gear;
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
        digitalRead(gear_up_pin) == LOW && (inNeutral || gear < 6))
    {
        shifting = true;
        shiftStartMs = now;
        if (inNeutral)
        {
            gear = 2;
            inNeutral = false;
        }
        else
        {
            gear++;
        }
        return gear;
    }

    // ── Down-shift ────────────────────────────────────────────────────────
    if (!shifting && !waitingForRelease && // ← guard added
        digitalRead(gear_down_pin) == LOW && (inNeutral || gear > 1))
    {
        shifting = true;
        shiftStartMs = now;
        if (inNeutral)
        {
            gear = 1;
            inNeutral = false;
        }
        else
        {
            gear--;
        }
        return gear;
    }

    return inNeutral ? 0 : gear;
}

double getSpeed(int gear)
{
    double gearRatio;

    switch (gear)
    {

    case 0:
        return 0.001;

    case 1:
        gearRatio = RATIO_1;
        break;

    case 2:
        gearRatio = RATIO_2;
        break;

    case 3:
        gearRatio = RATIO_3;
        break;

    case 4:
        gearRatio = RATIO_4;
        break;

    case 5:
        gearRatio = RATIO_5;
        break;

    case 6:
        gearRatio = RATIO_6;
        break;

    default:
        return -0.5;
    }

    // mph = engine rev/min ÷ total reduction × in/wheel-rev ÷ 1056 in/min-per-mph
    return readSensors(RPM) * TIRE_CIRCUMFERENCE_IN /
           (PRIMARY_RATIO * gearRatio * FINAL_DRIVE_RATIO * 1056.0);
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