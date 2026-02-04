#pragma once
#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESP8266WebServer.h>
#include <FS.h>

ESP8266WebServer server(80);

void serverRouting()
{
    server.on("/", HTTP_GET, []()
              { 
                File file = SPIFFS.open("/index.html", "r");
                
                if (!file) {
                    server.send(404, "text/plain", "File Not Found");
                    return;
                }

                server.streamFile(file, "text/html");
                file.close(); });

    server.on("/data", HTTP_GET, []()
              {
                Serial.print('d');

                delay(10);
                
                if (Serial.available() > 0)
                    server.send(201, "text/plain", Serial.readStringUntil('\n'));
                else
                {
                    server.send(500, "text/plain", "No data from serial");
                } });
}

#endif