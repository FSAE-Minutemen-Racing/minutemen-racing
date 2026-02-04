#include <Arduino.h>

#include "wifi.h"
#include "webserver.hpp"

void setup()
{
    Serial.begin(115200);
    Serial.println("");
    SPIFFS.begin();

    connectWiFi_AP();
    server.begin();
    serverRouting();
}

void loop()
{
    server.handleClient();
}