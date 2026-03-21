#include "storage.h"
#include "config.h"
#include <Preferences.h>

// ─────────────────────────────────────────────
//  storage.cpp
//  Uses ESP32 Preferences (NVS wrapper).
//  Each field is stored as a separate key so
//  adding new config fields doesn't corrupt
//  existing saved data.
// ─────────────────────────────────────────────

Config cfg;   // global config instance

static Preferences prefs;

bool configIsValid() {
  return strlen(cfg.wifi_ssid) > 0;
}

void storageInit() {
  Serial.println("[Storage] NVS ready");
}

void storageSaveConfig() {
  prefs.begin(NVS_NAMESPACE, false);

  // WiFi
  prefs.putString("wifi_ssid",   cfg.wifi_ssid);
  prefs.putString("wifi_pass",   cfg.wifi_password);
  prefs.putString("static_ip",   cfg.static_ip);
  prefs.putString("gateway",     cfg.gateway);
  prefs.putString("subnet",      cfg.subnet);

  // MQTT
  prefs.putString("mqtt_host",   cfg.mqtt_host);
  prefs.putInt   ("mqtt_port",   cfg.mqtt_port);
  prefs.putString("mqtt_user",   cfg.mqtt_user);
  prefs.putString("mqtt_pass",   cfg.mqtt_password);
  prefs.putString("mqtt_topic",  cfg.mqtt_base_topic);

  // Board
  prefs.putString("board_name",  cfg.board_name);

  // Pressure
  prefs.putFloat ("pres_max",    cfg.pressure_max_psi);
  prefs.putFloat ("pres_offset", cfg.pressure_zero_offset);
  prefs.putFloat ("pres_low",    cfg.low_pressure_psi);

  // Supabase
  prefs.putString("sb_url",      cfg.supabase_url);
  prefs.putString("sb_key",      cfg.supabase_key);

  prefs.end();
  Serial.println("[Storage] Config saved");
}

bool storageLoadConfig() {
  prefs.begin(NVS_NAMESPACE, true);

  if (!prefs.isKey("wifi_ssid")) {
    prefs.end();
    Serial.println("[Storage] No config in NVS");
    return false;
  }

  strlcpy(cfg.wifi_ssid,       prefs.getString("wifi_ssid",  "").c_str(),               sizeof(cfg.wifi_ssid));
  strlcpy(cfg.wifi_password,   prefs.getString("wifi_pass",  "").c_str(),               sizeof(cfg.wifi_password));
  strlcpy(cfg.static_ip,       prefs.getString("static_ip",  "").c_str(),               sizeof(cfg.static_ip));
  strlcpy(cfg.gateway,         prefs.getString("gateway",    "").c_str(),               sizeof(cfg.gateway));
  strlcpy(cfg.subnet,          prefs.getString("subnet",     "255.255.255.0").c_str(),  sizeof(cfg.subnet));

  strlcpy(cfg.mqtt_host,       prefs.getString("mqtt_host",  "").c_str(),               sizeof(cfg.mqtt_host));
  cfg.mqtt_port              = prefs.getInt   ("mqtt_port",  8883);
  strlcpy(cfg.mqtt_user,       prefs.getString("mqtt_user",  "").c_str(),               sizeof(cfg.mqtt_user));
  strlcpy(cfg.mqtt_password,   prefs.getString("mqtt_pass",  "").c_str(),               sizeof(cfg.mqtt_password));
  strlcpy(cfg.mqtt_base_topic, prefs.getString("mqtt_topic", "farm/irrigation1").c_str(), sizeof(cfg.mqtt_base_topic));

  strlcpy(cfg.board_name,      prefs.getString("board_name", "Irrigation Controller").c_str(), sizeof(cfg.board_name));

  cfg.pressure_max_psi       = prefs.getFloat ("pres_max",    100.0);
  cfg.pressure_zero_offset   = prefs.getFloat ("pres_offset", 0.0);
  cfg.low_pressure_psi       = prefs.getFloat ("pres_low",    5.0);

  strlcpy(cfg.supabase_url,    prefs.getString("sb_url", "").c_str(), sizeof(cfg.supabase_url));
  strlcpy(cfg.supabase_key,    prefs.getString("sb_key", "").c_str(), sizeof(cfg.supabase_key));

  prefs.end();
  Serial.println("[Storage] Config loaded");
  return true;
}

void storageSaveZones(Zone zones[MAX_ZONES]) {
  prefs.begin(NVS_NAMESPACE, false);
  for (int i = 0; i < MAX_ZONES; i++) {
    char key[16];
    snprintf(key, sizeof(key), "zname%d", i);
    prefs.putString(key, zones[i].name);
    snprintf(key, sizeof(key), "zen%d", i);
    prefs.putBool(key, zones[i].enabled);
  }
  prefs.end();
  Serial.println("[Storage] Zones saved");
}

bool storageLoadZones(Zone zones[MAX_ZONES]) {
  prefs.begin(NVS_NAMESPACE, true);
  bool found = false;
  for (int i = 0; i < MAX_ZONES; i++) {
    char key[16];
    snprintf(key, sizeof(key), "zname%d", i);
    if (prefs.isKey(key)) {
      strlcpy(zones[i].name, prefs.getString(key, zones[i].name).c_str(), sizeof(zones[i].name));
      found = true;
    } else {
      snprintf(zones[i].name, sizeof(zones[i].name), "Zone %d", i + 1);
    }
    snprintf(key, sizeof(key), "zen%d", i);
    zones[i].enabled = prefs.getBool(key, true);
  }
  prefs.end();
  return found;
}

void storageClear() {
  prefs.begin(NVS_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  Serial.println("[Storage] NVS cleared — factory reset");
}
