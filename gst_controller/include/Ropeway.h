/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Gegenstation (GST) Logik
 * ============================================================================
 * @file      Ropeway.h
 * @brief     Hauptlogik der Gegenstation (Slave). Steuert die 9 Umlenkmotoren,
 *            Gondelabstandshaltung via optischer Lichtschranken und empfängt
 *            Befehle via ESP-NOW von der Antriebsstation (AST).
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

#define normalAccel 500
#define maxSpeed 5000
#define ropeStepperMultiplier 0.9

/**
 * @class Station
 * @brief Virtuelle Basisklasse für Seilbahnstationen.
 */
class Station {
protected:
  int speed = 1875;             ///< Soll-Geschwindigkeit
  bool direction = true;        ///< Fahrtrichtung (true = vorwärts, false = rückwärts)
  bool halted = true;           ///< Betiebszustand: Halt
  bool nothaltActive = false;   ///< Nothalt aktiv
  bool sibreActive = false;     ///< Sicherheitsbremse aktiv

  unsigned long lastMessageReceived = 0;
  unsigned long lastHeartbeatSent = 0;
  bool espNowConnected = false;
  bool connectionLost = false;

public:
  virtual void update() = 0;
  virtual ~Station() {}
  virtual void setLedColor(int r, int g, int b) = 0;

  bool isEspNowConnected() const { return espNowConnected; }

  virtual void executeSpeed(int targetSpeed, int targetAccel) = 0;
  virtual void updateDirPin(bool dir) {}
  virtual void setDirection(bool dir) {
    direction = dir;
    updateDirPin(dir);
    if (!halted && !nothaltActive && !sibreActive) {
      executeSpeed(speed, normalAccel);
    }
  }

  virtual void setSpeed(int nSpeed, int nAccel = normalAccel) {
    speed = nSpeed;
    if (!halted && !nothaltActive && !sibreActive) {
      executeSpeed(nSpeed, nAccel);
    }
  }

  virtual int getSpeed() const { return speed; }
  virtual void startCalibration() {}
  virtual void setGondolaCount(int count) {}
  
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

  virtual void halt() {
    halted = true;
    Serial.println("[Station] HALT aktiviert.");
    executeSpeed(0, 3000);
  }

  virtual void resume() {
    halted = false;
    nothaltActive = false;
    sibreActive = false;
    Serial.println("[Station] Betrieb wieder aufgenommen.");
    executeSpeed(speed, normalAccel);
  }

  virtual void nothalt(bool active) {
    nothaltActive = active;
    if (active) {
      halted = true;
      Serial.println("[Station] NOTHALT aktiviert!");
      executeSpeed(0, 15000);
    } else {
      if (!sibreActive) {
        halted = false;
        executeSpeed(speed, normalAccel);
      }
      Serial.println("[Station] NOTHALT deaktiviert.");
    }
  }

  virtual void sibre(bool active) {
    sibreActive = active;
    if (active) {
      halted = true;
      Serial.println("[Station] SIBRE aktiviert!");
      executeSpeed(0, 50000);
    } else {
      if (!nothaltActive) {
        halted = false;
        executeSpeed(speed, normalAccel);
      }
      Serial.println("[Station] SIBRE deaktiviert.");
    }
  }

  bool isHalted() const { return halted; }

