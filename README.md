# 🚡 3S-Seilbahn Modellsteuerung (3S Ropeway Model Controller)

Ein professionelles, verteiltes Mikrocontroller-Steuerungssystem für ein funktionsfähiges **3S-Dreiseilbahn-Modell** (Dreiseilbahn / 3S Gondola Ropeway) mit **Dual-ESP32-S3**, drahtloser **ESP-NOW Synchronisation**, automatischer **Gondelabstandshaltung**, **Lichtschrankenauswertung** und einem modernen **WebUI-HMI Leitstand**.

---

## 🌟 Features

- **Dual ESP32 Architecture (Master/Slave)**:
  - **Antriebsstation (AST) - Master**: Steuert den Hauptseilmotor, Reibräder-Verzögerung/Beschleunigung, hostet das HMI WebUI auf **FreeRTOS Core 0** und verarbeitet Benutzerbefehle.
  - **Gegenstation (GST) - Slave**: Steuert 9 Stepper-Motoren (Reibräder & Wende), wertet Stations-Lichtschranken aus und regelt den Ausfahrtsabstand der Gondeln autonom.
- **Drahtlose Synchronisation via ESP-NOW**:
  - Latenzfreie Funkverbindung zwischen AST und GST per espNOW.
  - Automatische Verbindungsüberwachung mit Failsafe: Verbindungsabbruch führt automatisch zum Nothalt.
- **Autonome Gondelabstandshaltung (Distance Control)**:
  - Überwachung des Mindestabstands zwischen aufeinanderfolgenden Kabinen an der Gegenstation.
  - Automatische Halte- & Ausfahrtsverzögerung an der Lichtschranke (`Left4` / `Right4`), bis der geforderte Seilabstand erreicht ist.
- **Echtzeit-Standortverfolgung (Odometer & Zonen)**:
  - Live-Verfolgung aller Gondeln in den Zonen `AST`, `GST`, `leftTrack` und `rightTrack` inkl. Fortschrittsprozenten.
- **Sicherheits- & Betriebsmodi**:
  - **Betriebsarten**: Anwurf- & Abfahrt-Sequenz (Zweistufige Freigabe), Regulärer Halt, Nothalt, Sicherheitsbremse, Langsamfahrt (20% / 10%).
- **WebUI HMI Leitstand**:
  - Hostet ein Dashboard mit Drehregler (Soll-Geschwindigkeit), Tachometer (Echtzeit-Motor-Ist-Geschwindigkeit ab 0.5 m/s), Betriebs-LEDs, Richtungswechsel und Gondel-Übersicht.

---

## 🏗️ Systemarchitektur

```mermaid
graph TD
    Browser[💻 Browser / HMI WebUI] <-->|HTTP REST / WebSocket| AST[🧠 AST ESP32-S3 Master]
    AST <-->|ESP-NOW Wireless| GST[⚙️ GST ESP32-S3 Slave]
    
    subgraph Antriebsstation (AST)
        AST --> MotorRope[Seilmotor - Hauptantrieb]
        AST --> MotorAST[4x Reibräder-Förderer + Wende]
        AST --> SensorsAST[2x Lichtschranken Station]
    end

    subgraph Gegenstation (GST)
        GST --> MotorGST[8x Reibräder-Förderer + 1x Wende]
        GST --> SensorsGST[2x Lichtschranken Station]
    end
```

---

## 🛠️ Hardware- & Pin-Belegung

### Antriebsstation (AST - Master)
- **ESP32-S3** Controller
- **Motoren (6 Stepper via Step/Dir Treibern e.g. TMC2209 / A4988)**:
  - `STEP_PINS`: GPIO 3, 4, 5, 6, 7, 8 (Förderräder, Wende, Seilmotor)
  - `ACTUAL_DIR_PIN`: GPIO 1 (Gemeinsames Richtungs-Signal)
  - `ENA_PIN`: GPIO 2 (Driver Enable / Haltemoment)
