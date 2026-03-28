#pragma once

// ─────────────────────────────────────────────
//  config.h
//  FarmControl Irrigation Controller
//  KC868-A8v3  ESP32-S3  16MB Flash
//
//  All settings are persisted in NVS and
//  configurable via the built-in web UI.
// ─────────────────────────────────────────────

#include <Arduino.h>

// ── PIN DEFINITIONS (KC868-A8v3) ──────────────
// I2C bus — shared by PCF8575 and OLED
#define I2C_SDA             8
#define I2C_SCL             18

// PCF8575 I2C relay expander
#define PCF8575_ADDR        0x22   // A0=0, A1=1, A2=0 on this KC868-A8v3

// Supply pressure sensor — 0–5V ratiometric, GPIO1 = ADC1_CH0
#define PIN_PRESSURE        1

// ── OLED ──────────────────────────────────────
#define OLED_ADDRESS        0x3C
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

// ── FIRMWARE ──────────────────────────────────
#define FW_VERSION          "2.3.3"

// ── GITHUB OTA ────────────────────────────────
#define GITHUB_OWNER        "mandeepmildura"
#define GITHUB_REPO         "sandysoil-8z"
#define GITHUB_FW_ASSET     "firmware.bin"
#define DEVICE_ID           "irrigation1"
#define DEVICE_NAME         "Irrigation Controller"

// ── MQTT TOPICS ───────────────────────────────
#define MQTT_BASE           "farm/irrigation1"
#define MQTT_STATUS         MQTT_BASE "/status"
#define MQTT_ALERT          MQTT_BASE "/alert"
#define MQTT_ZONE_STATE     MQTT_BASE "/zone/%d/state"
#define MQTT_ZONE_CMD       MQTT_BASE "/zone/%d/cmd"
#define MQTT_DISCOVERY      "homeassistant"

// ── HOTSPOT ───────────────────────────────────
#define HOTSPOT_SSID        "FarmControl-Irrigation-Setup"
#define HOTSPOT_IP          "192.168.4.1"

// ── NVS ───────────────────────────────────────
#define NVS_NAMESPACE       "fc_irrig"

// ── ZONES ─────────────────────────────────────
#define MAX_ZONES           8
#define MAX_SCHEDULES       4
#define MAX_RUN_MINUTES     120
#define DEFAULT_RUN_MIN     10

// ── TIMERS ────────────────────────────────────
#define PRESSURE_INTERVAL_MS    5000
#define STATUS_INTERVAL_MS      30000
#define SUPABASE_INTERVAL_MS    60000
#define SCHEDULE_CHECK_MS       10000

// ── PRESSURE SENSOR ───────────────────────────
#define ADC_MAX             4095.0
#define ADC_VREF            3.3
#define SENSOR_V_MIN        0.5
#define SENSOR_V_MAX        4.5
#define PRESSURE_SAMPLES    5

// ── DEFAULTS (used on first boot if no NVS) ───
#define DEFAULT_WIFI_SSID   ""
#define DEFAULT_WIFI_PASS   ""
#define DEFAULT_MQTT_HOST   ""
#define DEFAULT_MQTT_PORT   8883
#define DEFAULT_MQTT_USER   ""
#define DEFAULT_MQTT_PASS   ""
#define DEFAULT_SB_URL      ""
#define DEFAULT_SB_KEY      ""

// ── SCHEDULE STRUCT ───────────────────────────
struct Schedule {
  bool     enabled      = false;
  uint8_t  daysOfWeek   = 0;       // bitmask: bit0=Sun … bit6=Sat
  uint8_t  hour         = 6;
  uint8_t  minute       = 0;
  uint16_t durationMin  = 10;
};

// ── ZONE STATE ────────────────────────────────
enum ZoneState { ZONE_OFF, ZONE_MANUAL, ZONE_SCHEDULE, ZONE_PROGRAM };

// ── ZONE STRUCT ───────────────────────────────
struct Zone {
  char     name[32]           = "Zone";
  bool     enabled            = true;
  bool     manualOn           = false;
  uint32_t manualOnAt         = 0;
  uint16_t manualDurMin       = 0;
  bool     scheduleActive     = false;
  uint32_t scheduleOnAt       = 0;
  uint16_t scheduleDurMin     = 0;
  bool     programOn          = false;
  uint32_t programOnAt        = 0;
  uint16_t programDurMin      = 0;
  Schedule schedules[MAX_SCHEDULES];
};

inline ZoneState getZoneState(const Zone& z) {
  if (z.manualOn)       return ZONE_MANUAL;
  if (z.programOn)      return ZONE_PROGRAM;
  if (z.scheduleActive) return ZONE_SCHEDULE;
  return ZONE_OFF;
}

// ── CONFIG STRUCT ─────────────────────────────
struct Config {
  // WiFi
  char  wifi_ssid[64]          = "";
  char  wifi_password[64]      = "";
  char  static_ip[16]          = "";   // blank = DHCP
  char  gateway[16]            = "";
  char  subnet[16]             = "255.255.255.0";

  // MQTT
  char  mqtt_host[128]         = "";
  int   mqtt_port              = 8883;
  char  mqtt_user[64]          = "";
  char  mqtt_password[64]      = "";
  char  mqtt_base_topic[64]    = "farm/irrigation1";

  // Board
  char  board_name[64]         = "Irrigation Controller";

  // Pressure sensor
  float pressure_max_psi       = 100.0;
  float pressure_zero_offset   = 0.0;
  float low_pressure_psi       = 5.0;

  // Supabase
  char  supabase_url[128]      = "";
  char  supabase_key[256]      = "";
};

// Global config instance — accessible from all modules
extern Config cfg;

// Returns true if WiFi credentials have been saved
bool configIsValid();
