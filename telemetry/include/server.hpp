#pragma once
#ifndef SERVER_HPP
#define SERVER_HPP

#include <WiFiS3.h>
#include "gps.hpp"

const char ssid[] = "minutemen-racing";
const char pass[] = "password";
WiFiServer server(80);

// Bounds on how long one HTTP client can hold up loop() (issue #3)
#define SERVER_TIMEOUT_MS 250
#define SERVER_MAX_REQUEST_LEN 128

void initServer()
{
    WiFi.beginAP(ssid, pass);
    server.begin();
}

void runServer()
{
    WiFiClient client = server.available();
    if (!client)
        return;

    // The method and path are always in the first request line, so only that
    // line is read — bounded by a deadline and a length cap so a slow,
    // silent, or half-open client cannot stall loop() (issue #3).
    const unsigned long startMs = millis();
    String request = "";
    bool haveRequestLine = false;

    while (client.connected() && millis() - startMs < SERVER_TIMEOUT_MS)
    {
        if (!client.available())
            continue;

        char c = client.read();
        if (c == '\n')
        {
            haveRequestLine = true;
            break;
        }
        if (request.length() >= SERVER_MAX_REQUEST_LEN)
            break; // oversized request line — drop the client unanswered
        request += c;
    }

    if (haveRequestLine)
    {
        if (request.indexOf("GET /data") >= 0)
        {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/plain");
            client.println("Access-Control-Allow-Origin: *");
            client.println("Connection: close");
            client.println();

            client.print(readSensors(RPM));
            client.print(",");
            client.print(readSensors(AFR));
            client.print(",");
            client.print(readSensors(TPS));
            client.print(",");
            client.print(readSensors(MAP));
            client.print(",");
            client.print(getGPSData());
        }
        else
        {
            // Answer unknown paths (browser favicon probes, "/") so the
            // client closes instead of waiting out its own timeout
            client.println("HTTP/1.1 404 Not Found");
            client.println("Connection: close");
            client.println();
        }
    }
    client.stop();
}

#endif