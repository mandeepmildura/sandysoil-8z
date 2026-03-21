#include "supabase.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────
//  supabase.cpp
//  Posts JSON to Supabase REST API (PostgREST).
//  Endpoint: cfg.supabase_url/rest/v1/device_telemetry
//
//  Skips silently if supabase_url is not configured.
// ─────────────────────────────────────────────

void supabaseInit() {
  if (strlen(cfg.supabase_url) == 0) {
    Serial.println("[Supabase] No URL configured — logging disabled");
    return;
  }
  Serial.printf("[Supabase] Logging to %s\n", cfg.supabase_url);
}

void supabaseLog(float supplyPsi, Zone zones[MAX_ZONES]) {
  if (strlen(cfg.supabase_url) == 0 || strlen(cfg.supabase_key) == 0) return;

  JsonDocument doc;
  doc["device_id"]   = DEVICE_ID;
  doc["supply_psi"]  = supplyPsi;

  JsonArray zonesArr = doc["zones"].to<JsonArray>();
  for (int i = 0; i < MAX_ZONES; i++) {
    JsonObject z = zonesArr.add<JsonObject>();
    z["id"]  = i + 1;
    z["on"]  = zones[i].manualOn || zones[i].scheduleActive;
  }

  String payload;
  serializeJson(doc, payload);

  char url[256];
  snprintf(url, sizeof(url), "%s/rest/v1/device_telemetry", cfg.supabase_url);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", cfg.supabase_key);
  http.addHeader("Authorization", String("Bearer ") + cfg.supabase_key);

  int code = http.POST(payload);
  if (code > 0) {
    Serial.printf("[Supabase] POST %d\n", code);
  } else {
    Serial.printf("[Supabase] Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}
