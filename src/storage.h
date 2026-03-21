#pragma once

// ─────────────────────────────────────────────
//  storage.h
//  FarmControl Irrigation Controller
//  NVS (non-volatile storage) persistence.
//  Config and zone names stored as key/value
//  pairs so new fields don't corrupt old data.
// ─────────────────────────────────────────────

#include <Arduino.h>
#include "config.h"

// Initialise NVS — call once in setup()
void storageInit();

// Save/load the full Config struct
void storageSaveConfig();
bool storageLoadConfig();    // returns true if config found

// Save/load zone names + schedules
void storageSaveZones(Zone zones[MAX_ZONES]);
bool storageLoadZones(Zone zones[MAX_ZONES]);

// Clear all NVS data (factory reset)
void storageClear();

// ── Legacy-compat wrappers used in the main .ino ──────────────
inline void storage_init()                  { storageInit(); }
inline bool storage_load_config()           { return storageLoadConfig(); }
inline void storage_save_config()           { storageSaveConfig(); }
inline void storage_load_zones(Zone z[])    { storageLoadZones(z); }
inline void storage_save_zones(Zone z[])    { storageSaveZones(z); }

inline void storage_load_wifi(char* ssid, char* pass) {
  strlcpy(ssid, cfg.wifi_ssid,     64);
  strlcpy(pass, cfg.wifi_password, 64);
}
inline void storage_save_wifi(const char* ssid, const char* pass) {
  strlcpy(cfg.wifi_ssid,     ssid, sizeof(cfg.wifi_ssid));
  strlcpy(cfg.wifi_password, pass, sizeof(cfg.wifi_password));
  storageSaveConfig();
}
inline void storage_load_mqtt(char* host, int* port, char* user, char* pass) {
  strlcpy(host, cfg.mqtt_host,     128);
  *port = cfg.mqtt_port;
  strlcpy(user, cfg.mqtt_user,     64);
  strlcpy(pass, cfg.mqtt_password, 64);
}
inline void storage_save_mqtt(const char* host, int port,
                               const char* user, const char* pass) {
  strlcpy(cfg.mqtt_host,     host, sizeof(cfg.mqtt_host));
  cfg.mqtt_port = port;
  strlcpy(cfg.mqtt_user,     user, sizeof(cfg.mqtt_user));
  strlcpy(cfg.mqtt_password, pass, sizeof(cfg.mqtt_password));
  storageSaveConfig();
}
inline void storage_load_supabase(char* url, char* key) {
  strlcpy(url, cfg.supabase_url, 128);
  strlcpy(key, cfg.supabase_key, 256);
}
inline void storage_save_supabase(const char* url, const char* key) {
  strlcpy(cfg.supabase_url, url, sizeof(cfg.supabase_url));
  strlcpy(cfg.supabase_key, key, sizeof(cfg.supabase_key));
  storageSaveConfig();
}
