#pragma once

// ─────────────────────────────────────────────
//  wifi_setup.h
//  FarmControl Irrigation Controller
//
//  Two scenarios:
//  1. No config saved → start hotspot and serve
//     a setup page so the user can enter WiFi
//     credentials and MQTT settings.
//  2. Normal boot → connect to saved network.
//
//  Hotspot SSID: FarmControl-Irrigation-Setup
//  Setup page:   http://192.168.4.1
// ─────────────────────────────────────────────

#include <Arduino.h>

// Start WiFi hotspot setup portal.
// Blocks until user submits the setup form,
// saves config to NVS, then reboots.
void wifiStartSetupPortal();

// Connect to WiFi using saved config.
// Returns true if connected within 20s.
bool wifiConnect();

// Returns current local IP as string
String wifiGetIP();

// Returns true if currently connected
bool wifiIsConnected();

// Call from main loop — handles reconnection
void wifiMaintain();

// Legacy alias used in main .ino
inline void wifi_init(const char*, const char*) { wifiConnect(); }
inline void wifi_loop()                          { wifiMaintain(); }
inline bool wifi_connected()                     { return wifiIsConnected(); }
