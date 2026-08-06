#include <Arduino.h>
#include "sensors.hpp"
#include "kline.h"
#include "gps.hpp"
#include "screen_control.hpp"

void setup()
{
    Serial1.begin(DASHBOARD_SERIAL_BAUD);

    initSensors();
    initKLine();
    initGPS();
}

void loop()
{
    updateKLine();
    pollGPS();

    const int rpm = calculateRPM();

    // Once per fresh GPS fix, sanity-check the dead-reckoned gear against
    // the RPM-to-ground-speed ratio (reading mph() clears the updated flag)
    if (gps.speed.isUpdated() && gps.speed.isValid())
        crossCheckGear(rpm, gps.speed.mph());

    const int gear = senseGear();
    const double speed = getSpeed(gear, rpm);
    updateDashboard(gear, speed, rpm);
}
