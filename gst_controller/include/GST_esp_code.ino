/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Gegenstation (GST) [Slave]
 * ============================================================================
 * @file      GST_esp_code.ino
 * @brief     Haupt-Code der Gegenstation (Slave). Steuert die Umlenkmotoren,
 *            Stationseinfahrt-Lichtschranken, Gondelabstandshaltung und 
 *            Empfang von Steuerbefehlen via ESP-NOW von der Antriebsstation (AST).
 * 
 * @author    Open Source Contributor
 * @date      2026
 * @license   MIT License
 * ============================================================================
 */

#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

#include "Gondel.h"
#include "Ropeway.h"
#include "Messages.h"

// ============================================================================
// ESP-NOW MAC-ADRESSEN (Kopplung von AST und GST)
// ============================================================================
const uint8_t GST_MAC[6] = { 0x14, 0xC1, 0x9F, 0x2B, 0x33, 0xEC };
const uint8_t AST_MAC[6] = { 0x44, 0x1B, 0xF6, 0xFF, 0x51, 0x60 };

// Globaler Zeiger auf die Gegenstation
GegenStation* station = nullptr;

/**
 * @brief ESP-NOW Sende-Callback (wird aufgerufen, wenn Daten an die AST gesendet wurden)
 */
void onDataSent(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
  // Optionale Sendediagnose
}

/**
 * @brief ESP-NOW Empfangs-Callback (wird aufgerufen, wenn Pakete von der AST empfangen werden)
 */
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  if (len == sizeof(RopewayMessage) && station != nullptr) {
    RopewayMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));
    station->handleMessage(msg);
  }
}

/**
 * @brief Setup-Funktion für den Slave-ESP32 (Gegenstation GST)
 */
void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n==========================================");
  Serial.println("  3S-Seilbahn Modell - Gegenstation (GST)");
  Serial.println("==========================================\n");

  // 1. WLAN Initialisieren (Station-Modus für ESP-NOW Kanal-Matching)
  WiFi.mode(WIFI_STA);

  // 2. ESP-NOW drahtlose Kommunikation initialisieren
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Fehler bei der Initialisierung!");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // 3. Master (AST) als ESP-NOW Peer hinzufügen
  esp_now_peer_info_t peerInfo = {};
  memset(&peerInfo, 0, sizeof(peerInfo));
  peerInfo.channel = WiFi.channel();
  peerInfo.encrypt = false;
  memcpy(peerInfo.peer_addr, AST_MAC, 6);

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Kopplung mit AST fehlgeschlagen!");
    return;
  }
  Serial.println("[ESP-NOW] Kopplung mit Antriebsstation (AST) OK.");

  // 4. GegenStation-Instanz erstellen
  station = new GegenStation();
  Serial.println("[Station] GegenStation (Slave) erfolgreich initialisiert.");
  Serial.println("==========================================");
}

/**
 * @brief Hauptschleife (Real-Time Schrittmotorsteuerung und Lichtschrankenauswertung)
 */
void loop() {
  if (station != nullptr) {
    station->update();
  }
}