- **Lichtschranken**:
  - Links: GPIO 11
  - Rechts: GPIO 10
- **Status-LED**: NeoPixel GPIO 48

### Gegenstation (GST - Slave)
- **ESP32-S3** Controller
- **Motoren (9 Stepper)**:
  - `STEP_PINS`: GPIO 4, 9, 7, 8, 5, 10, 6, 3, 11 (4x Links, 4x Rechts, 1x Wende)
  - `ACTUAL_DIR_PIN`: GPIO 1 (Invertiertes Richtungs-Signal)
  - `ENA_PIN`: GPIO 2
- **Lichtschranken**:
  - Links: GPIO 14
  - Rechts: GPIO 13
- **Status-LED**: NeoPixel GPIO 48

---

## 📂 Projektstruktur

```text
.
├── AST_esp_code/           # Quellcode für die Antriebsstation (Master)
│   ├── AST_esp_code.ino    # Hauptdatei, Webserver Setup & FreeRTOS Tasking
│   ├── Ropeway.h           # Basisklasse Station & AntriebsStation Logik
│   ├── Gondel.h            # Gondel-Klasse & Odometer-Tracking
│   ├── Messages.h          # ESP-NOW Nachrichtenstruktur
│   ├── Zone.h              # Enum der Streckenabschnitte
│   ├── WebUI.h             # Gecodete HTML/JS-Oberfläche (generiert)
│   └── index.html          # Quell-Weboberfläche (HTML5/CSS3/JS)
│
├── GST_esp_code/           # Quellcode für die Gegenstation (Slave)
│   ├── GST_esp_code.ino    # Hauptdatei & ESP-NOW Empfänger
│   ├── Ropeway.h           # GegenStation Logik & Abstandshaltersteuerung
│   ├── Gondel.h            # Gondel-Klasse
│   ├── Messages.h          # ESP-NOW Nachrichtenstruktur
│   └── Zone.h              # Enum der Streckenabschnitte
│
├── pack_webui.py           # Python-Skript zum Komprimieren von index.html in WebUI.h
└── README.md               # Dokumentation
```

---

## 🚀 Installation & Inbetriebnahme

### 1. Abhängigkeiten
Folgende Arduino-Bibliotheken werden benötigt (via Bibliotheksverwalter installierbar):
- **AccelStepper** (v1.61+)
- **ArduinoJson** (v7.x)
- **Adafruit NeoPixel**

### 2. MAC-Adressen anpassen
Trage vor dem Flashen die MAC-Adressen deiner beiden ESP32-S3 Boards in `AST_esp_code.ino` und `GST_esp_code.ino` ein:
```cpp
const uint8_t GST_MAC[6] = { 0x14, 0xC1, 0x9F, 0x2B, 0x33, 0xEC };
const uint8_t AST_MAC[6] = { 0x44, 0x1B, 0xF6, 0xFF, 0x51, 0x60 };
```

### 3. Kompilieren & Flashen
1. **AST_esp_code** auf den Master-ESP32 hochladen.
2. **GST_esp_code** auf den Slave-ESP32 hochladen.
3. Nach dem Start verbindet sich die AST mit deinem WLAN (SSID in `AST_esp_code.ino` konfigurierbar).
4. Ruf die angezeigte IP-Adresse der AST im Browser auf, um das HMI-Leitstand WebUI zu öffnen.

### 4. WebUI anpassen (`index.html`)
Falls du Änderungen am WebUI in `AST_esp_code/index.html` vornimmst, führe im Projektverzeichnis einfach das Pack-Skript aus:
```bash
python3 pack_webui.py
```
Dies aktualisiert automatisch die `WebUI.h` Header-Datei für den ESP32.

---

## 📜 Lizenz

Dieses Projekt steht unter der **GPL-3.0 license**. Frei nutzbar, anpassbar und weiterverbreitbar für Modellbau-Enthusiasten und Open-Source-Projekte.
