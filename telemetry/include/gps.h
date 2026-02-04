#pragma once
#ifndef GPS_HPP
#define GPS_HPP

#include <TinyGPSPlus.h>

#define GPS_SERIAL Serial2

TinyGPSPlus gps;

String getGPSData()
{
    while (GPS_SERIAL.available() > 0)
        if (gps.encode(GPS_SERIAL.read()))
        {
            if (gps.location.isValid())
            {
                String output;
                output += String(gps.location.lat(), 6);
                output += ",";
                output += String(gps.location.lng(), 6);
                return output;
            }
            else
            {
                return "INVALID,INVALID";
            }
        }
    return "NO GPS,NO GPS";
}

#endif