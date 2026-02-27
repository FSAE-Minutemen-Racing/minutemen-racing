#include <Arduino.h>
#include <WiFiS3.h>
#include <Arduino_LED_Matrix.h>
#include "gps.hpp"

const char ssid[] = "minutemen-racing";
const char pass[] = "password";
WiFiServer server(80);

const byte inputPin = 2;
volatile unsigned long pulseCount = 0;

enum Sensors
{
    RPM, // Revolutions Per Minute
    AFR, // Air Fuel Ratio
    TPS, // Throttle Position Sensor
    MAP  // Manifold Absolute Pressure
};

ArduinoLEDMatrix matrix;
const uint32_t logo[] = {
		0x6d8ffc6d,
		0xe6db6d92,
		0xdb05a009
};

void setup()
{
    Serial.begin(9600);

    matrix.begin();
    matrix.loadFrame(logo);

    WiFi.beginAP(ssid, pass);
    server.begin();
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    pinMode(inputPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(inputPin), pulseISR, FALLING);
}

void loop()
{
    WiFiClient client = server.available();
    if (client)
    {
        String request = "";
        while (client.connected())
        {
            if (client.available())
            {
                char c = client.read();
                request += c;
                if (c == '\n')
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
                        client.print(",0,0");

                        Serial.println("Client served");
                        break;
                    }
                }
            }
        }
        client.stop();
    }
}

void pulseISR()
{
    pulseCount++;
}

unsigned long measureFrequency(int windowMs)
{
    pulseCount = 0;
    unsigned long startTime = millis();
    delay(windowMs);
    unsigned long endTime = millis();

    float gateTime = (endTime - startTime) / 1000.0;
    unsigned long frequency = pulseCount / gateTime;

    return frequency;
}

int readSensors(int sensor)
{
    switch (sensor)
    {
    case RPM:
        return measureFrequency(100);

    case AFR:
        return analogRead(A0);

    case TPS:
        return analogRead(A1);

    case MAP:
        return analogRead(A2);

    default:
        return -1;
    }
}