  void setLangsamfahrt(short mode) {
    Serial.printf("[Station] Langsamfahrt %d aktiviert\n", mode);
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
 * @class GegenStation
 * @brief Steuerung der Gegenstation (Slave) mit 9 Stepper-Motoren und Gondelabstandshaltung.
 */
class GegenStation : public Station {
public:
  float gstSpeedMultiplier = 1.4f; ///< Feintuning-Multiplikator für GST-Motoren

private:
  // Pin-Belegungen ESP32-S3 (GST)
  static constexpr uint8_t LEFT_PIN       = 14;
  static constexpr uint8_t RIGHT_PIN      = 13;
  static constexpr uint8_t ACTUAL_DIR_PIN = 1;
  static constexpr uint8_t ENA_PIN        = 2;
  static constexpr uint8_t NUM_MOTORS     = 9;
  static constexpr uint8_t LED_PIN        = 48;

  const uint8_t STEP_PINS[NUM_MOTORS] = { 4, 9, 7, 8, 5, 10, 6, 3, 11 };

  // Stepper-Motoren GST (Linke Förderräder, Rechte Förderräder, Umlenkwende)
  AccelStepper stepperLeft1;
  AccelStepper stepperLeft2;
  AccelStepper stepperLeft3;
  AccelStepper stepperLeft4;
  AccelStepper stepperRight1;
  AccelStepper stepperRight2;
  AccelStepper stepperRight3;
  AccelStepper stepperRight4;
  AccelStepper stepperTurn;

  AccelStepper* steppers[NUM_MOTORS];
  Adafruit_NeoPixel pixels;

  // Sensorik & Abstandshalter-Logik
  bool leftSensorActive = false;
  bool rightSensorActive = false;
  unsigned long lastUpdateTime = 0;
  int gondolaDistance = 10000;
  float virtualRopeSteps = 999999999.0f; ///< Start mit hohem Wert, damit 1. Gondel durchfährt
  bool isHoldingGondola = false;
  bool isMotorStoppedForSpacing = false;

  void updateDirPin(bool dir) override {
    digitalWrite(ACTUAL_DIR_PIN, dir ? LOW : HIGH); // Invertierte Richtung für GST
  }

  /**
   * @brief Übermittelt aktuelle Lichtschrankenzustände per ESP-NOW an die AST
   */
  void sendSensorData() {
    RopewayMessage msg;
    msg.type = MSG_SENSORS;
    msg.leftSensor = this->leftSensorActive;
    msg.rightSensor = this->rightSensorActive;
    esp_now_send(AST_MAC, (uint8_t *) &msg, sizeof(msg));
  }

public:
  GegenStation()
    : Station(),
      stepperLeft1(AccelStepper::DRIVER, STEP_PINS[0], 255),
      stepperLeft2(AccelStepper::DRIVER, STEP_PINS[1], 255),
      stepperLeft3(AccelStepper::DRIVER, STEP_PINS[2], 255),
      stepperLeft4(AccelStepper::DRIVER, STEP_PINS[3], 255),
      stepperRight1(AccelStepper::DRIVER, STEP_PINS[4], 255),
      stepperRight2(AccelStepper::DRIVER, STEP_PINS[5], 255),
      stepperRight3(AccelStepper::DRIVER, STEP_PINS[6], 255),
      stepperRight4(AccelStepper::DRIVER, STEP_PINS[7], 255),
      stepperTurn(AccelStepper::DRIVER, STEP_PINS[8], 255),
      pixels(1, LED_PIN, NEO_GRB + NEO_KHZ800) {

    steppers[0] = &stepperLeft1;
    steppers[1] = &stepperLeft2;
    steppers[2] = &stepperLeft3;
    steppers[3] = &stepperLeft4;
    steppers[4] = &stepperRight1;
    steppers[5] = &stepperRight2;
    steppers[6] = &stepperRight3;
    steppers[7] = &stepperRight4;
    steppers[8] = &stepperTurn;

    pinMode(LEFT_PIN, INPUT);
    pinMode(RIGHT_PIN, INPUT);
    pinMode(ACTUAL_DIR_PIN, OUTPUT);
    pinMode(ENA_PIN, OUTPUT);
    digitalWrite(ENA_PIN, LOW); // Motoren aktivieren
    digitalWrite(ACTUAL_DIR_PIN, direction ? LOW : HIGH);

    pixels.begin();
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();

    for (AccelStepper* stepper : steppers) {
      stepper->setMinPulseWidth(2);
    }
  }

  void setLedColor(int r, int g, int b) override {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
  }

  /**
   * @brief Stellt Geschwindigkeiten für alle 9 Stepper-Motoren der Gegenstation ein
   */
  void executeSpeed(int targetSpeed, int targetAccel) override {
    float scale = (3000.0f / 16125.0f) * gstSpeedMultiplier;
    targetSpeed = targetSpeed * scale;
    targetAccel = targetAccel * scale;
    long targetPos = 2147483647;

    // Linke Förderradgruppe
    stepperLeft1.setAcceleration(targetAccel);
    if (targetSpeed > 0) { stepperLeft1.setMaxSpeed(targetSpeed); stepperLeft1.moveTo(targetPos); }
    else stepperLeft1.stop();
    
    stepperLeft2.setAcceleration(targetAccel * 0.6);
    if (targetSpeed > 0) { stepperLeft2.setMaxSpeed(targetSpeed * 0.6); stepperLeft2.moveTo(targetPos); }
    else stepperLeft2.stop();
    
    stepperLeft3.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperLeft3.setMaxSpeed(targetSpeed * 0.1); stepperLeft3.moveTo(targetPos); }
    else stepperLeft3.stop();

    stepperLeft4.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperLeft4.setMaxSpeed(targetSpeed * 0.1); stepperLeft4.moveTo(targetPos); }
    else stepperLeft4.stop();
    
    // Rechte Förderradgruppe
    stepperRight1.setAcceleration(targetAccel);
    if (targetSpeed > 0) { stepperRight1.setMaxSpeed(targetSpeed); stepperRight1.moveTo(targetPos); }
    else stepperRight1.stop();
    
    stepperRight2.setAcceleration(targetAccel * 0.6);
    if (targetSpeed > 0) { stepperRight2.setMaxSpeed(targetSpeed * 0.6); stepperRight2.moveTo(targetPos); }
    else stepperRight2.stop();
    
    stepperRight3.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperRight3.setMaxSpeed(targetSpeed * 0.1); stepperRight3.moveTo(targetPos); }
    else stepperRight3.stop();

