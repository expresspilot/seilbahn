/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Antriebsstation (AST) Logik
 * ============================================================================
 * @file      Ropeway.h
 * @brief     Hauptlogik der Antriebsstation (Master). Enthält die Basisklasse Station
 *            und die Implementierung der AntriebsStation (Hauptseilantrieb,
 *            Stationsförderer, HMI-Status-JSON & Gondel-Tracking).
 * 
 * @author    Open Source Contributor
 * @date      2026
 * @license   MIT License
 * ============================================================================
 */

#pragma once

#include <stdint.h>
#include <vector>
#include <AccelStepper.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#include "Gondel.h"
#include "Messages.h"

// MAC-Adressen der beiden ESP32 Controller
extern const uint8_t AST_MAC[6];
extern const uint8_t GST_MAC[6];

// Standardkonstanten für Antrieb & Streckenprofil
#define normalAccel 3000
#define maxSpeed 30000
#define ropeStepperMultiplier 0.8
#define DEFAULT_TRACK_LENGTH 400000L      ///< Standard-Streckenlänge in Seilschritten
#define DEFAULT_STATION_LENGTH 300000L    ///< Standard-Stationslänge in Schritten

/**
 * @class Station
 * @brief Virtuelle Basisklasse für beide Seilbahn-Stationen (AST & GST).
 */
class Station {
public:
  enum StartupState { 
    READY,           ///< Normaler Betrieb läuft
    WAITING_ANWURF,  ///< Warten auf "Anwurf"-Taster (Freigabe Stufe 1)
    WAITING_ABFAHRT  ///< Warten auf "Abfahrt"-Taster (Freigabe Stufe 2)
  };

protected:
  StartupState startupState = WAITING_ANWURF;
  int speed = 1875;             ///< Aktuelle Soll-Geschwindigkeit (Start bei 0.5 m/s)
  bool direction = true;        ///< Fahrtrichtung (true = Vorwärts / rightTrack, false = Rückwärts)
  bool halted = true;           ///< Betiebszustand: Halt aktiv
  bool nothaltActive = false;   ///< Betiebszustand: Nothalt aktiv
  bool sibreActive = false;     ///< Betiebszustand: Sicherheitsbremse aktiv

  unsigned long lastMessageReceived = 0;  ///< Zeitstempel des letzten ESP-NOW Empfangs
  unsigned long lastHeartbeatSent = 0;    ///< Zeitstempel des letzten gesendeten Heartbeats
  bool espNowConnected = false;           ///< Status der Funkverbindung
  bool connectionLost = false;            ///< Timeout-Flag bei Verbindungsabbruch

public:
  virtual ~Station() {}

  // Pure virtual methods
  virtual void update() = 0;
  virtual void executeSpeed(int targetSpeed, int targetAccel) = 0;
  virtual void setLedColor(int r, int g, int b) {}

  // Getter & Setter
  bool isEspNowConnected() const { return espNowConnected; }
  bool isHalted() const { return halted; }
  virtual int getSpeed() const { return speed; }

  virtual void setSpeed(int nSpeed, int nAccel = normalAccel) {
    speed = nSpeed;
    if (startupState == READY && !halted && !nothaltActive && !sibreActive) {
      executeSpeed(nSpeed, nAccel);
    }
  }

  virtual void updateDirPin(bool dir) {}
  virtual void setDirection(bool dir) {
    direction = dir;
    updateDirPin(dir);
    if (startupState == READY && !halted && !nothaltActive && !sibreActive) {
      executeSpeed(speed, normalAccel);
    }
  }

  virtual void startCalibration() {}
  virtual void setGondolaCount(int count) {}

  /**
   * @brief Verarbeitet eingehende ESP-NOW Nachrichten (Verbindungsüberwachung)
   */
  virtual void handleMessage(const RopewayMessage& msg) {
    lastMessageReceived = millis();
    if (!espNowConnected) {
      espNowConnected = true;
      Serial.println("[Station] ESP-NOW Verbindung hergestellt.");
      if (connectionLost) {
        connectionLost = false;
        this->nothalt(false);
      }
    }
  }

