#include <Arduino.h>

#include "kline.h"
#include "gps.h"
#include "sensors.h"

void setup()
{
  Serial.begin(115200);
  K_LINE_SERIAL.begin(15800, SERIAL_8N1);
  GPS_SERIAL.begin(115200);
  VIEWER_SERIAL.begin(115200);

  // Start sensor pins
  initSensors();

  // Initialize the K-line communication protocol
  initKLine();

  // Enter Diagnostic Mode
  // enterDiagMode();
}

void loop()
{
  if (VIEWER_SERIAL.available() > 0) {

    char command = VIEWER_SERIAL.read();
   
    if (command == 'd') {
      VIEWER_SERIAL.print(readSensors());
      VIEWER_SERIAL.print(",");
      VIEWER_SERIAL.print(getGPSData());
    }
  }
}