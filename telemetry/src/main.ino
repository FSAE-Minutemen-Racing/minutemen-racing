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
    incrementLaptimer();

    // Once per fresh GPS fix, sanity-check the dead-reckoned gear against
    // the RPM-to-ground-speed ratio (reading mph() clears the updated flag)
    if (gps.speed.isUpdated() && gps.speed.isValid())
        crossCheckGear(readSensors(RPM), gps.speed.mph());

    gear = senseGear();
    speed = getSpeed(gear);
    updateDashboard(gear, speed);
}
