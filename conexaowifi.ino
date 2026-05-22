#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include "time.h" // Biblioteca para sincronizar hora

// --- CONFIGURAÇÕES HIVEMQ ---
const char* mqtt_server = "3d7025d09abe47afb7c0c295d210669c.s1.eu.hivemq.cloud";
const int mqtt_port    = 8883;
const char* mqtt_user   = "esp32"; 
const char* mqtt_pass   = "Senhaesp32";
const char* topic_led   = "glub/led";
const int ledPin        = 2;

// --- CONFIGURAÇÕES PORTAL ---
const char* ssid_ap = "SISTEMA_TCC_ALR";
const char* pass_ap = "85229150";

WebServer server(80);
WiFiClientSecure espClient;
PubSubClient client(espClient);
Preferences preferences; 

String rede_local = "";
String senha_local = "";
unsigned long anteriorMillis = 0;

void handleRoot() {
  String html = "<html><body style='text-align:center; font-family:sans-serif;'>";
  html += "<h2>Configurar Wi-Fi</h2><form action='/save' method='POST'>";
  html += "Rede: <input type='text' name='ssid'><br><br>";
  html += "Senha: <input type='password' name='pass'><br><br>";
  html += "<input type='submit' value='Salvar'></form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  preferences.begin("wifi-config", false);
  preferences.putString("ssid", server.arg("ssid"));
  preferences.putString("pass", server.arg("pass"));
  preferences.end();
  server.send(200, "text/html", "Salvo! Reiniciando...");
  delay(2000);
  ESP.restart();
}

// Sincroniza a hora para o SSL não falhar
void sincronizarHora() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Aguardando sincronizacao de hora");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("\nHora sincronizada!");
}

void manterConexoes() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!client.connected()) {
    if (millis() - anteriorMillis >= 10000) {
      Serial.print("Tentando MQTT...");
      String clientId = "ESP32_" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass)) {
        Serial.println(" CONECTADO!");
        client.subscribe(topic_led);
      } else {
        Serial.printf(" ERRO rc=%d\n", client.state());
      }
      anteriorMillis = millis();
    }
  }
}

void setup() {
  pinMode(ledPin, OUTPUT);
  Serial.begin(115200);

  preferences.begin("wifi-config", true);
  rede_local = preferences.getString("ssid", "");
  senha_local = preferences.getString("pass", "");
  preferences.end();

  if (rede_local != "") {
    WiFi.begin(rede_local.c_str(), senha_local.c_str());
    int count = 0;
    while (WiFi.status() != WL_CONNECTED && count < 20) { delay(500); count++; }
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.softAP(ssid_ap, pass_ap);
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    while (WiFi.status() != WL_CONNECTED) {
      server.handleClient();
      digitalWrite(ledPin, (millis() / 200) % 2 == 0);
    }
    server.stop(); // FECHA O SERVIDOR PARA LIBERAR RAM
    WiFi.softAPdisconnect(true);
  }

  // --- CORREÇÕES PARA RC -2 ---
  sincronizarHora(); 
  espClient.setInsecure();
  
  // Aumenta o buffer para aguentar mensagens TLS
  client.setBufferSize(1024); 
  client.setServer(mqtt_server, mqtt_port);
  
  client.setCallback([](char* topic, byte* payload, unsigned int length) {
    String msg;
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    if (msg == "1") digitalWrite(ledPin, HIGH);
    else if (msg == "0") digitalWrite(ledPin, LOW);
  });
}

void loop() {
  manterConexoes();
  if (client.connected()) client.loop();
}