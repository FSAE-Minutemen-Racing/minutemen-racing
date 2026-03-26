#include <Arduino.h>
#include <Arduino_LED_Matrix.h>
#include "sensors.hpp"
#include "kline.h"
#include "server.hpp"
#include "screen_control.hpp"

ArduinoLEDMatrix matrix;
const uint32_t logo[] = {
		0x6d8ffc6d,
		0xe6db6d92,
		0xdb05a009
};

void setup()
{
    Serial1.begin(9600);

    // matrix.begin();
    // matrix.loadFrame(logo);

    initServer();

    initSensors();

    // Clear warning lights
    Serial1.println('k');
    Serial1.println('b');
    Serial1.println('x');
    Serial1.println('h');
}

void loop()
{
    updateCarState();
    updateDashboard();

    runServer();
}
