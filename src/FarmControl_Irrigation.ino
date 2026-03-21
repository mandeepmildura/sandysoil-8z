/*
 * Sandy Soil 8Z — KC868-A8v3 ESP32-S3
 * 8-zone irrigation controller with schedule + manual control
 * MQTT via HiveMQ Cloud, Supabase cloud logging
 * Web dashboard + OTA firmware updates
 *
 * Sandy Soil Automations — Mildura
 *
 * First boot: connect to WiFi hotspot "FarmControl-Irrigation-Setup"
 *             then open http://192.168.4.1 to configure WiFi & MQTT.
 *
 * After setup: open http://<board-ip> for dashboard, /update for OTA.
 */

#include "config.h"
#include "storage.h"
#include "wifi_setup.h"
#include "zones.h"
#include "pressure.h"
#include "display.h"
#include "supabase.h"
#include "mqtt.h"
#include "api.h"
#include "webui.h"
#include "ota_github.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

// ── GLOBALS ──────────────────────────────────────────────────
Zone             zones[MAX_ZONES];
float            supplyPsi    = 0.0f;
AsyncWebServer   server(80);

// ── NTP ──────────────────────────────────────────────────────
WiFiUDP   ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 36000);  // UTC+10 AEST
static bool    ntpSynced          = false;
static int     lastScheduleMinute = -1;

// ── TIMERS ───────────────────────────────────────────────────
static uint32_t lastPressure  = 0;
static uint32_t lastStatus    = 0;
static uint32_t lastSupabase  = 0;
static uint32_t lastSchedule  = 0;

// ── ArduinoOTA password (used by VS Code / PlatformIO OTA) ──
#define OTA_PASSWORD   "irrigation8z"

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n=== Sandy Soil 8Z v" FW_VERSION " ===");

  // 1. Storage
  storageInit();

  // 2. Load or initialise config
  if (!storageLoadConfig()) {
    Serial.println("[Setup] No config — starting setup portal");
    zonesInit(zones);
    displayInit();
    displayHotspot();
    wifiStartSetupPortal();  // blocks until user submits, then reboots
  }

  // 3. Zones + relays (starts Wire/I2C)
  storageLoadZones(zones);
  zonesInit(zones);

  // I2C bus scan — find all devices
  Serial.println("[I2C] Scanning bus (SDA=" + String(I2C_SDA) + " SCL=" + String(I2C_SCL) + ")...");
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("[I2C] Device found at 0x%02X\n", addr);
    }
  }
  Serial.println("[I2C] Scan complete");

  // 4. Display (Wire already up)
  displayInit();
  displayBoot("Starting...");

  // 5. Pressure sensor
  pressureInit();

  // 6. Connect WiFi
  displayWiFiConnecting(cfg.wifi_ssid);
  bool wifiOk = wifiConnect();
  if (wifiOk) {
    displayWiFiConnected(wifiGetIP().c_str());
    delay(800);
  } else {
    displayWiFiFailed();
    delay(2000);
  }

  // 7. NTP (only if WiFi connected)
  if (wifiOk) {
    timeClient.begin();
    timeClient.update();
    ntpSynced = true;
    Serial.printf("[NTP] %s\n", timeClient.getFormattedTime().c_str());
  }

  // 8. HTTP server — API + Web UI + OTA page
  displayBoot("HTTP...");
  apiInit(server, zones);
  webuiInit(server);
  server.begin();
  Serial.printf("[HTTP] Dashboard at http://%s\n", wifiGetIP().c_str());
  Serial.printf("[HTTP] OTA update at http://%s/update\n", wifiGetIP().c_str());

  // 9. ArduinoOTA — VS Code / PlatformIO over-the-air upload
  ArduinoOTA.setHostname("sandysoil-8z");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] Start — do not power off");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] Done");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", progress * 100 / total);
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("[OTA] Error[%u]\n", e);
  });
  ArduinoOTA.begin();
  Serial.println("[OTA] ArduinoOTA ready (password: " OTA_PASSWORD ")");

  // 10. MQTT
  displayBoot("MQTT...");
  mqttInit(zones);

  // 11. Supabase
  supabaseInit();

  Serial.println("[Setup] Ready\n");
  displayBoot("Ready");
  delay(800);
}

// ─────────────────────────────────────────────────────────────
void loop() {
  uint32_t now = millis();

  wifiMaintain();
  ArduinoOTA.handle();
  otaRunPending();
  mqttLoop(zones);
  zonesLoop(zones);

  // Publish MQTT state for zones that auto-turned off (timer expiry)
  uint8_t dirty = zonesGetDirty();
  if (dirty) {
    zonesClearDirty();
    for (int i = 0; i < MAX_ZONES; i++) {
      if (dirty & (1 << i)) mqttPublishZone(i, zones[i]);
    }
  }

  // ── PRESSURE ─────────────────────────────────────────────
  if (now - lastPressure >= PRESSURE_INTERVAL_MS) {
    lastPressure = now;
    supplyPsi    = pressureGetSupplyPsi();

    bool anyOn = false;
    for (int i = 0; i < MAX_ZONES; i++) if (zoneIsOn(i)) { anyOn = true; break; }
    if (anyOn && pressureIsLow(supplyPsi, cfg.low_pressure_psi)) {
      char msg[64];
      snprintf(msg, sizeof(msg), "Low supply pressure: %.1f PSI", supplyPsi);
      mqttPublishAlert(msg);
    }
  }

  // ── STATUS PUBLISH ───────────────────────────────────────
  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    mqttPublishStatus(zones, supplyPsi);
  }

  // ── SUPABASE LOG ─────────────────────────────────────────
  if (now - lastSupabase >= SUPABASE_INTERVAL_MS) {
    lastSupabase = now;
    if (wifiIsConnected()) supabaseLog(supplyPsi, zones);
  }

  // ── SCHEDULE CHECK ───────────────────────────────────────
  if (now - lastSchedule >= SCHEDULE_CHECK_MS) {
    lastSchedule = now;
    if (ntpSynced && wifiIsConnected()) {
      timeClient.update();
      int h   = timeClient.getHours();
      int m   = timeClient.getMinutes();
      int dow = timeClient.getDay();
      if (m != lastScheduleMinute) {
        lastScheduleMinute = m;
        scheduleCheck(zones, dow, h, m);
      }
    }
  }

  // ── DISPLAY ──────────────────────────────────────────────
  displayLoop(zones, supplyPsi, mqttIsConnected(), wifiIsConnected());

  delay(10);
}
