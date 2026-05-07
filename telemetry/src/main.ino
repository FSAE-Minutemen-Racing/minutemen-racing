/*
***NOT READY FOR USE***

K-Line not yet implemented.

Serial port alloactions undecided
*/

#include <Arduino.h>
#include "sensors.hpp"
#include "kline.h"
#include "screen_control.hpp"

int gear;
double speed;

void setup()
{
    Serial1.begin(9600);
    initSensors();
}

void loop()
{
    incrementLaptimer();
    gear = senseGear();
    speed = getSpeed(gear);
    updateDashboard(gear, speed);
}
