/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Gondel-Klasse (Gegenstation)
 * ============================================================================
 * @file      Gondel.h
 * @brief     Repräsentiert eine Gondel für die Simulation/Verfolgung an der 
 *            Gegenstation (GST).
 * ============================================================================
 */

#pragma once
#include "Zone.h"

/**
 * @class Gondola
 * @brief Datenmodell zur Nachverfolgung der Kabinen auf den Streckenabschnitten.
 */
class Gondola {
  private:
    int id;                 ///< Eindeutige Gondel-ID
    bool isStopped;         ///< Status: Kabine aktuell in Halteposition
    Zone zone;              ///< Aktueller Streckenabschnitt
    short progress;         ///< Fortschritt in % innerhalb des Abschnitts
    int stepsAccumulated;   ///< Akkumulierte Schrittzahl

  public:
    Gondola(int nId) {
      id = nId;
      isStopped = false;
      zone = AST;
      progress = 0;
      stepsAccumulated = 0;
    }

    void setZone(Zone nZone) { zone = nZone; }
    void setProgress(short nProgress) { progress = nProgress; }

    int getId() const { return id; }
    Zone getZone() const { return zone; }
    short getProgress() const { return progress; }
    bool getIsStopped() const { return isStopped; }
    int getStepsAccumulated() const { return stepsAccumulated; }

    /**
     * @brief Aktualisiert die Position der Gondel auf der Strecke basierend auf gefahrenen Schritten.
     */
    void updatePosition(int steps, int stepsPerSide) {
      int refSteps = (stepsPerSide > 0) ? stepsPerSide : 1000;
      int stepsForZone = (zone == AST || zone == GST) ? (refSteps * 2 / 10) : refSteps;
      if (stepsForZone <= 0) stepsForZone = 200;

      stepsAccumulated += steps;
      if (steps > 0) {
        while (stepsAccumulated >= stepsForZone) {
          stepsAccumulated -= stepsForZone;
          if (zone == AST) zone = rightTrack;
          else if (zone == rightTrack) zone = GST;
          else if (zone == GST) zone = leftTrack;
          else if (zone == leftTrack) zone = AST;
          
          stepsForZone = (zone == AST || zone == GST) ? (refSteps * 2 / 10) : refSteps;
          if (stepsForZone <= 0) stepsForZone = 200;
        }
      } else if (steps < 0) {
        while (stepsAccumulated < 0) {
          if (zone == AST) zone = leftTrack;
          else if (zone == leftTrack) zone = GST;
          else if (zone == GST) zone = rightTrack;
          else if (zone == rightTrack) zone = AST;
          
          stepsForZone = (zone == AST || zone == GST) ? (refSteps * 2 / 10) : refSteps;
          if (stepsForZone <= 0) stepsForZone = 200;
          
          stepsAccumulated += stepsForZone;
        }
      }
      progress = 0;
    }

    void setIsStopped(bool nIsStopped) {
      isStopped = nIsStopped;
    }
};