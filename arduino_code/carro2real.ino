  //carro2 
  #include <esp_now.h>
  #include <WiFi.h>
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
  //enumerar los numeros (pasos) XD

  enum MoveType { FORWARD, BACKWARD, TURN_RIGHT, TURN_LEFT };
  struct Move { MoveType type; int value; };
  //pines
  int IN1 = 25, IN2 = 26, IN3 = 27, IN4 = 14, ENA = 12, ENB = 13;
  int velAvance = 190;
  //mac adress 
  uint8_t motherMAC[] = {0xD0, 0xEF, 0x76, 0x32, 0x31, 0x34};
  uint8_t car3MAC[] = {0x38, 0x18, 0x2B, 0xB2, 0x25, 0x10};
  //general
  bool started = false, ejecutando = false;
  unsigned long stepStart = 0;
  int currentStep = 0;
  bool isForward = true;
  int pathLength = 7;

  Move forwardPath[] = {{FORWARD, 400}, {TURN_RIGHT, 700}, {FORWARD, 300}, {TURN_LEFT, 700}, {FORWARD, 700}, {TURN_LEFT, 700}, {FORWARD, 400}};
  Move returnPath[] = {{BACKWARD, 500}, {TURN_RIGHT, 700}, {BACKWARD, 700}, {TURN_RIGHT, 700 }, {BACKWARD, 450}, {TURN_LEFT, 700}, {BACKWARD, 400}};

  void motoresAvanzar(int vel) { digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW); digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW); analogWrite(ENA,vel);analogWrite(ENB,vel); }
  void motoresRetroceder(int vel) { digitalWrite(IN1,LOW);digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW);digitalWrite(IN4,HIGH); analogWrite(ENA,vel);analogWrite(ENB,vel); }
  void motoresGiroDerecha(int vel) { digitalWrite(IN1,LOW);digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH);digitalWrite(IN4,LOW); analogWrite(ENA,vel);analogWrite(ENB,vel); }
  void motoresGiroIzquierda(int vel) { digitalWrite(IN1,HIGH);digitalWrite(IN2,LOW); digitalWrite(IN3,LOW);digitalWrite(IN4,HIGH); analogWrite(ENA,vel);analogWrite(ENB,vel); }
  void motoresParar() { digitalWrite(IN1,LOW);digitalWrite(IN2,LOW); digitalWrite(IN3,LOW);digitalWrite(IN4,LOW); analogWrite(ENA,0);analogWrite(ENB,0); }

  void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (len == 1) {
      uint8_t cmd = data[0];
      started = false; ejecutando = false; currentStep = 0;
      Serial.print(">>> Recibido 0x"); Serial.print(cmd, HEX); Serial.println(" (de Madre o C3)");
      
      switch(cmd) {
        case 0x01: Serial.println(">>> AVANZAR"); isForward = true; started = true; break;
        case 0x02: Serial.println(">>> RETROCEDER"); isForward = false; started = true; break;
        case 0x99: Serial.println(">>> TEST OK"); break;
      }
    }
  }

  bool setupESPNOW() {
    pinMode(IN1,OUTPUT);pinMode(IN2,OUTPUT);pinMode(IN3,OUTPUT);pinMode(IN4,OUTPUT);
    pinMode(ENA,OUTPUT);pinMode(ENB,OUTPUT); motoresParar();
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
    WiFi.mode(WIFI_STA);
    Serial.print("MAC CARRO 2: "); Serial.println(WiFi.macAddress());
    if (esp_now_init() != ESP_OK) return false;
    
    esp_now_peer_info_t peer = {0}; peer.channel = 1; peer.encrypt = false;
    memcpy(peer.peer_addr, motherMAC, 6); esp_now_add_peer(&peer);
    
    // 🔥 NUEVO: Recibir de Carro 3
    memcpy(peer.peer_addr, car3MAC, 6); esp_now_add_peer(&peer);
    
    esp_now_register_recv_cb(OnDataRecv);
    return true;
  }

  void setup() {
    Serial.begin(115200); delay(3000);
    if (!setupESPNOW()) while(1);
    
    uint8_t ready = 0x99;
    esp_now_send(motherMAC, &ready, 1);
    delay(100);
    
    Serial.println("\n>>> CARRO 2 LISTO <<<");
  }

  void loop() {
    if (started && currentStep < pathLength) {
      Move move = isForward ? forwardPath[currentStep] : returnPath[currentStep];
      if (!ejecutando) {
        ejecutando = true; stepStart = millis();
        switch(move.type) {
          case FORWARD: motoresAvanzar(velAvance); Serial.print("Avanzar "); break;
          case BACKWARD: motoresRetroceder(velAvance); Serial.print("Retroceder "); break;
          case TURN_RIGHT: motoresGiroDerecha(velAvance); Serial.print("Giro Der "); break;
          case TURN_LEFT: motoresGiroIzquierda(velAvance); Serial.print("Giro Izq "); break;
        }
        Serial.println(move.value);
      }
      else if (millis() - stepStart >= move.value) {
        motoresParar(); ejecutando = false; currentStep++; delay(300);
        if (currentStep >= pathLength) {
          Serial.println(isForward ? ">>> LLEGUE!" : ">>> REGRESE!");
          started = false;
        }
      }
    }
    delay(50);
  }