  /**
   * @brief Prüft den ESP-NOW Verbindungstimeout (3000ms -> Nothalt)
   */
  virtual void updateConnection() {
    unsigned long now = millis();
    if (espNowConnected && (now - lastMessageReceived > 3000)) {
      espNowConnected = false;
      connectionLost = true;
      Serial.println("[Station] ESP-NOW VERBINDUNGSABBRUCH -> NOTHALT!");
      this->nothalt(true);
    }
  }

  virtual void handleWebCommand(const String& action, JsonVariant doc) {}
  virtual String getStatusJson() { return "{}"; }

  /**
   * @brief Löst den regulären Halt aus
   */
  virtual void halt() {
    halted = true;
    startupState = WAITING_ANWURF;
    Serial.println("[Station] Regulärer HALT ausgelöst.");
    executeSpeed(0, 3000);
  }

  /**
   * @brief Prüft und startet den Betrieb nach Anwurf & Abfahrt
   */
  void checkStartupSequence() {
    if (halted || nothaltActive || sibreActive) return;
    if (startupState == READY) {
      Serial.println("[Station] Betrieb wieder aufgenommen.");
      executeSpeed(speed, normalAccel);
    }
  }

  /**
   * @brief Schaltet den Nothalt (Ein/Aus)
   */
  virtual void nothalt(bool active) {
    nothaltActive = active;
    if (active) {
      startupState = WAITING_ANWURF;
      Serial.println("[Station] NOTHALT aktiviert!");
      executeSpeed(0, 15000);
    } else {
      checkStartupSequence();
      Serial.println("[Station] NOTHALT deaktiviert.");
    }
  }

  /**
   * @brief Schaltet die Sicherheitsbremse (SIBRE)
   */
  virtual void sibre(bool active) {
    sibreActive = active;
    if (active) {
      startupState = WAITING_ANWURF;
      Serial.println("[Station] SIBRE aktiviert!");
      executeSpeed(0, 50000);
    } else {
      checkStartupSequence();
      Serial.println("[Station] SIBRE deaktiviert.");
    }
  }

  /**
   * @brief Verarbeitet physikalische Button-Events (Anwurf / Abfahrt Taster)
   */
  virtual void handleButtonPress(String button, bool state) {
    if (state) {
      if (button == "anwurf") {
        if (!espNowConnected) {
          Serial.println("[Station] Anwurf ignoriert: Keine ESP-NOW Verbindung!");
        } else if (!nothaltActive && !sibreActive) {
          if (startupState == WAITING_ANWURF) {
            startupState = WAITING_ABFAHRT;
            Serial.println("[Station] Anwurf empfangen. Warte auf Abfahrt-Freigabe.");
          }
        } else {
          Serial.println("[Station] Anwurf ignoriert: Nothalt/Sibre noch aktiv!");
        }
      } else if (button == "abfahrt") {
        if (startupState == WAITING_ABFAHRT) {
          startupState = READY;
          halted = false;
          Serial.println("[Station] Abfahrt empfangen. Betrieb laeuft an.");
          checkStartupSequence();
        }
      }
    }
  }

  /**
   * @brief Schaltet den Langsamfahr-Modus (0 = Normal, 1 = 20%, 2 = 10%)
   */
  void setLangsamfahrt(short mode) {
    Serial.printf("[Station] Langsamfahrt Modus %d aktiviert\n", mode);
    switch (mode) {
      case 0:
        this->setSpeed(speed);
        break;
      case 1:
        if (speed > maxSpeed * 0.2) setSpeed(maxSpeed * 0.2);
        break;
      case 2:
        if (speed > maxSpeed * 0.1) setSpeed(maxSpeed * 0.1);
        break;
    }
  }
};

/**
 * @class AntriebsStation
 * @brief Implementiert die Steuerung der Hauptantriebsstation (Master).
 */
