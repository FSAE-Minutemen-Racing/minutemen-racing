#include <Arduino.h>
#include "sensors.hpp"
#include "gps.hpp"
#include "server.hpp"
#include "screen_control.hpp"

void setup()
{
    Serial1.begin(DASHBOARD_SERIAL_BAUD);

    initServer();
    initSensors();
    initGPS();
}

void loop()
{
    pollGPS();
    runServer();

    const int rpm = readSensors(RPM);

    // Once per fresh GPS fix, sanity-check the dead-reckoned gear against
    // the RPM-to-ground-speed ratio (reading mph() clears the updated flag)
    if (gps.speed.isUpdated() && gps.speed.isValid())
        crossCheckGear(rpm, gps.speed.mph());

    const int gear = senseGear();
    const double speed = getSpeed(gear, rpm);
    updateDashboard(gear, speed, rpm);
}
