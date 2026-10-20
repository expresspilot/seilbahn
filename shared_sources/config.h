

#include <stdint.h>

// ============================================================================
// WLAN-KONFIGURATION
// ============================================================================
const char *ssid = "UPC814C61F";
const char *password = "h5t4xwhyzdcP";

// ============================================================================
// ESP-NOW MAC-ADRESSEN (Kopplung von AST und GST)
// ============================================================================
// const uint8_t GST_MAC[6] = {0x14, 0xC1, 0x9F, 0x2B, 0x33, 0xEC};
const uint8_t GST_MAC[6] = {0xAC, 0x27, 0x6E, 0xA3, 0x4E, 0xB0};
// const uint8_t AST_MAC[6] = {0x44, 0x1B, 0xF6, 0xFF, 0x51, 0x60};
const uint8_t AST_MAC[6] = {0xAC, 0x27, 0x6E, 0xA3, 0x00, 0x84};
