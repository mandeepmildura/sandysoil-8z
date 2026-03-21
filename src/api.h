#pragma once

// ─────────────────────────────────────────────
//  api.h
//  FarmControl Irrigation Controller
//  HTTP REST API — ESPAsyncWebServer
//
//  Endpoints:
//    GET  /api/status        — board info, WiFi, MQTT
//    GET  /api/zones         — all zone states
//    GET  /api/pressure      — supply pressure
//    GET  /api/config        — current config
//    POST /api/config        — update config (JSON body)
//    POST /api/zone/{n}/on   — turn zone on  {"duration":10}
//    POST /api/zone/{n}/off  — turn zone off
//    POST /api/zones/off     — all zones off
//    POST /api/restart       — reboot board
// ─────────────────────────────────────────────

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "config.h"

// Initialise all API routes — call once in setup()
// zones pointer must remain valid for the lifetime of the server
void apiInit(AsyncWebServer& server, Zone zones[MAX_ZONES]);
