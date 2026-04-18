#include "supabase.h"
#include "config.h"
#include "storage.h"
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

// Device UUID in Supabase devices table
#define SUPABASE_DEVICE_UUID "6e276ee9-224d-4529-b96b-b165687f6e94"

void supabaseLog(float supplyPsi, Zone zones[MAX_ZONES]) {
  if (strlen(cfg.supabase_url) == 0 || strlen(cfg.supabase_key) == 0) return;

  // Build payload matching the MQTT bridge format
  JsonDocument inner;
  inner["device"]     = DEVICE_ID;
  inner["fw"]         = FW_VERSION;
  inner["online"]     = true;
  inner["supply_psi"] = supplyPsi;
  inner["uptime"]     = millis() / 1000;
  JsonArray zonesArr  = inner["zones"].to<JsonArray>();
  for (int i = 0; i < MAX_ZONES; i++) {
    JsonObject z = zonesArr.add<JsonObject>();
    z["id"]    = i + 1;
    z["name"]  = zones[i].name;
    z["on"]    = zones[i].manualOn || zones[i].scheduleActive;
    ZoneState st = getZoneState(zones[i]);
    z["state"] = (st == ZONE_MANUAL) ? "manual" :
                 (st == ZONE_SCHEDULE) ? "schedule" : "off";
  }

  // Wrap in the device_telemetry row format
  JsonDocument doc;
  doc["device_id"] = SUPABASE_DEVICE_UUID;
  doc["topic"]     = cfg.mqtt_base_topic + String("/status");
  doc["payload"]   = inner;

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

// ── Schedule sync from Supabase ─────────────────────────────
// Reads the zone_schedules_effective view (unions zone_schedules +
// simultaneous group_schedules) and rewrites Zone.schedules[] on this
// device. Source of truth lives in Supabase; firmware is a cache.
void supabaseSyncSchedules(Zone zones[MAX_ZONES]) {
  if (!strlen(cfg.supabase_url) || !strlen(cfg.supabase_key)) return;

  String url = String(cfg.supabase_url)
             + "/rest/v1/zone_schedules_effective"
             + "?select=zone_num,days_of_week,start_time,duration_min,enabled"
             + "&enabled=eq.true"
             + "&device=eq." DEVICE_ID
             + "&order=zone_num.asc,start_time.asc";

  HTTPClient http;
  http.begin(url);
  http.addHeader("apikey",        cfg.supabase_key);
  http.addHeader("Authorization", String("Bearer ") + cfg.supabase_key);
  http.addHeader("Accept",        "application/json");

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[Supabase] schedule sync HTTP %d\n", code);
    http.end();
    return;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;                   // ArduinoJson 7 — heap-backed
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[Supabase] schedule sync JSON error: %s\n", err.c_str());
    return;
  }

  // Wipe local schedules — Supabase is the source of truth
  for (int z = 0; z < MAX_ZONES; z++)
    for (int s = 0; s < MAX_SCHEDULES; s++)
      zones[z].schedules[s] = Schedule();

  uint8_t slotIdx[MAX_ZONES] = {0};
  int imported = 0, skipped = 0;

  for (JsonObject row : doc.as<JsonArray>()) {
    int zn = row["zone_num"] | 0;
    if (zn < 1 || zn > MAX_ZONES)     { skipped++; continue; }
    int zi = zn - 1;
    if (slotIdx[zi] >= MAX_SCHEDULES) { skipped++; continue; }

    // days_of_week comes as a JSON array of ints (0=Sun..6=Sat); pack to bitmask
    uint8_t dowMask = 0;
    for (int d : row["days_of_week"].as<JsonArray>()) {
      if (d >= 0 && d <= 6) dowMask |= (uint8_t)(1 << d);
    }
    if (dowMask == 0) { skipped++; continue; }

    const char* t = row["start_time"] | "06:00:00";
    Schedule& sch = zones[zi].schedules[slotIdx[zi]++];
    sch.enabled     = true;
    sch.daysOfWeek  = dowMask;
    sch.durationMin = row["duration_min"] | 10;
    sch.hour        = atoi(t);
    sch.minute      = (t[3] && t[4]) ? atoi(t + 3) : 0;
    imported++;
  }

  Serial.printf("[Supabase] schedules: %d imported, %d skipped\n",
                imported, skipped);
  storageSaveZones(zones);
}
