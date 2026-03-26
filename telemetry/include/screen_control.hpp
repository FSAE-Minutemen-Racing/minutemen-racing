#pragma once
#ifndef SCREEN_CONTROL_HPP
#define SCREEN_CONTROL_HPP

int current_screen = 1;
bool engine_running = false;

int laptimer = 0;
int starttime = 0;

void updateCarState()
{
    if (readSensors(MAP) < 20 and current_screen != 1)
    {
        current_screen = 1;
        Serial1.println('1');
    }

    else if (readSensors(MAP) >= 20 and current_screen != 2)
    {
        current_screen = 2;
        Serial1.println('2');
    }

    if (readSensors(RPM) >= 10 and engine_running != true)
    {
        laptimer = 0;
        engine_running = true;
    }

    else if (readSensors(RPM) < 10)
    {
        engine_running = false;
    }
}

void updateDashboard()
{
    Serial1.print('R');
    Serial1.println(5 * readSensors(RPM));

    Serial1.print("T");
    Serial1.println(max(0, min(100, (int)((readSensors(TPS) - 130) * 100) / (865 - 120))));

    if (engine_running == true)
    {
        if (millis() - starttime >= 1000)
        {
            starttime = millis();

            laptimer++;
            int hours = laptimer / 3600;
            int minutes = (laptimer % 3600) / 60;
            int seconds = laptimer % 60;

            Serial1.print('L');
            Serial1.print(hours);
            Serial1.print(':');
            Serial1.print(minutes);
            Serial1.print(':');
            Serial1.println(seconds);
        }
    }
}

#endif