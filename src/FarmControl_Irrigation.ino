/*
 * Sandy Soil 8Z — KC868-A8v3 ESP32-S3
 * 8-zone irrigation controller with schedule + manual control
 * MQTT via HiveMQ Cloud, Supabase cloud logging
 * Sandy Soil Automations — Mildura
 */

#include "config.h"
#include "secrets.h"
#include "storage.h"
#include "wifi_setup.h"
#include "zones.h"
#include "pressure.h"
#include "mqtt.h"
#include "display.h"
#include "supabase.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

// ── GLOBALS ───────────────────────────────────────────────────
Zone     zones[MAX_ZONES];
float    supplyPsi      = 0.0f;

// ── NTP for scheduling ────────────────────────────────────────
WiFiUDP        ntpUDP;
NTPClient      timeClient(ntpUDP, "pool.ntp.org", 36000); // UTC+10 AEST
static bool    ntpSynced = false;
static int     lastScheduleMinute = -1;

// ── TIMERS ────────────────────────────────────────────────────
static uint32_t lastPressure  = 0;
static uint32_t lastStatus    = 0;
static uint32_t lastSupabase  = 0;
static uint32_t lastSchedule  = 0;

#define LOW_PRESSURE_PSI    5.0f

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== Sandy Soil 8Z v" FW_VERSION " ===");

  // Storage
  storage_init();

  // First-run: save default credentials to NVS
  char ssid[64], pass[64];
  storage_load_wifi(ssid, pass);
  if (strlen(ssid) == 0) {
    Serial.println("[Setup] First run — saving default credentials");
    storage_save_wifi(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
    storage_save_mqtt(DEFAULT_MQTT_HOST, DEFAULT_MQTT_PORT,
                      DEFAULT_MQTT_USER, DEFAULT_MQTT_PASS);
    storage_save_supabase(DEFAULT_SB_URL, DEFAULT_SB_KEY);
    storage_load_wifi(ssid, pass);
  }

  // Zones + relays FIRST (starts Wire/I2C)
  storage_load_zones(zones);
  zones_init(zones);

  // Display AFTER zones_init (Wire already started)
  display_init();

  // Pressure
  pressure_init();

  // WiFi
  wifi_init(ssid, pass);

  // NTP
  if (wifi_connected()) {
    timeClient.begin();
    timeClient.update();
    ntpSynced = true;
    Serial.printf("[NTP] Time: %s\n", timeClient.getFormattedTime().c_str());
  }

  // MQTT
  mqtt_init(zones);

  // Supabase
  supabase_init();

  Serial.println("[Setup] Ready\n");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  wifi_loop();
  mqtt_loop(zones);
  zones_loop(zones);

  // Publish MQTT state for any zones that auto-turned off (timer expiry)
  uint8_t dirty = zones_get_dirty();
  if (dirty) {
    zones_clear_dirty();
    for (int i = 0; i < MAX_ZONES; i++) {
      if (dirty & (1 << i)) mqtt_publish_zone(i, zones[i]);
    }
  }

  // ── PRESSURE ──────────────────────────────────────────────
  if (now - lastPressure >= PRESSURE_INTERVAL_MS) {
    lastPressure = now;
    supplyPsi = pressure_get_supply_psi();
    Serial.printf("[Pressure] Supply: %.1f PSI\n", supplyPsi);

    bool anyOn = false;
    for (int i = 0; i < MAX_ZONES; i++) if (zone_is_on(i)) { anyOn = true; break; }
    if (anyOn && pressure_is_low(supplyPsi, LOW_PRESSURE_PSI)) {
      char msg[64];
      snprintf(msg, sizeof(msg), "Low supply pressure: %.1f PSI", supplyPsi);
      mqtt_publish_alert(msg);
    }
  }

  // ── STATUS PUBLISH ────────────────────────────────────────
  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    mqtt_publish_status(zones, supplyPsi);
  }

  // ── SUPABASE LOG ──────────────────────────────────────────
  if (now - lastSupabase >= SUPABASE_INTERVAL_MS) {
    lastSupabase = now;
    if (wifi_connected()) supabase_log(supplyPsi, zones);
  }

  // ── SCHEDULE CHECK ────────────────────────────────────────
  if (now - lastSchedule >= SCHEDULE_CHECK_MS) {
    lastSchedule = now;
    if (ntpSynced && wifi_connected()) {
      timeClient.update();
      int h   = timeClient.getHours();
      int m   = timeClient.getMinutes();
      int dow = timeClient.getDay();

      if (m != lastScheduleMinute) {
        lastScheduleMinute = m;
        schedule_check(zones, dow, h, m);
      }
    }
  }

  // ── DISPLAY ───────────────────────────────────────────────
  display_loop(zones, supplyPsi, mqtt_connected(), wifi_connected());
}
