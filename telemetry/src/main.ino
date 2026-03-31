#include <Arduino.h>
#include <elapsedMillis.h>
#include "sensors.hpp"
#include "screen_control.hpp"
#include "server.hpp"

elapsedMillis updateGauges_timer;
elapsedMillis incrementLaptimer_timer;

void setup()
{
    Serial1.begin(9600);

    initSensors();
    initServer();
}

void loop()
{

    if (updateGauges_timer >= 100) {
        updateGauges();
        updateGauges_timer = 0;
    }

    if (incrementLaptimer_timer >= 1000) {
        incrementLaptimer();
        incrementLaptimer_timer = 0;
    }

    runServer();
}
