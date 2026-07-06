#include <Arduino.h>
#include "sensors.hpp"
#include "server.hpp"
#include "kline.h"
#include "screen_control.hpp"

int gear;
double speed;

void setup()
{
    Serial1.begin(9600);

    initServer();
    initSensors();
    initKLine();
}

void loop()
{
    runServer();
    updateKLine();
    incrementLaptimer();
    gear = senseGear();
    speed = getSpeed(gear);
    updateDashboard(gear, speed);
}
