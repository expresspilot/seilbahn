/**
 * ============================================================================
 * 3S-Seilbahn Modellsteuerung - Streckenabschnitt / Zonen-Definition
 * ============================================================================
 * @file      Zone.h
 * @brief     Definiert die vier physischen Streckenabschnitte der Seilbahnanlage:
 *            - AST: Antriebsstation (Wende / Ein-/Auskupplung)
 *            - GST: Gegenstation (Wende / Ein-/Auskupplung)
 *            - rightTrack: Rechter Streckenabschnitt (Fahrbahn 1)
 *            - leftTrack: Linker Streckenabschnitt (Fahrbahn 2)
 * ============================================================================
 */

#pragma once

enum Zone {
  AST,        ///< Antriebsstation
  GST,        ///< Gegenstation
  leftTrack,  ///< Linke Fahrstrecke
  rightTrack  ///< Rechte Fahrstrecke
};