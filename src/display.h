#pragma once

// ─────────────────────────────────────────────
//  display.h
//  FarmControl Irrigation Controller
//  SSD1306 0.96" OLED via I2C
//  SDA: GPIO8 / SCL: GPIO18 / Address: 0x3C
// ─────────────────────────────────────────────

#include <Arduino.h>
#include "config.h"

// Initialise display — call once in setup()
// Returns false if display not found on I2C bus
bool displayInit();

// Boot message — shown during startup
void displayBoot(const char* message);

// Hotspot mode — shown while setup portal is active
void displayHotspot();

// WiFi connecting
void displayWiFiConnecting(const char* ssid);

// WiFi connected — shows IP
void displayWiFiConnected(const char* ip);

// WiFi failed
void displayWiFiFailed();

// Main status screen — call from loop
void displayLoop(Zone zones[MAX_ZONES], float supplyPsi,
                 bool mqttConnected, bool wifiConnected);

// Legacy alias used in main .ino
inline void display_init() { displayInit(); }
inline void display_loop(Zone z[], float p, bool m, bool w) {
  displayLoop(z, p, m, w);
}
