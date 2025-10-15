#include <WiFi.h>
// Carga este código temporal en cada ESP32 para obtener su MAC
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}
void loop() {}
