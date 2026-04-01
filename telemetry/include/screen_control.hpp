#pragma once
#ifndef SCREEN_CONTROL_HPP
#define SCREEN_CONTROL_HPP

void updateDashboard()
{
    Serial1.print('R');
    Serial1.println(5 * readSensors(RPM));

    Serial1.print('S');
    Serial1.println(readSensors(RPM));

    Serial1.print('C');
    Serial1.println(readSensors(MAP));

    Serial1.print("T");
    Serial1.println(max(0, min(100, (int)((readSensors(TPS) - 140) * 100) / (865 - 140))));
}

unsigned int laptimer = 0;

void incrementLaptimer()
{
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (now - lastTick < 1000) return;
    lastTick = now;

    laptimer++;

    int seconds = laptimer % 60;
    int minutes = (laptimer / 60) % 60;
    int hours = (laptimer / 3600) % 24;

    char laptimerString[8];
    sprintf(laptimerString, "%02d:%02d:%02d", hours, minutes, seconds);

    Serial1.print('L');
    Serial1.println(laptimerString);
}

enum Warnings
{
    KILL,
    HEAT,
    STALL,
    BATT
};

void warning_lights(int warning, bool state)
{
    switch (warning)
    {

    case KILL:
        if (state)
            Serial1.println('K');
        else
            Serial1.println('k');
        break;

    case HEAT:
        if (state)
            Serial1.println('H');
        else
            Serial1.println('h');
        break;

    case STALL:
        if (state)
            Serial1.println('X');
        else
            Serial1.println('x');
        break;

    case BATT:
        if (state)
            Serial1.println('B');
        else
            Serial1.println('b');
        break;
    }
}


#endif