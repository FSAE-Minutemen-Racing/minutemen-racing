#include <Arduino.h>
#include "sensors.hpp"
#include "server.hpp"
#include "screen_control.hpp"

void setup()
{
    Serial1.begin(9600);

    initServer();
    initSensors();
}

void loop()
{
    runServer();
    incrementLaptimer();
    updateDashboard();
}
