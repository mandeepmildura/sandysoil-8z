#pragma once

// ── Web UI + OTA ──────────────────────────────────────────────
// Provides:
//   • HTTP config page  → /config  (WiFi + MQTT credentials)
//   • HTTP OTA page     → /update  (upload .bin firmware)
//   • ArduinoOTA        → VS Code / PlatformIO over-the-air upload
//
// AP fallback: if WiFi fails, call webui_start_ap() before webui_init().
// The device creates a hotspot "SandySoil-Setup" so you can connect and
// browse to http://192.168.4.1 to enter credentials.

void webui_start_ap();   // start AP hotspot (call when WiFi fails)
void webui_init();       // start web server + ArduinoOTA  (call in setup)
void webui_loop();       // must be called every loop() iteration
bool webui_in_ap_mode(); // true when running as AP (no station WiFi)
