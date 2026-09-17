/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Antriebsstation (AST) [Master]
 * ============================================================================
 * @file      AST_esp_code.ino
 * @brief     Haupt-Code der Antriebsstation (Master). Steuert die Seilbahn-Motoren,
 *            hostet das responsive HMI WebUI (HTTP/JSON) auf Core 0 und 
 *            kommuniziert drahtlos via ESP-NOW mit der Gegenstation (GST).
 * 
 * @author    Open Source Contributor
 * @date      2026
 * @license   MIT License
 * ============================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "config.h"
#include "Gondel.h"
#include "Ropeway.h"
#include "WebUI.h"
#include "Messages.h"

// Globale Zeiger auf die Station und den Webserver
AntriebsStation* station = nullptr;
WebServer* server        = nullptr;

/**
 * @brief ESP-NOW Sende-Callback (wird aufgerufen, wenn Daten gesendet wurden)
 */
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Optionale Diagnose/Logging für Paketsende-Status
}

/**
 * @brief ESP-NOW Empfangs-Callback (wird aufgerufen, wenn Pakete von der GST empfangen werden)
 */
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len == sizeof(RopewayMessage) && station != nullptr) {
    RopewayMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));
    station->handleMessage(msg);
  }
}

/**
 * @brief Dedicated FreeRTOS Task für den Webserver.
 * Runs on Core 0, to leave Core 1 100% free for real-time motor/stepper loops.
 */
void webServerTask(void *pvParameters) {
  // Warte kurz, bis der Webserver in setup() instanziiert wurde
  while (server == nullptr) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  Serial.println("[FreeRTOS Core 0] Webserver-Task gestartet.");

  for (;;) {
    server->handleClient();
    vTaskDelay(pdMS_TO_TICKS(1)); // Yield for WiFi stack processing
  }
}

/**
 * @brief Arduino Setup Funktion
 */
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n==========================================");
  Serial.println("  3S-Seilbahn Modell - Antriebsstation (AST)");
  Serial.println("==========================================\n");

  // 1. WLAN verbinden (Access Point oder Station Mode)
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("[WLAN] Verbinde mit '%s' ", ssid);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 8000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WLAN] Erfolgreich verbunden! IP: %s | Kanal: %d\n",
                  WiFi.localIP().toString().c_str(), WiFi.channel());
  } else {
    Serial.println("[WLAN] Verbindung fehlgeschlagen. Starte ohne IP-Verbindung.");
  }

  // 2. ESP-NOW drahtloses Kommunikationsprotokoll initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Fehler bei der Initialisierung!");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // 3. Gegenstation (GST) als ESP-NOW Peer registrieren
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  peerInfo.channel = WiFi.channel();
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, GST_MAC, 6);

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Kopplung mit GST fehlgeschlagen!");
    return;
  }
  Serial.println("[ESP-NOW] Kopplung mit Gegenstation (GST) OK.");

  // 4. AntriebsStation-Instanz erstellen
  station = new AntriebsStation();
  Serial.println("[Station] AntriebsStation (Master) erfolgreich initialisiert.");

  // 5. HTTP Webserver konfigurieren (Port 80)
  server = new WebServer(80);

  // Hauptseite ausliefern (gechunked für große HTML-Dateien)
  server->on("/", HTTP_GET, []() {
    size_t totalLen = strlen_P(INDEX_HTML);
    server->setContentLength(totalLen);
    server->send(200, "text/html", "");
    
    const size_t chunkSize = 4096;
    size_t sent = 0;
    while (sent < totalLen) {
      size_t toSend = totalLen - sent;
      if (toSend > chunkSize) toSend = chunkSize;
      
      char buf[chunkSize + 1];
      memcpy_P(buf, INDEX_HTML + sent, toSend);
      buf[toSend] = '\0';
      server->sendContent(buf, toSend);
      sent += toSend;
    }
  });

  // REST API Endpunkt: Status (polled by HMI WebUI every 250ms)
  server->on("/status", HTTP_GET, []() {
    if (station != nullptr) {
      String json = station->getStatusJson();
      server->send(200, "application/json", json);
    } else {
      server->send(200, "application/json", "{}");
    }
  });

  // REST API Endpunkt: Kommandos (POST JSON)
  server->on("/cmd", HTTP_POST, []() {
    if (server->hasArg("plain") && station != nullptr) {
      String body = server->arg("plain");
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, body);
      if (!error) {
        String action = doc["action"] | "";
        station->handleWebCommand(action, doc);
        server->send(200, "application/json", "{\"ok\":true}");
      } else {
        server->send(400, "application/json", "{\"error\":\"json_parse_failed\"}");
      }
    } else {
      server->send(400, "application/json", "{\"error\":\"empty_body\"}");
    }
  });

  server->begin();
  Serial.println("[Webserver] HTTP-Server auf Port 80 gestartet.");

  // 6. Webserver-Task auf Core 0 pinnieren (Arduino loop läuft auf Core 1)
  xTaskCreatePinnedToCore(
    webServerTask,    // Task-Funktion
    "WebServerTask",  // Task-Name
    8192,             // Stack-Größe (8KB)
    NULL,             // Parameter
    1,                // Priorität
    NULL,             // Task-Handle
    0                 // Core 0 Pinning
  );
  Serial.println("[FreeRTOS] Webserver-Task auf Core 0 gepinnt.");
  Serial.println("==========================================");
}

/**
 * @brief Hauptschleife (läuft auf Core 1 mit höchster Priorität für Stepper-Timing)
 */
void loop() {
  if (station != nullptr) {
    station->update();
  }
}