#pragma once
#ifndef SENSOR_HPP
#define SENSOR_HPP

#define VIEWER_SERIAL Serial5

#define AFR_POS_PIN A0
#define AFR_NEG_PIN A1
#define RPM_PIN A2
#define TPS_PIN A3

void initSensors()
{
    pinMode(AFR_POS_PIN, INPUT);
    pinMode(AFR_NEG_PIN, INPUT);
    pinMode(RPM_PIN, INPUT);
    pinMode(TPS_PIN, INPUT);
}

String readSensors()
{
    String sensorData;

    // AFR
    sensorData += analogRead(AFR_POS_PIN) - analogRead(AFR_NEG_PIN);
    sensorData += ',';

    // RPM
    sensorData += analogRead(RPM_PIN);
    sensorData += ',';

    // TPS
    sensorData += analogRead(TPS_PIN);

    return sensorData;
}

#endif