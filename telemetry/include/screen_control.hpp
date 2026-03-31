#pragma once
#ifndef SCREEN_CONTROL_HPP
#define SCREEN_CONTROL_HPP

void neutralSwitch()
{
    Serial1.print('G');
    Serial1.println('N');
}

void updateGear(int gear)
{
    Serial1.print('G');
    Serial1.println(gear);
}

void updateGauges()
{
    // Upper Gauges
    Serial1.print('R');
    Serial1.println(5 * readSensors(RPM));

    Serial1.print('S');
    Serial1.println(0);

    Serial1.print('C');
    Serial1.println(0);

    // Lower Gauges
    Serial1.print('V');
    Serial1.println(0);

    Serial1.print('F');
    Serial1.println(0);

    Serial1.print('N');
    Serial1.println(0);

    Serial1.print("T");
    Serial1.println(max(0, min(100, (int)((readSensors(TPS) - 140) * 100) / (860 - 140))));
}

int laptimer = 0;

void incrementLaptimer()
{
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