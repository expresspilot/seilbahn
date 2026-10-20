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
#include <Arduino.h>

#include <WiFi.h>
#include <esp_now.h>
#include <ArduinoJson.h>

#include "config.h"
#include "Gondel.h"
#include "Ropeway.h"
#include "Messages.h"
#include <esp_wifi.h>



// Globaler Zeiger auf die Gegenstation
GegenStation* station = nullptr;

/**
 * @brief ESP-NOW Sende-Callback (wird aufgerufen, wenn Daten an die AST gesendet wurden)
 */
//id onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
//// Optionale Sendediagnose
//
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("[ESP-NOW GST -> AST] %s\n",
                status == ESP_NOW_SEND_SUCCESS ? "OK" : "FEHLER");
}


















/**
 * @brief ESP-NOW Empfangs-Callback (wird aufgerufen, wenn Pakete von der AST empfangen werden)
 */


//id onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
//if (len == sizeof(RopewayMessage) && station != nullptr) {
//  RopewayMessage msg;
//  memcpy(&msg, incomingData, sizeof(msg));
//  station->handleMessage(msg);
//}
//

//vorübergehend wegen pakete zum testen jetzt 
//void onDataRecv(const uint8_t *mac_addr,
//                const uint8_t *incomingData,
//                int len) {
//
//  Serial.printf("[ESP-NOW GST] Paket empfangen: %d Bytes\n", len);
//
//  if (len == sizeof(RopewayMessage) && station != nullptr) {
//    RopewayMessage msg;
//    memcpy(&msg, incomingData, sizeof(msg));
//    station->handleMessage(msg);
//  }
//}






















//zum testen der gesendeteten pakete


void onDataRecv(const uint8_t *mac_addr,
                const uint8_t *incomingData,
                int len) {

  Serial.printf("[ESP-NOW GST <- AST] RX len=%d expected=%u\n",
                len, (unsigned)sizeof(RopewayMessage));

  if (len == sizeof(RopewayMessage) && station != nullptr) {
    RopewayMessage msg;
    memcpy(&msg, incomingData, sizeof(msg));

    Serial.printf("[ESP-NOW GST <- AST] type=%d\n",
                  (int)msg.type);

    station->handleMessage(msg);
  } else {
    Serial.println("[ESP-NOW GST <- AST] PAKET VERWORFEN!");
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

  // 1. WLAN Initialisieren (Station-Modus für ESP-NOW Kanal-Matching)           so wenn zuhause wlan verwendet wird 
//  WiFi.mode(WIFI_STA);

//esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE);

//Serial.printf("[WLAN] GST MAC: %s | Kanal: %d\n",
  //            WiFi.macAddress().c_str(),
 //             11);
//




// 1. WLAN initialisieren (kein Router, nur ESP-NOW)                               so wenn access point esp verwendet wird
WiFi.mode(WIFI_STA);
WiFi.disconnect();

// Muss mit dem AST-Access-Point übereinstimmen: Kanal 1
esp_err_t channelResult =
esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

if (channelResult != ESP_OK) {
Serial.printf("[WLAN] Kanal setzen fehlgeschlagen: %d\n",
(int)channelResult);
return;
}

Serial.printf("[WLAN] GST MAC: %s | Kanal: 1\n",
WiFi.macAddress().c_str());





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
  peerInfo.channel = 1 ;
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