    stepperRight4.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperRight4.setMaxSpeed(targetSpeed * 0.1); stepperRight4.moveTo(targetPos); }
    else stepperRight4.stop();
    
    // Umlenkwende
    stepperTurn.setAcceleration(targetAccel * 0.1);
    if (targetSpeed > 0) { stepperTurn.setMaxSpeed(targetSpeed * 0.1); stepperTurn.moveTo(targetPos); }
    else stepperTurn.stop();
  }

  /**
   * @brief Hauptupdate-Schleife der Gegenstation:
   *        Akkumuliert virtuelle Seilschritte, liest Lichtschranken aus
   *        und verzögert bei Bedarf die Gondel-Ausfahrt zur Abstandshaltung.
   */
  void update() override {
    unsigned long now = millis();
    this->updateConnection(); // Timeout-Prüfung

    // Heartbeat an AST senden
    static unsigned long lastHeartbeatSent = 0;
    if (now - lastHeartbeatSent > 1000) {
      lastHeartbeatSent = now;
      RopewayMessage hb;
      hb.type = MSG_HEARTBEAT;
      hb.virtualRopeSteps = (long)this->virtualRopeSteps;
      hb.isHoldingGondola = this->isHoldingGondola;
      esp_now_send(AST_MAC, (uint8_t *) &hb, sizeof(hb));
    }

    // 1. Virtuelle Seilschritte akkumulieren
    if (lastUpdateTime > 0 && !halted && speed > 0) {
      float dt = (now - lastUpdateTime) / 1000.0f;
      float ropeScale = 3000.0f / 16125.0f;
      virtualRopeSteps += (speed * ropeScale) * dt;
    }
    lastUpdateTime = now;

    // 2. Sensorik & Abstandshaltungs-Logik
    if (direction) { // Vorwärtsfahrt -> Ausfahrt auf der linken Seite
      if (digitalRead(LEFT_PIN) == LOW && !leftSensorActive) {
        leftSensorActive = true;
        sendSensorData();
        
        if (virtualRopeSteps > 3000) {
          if (virtualRopeSteps < gondolaDistance) {
            isHoldingGondola = true; // Abstand zu gering -> Ausfahrt verzögern
          } else {
            isHoldingGondola = false;
            virtualRopeSteps = 0;    // Abstand ausreichend -> Zähler zurücksetzen
          }
        }
      } else if (digitalRead(LEFT_PIN) == HIGH && leftSensorActive) {
        leftSensorActive = false;
        sendSensorData();
      }

      if (isHoldingGondola && virtualRopeSteps >= gondolaDistance) {
        isHoldingGondola = false;
        virtualRopeSteps = 0;
      }
      
      float scale = (3000.0f / 16125.0f) * gstSpeedMultiplier;
      int tSpeed = speed * scale;
      if (isHoldingGondola && leftSensorActive) {
        if (!isMotorStoppedForSpacing) {
          stepperLeft4.setSpeed(0);
          stepperLeft4.moveTo(stepperLeft4.currentPosition());
          isMotorStoppedForSpacing = true;
        }
      } else {
        if (isMotorStoppedForSpacing) {
          stepperLeft4.setMaxSpeed(tSpeed * 0.1);
          stepperLeft4.moveTo(2147483647);
          isMotorStoppedForSpacing = false;
        }
      }
      
      if (digitalRead(RIGHT_PIN) == LOW && !rightSensorActive) {
        rightSensorActive = true;
        sendSensorData();
      } else if (digitalRead(RIGHT_PIN) == HIGH && rightSensorActive) {
        rightSensorActive = false;
        sendSensorData();
      }

    } else { // Rückwärtsfahrt -> Ausfahrt auf der rechten Seite
      if (digitalRead(RIGHT_PIN) == LOW && !rightSensorActive) {
        rightSensorActive = true;
        sendSensorData();
        
        if (virtualRopeSteps > 3000) {
          if (virtualRopeSteps < gondolaDistance) {
            isHoldingGondola = true;
          } else {
            isHoldingGondola = false;
            virtualRopeSteps = 0;
          }
        }
      } else if (digitalRead(RIGHT_PIN) == HIGH && rightSensorActive) {
        rightSensorActive = false;
        sendSensorData();
      }

      if (isHoldingGondola && virtualRopeSteps >= gondolaDistance) {
        isHoldingGondola = false;
        virtualRopeSteps = 0;
      }
      
      float scale = (3000.0f / 16125.0f) * gstSpeedMultiplier;
      int tSpeed = speed * scale;
      if (isHoldingGondola && rightSensorActive) {
        if (!isMotorStoppedForSpacing) {
          stepperRight4.setSpeed(0);
          stepperRight4.moveTo(stepperRight4.currentPosition());
          isMotorStoppedForSpacing = true;
        }
      } else {
        if (isMotorStoppedForSpacing) {
          stepperRight4.setMaxSpeed(tSpeed * 0.1);
          stepperRight4.moveTo(2147483647);
          isMotorStoppedForSpacing = false;
        }
      }
      
      if (digitalRead(LEFT_PIN) == LOW && !leftSensorActive) {
        leftSensorActive = true;
        sendSensorData();
      } else if (digitalRead(LEFT_PIN) == HIGH && leftSensorActive) {
        leftSensorActive = false;
        sendSensorData();
      }
    }

    // Stepper-Motoren ausführen
    for (int i = 0; i < NUM_MOTORS; i++) {
      steppers[i]->run();
    }
  }

  void handleMessage(const RopewayMessage& msg) override {
    Station::handleMessage(msg);
    if (msg.type == MSG_SPEED) {
      if (this->direction != msg.direction) {
        this->setDirection(msg.direction);
      }
      
      if (msg.speed > 0) {
        if (this->halted) this->resume();
        this->setSpeed(msg.speed, msg.accel);
      } else {
        this->halted = true;
        this->speed = 0;
        this->executeSpeed(0, msg.accel);
      }

      this->gondolaDistance = msg.gondolaDistance;
    }
  }
};