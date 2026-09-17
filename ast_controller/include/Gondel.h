/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Gondel-Klasse
 * ============================================================================
 * @file      Gondel.h
 * @brief     Repräsentiert eine Gondel im Seilbahnsystem zur Standortverfolgung 
 *            (Zone/Streckenabschnitt und Odometer-Fortschritt).
 * ============================================================================
 */

#pragma once
#include "Zone.h"

/**
 * @class Gondola
 * @brief Datenmodell für eine einzelne Kabine / Gondel auf der Strecke.
 */
class Gondola {
  private:
    int id;               ///< Eindeutige Gondel-ID (1, 2, 3...)
    Zone zone;            ///< Aktueller Streckenabschnitt (AST, GST, leftTrack, rightTrack)
    long entryOdometer;   ///< Odometer-Schrittstand beim Eintritt in die aktuelle Zone

  public:
    /**
     * @brief Konstruktor
     * @param nId Eindeutige Kabinen-ID
     */
    Gondola(int nId) {
      id = nId;
      zone = AST;
      entryOdometer = 0;
    }

    // Getter
    int getId() const { return id; }
    Zone getZone() const { return zone; }
    long getEntryOdometer() const { return entryOdometer; }

    /**
     * @brief Aktualisiert Zone und merkt sich den Odometer-Stand beim Eintritt
     */
    void setZone(Zone nZone, long currentOdo) {
      zone = nZone;
      entryOdometer = currentOdo;
    }
    
    /**
     * @brief Kompatibilitätsfunktion zur Initialisierung der Zone
     */
    void setZone(Zone nZone) {
      zone = nZone;
    }
};