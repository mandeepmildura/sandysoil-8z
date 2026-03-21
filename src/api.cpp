#include "api.h"
#include "config.h"
#include "zones.h"
#include "pressure.h"
#include "storage.h"
#include "wifi_setup.h"
#include "mqtt.h"
#include "ota_github.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>

// ─────────────────────────────────────────────
//  api.cpp
//  All responses use JSON with CORS headers so
//  the Netlify dashboard can call them cross-origin.
// ─────────────────────────────────────────────

static Zone* _zones = nullptr;

static void addCORS(AsyncWebServerResponse* res) {
  res->addHeader("Access-Control-Allow-Origin",  "*");
  res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  res->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJSON(AsyncWebServerRequest* req, const String& json, int code = 200) {
  AsyncWebServerResponse* res = req->beginResponse(code, "application/json", json);
  addCORS(res);
  req->send(res);
}

void apiInit(AsyncWebServer& server, Zone zones[MAX_ZONES]) {
  _zones = zones;

  // ── OPTIONS preflight ──
  server.onNotFound([](AsyncWebServerRequest* req) {
    if (req->method() == HTTP_OPTIONS) {
      AsyncWebServerResponse* res = req->beginResponse(204);
      res->addHeader("Access-Control-Allow-Origin",  "*");
      res->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      res->addHeader("Access-Control-Allow-Headers", "Content-Type");
      req->send(res);
    } else {
      req->send(404, "application/json", "{\"error\":\"not found\"}");
    }
  });

  // ── GET /api/status ──
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["board_name"]     = cfg.board_name;
    doc["ip"]             = wifiGetIP();
    doc["rssi"]           = WiFi.RSSI();
    doc["uptime_sec"]     = millis() / 1000;
    doc["firmware"]       = FW_VERSION;
    doc["mqtt_connected"] = mqttIsConnected();
    doc["wifi_ssid"]      = cfg.wifi_ssid;
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  // ── GET /api/pressure ──
  server.on("/api/pressure", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["supply_psi"] = pressureGetSupplyPsi();
    doc["simulated"]  = pressureIsSimulated();
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  // ── POST /api/pressure/simulate ──
  server.on("/api/pressure/simulate", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        sendJSON(req, "{\"error\":\"invalid JSON\"}", 400);
        return;
      }
      if (doc["clear"].is<bool>() && doc["clear"]) {
        pressureSimulateClear();
        Serial.println("[API] Pressure simulator OFF");
        sendJSON(req, "{\"ok\":true,\"simulated\":false}");
      } else if (doc["psi"].is<float>() || doc["psi"].is<int>()) {
        float psi = doc["psi"];
        pressureSimulate(psi);
        Serial.printf("[API] Pressure simulator ON: %.1f PSI\n", psi);
        sendJSON(req, "{\"ok\":true,\"simulated\":true}");
      } else {
        sendJSON(req, "{\"error\":\"provide psi or clear\"}", 400);
      }
    }
  );

  // ── GET /api/zones ──
  server.on("/api/zones", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < MAX_ZONES; i++) {
      JsonObject z = arr.add<JsonObject>();
      z["zone"]  = i + 1;
      z["name"]  = _zones[i].name;
      z["on"]    = zoneIsOn(i);
      z["enabled"] = _zones[i].enabled;
      ZoneState st = getZoneState(_zones[i]);
      z["state"] = (st == ZONE_MANUAL)   ? "manual" :
                   (st == ZONE_SCHEDULE) ? "schedule" : "off";
      if (_zones[i].manualOn && _zones[i].manualDurMin > 0) {
        uint32_t elapsed = (millis() - _zones[i].manualOnAt) / 1000;
        uint32_t total   = (uint32_t)_zones[i].manualDurMin * 60;
        z["remaining_sec"] = total > elapsed ? total - elapsed : 0;
      }
    }
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  // ── GET /api/config ──
  server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["wifi_ssid"]             = cfg.wifi_ssid;
    doc["board_name"]            = cfg.board_name;
    doc["static_ip"]             = cfg.static_ip;
    doc["mqtt_host"]             = cfg.mqtt_host;
    doc["mqtt_port"]             = cfg.mqtt_port;
    doc["mqtt_user"]             = cfg.mqtt_user;
    doc["mqtt_password"]         = cfg.mqtt_password;
    doc["mqtt_base_topic"]       = cfg.mqtt_base_topic;
    doc["pressure_max_psi"]      = cfg.pressure_max_psi;
    doc["pressure_zero_offset"]  = cfg.pressure_zero_offset;
    doc["low_pressure_psi"]      = cfg.low_pressure_psi;
    doc["supabase_url"]          = cfg.supabase_url;
    doc["supabase_key"]          = cfg.supabase_key;
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  // ── POST /api/config ──
  server.on("/api/config", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len)) {
        sendJSON(req, "{\"error\":\"invalid JSON\"}", 400);
        return;
      }
      // WiFi
      if (doc["wifi_ssid"].is<const char*>())
        strlcpy(cfg.wifi_ssid,          doc["wifi_ssid"],          sizeof(cfg.wifi_ssid));
      if (doc["wifi_password"].is<const char*>())
        strlcpy(cfg.wifi_password,      doc["wifi_password"],      sizeof(cfg.wifi_password));
      // Board
      if (doc["board_name"].is<const char*>())
        strlcpy(cfg.board_name,         doc["board_name"],         sizeof(cfg.board_name));
      // MQTT
      if (doc["mqtt_host"].is<const char*>())
        strlcpy(cfg.mqtt_host,          doc["mqtt_host"],          sizeof(cfg.mqtt_host));
      if (doc["mqtt_port"].is<int>())
        cfg.mqtt_port =                 doc["mqtt_port"];
      if (doc["mqtt_user"].is<const char*>())
        strlcpy(cfg.mqtt_user,          doc["mqtt_user"],          sizeof(cfg.mqtt_user));
      if (doc["mqtt_password"].is<const char*>())
        strlcpy(cfg.mqtt_password,      doc["mqtt_password"],      sizeof(cfg.mqtt_password));
      if (doc["mqtt_base_topic"].is<const char*>())
        strlcpy(cfg.mqtt_base_topic,    doc["mqtt_base_topic"],    sizeof(cfg.mqtt_base_topic));
      // Pressure
      if (doc["pressure_max_psi"].is<float>())
        cfg.pressure_max_psi =          doc["pressure_max_psi"];
      if (doc["pressure_zero_offset"].is<float>())
        cfg.pressure_zero_offset =      doc["pressure_zero_offset"];
      if (doc["low_pressure_psi"].is<float>())
        cfg.low_pressure_psi =          doc["low_pressure_psi"];
      // Supabase
      if (doc["supabase_url"].is<const char*>())
        strlcpy(cfg.supabase_url,       doc["supabase_url"],       sizeof(cfg.supabase_url));
      if (doc["supabase_key"].is<const char*>())
        strlcpy(cfg.supabase_key,       doc["supabase_key"],       sizeof(cfg.supabase_key));
      storageSaveConfig();
      Serial.println("[API] Config updated");
      sendJSON(req, "{\"ok\":true}");
    }
  );

  // ── POST /api/zone/{n}/on ──
  server.on("^\\/api\\/zone\\/([0-9]+)\\/on$", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      int n = req->pathArg(0).toInt();
      if (n < 1 || n > MAX_ZONES) { sendJSON(req, "{\"error\":\"invalid zone\"}", 400); return; }
      int dur = DEFAULT_RUN_MIN;
      if (len > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, data, len) && doc["duration"].is<int>())
          dur = doc["duration"];
      }
      zoneOnManual(_zones, n - 1, dur);
      mqttPublishZone(n - 1, _zones[n - 1]);
      sendJSON(req, "{\"ok\":true}");
    }
  );

  // ── POST /api/zone/{n}/off ──
  server.on("^\\/api\\/zone\\/([0-9]+)\\/off$", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      int n = req->pathArg(0).toInt();
      if (n < 1 || n > MAX_ZONES) { sendJSON(req, "{\"error\":\"invalid zone\"}", 400); return; }
      zoneOff(_zones, n - 1);
      mqttPublishZone(n - 1, _zones[n - 1]);
      sendJSON(req, "{\"ok\":true}");
    }
  );

  // ── POST /api/zones/off ──
  server.on("/api/zones/off", HTTP_POST, [](AsyncWebServerRequest* req) {
    zoneOffAll(_zones);
    sendJSON(req, "{\"ok\":true}");
  });

  // ── POST /api/restart ──
  server.on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
    sendJSON(req, "{\"ok\":true}");
    delay(500);
    ESP.restart();
  });

  // ── POST /api/update — OTA firmware upload ──
  server.on("/api/update", HTTP_POST,
    // Response handler (called after upload completes)
    [](AsyncWebServerRequest* req) {
      bool ok = !Update.hasError();
      AsyncWebServerResponse* res = req->beginResponse(
          ok ? 200 : 500, "text/plain",
          ok ? "OK — rebooting" : "Update failed");
      addCORS(res);
      res->addHeader("Connection", "close");
      req->send(res);
      if (ok) {
        delay(500);
        ESP.restart();
      }
    },
    // Upload handler — streams firmware into OTA partition
    [](AsyncWebServerRequest* req, const String& filename, size_t index,
       uint8_t* data, size_t len, bool final) {
      if (index == 0) {
        Serial.printf("[OTA] Upload start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
          Update.printError(Serial);
        }
      }
      if (len > 0) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("[OTA] Upload complete: %u bytes\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    }
  );

  // ── GET /api/ota/check — check GitHub for update ──
  server.on("/api/ota/check", HTTP_GET, [](AsyncWebServerRequest* req) {
    String newVer, dlUrl;
    bool available = otaCheckForUpdate(newVer, dlUrl);
    JsonDocument doc;
    doc["update_available"] = available;
    doc["current_version"]  = FW_VERSION;
    doc["latest_version"]   = newVer;
    if (available) doc["download_url"] = dlUrl;
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  // ── POST /api/ota/update — start GitHub OTA update ──
  server.on("/api/ota/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {},
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      JsonDocument doc;
      if (deserializeJson(doc, data, len) || !doc["url"].is<const char*>()) {
        sendJSON(req, "{\"error\":\"provide url\"}", 400);
        return;
      }
      String url = doc["url"].as<String>();
      otaSetPending(url);
      sendJSON(req, "{\"ok\":true,\"status\":\"pending\"}");
    }
  );

  // ── GET /api/ota/status — poll OTA progress ──
  server.on("/api/ota/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    JsonDocument doc;
    doc["status"]   = otaGetStatus();
    doc["progress"] = otaGetProgress();
    String json;
    serializeJson(doc, json);
    sendJSON(req, json);
  });

  Serial.println("[API] HTTP API ready");
}
