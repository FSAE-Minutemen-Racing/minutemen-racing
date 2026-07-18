#pragma once
#ifndef GPS_HPP
#define GPS_HPP

#include <Wire.h>
#include <TinyGPSPlus.h>
#include <math.h>
#include <stdio.h>

// GNSS 15 Click (ST Teseo-VIC3DA) in I2C slave mode.
// Uses the Qwiic connector (Wire1, 3.3 V) — the A4/A5 bus is unusable
// because A4 doubles as the battery voltage sense input.
#define GPS_WIRE Wire1
#define GPS_I2C_ADDR 0x3A
#define GPS_RST_PIN 7

// Bytes per I2C read and max reads per poll (bounds time spent per loop)
#define GPS_CHUNK_SIZE 32
#define GPS_MAX_CHUNKS 2

TinyGPSPlus gps;

void initGPS()
{
    // Active-low reset pulse so the module boots cleanly with the MCU
    pinMode(GPS_RST_PIN, OUTPUT);
    digitalWrite(GPS_RST_PIN, LOW);
    delay(100);
    digitalWrite(GPS_RST_PIN, HIGH);

    GPS_WIRE.begin();
}

// The Teseo streams NMEA sentences straight over I2C and pads reads
// with 0xFF when it has nothing to send. Call once per loop().
void pollGPS()
{
    for (int chunk = 0; chunk < GPS_MAX_CHUNKS; chunk++)
    {
        uint8_t received = GPS_WIRE.requestFrom(GPS_I2C_ADDR, GPS_CHUNK_SIZE);
        if (received == 0)
            return; // no ACK — module absent or still resetting

        bool sawData = false;
        while (GPS_WIRE.available())
        {
            uint8_t c = GPS_WIRE.read();
            if (c != 0xFF)
            {
                sawData = true;
                gps.encode((char)c);
            }
        }

        if (!sawData)
            return; // stream drained for now
    }
}

String getGPSData()
{
    if (!gps.location.isValid())
        return "INVALID,INVALID";

    if (gps.location.age() > 5000)
        return "STALE,STALE";

    String output;
    output += String(gps.location.lat(), 6);
    output += ",";
    output += String(gps.location.lng(), 6);
    return output;
}

void formatGPSData(char *output, size_t outputSize)
{
    if (!gps.location.isValid())
    {
        snprintf(output, outputSize, "INVALID,INVALID");
        return;
    }

    if (gps.location.age() > 5000)
    {
        snprintf(output, outputSize, "STALE,STALE");
        return;
    }

    double lat = gps.location.lat();
    double lng = gps.location.lng();
    bool latNegative = lat < 0.0;
    bool lngNegative = lng < 0.0;

    lat = fabs(lat);
    lng = fabs(lng);

    long latWhole = (long)lat;
    long lngWhole = (long)lng;
    long latFrac = (long)((lat - latWhole) * 1000000.0 + 0.5);
    long lngFrac = (long)((lng - lngWhole) * 1000000.0 + 0.5);

    if (latFrac >= 1000000L)
    {
        latWhole++;
        latFrac -= 1000000L;
    }
    if (lngFrac >= 1000000L)
    {
        lngWhole++;
        lngFrac -= 1000000L;
    }

    snprintf(output, outputSize, "%s%ld.%06ld,%s%ld.%06ld",
             latNegative ? "-" : "", latWhole, latFrac,
             lngNegative ? "-" : "", lngWhole, lngFrac);
}

#endif
