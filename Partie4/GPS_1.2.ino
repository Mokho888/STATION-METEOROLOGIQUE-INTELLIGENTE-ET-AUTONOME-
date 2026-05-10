#include <TinyGPS++.h>
#include <HardwareSerial.h>

TinyGPSPlus gps;
HardwareSerial GPS_Serial(2);

void setup() {
  Serial.begin(115200);
  GPS_Serial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("=== GPS NEO-6M ===");
}

void loop() {
  while (GPS_Serial.available() > 0) {
    gps.encode(GPS_Serial.read());
  }

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 2000) {
    lastPrint = millis();

    Serial.println("------------------------------");
    Serial.print("Satellites : ");
    Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);

    if (gps.location.isValid()) {
      Serial.print("Latitude   : "); Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude  : "); Serial.println(gps.location.lng(), 6);
    } else {
      Serial.println("Position   : pas encore fixee");
    }

    if (gps.altitude.isValid()) {
      Serial.print("Altitude   : ");
      Serial.print(gps.altitude.meters());
      Serial.println(" m");
    }
  }
}