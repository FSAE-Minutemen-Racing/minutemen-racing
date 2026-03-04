#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>

TinyGPSPlus gps;

static const int RXPin = 0, TXPin = 1; // Serial1 on Teensy4.1 pins 
static const uint32_t GPSBaud = 115200; 

SoftwareSerial gpsSerial(RXPin, TXPin);

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPSBaud);

  Serial.println(F("Waiting for GPS signal..."));
  Serial.println(F("This may take a few minutes if cold starting."));
}

void loop() {
  while (gpsSerial.available() > 0) { // 0 means no bytes available
    char c = gpsSerial.read();
    gps.encode(c); // send NMEA to TinyGPS

    if (gps.location.isUpdated()) { // print location and satellite count
      Serial.print(F("Latitude: "));
      Serial.println(gps.location.lat(), 6);

      Serial.print(F("Longitude: "));
      Serial.println(gps.location.lng(), 6);

      Serial.print(F("Satellites: "));
      Serial.println(gps.satellites.value());

      Serial.println(F("-----------------------------"));
    }
  }
}