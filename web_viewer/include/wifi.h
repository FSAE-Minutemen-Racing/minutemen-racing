#pragma once
#ifndef WIFI_H
#define WIFI_H

#include <ESP8266WiFi.h>

#define DEVICE_NAME "MMR"

const char* ap_ssid = "MMR";
const char* ap_password = "password";

void connectWiFi_AP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
}

#endif