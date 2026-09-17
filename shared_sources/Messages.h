/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - ESP-NOW Nachrichtenstruktur
 * ============================================================================
 * @file      Messages.h
 * @brief     Definiert die drahtlose ESP-NOW Paketstruktur für die 
 *            Kommunikation zwischen Antriebsstation (AST) und Gegenstation (GST).
 * ============================================================================
 */

#pragma once

/**
 * @enum MessageType
 * @brief Identifikator für den Nachrichtentyp im ESP-NOW Protokoll.
 */
enum MessageType {
  MSG_SPEED,      ///< Geschwindigkeits- & Beschleunigungskommando von AST an GST
  MSG_SENSORS,    ///< Sensor-Status (Lichtschranken) von GST an AST
  MSG_HEARTBEAT   ///< Regelmäßiger Heartbeat zur Überwachung der Funkverbindung
};

/**
 * @struct RopewayMessage
 * @brief ESP-NOW Datenpaket für die Echtzeit-Synchronisation beider Stationen.
 */
struct RopewayMessage {
  MessageType type;        ///< Nachrichtentyp (MSG_SPEED, MSG_SENSORS, MSG_HEARTBEAT)
  
  // Parameter für MSG_SPEED (AST -> GST)
  int speed;               ///< Soll-Geschwindigkeit in Schritten/Sekunde
  int accel;               ///< Beschleunigung in Schritten/Sekunde²
  int gondolaDistance;     ///< Soll-Abstand zwischen Gondeln in Schritten
  bool direction;          ///< Fahrtrichtung (true = vorwärts, false = rückwärts)
  
  // Parameter für MSG_SENSORS (GST -> AST)
  bool leftSensor;         ///< Status linke Lichtschranke GST
  bool rightSensor;        ///< Status rechte Lichtschranke GST
  
  // Parameter für MSG_HEARTBEAT (GST -> AST / AST -> GST)
  long virtualRopeSteps;   ///< Virtueller Seilschrittzähler für Gondelabstandshaltung
  bool isHoldingGondola;   ///< Status: Gondel wird aktuell wegen zu geringem Abstand angehalten
};
