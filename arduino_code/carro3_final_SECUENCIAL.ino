#include <esp_now.h>
#include <WiFi.h>
#include <DHT.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// === DEFINICIONES ===
enum MoveType { FORWARD, BACKWARD, TURN_RIGHT, TURN_LEFT };
struct Move { MoveType type; int value; };

// Pines motores
int IN1 = 25, IN2 = 26, IN3 = 27, IN4 = 14, ENA = 12, ENB = 13;
int velAvance = 210;

// MACs (TUS MACs CORRECTAS)
uint8_t motherMAC[] = {0xD0, 0xEF, 0x76, 0x32, 0x31, 0x34};
uint8_t car1MAC[] = {0x1C, 0x69, 0x20, 0x94, 0x49, 0x30};
uint8_t car2MAC[] = {0x08, 0xA6, 0xF7, 0x47, 0x00, 0x68};

bool started = false, ejecutando = false;
unsigned long stepStart = 0;
int currentStep = 0;
bool isForward = true;
int pathLength = 1;
bool tempAlertSent = false;

Move forwardPath[] = {{FORWARD, 600}};
Move returnPath[] = {{BACKWARD, 600}};

void motoresAvanzar(int vel) { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); analogWrite(ENA,vel); analogWrite(ENB,vel); }
void motoresRetroceder(int vel) { digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); analogWrite(ENA,vel); analogWrite(ENB,vel); }
void motoresGiroDerecha(int vel) { digitalWrite(IN1,LOW); digitalWrite(IN2,HIGH); digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW); analogWrite(ENA,vel); analogWrite(ENB,vel); }
void motoresGiroIzquierda(int vel) { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,HIGH); analogWrite(ENA,vel); analogWrite(ENB,vel); }
void motoresParar() { digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); digitalWrite(IN3,LOW); digitalWrite(IN4,LOW); analogWrite(ENA,0); analogWrite(ENB,0); }

void sendTemp(float temp) { esp_now_send(motherMAC, (uint8_t*)&temp, sizeof(float)); }
void sendReady() { uint8_t ready = 0x99; esp_now_send(motherMAC, &ready, 1); }

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len == 1 && memcmp(recv_info->src_addr, motherMAC, 6) == 0) {
    uint8_t cmd = data[0];
    Serial.print(">>> Recibido de Madre: 0x"); Serial.println(cmd, HEX);
    started = false; ejecutando = false; currentStep = 0; tempAlertSent = false;
    
    switch(cmd) {
      case 0x01: Serial.println(">>> AVANZAR"); isForward = true; started = true; break;
      case 0x02: Serial.println(">>> RETROCEDER"); isForward = false; started = true; break;
      case 0x99: Serial.println(">>> TEST OK"); break;
    }
  }
}

bool setupESPNOW() {
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT); pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT);
  pinMode(ENA,OUTPUT); pinMode(ENB,OUTPUT); motoresParar();
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  WiFi.mode(WIFI_STA);
  Serial.print("MAC CARRO 3: "); Serial.println(WiFi.macAddress());
  if (esp_now_init() != ESP_OK) return false;
  
  esp_now_peer_info_t peer = {0}; peer.channel = 1; peer.encrypt = false;
  memcpy(peer.peer_addr, motherMAC, 6); esp_now_add_peer(&peer);
  memcpy(peer.peer_addr, car1MAC, 6); esp_now_add_peer(&peer);
  memcpy(peer.peer_addr, car2MAC, 6); esp_now_add_peer(&peer);
  esp_now_register_recv_cb(OnDataRecv);
  return true;
}

void setup() {
  Serial.begin(115200); delay(3000);
  Serial.println("\n=== CARRO 3 DHT11 ===");
  dht.begin();
  if (!setupESPNOW()) while(1);
  delay(500); sendReady(); delay(100);
  Serial.println("\n>>> CARRO 3 LISTO (DHT11) <<<");
}

void loop() {
  if (started && currentStep < pathLength) {
    Move move = isForward ? forwardPath[currentStep] : returnPath[currentStep];
    if (!ejecutando) {
      ejecutando = true; stepStart = millis();
      Serial.print("Paso "); Serial.print(currentStep+1); Serial.print(": ");
      switch(move.type) {
        case FORWARD: motoresAvanzar(velAvance); Serial.print("AVANZAR "); break;
        case BACKWARD: motoresRetroceder(velAvance); Serial.print("RETROCEDER "); break;
        case TURN_RIGHT: motoresGiroDerecha(velAvance); Serial.print("GIRO DERECHA "); break;
        case TURN_LEFT: motoresGiroIzquierda(velAvance); Serial.print("GIRO IZQUIERDA "); break;
      }
      Serial.println(move.value);
    }
    else if (millis() - stepStart >= move.value) {
      motoresParar(); ejecutando = false; Serial.println("✓ Completado");
      currentStep++; delay(300);
      if (currentStep >= pathLength) {
        Serial.println(isForward ? ">>> LLEGUE AL DESTINO!" : ">>> REGRESE A INICIO!");
        started = false;
        
        // 🔥 RETORNO SECUENCIAL 3→2→1
        if (tempAlertSent) {
          delay(1000);
          uint8_t cmd = 0x02;
          esp_now_send(car2MAC, &cmd, 1);
          Serial.println("✓ Carro 2 RETORNA");
          delay(7000); // Espera C2
          
          esp_now_send(car1MAC, &cmd, 1);
          Serial.println("✓ Carro 1 RETORNA");
          Serial.println(">>> RETORNO SECUENCIAL COMPLETO!");
        }
      }
    }
  }
  
  static unsigned long lastTemp = 0;
  if (millis() - lastTemp > 5000) {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
      sendTemp(temp);
      Serial.print("Temp: "); Serial.print(temp); Serial.println("°C");
      if (temp >= 30 && !tempAlertSent && !started) {
        Serial.println(">>> ALERTA TEMP >=30°C! TODOS RETORNAN");
        tempAlertSent = true;
        uint8_t cmd = 0x02;
        esp_now_send(motherMAC, &cmd, 1);
        isForward = false; started = true; currentStep = 0;
      }
    }
    lastTemp = millis();
  }
  delay(50);
}