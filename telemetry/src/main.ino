#include <Arduino.h>
#include "sensors.hpp"
#include "gps.hpp"
#include "server.hpp"
#include "screen_control.hpp"

int gear;
double speed;

void setup()
{
    Serial1.begin(9600);

    initServer();
    initSensors();
    initGPS();
}

void loop()
{
    pollGPS();
    runServer();
    gear = senseGear();
    speed = getSpeed(gear);
    updateDashboard(gear, speed);
}
