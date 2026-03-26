#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

const byte inputPin = 2;
volatile unsigned long pulseCount = 0;

enum Sensors
{
    RPM, // Revolutions Per Minute
    AFR, // Air Fuel Ratio
    TPS, // Throttle Position Sensor
    MAP  // Manifold Absolute Pressure
};

void pulseISR()
{
    pulseCount++;
}

unsigned long measureFrequency(int windowMs)
{
    pulseCount = 0;
    unsigned long startTime = millis();
    delay(windowMs);
    unsigned long endTime = millis();

    float gateTime = (endTime - startTime) / 1000.0;
    unsigned long frequency = pulseCount / gateTime;

    return frequency;
}

int readSensors(int sensor)
{
    switch (sensor)
    {
    case RPM:
        return measureFrequency(100);

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

void initSensors() {
    pinMode(inputPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(inputPin), pulseISR, FALLING);
}

#endif