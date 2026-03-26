#pragma once
#ifndef SERVER_HPP
#define SERVER_HPP

#include <WiFiS3.h>

const char ssid[] = "minutemen-racing";
const char pass[] = "password";
WiFiServer server(80);

void initServer()
{
    WiFi.beginAP(ssid, pass);
    server.begin();
}

void runServer()
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

                        break;
                    }
                }
            }
        }
        client.stop();
    }
}

#endif