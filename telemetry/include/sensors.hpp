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
    static bool shifting = false;
    static bool waitingForRelease = false;
    static unsigned long shiftStartMs = 0;

    const unsigned long now = millis();

    // ── Neutral sensor ────────────────────────────────────────────────────
    if (!shifting && digitalRead(neutral_pin) == LOW)
    {
        gear = 1;
        return 0;
    }

    // ── Both paddles LOW → invalid / ignition off ─────────────────────────
    if (digitalRead(gear_up_pin) == LOW && digitalRead(gear_down_pin) == LOW)
    {
        shifting = false;
        waitingForRelease = false;
        return gear;
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
        digitalRead(gear_up_pin) == LOW && gear < 6)
    {
        shifting = true;
        shiftStartMs = now;
        gear++;
        return gear;
    }

    // ── Down-shift ────────────────────────────────────────────────────────
    if (!shifting && !waitingForRelease && // ← guard added
        digitalRead(gear_down_pin) == LOW && gear > 1)
    {
        shifting = true;
        shiftStartMs = now;
        gear--;
        return gear;
    }

    return gear;
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
    return (((analogRead(A3) / 1023.0) * 5.0) * 3.2 * 3.42) + 0.22; // plus 0.22 accounts for voltage drop from battery to Vin
}

#endif