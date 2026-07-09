#pragma once
#ifndef SCREEN_CONTROL_HPP
#define SCREEN_CONTROL_HPP

#define SCREEN_UPDATE_INTERVAL 200

// Battery warning hysteresis band: on below 12.0 V, off above 12.4 V,
// so noise around a single threshold can't chatter the BATT light.
// Values provisional pending confirmation against the pack chemistry (issue #11).
#define BATT_WARN_ON_VOLTAGE 12.0
#define BATT_WARN_OFF_VOLTAGE 12.4

enum Warnings
{
    KILL,
    HEAT,
    STALL,
    BATT
};

bool warning_state[4] = {false, false, false, false};

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

void changeGear(int gear)
{
    Serial1.print('G');

    if (gear > 0 and gear <= 6)
    {
        Serial1.println(gear);
    }
    else if (gear == 0)
    {
        Serial1.println('N');
    }
}

void updateDashboard(int gear, double speed)
{
    static unsigned long lastUpdate = 0;
    unsigned long now = millis();

    if (now - lastUpdate >= SCREEN_UPDATE_INTERVAL)
    {
        lastUpdate = now;

        // Gear
        changeGear(gear);

        // RPM
        Serial1.print('R');
        Serial1.println(readSensors(RPM));

        // Speed
        Serial1.print('S');
        Serial1.println(speed);

        // TPS
        Serial1.print("T");
        Serial1.println(max(0, min(100, (int)((readSensors(TPS) - 195) * 100) / (865 - 195))));

        // Battery voltage and BATT light
        float voltage = getBatteryVoltage();
        if (voltage < BATT_WARN_ON_VOLTAGE and not warning_state[BATT])
        {
            warning_state[BATT] = true;
            warning_lights(BATT, true);
        }
        else if (voltage > BATT_WARN_OFF_VOLTAGE and warning_state[BATT])
        {
            warning_state[BATT] = false;
            warning_lights(BATT, false);
        }
        Serial1.print("V");
        Serial1.println(voltage);
    }
}

unsigned int laptimer = 0;

void incrementLaptimer()
{
    static unsigned long lastTick = 0;
    unsigned long now = millis();

    if (now - lastTick >= 1000)
    {
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
}

#endif