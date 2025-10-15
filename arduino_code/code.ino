#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>
#include <esp_now.h>

// Configuración WiFi
const char* WIFI_SSID = "esp32camcompucai";  // Tu SSID
const char* WIFI_PASS = "GGbond369857";      // Tu contraseña

WebServer server(80);  // Servidor en el puerto 80

static auto loRes = esp32cam::Resolution::find(800, 600);  // Resolución 800x600
static auto hiRes = esp32cam::Resolution::find(800, 600);  // Máximo 800x600

// MAC Address del ESP32 hijo
uint8_t hijoMAC[] = {0x08, 0xA6, 0xF7, 0x47, 0x00, 0x68};

// Comando para detener (0x00)
uint8_t stopCommand = 0x00;

// === Funciones de cámara y servidor ===
void serveJpg() {
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(), static_cast<int>(frame->size()));
  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

// Endpoint para detener: Envía comando ESP-NOW al hijo
void handleStop() {
  esp_now_send(hijoMAC, &stopCommand, sizeof(stopCommand));
  Serial.println("Obstáculo detectado: Enviando comando de detención al hijo");
  server.send(200, "text/plain", "Comando de detención enviado al hijo");
}

// Callback de envío ESP-NOW
void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  Serial.print("Envío ESP-NOW: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Éxito" : "Fallo");
}

// Configurar peer del hijo
bool addHijoPeer() {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, hijoMAC, 6);
  peerInfo.channel = 1; // Mismo canal que el hijo
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Fallo al agregar hijo como peer");
    return false;
  }
  Serial.println("Hijo agregado como peer");
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Iniciando ESP32-CAM...");

  // Inicializar cámara con configuración optimizada para velocidad
  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(loRes);  // Resolución 800x600
    cfg.setBufferCount(2);    // Buffers para fluidez
    cfg.setJpeg(12);          // Calidad moderada
    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CÁMARA OK" : "CÁMARA FAIL");
    if (!ok) {
      while (true) delay(100); // Bloquear si falla
    }
  }

  // Esperar un momento para estabilizar
  delay(2000);

  // Conectar a WiFi
  Serial.println("Conectando a WiFi...");
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_19_5dBm); // Máxima potencia
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retryCount = 0;
  while (WiFi.status() != WL_CONNECTED && retryCount < 40) {
    delay(500);
    Serial.print(".");
    retryCount++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFALLO al conectar WiFi. Estado: ");
    Serial.println(WiFi.status());
    while (true) delay(100);
  } else {
    Serial.println("\nConectado a WiFi!");
    Serial.print("http://");
    Serial.print(WiFi.localIP());
    Serial.println("/cam-lo.jpg");
  }

  // Inicializar ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error inicializando ESP-NOW");
    while (true) delay(100);
  }
  esp_now_register_send_cb(onDataSent);
  if (!addHijoPeer()) {
    while (true) delay(100);
  }

  // Configurar endpoints del servidor
  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/stop", handleStop);
  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  server.handleClient();
  delay(20); // Retraso mínimo para mayor fluidez
}