class AntriebsStation : public Station {
private:
  // Pin-Belegungen ESP32-S3 (AST)
  static constexpr uint8_t LEFT_PIN       = 11;
  static constexpr uint8_t RIGHT_PIN      = 10;
  static constexpr uint8_t ACTUAL_DIR_PIN = 1;
  static constexpr uint8_t ENA_PIN        = 2;
  static constexpr uint8_t NUM_MOTORS     = 6;
  static constexpr uint8_t LED_PIN        = 48;

  const uint8_t STEP_PINS[NUM_MOTORS] = { 3, 4, 5, 6, 7, 8 };

  // Stepper-Motoren für Reibräder & Hauptseil
  AccelStepper stepper1;
  AccelStepper stepper2;
  AccelStepper stepper3;
  AccelStepper stepper4;
  AccelStepper stepperTurn;
  AccelStepper stepperRope;  ///< Seilmotor (Hauptantrieb)

  AccelStepper* steppers[NUM_MOTORS];
  Adafruit_NeoPixel pixels;

  // Sensor-Zustände
  bool leftSensorActive     = false;
  bool rightSensorActive    = false;
  bool gstLeftSensorActive  = false;
  bool gstRightSensorActive = false;

  long gstVirtualRopeSteps  = 0;
  bool gstHoldingGondola    = false;

  // Sensor Flankenerkennung für Zonen-Transitiom
  bool lastAstLeftSensor    = false;
  bool lastAstRightSensor   = false;
  bool lastGstLeftSensor    = false;
  bool lastGstRightSensor   = false;

  // Gondel- & Streckenkonfiguration
  int gondolaCount          = 3;
  long trackLength          = DEFAULT_TRACK_LENGTH;
  int gondolaDistance       = 10000;
  static constexpr long AST_STATION_LENGTH = DEFAULT_STATION_LENGTH;
  static constexpr long GST_STATION_LENGTH = DEFAULT_STATION_LENGTH;
  std::vector<Gondola> gondolas;
  
  void updateDirPin(bool dir) override {
    digitalWrite(ACTUAL_DIR_PIN, dir ? HIGH : LOW);
  }

  /**
   * @brief Verschiebt die am längsten in fromZone befindliche Gondola in toZone.
   */
  void transitionGondola(Zone fromZone, Zone toZone, long currentOdo) {
    Gondola* bestGondola = nullptr;
    long maxDist = -1;
    for (auto& g : gondolas) {
      if (g.getZone() == fromZone) {
        long dist = abs(currentOdo - g.getEntryOdometer());
        if (dist > maxDist) {
          maxDist = dist;
          bestGondola = &g;
        }
      }
    }
    if (bestGondola != nullptr) {
      bestGondola->setZone(toZone, currentOdo);
    }
  }

