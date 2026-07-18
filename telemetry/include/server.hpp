#pragma once

#include <WiFiS3.h>
#include <stdio.h>
#include <string.h>
#include "gps.hpp"

const char ssid[] = "minutemen-racing";
const char pass[] = "password";
WiFiServer server(80);

// Bounds on how much one HTTP client may send before answering (issue #3);
// the deadline now spans loop() iterations instead of being waited out.
#define SERVER_TIMEOUT_MS 250
#define SERVER_MAX_REQUEST_LEN 128

void initServer()
{
    WiFi.beginAP(ssid, pass);
    server.begin();
}

// The whole response goes out as one write: each print() is a separate
// round-trip to the ESP32-S3 co-processor, and repeated String growth can
// fragment heap during long browser polling sessions.
static void sendResponse(WiFiClient &client, const char *request)
{
    char response[224];
    int responseLen = 0;

    if (strstr(request, "GET /data") != NULL)
    {
        char gpsData[36];
        formatGPSData(gpsData, sizeof(gpsData));
        responseLen = snprintf(response, sizeof(response),
                               "HTTP/1.1 200 OK\r\n"
                               "Content-type:text/plain\r\n"
                               "Access-Control-Allow-Origin: *\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "%d,%d,%d,%d,%s",
                               readSensors(RPM),
                               readSensors(AFR),
                               readSensors(TPS),
                               readSensors(MAP),
                               gpsData);
    }
    else
    {
        // Answer unknown paths (browser favicon probes, "/") so the
        // client closes instead of waiting out its own timeout
        responseLen = snprintf(response, sizeof(response),
                               "HTTP/1.1 404 Not Found\r\n"
                               "Connection: close\r\n"
                               "\r\n");
    }

    if (responseLen > 0)
    {
        if ((size_t)responseLen >= sizeof(response))
            responseLen = sizeof(response) - 1;
        client.write((const uint8_t *)response, (size_t)responseLen);
    }
}

// Non-blocking; call every loop(). One client is handled at a time and its
// state persists across iterations, so each call only consumes bytes the
// modem has already buffered and never waits (issue #27). Further
// connections queue in the modem until this one is answered or dropped.
void runServer()
{
    static WiFiClient client;
    static char request[SERVER_MAX_REQUEST_LEN + 1];
    static size_t requestLen;
    static unsigned long acceptMs;

    if (!client)
    {
        client = server.available();
        if (!client)
            return;
        acceptMs = millis();
        requestLen = 0;
        request[0] = '\0';
    }

    // Same drop semantics as the old blocking read (issue #3): a slow,
    // silent, or half-open client is dropped unanswered at the deadline.
    if (!client.connected() || millis() - acceptMs >= SERVER_TIMEOUT_MS)
    {
        client.stop();
        return;
    }

    // The method and path are always in the first request line, so only
    // that line is read.
    while (client.available())
    {
        char c = client.read();
        if (c == '\n')
        {
            request[requestLen] = '\0';
            sendResponse(client, request);
            client.stop();
            return;
        }
        if (c == '\r')
            continue;

        if (requestLen >= SERVER_MAX_REQUEST_LEN)
        {
            client.stop(); // oversized request line — drop unanswered
            return;
        }
        request[requestLen++] = c;
    }
}
