#include <esp_now.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

uint8_t car2MAC[] = {0x08, 0xA6, 0xF7, 0x47, 0x00, 0x68}; //08:A6:F7:47:00:68
uint8_t car1MAC[] = {0x1C, 0x69, 0x20, 0x94, 0x49, 0x30}; // 1C:69:20:94:49:30
uint8_t car3MAC[] = {0x08, 0x3A, 0xF2, 0x93, 0x16, 0xB8};

bool car1Ready = false, car2Ready = false, car3Ready = false;
float temperature = 25.0;

void sendCommand(uint8_t* mac, uint8_t cmd) {
  esp_now_send(mac, &cmd, 1);
}

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  const uint8_t *mac = recv_info->src_addr;
  
  if (len == 1) {
    uint8_t cmd = data[0];
    if (cmd == 0x99) {
      if (memcmp(mac, car1MAC, 6) == 0) car1Ready = true;
      if (memcmp(mac, car2MAC, 6) == 0) car2Ready = true;
      if (memcmp(mac, car3MAC, 6) == 0) car3Ready = true;
      Serial.println((memcmp(mac, car1MAC, 6) == 0) ? "Carro 1 listo!" : 
                    (memcmp(mac, car2MAC, 6) == 0) ? "Carro 2 listo!" : "Carro 3 listo!");
    }
  } 
  else if (len == 4 && memcmp(mac, car3MAC, 6) == 0) {
    memcpy(&temperature, data, 4);
    Serial.print("Temp Carro 3: "); Serial.print(temperature); Serial.println("°C");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Temp Carro 3:");
    lcd.setCursor(0, 1); lcd.print(temperature, 1); lcd.print(" C");
  }
}

bool setupESPNOW() {
  WiFi.mode(WIFI_STA);
  Serial.print("MAC MADRE: "); Serial.println(WiFi.macAddress());
  if (esp_now_init() != ESP_OK) return false;
  
  esp_now_peer_info_t peer = {0}; peer.channel = 1; peer.encrypt = false;
  memcpy(peer.peer_addr, car1MAC, 6); esp_now_add_peer(&peer);
  memcpy(peer.peer_addr, car2MAC, 6); esp_now_add_peer(&peer);
  memcpy(peer.peer_addr, car3MAC, 6); esp_now_add_peer(&peer);
  
  esp_now_register_recv_cb(OnDataRecv);
  sendCommand(car1MAC, 0x99);
  sendCommand(car2MAC, 0x99);
  sendCommand(car3MAC, 0x99);
  return true;
}

void setup() {
  Serial.begin(115200); delay(3000);
  lcd.init(); lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("Madre Central");
  delay(2000);
  lcd.clear(); lcd.print("Esperando...");
  
  if (!setupESPNOW()) while(1);
  
  Serial.println("\nCOMANDOS:");
  Serial.println("1 = AVANZAR (1→2→3)");
  Serial.println("2 = RETROCEDER (3→2→1)");
  Serial.println("99 = TEST");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n'); cmd.trim();
    if (cmd == "1" && car1Ready && car2Ready && car3Ready) {
      Serial.println(">>> AVANZAR: 1→2→3");
      lcd.clear(); lcd.print("AVANZANDO...");
      sendCommand(car1MAC, 0x01); delay(3000);
      sendCommand(car2MAC, 0x01); delay(3000);
      sendCommand(car3MAC, 0x01);
    }
    else if (cmd == "2" && car1Ready && car2Ready && car3Ready) {
      Serial.println(">>> RETROCEDER: 3→2→1");
      lcd.clear(); lcd.print("RETROCEDIENDO...");
      sendCommand(car3MAC, 0x02); delay(3000);
      sendCommand(car2MAC, 0x02); delay(3000);
      sendCommand(car1MAC, 0x02);
    }
    else if (cmd == "99") {
      car1Ready = car2Ready = car3Ready = false;
      sendCommand(car1MAC, 0x99);
      sendCommand(car2MAC, 0x99);
      sendCommand(car3MAC, 0x99);
    }
  }
  delay(100);
}