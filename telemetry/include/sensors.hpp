#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

int gear_up_pin = 8;
int gear_down_pin = 19;
int neutral_pin = 4;

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
    pinMode(inputPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(inputPin), pulseISR, FALLING);

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

int senseGear() {
    static int            gear         = 1;               // assume 1st on boot
    static bool           shifting     = false;
    static unsigned long  shiftStartMs = 0;

    const unsigned long now = millis();

    // ── Neutral sensor: absolute recalibration ────────────────────────────
    if (!shifting && digitalRead(neutral_pin) == LOW) {
        gear = 1;
        return 0;
    }

    // ── Both paddles LOW simultaneously → ignition off, invalid state ─────
    if (digitalRead(gear_up_pin) == LOW && digitalRead(gear_down_pin) == LOW) {
        shifting = false;
        return gear;                                      // hold last known gear
    }

    // ── Both paddles released → shift stroke complete ─────────────────────
    if (digitalRead(gear_up_pin) == HIGH && digitalRead(gear_down_pin) == HIGH) {
        shifting = false;
    }

    // ── Timeout guard ─────────────────────────────────────────────────────
    if (shifting && (now - shiftStartMs >= SHIFT_TIMEOUT_MS)) {
        shifting = false;
    }

    // ── Up-shift ──────────────────────────────────────────────────────────
    if (!shifting && digitalRead(gear_up_pin) == LOW && gear < 6) {
        shifting     = true;
        shiftStartMs = now;
        gear++;
        return gear;
    }

    // ── Down-shift ────────────────────────────────────────────────────────
    if (!shifting && digitalRead(gear_down_pin) == LOW && gear > 1) {
        shifting     = true;
        shiftStartMs = now;
        gear--;
        return gear;
    }

    // ── Steady state ──────────────────────────────────────────────────────
    return gear;
}

#endif