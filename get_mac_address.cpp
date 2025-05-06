#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(1000);

  // print the MAC address to Serial Monitor
  String macAddress = WiFi.macAddress();
  Serial.print("ESP32-S3 MAC Address: ");
  Serial.println(macAddress);
}

void loop() {
}
