#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

const byte rpmPin = 2;
volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;
volatile bool newPulse = false;

enum Sensors
{
    RPM, // Revolutions Per Minute
    AFR, // Air Fuel Ratio
    TPS, // Throttle Position Sensor
    MAP  // Manifold Absolute Pressure
};

void handlePulse()
{
    unsigned long now = micros();
    pulseInterval = now - lastPulseTime;
    lastPulseTime = now;
    newPulse = true;
}

void initSensors()
{
    pinMode(rpmPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(rpmPin), handlePulse, FALLING);
}

float measureRPM()
{
    if (newPulse)
    {
        // Disable interrupts briefly to read volatile variables safely
        noInterrupts();
        unsigned long interval = pulseInterval;
        newPulse = false;
        interrupts();

        // Calculate RPM: (1,000,000 micros / interval) * 60 seconds
        // Note: If your sensor gives 2 pulses per revolution, divide by 2
        float rpm = (60000000.0 / interval);
        return rpm;
    }

    // Timeout: If no pulse for 2 seconds, assume 0 RPM
    if (micros() - lastPulseTime > 2000000)
    {
        return 0.0F;
    }
}

int readSensors(int sensor)
{
    switch (sensor)
    {
    case RPM:
        return measureRPM();

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

#endif