  /**
   * @brief Aktualisiert das Gondel-Tracking bei Sensorflanken (Lichtschranken AST/GST)
   */
  void updateTracking(long currentOdo) {
    bool currentAstLeft = (digitalRead(LEFT_PIN) == LOW);
    bool currentAstRight = (digitalRead(RIGHT_PIN) == LOW);

    if (direction) { // Vorwärtsfahrt
      if (currentAstRight && !lastAstRightSensor) transitionGondola(AST, rightTrack, currentOdo);
      if (gstRightSensorActive && !lastGstRightSensor) transitionGondola(rightTrack, GST, currentOdo);
      if (gstLeftSensorActive && !lastGstLeftSensor) transitionGondola(GST, leftTrack, currentOdo);
      if (currentAstLeft && !lastAstLeftSensor) transitionGondola(leftTrack, AST, currentOdo);
    } else { // Rückwärtsfahrt
      if (currentAstLeft && !lastAstLeftSensor) transitionGondola(AST, leftTrack, currentOdo);
      if (gstLeftSensorActive && !lastGstLeftSensor) transitionGondola(leftTrack, GST, currentOdo);
      if (gstRightSensorActive && !lastGstRightSensor) transitionGondola(GST, rightTrack, currentOdo);
      if (currentAstRight && !lastAstRightSensor) transitionGondola(rightTrack, AST, currentOdo);
    }

    lastAstLeftSensor = currentAstLeft;
    lastAstRightSensor = currentAstRight;
    lastGstLeftSensor = gstLeftSensorActive;
    lastGstRightSensor = gstRightSensorActive;
  }

public:
  AntriebsStation()
    : Station(),
      stepper1(AccelStepper::DRIVER, STEP_PINS[1], 255),
      stepper2(AccelStepper::DRIVER, STEP_PINS[0], 255),
      stepper3(AccelStepper::DRIVER, STEP_PINS[2], 255),
      stepper4(AccelStepper::DRIVER, STEP_PINS[3], 255),
      stepperTurn(AccelStepper::DRIVER, STEP_PINS[4], 255),
      stepperRope(AccelStepper::DRIVER, STEP_PINS[5], 255),
      pixels(1, LED_PIN, NEO_GRB + NEO_KHZ800) {

    steppers[0] = &stepper1;
    steppers[1] = &stepper2;
    steppers[2] = &stepper3;
    steppers[3] = &stepper4;
    steppers[4] = &stepperTurn;
    steppers[5] = &stepperRope;

    pinMode(LEFT_PIN, INPUT);
    pinMode(RIGHT_PIN, INPUT);
    pinMode(ACTUAL_DIR_PIN, OUTPUT);
    pinMode(ENA_PIN, OUTPUT);
    digitalWrite(ENA_PIN, LOW); // Motoren aktivieren (Haltemoment)
    digitalWrite(ACTUAL_DIR_PIN, direction ? HIGH : LOW);

    pixels.begin();
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();

    // Pulse-Breite auf 2us setzen (Spart erhebliche CPU-Zeit auf ESP32)
    for (AccelStepper* stepper : steppers) {
      stepper->setMinPulseWidth(2);
    }

    setGondolaCount(gondolaCount);
  }

  void setGondolaCount(int count) override {
    if (count < 1) count = 1;
    if (count > 10) count = 10;
    gondolaCount = count;
    gondolas.clear();
    
    long currentOdo = stepperRope.currentPosition();
    long totalLoop = 2 * trackLength + AST_STATION_LENGTH + GST_STATION_LENGTH;
    
    for (int i = 0; i < gondolaCount; i++) {
      Gondola g(i + 1);
      long pos = (i * totalLoop) / gondolaCount;
      
      if (pos < trackLength) {
        g.setZone(rightTrack, currentOdo - pos);
      } else if (pos < trackLength + GST_STATION_LENGTH) {
        g.setZone(GST, currentOdo - (pos - trackLength));
      } else if (pos < 2 * trackLength + GST_STATION_LENGTH) {
        g.setZone(leftTrack, currentOdo - (pos - (trackLength + GST_STATION_LENGTH)));
      } else {
        g.setZone(AST, currentOdo - (pos - (2 * trackLength + GST_STATION_LENGTH)));
      }
      
      gondolas.push_back(g);
    }
  }

  /**
   * @brief Sendet Soll-Geschwindigkeit via ESP-NOW an GST und stellt alle Stepper ein.
   */
  void executeSpeed(int targetSpeed, int targetAccel) override {
    Serial.printf("[AST] executeSpeed: targetSpeed=%d, targetAccel=%d\n", targetSpeed, targetAccel);
    
    RopewayMessage msg;
    msg.type = MSG_SPEED;
    msg.speed = targetSpeed;
    msg.accel = targetAccel;
    msg.direction = this->direction;
    msg.gondolaDistance = this->gondolaDistance;
    esp_now_send(GST_MAC, (uint8_t *) &msg, sizeof(msg));

    long targetPos = 2147483647; // Vorwärtsfahrt ohne festes Ziel

    // Förderräder AST
    stepper1.setAcceleration(targetAccel);
    if (targetSpeed > 0) { stepper1.setMaxSpeed(targetSpeed); stepper1.moveTo(targetPos); }
    else stepper1.stop();
    
    stepper2.setAcceleration(targetAccel * 0.6);
    if (targetSpeed > 0) { stepper2.setMaxSpeed(targetSpeed * 0.6); stepper2.moveTo(targetPos); }
    else stepper2.stop();
    
    stepper3.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepper3.setMaxSpeed(targetSpeed * 0.1); stepper3.moveTo(targetPos); }
    else stepper3.stop();

