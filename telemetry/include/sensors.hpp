#pragma once
#ifndef SENSORS_HPP
#define SENSORS_HPP

volatile unsigned long lastPulseTime = 0;
volatile unsigned long pulseInterval = 0;

void pulseISR() {
    unsigned long now = micros();
    unsigned long interval = now - lastPulseTime;

    if (interval > 3750) { 
        pulseInterval = interval;
        lastPulseTime = now;
    }
}

const byte inputPin = 2;

void initSensors() {
    pinMode(inputPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(inputPin), pulseISR, FALLING);
}

int calculateRPM() {
    if (micros() - lastPulseTime > 500000) return 0;
    
    return (60000000UL / pulseInterval) / 5; // Experiemental 5:1 raito
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

#endif