    stepper4.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepper4.setMaxSpeed(targetSpeed * 0.1); stepper4.moveTo(targetPos); }
    else stepper4.stop();

    // Umlenkwende
    stepperTurn.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperTurn.setMaxSpeed(targetSpeed * 0.1); stepperTurn.moveTo(targetPos); }
    else stepperTurn.stop();

    // Seilmotor
    stepperRope.setAcceleration(targetAccel * ropeStepperMultiplier);
    if (targetSpeed > 0) { stepperRope.setMaxSpeed(targetSpeed * ropeStepperMultiplier); stepperRope.moveTo(targetPos); }
    else stepperRope.stop();
  }

  /**
   * @brief Hauptupdate-Schleife der AST (Real-Time Stepper Execution & Sensorik)
   */
  void update() override {
    unsigned long now = millis();
    this->updateConnection(); // Timeout-Prüfung
    
    // Heartbeat an GST senden (jede 1000ms)
    if (now - lastHeartbeatSent > 1000) {
      lastHeartbeatSent = now;
      RopewayMessage hb;
      hb.type = MSG_HEARTBEAT;
      esp_now_send(GST_MAC, (uint8_t *) &hb, sizeof(hb));
    }

    // Stepper-Schritte ausführen
    for (AccelStepper* stepper : steppers) {
      stepper->run();
    }

    long currentRopeSteps = stepperRope.currentPosition();
    updateTracking(currentRopeSteps);

    // Lichtschranken AST abfragen
    if (digitalRead(LEFT_PIN) == LOW && !leftSensorActive) {
      leftSensorActive = true;
    } else if (digitalRead(LEFT_PIN) == HIGH && leftSensorActive) {
      leftSensorActive = false;
    }

    if (digitalRead(RIGHT_PIN) == LOW && !rightSensorActive) {
      rightSensorActive = true;
    } else if (digitalRead(RIGHT_PIN) == HIGH && rightSensorActive) {
      rightSensorActive = false;
    }
  }

  void handleMessage(const RopewayMessage& msg) override {
    Station::handleMessage(msg);
    if (msg.type == MSG_HEARTBEAT) {
      this->gstVirtualRopeSteps = msg.virtualRopeSteps;
      this->gstHoldingGondola = msg.isHoldingGondola;
    }

    if (msg.type == MSG_SENSORS) {
      this->gstLeftSensorActive = msg.leftSensor;
      this->gstRightSensorActive = msg.rightSensor;
    }
  }

  /**
   * @brief Verarbeitet Befehle vom WebUI (REST API)
   */
  void handleWebCommand(const String& action, JsonVariant doc) override {
    if (action == "setSpeed") {
      double val = doc["value"] | 0.0;
      int stepsSec = (val / 8.0) * maxSpeed;
      this->setSpeed(stepsSec);
    }
    else if (action == "setDirection") {
      String dirStr = doc["value"] | "forward";
      bool dir = (dirStr == "forward");
      if (dir != this->direction) {
        if (stepper1.isRunning()) {
          Serial.println("[AST] Richtungswechsel während der Fahrt blockiert -> NOTHALT!");
          this->nothalt(true);
        }
        this->setDirection(dir);
      }
    }
    else if (action == "buttonPress") {
      String btn = doc["button"].as<String>();
      bool state = doc["state"].as<bool>();
      this->handleButtonPress(btn, state);
    }
    else if (action == "ledColor") {
      int r = doc["r"] | 0;
      int g = doc["g"] | 0;
      int b = doc["b"] | 0;
      this->setLedColor(r, g, b);
    }
    else if (action == "halt") {
      this->halt();
    }
    else if (action == "nothalt") {
      bool active = doc["value"] | false;
      this->nothalt(active);
    }
    else if (action == "sibre") {
      bool active = doc["value"] | false;
      this->sibre(active);
    }
    else if (action == "setGondolaCount") {
      int count = doc["value"] | 3;
      this->setGondolaCount(count);
      this->executeSpeed(this->speed, normalAccel);
    }
    else if (action == "setTrackLength") {
      trackLength = doc["value"] | DEFAULT_TRACK_LENGTH;
    }
    else if (action == "setGondolaDistance") {
      int dist = doc["value"] | 10000;
      this->gondolaDistance = dist;
      this->executeSpeed(this->speed, normalAccel);
    }
    else if (action == "setSlowMode") {
      String mode = doc["value"] | "normal";
      if (mode == "normal") this->setLangsamfahrt(0);
      else if (mode == "slow1") this->setLangsamfahrt(1);
      else if (mode == "slow2") this->setLangsamfahrt(2);
    }
  }

  /**
   * @brief Generiert das Status-JSON für das WebUI (Tachometer, Gondeln, Sensoren)
   */
  String getStatusJson() override {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["distance"] = abs(stepperRope.currentPosition()) * 0.01;

    // Echte Ist-Geschwindigkeit (ab 0.5 m/s, unter 0.5 m/s = 0.0 m/s)
    double realSpeed = 0.0;
    if (stepperRope.distanceToGo() != 0) {
      double rawMps = ((double)abs(stepperRope.speed()) / ((double)maxSpeed * ropeStepperMultiplier)) * 8.0;
      realSpeed = (rawMps < 0.5) ? 0.0 : (round(rawMps * 10.0) / 10.0);
    }
    doc["speed"] = realSpeed;
    doc["nothalt"] = nothaltActive;
    doc["sibre"] = sibreActive;
    doc["halt"] = halted;
    doc["esp_now_connected"] = espNowConnected;
    doc["mode"] = (realSpeed == 0) ? "Stop" : "Normal";
    doc["direction"] = direction ? "forward" : "backwards";
    
    doc["gstVirtualRopeSteps"] = gstVirtualRopeSteps;
    doc["gstHoldingGondola"] = gstHoldingGondola;

    JsonObject sensors = doc.createNestedObject("sensors");
    sensors["astLeft"] = leftSensorActive;
    sensors["astRight"] = rightSensorActive;
    sensors["gstLeft"] = gstLeftSensorActive;
    sensors["gstRight"] = gstRightSensorActive;

    JsonArray gArr = doc["gondolas"].to<JsonArray>();
    for (const auto& g : gondolas) {
      JsonObject gObj = gArr.add<JsonObject>();
      gObj["id"] = g.getId();
      
      long currentOdo = stepperRope.currentPosition();
      long dist = abs(currentOdo - g.getEntryOdometer());
      float progress = 0.0f;
      if (g.getZone() == AST) progress = (float)dist / AST_STATION_LENGTH;
      else if (g.getZone() == GST) progress = (float)dist / GST_STATION_LENGTH;
      else progress = (float)dist / trackLength;
      
      if (progress > 1.0f) progress = 1.0f;
      gObj["progress"] = progress;
      gObj["isStopped"] = false;
      
      switch (g.getZone()) {
        case AST: gObj["zone"] = "AST"; break;
        case GST: gObj["zone"] = "GST"; break;
        case leftTrack: gObj["zone"] = "leftTrack"; break;
        case rightTrack: gObj["zone"] = "rightTrack"; break;
      }
    }

    JsonObject gconf = doc["gondolaConfig"].to<JsonObject>();
    gconf["gondolaCount"] = gondolaCount;
    gconf["trackLength"] = trackLength;
    gconf["gondolaDistance"] = gondolaDistance;
    gconf["currentRopeSteps"] = stepperRope.currentPosition();

    String output;
    serializeJson(doc, output);
    return output;
  }
};