#include "mqtt.h"
#include "zones.h"
#include "storage.h"
#include "ota_github.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure _wifiClient;
static PubSubClient     _mqtt(_wifiClient);
static Zone*            _zones = nullptr;

static uint32_t _lastReconnect       = 0;
static bool     _discoveryPublished  = false;

// ── Topic helpers — build at runtime from cfg.mqtt_base_topic ───
// Returns the chip's unique 12-hex MAC as a lowercase string. Used
// in the published status payload as `device_id` so the dashboard
// can auto-create an "unclaimed device" record.
static String chipSerial() {
  char buf[13];
  uint64_t mac = ESP.getEfuseMac();
  snprintf(buf, sizeof(buf), "%012llx", (unsigned long long)mac);
  return String(buf);
}
static inline String mqttBase()              { return String(cfg.mqtt_base_topic); }
static inline String mqttStatusTopic()       { return mqttBase() + "/status"; }
static inline String mqttAlertTopic()        { return mqttBase() + "/alert"; }
static inline String mqttAllOffTopic()       { return mqttBase() + "/all/off"; }
static inline String mqttSyncTopic()         { return mqttBase() + "/cmd/sync"; }
static inline String mqttOtaCmdTopic()       { return mqttBase() + "/cmd/ota"; }
static inline String mqttOtaStatusTopic()    { return mqttBase() + "/ota/status"; }
static inline String mqttZoneStateTopic(int n) { return mqttBase() + "/zone/" + String(n) + "/state"; }
static inline String mqttZoneCmdTopic(int n)   { return mqttBase() + "/zone/" + String(n) + "/cmd"; }

// ── INCOMING MESSAGE HANDLER ─────────────────────────────────
static void onMessage(char* topic, byte* payload, unsigned int length) {
  char msg[256] = {0};
  strncpy(msg, (char*)payload, min((unsigned int)255, length));
  Serial.printf("[MQTT] <- %s: %s\n", topic, msg);

  // Zone command: ../zone/N/cmd
  for (int i = 0; i < MAX_ZONES; i++) {
    String cmdTopic = mqttZoneCmdTopic(i + 1);
    if (strcmp(topic, cmdTopic.c_str()) == 0) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, msg);
      const char* cmd = doc["cmd"] | msg;
      int dur = doc["duration"] | DEFAULT_RUN_MIN;

      if (strcmp(cmd, "on") == 0 || strcmp(cmd, "ON") == 0) {
        const char* src = doc["source"] | "manual";
        if (strcmp(src, "program") == 0 || strcmp(src, "schedule") == 0) {
          zoneOnProgram(_zones, i, dur);
        } else {
          zoneOnManual(_zones, i, dur);
        }
        mqttPublishZone(i, _zones[i]);
      } else if (strcmp(cmd, "off") == 0 || strcmp(cmd, "OFF") == 0) {
        zoneOff(_zones, i);
        mqttPublishZone(i, _zones[i]);
      } else if (strcmp(cmd, "auto") == 0) {
        zoneOff(_zones, i);
        mqttPublishZone(i, _zones[i]);
      }
      return;
    }
  }

  // All off
  if (strcmp(topic, mqttAllOffTopic().c_str()) == 0) {
    zoneOffAll(_zones);
    return;
  }

  // Sync request
  if (strcmp(topic, mqttSyncTopic().c_str()) == 0) {
    Serial.println("[MQTT] Sync requested");
    mqttPublishStatus(_zones, 0);
    for (int i = 0; i < MAX_ZONES; i++) mqttPublishZone(i, _zones[i]);
    return;
  }

  // OTA command: .../cmd/ota
  if (strcmp(topic, mqttOtaCmdTopic().c_str()) == 0) {
    StaticJsonDocument<256> doc;
    deserializeJson(doc, msg);
    const char* action = doc["action"] | "update";

    if (strcmp(action, "check") == 0) {
      Serial.println("[MQTT] OTA check requested");
      String newVer, dlUrl;
      bool avail = otaCheckForUpdate(newVer, dlUrl);
      char resp[256];
      snprintf(resp, sizeof(resp),
        "{\"update_available\":%s,\"current\":\"%s\",\"latest\":\"%s\"}",
        avail ? "true" : "false", FW_VERSION, newVer.c_str());
      _mqtt.publish(mqttOtaStatusTopic().c_str(), resp);
    } else if (strcmp(action, "update") == 0) {
      Serial.println("[MQTT] OTA update requested via MQTT");
      String newVer, dlUrl;
      if (otaCheckForUpdate(newVer, dlUrl)) {
        char resp[128];
        snprintf(resp, sizeof(resp),
          "{\"status\":\"downloading\",\"version\":\"%s\"}", newVer.c_str());
        _mqtt.publish(mqttOtaStatusTopic().c_str(), resp);
        otaSetPending(dlUrl);
      } else {
        _mqtt.publish(mqttOtaStatusTopic().c_str(),
          "{\"status\":\"up_to_date\",\"version\":\"" FW_VERSION "\"}");
      }
    }
    return;
  }
}

// ── RECONNECT ────────────────────────────────────────────────
static void reconnect() {
  if (cfg.mqtt_host[0] == '\0') return;
  if (millis() - _lastReconnect < 5000) return;
  _lastReconnect = millis();

  Serial.printf("[MQTT] Connecting to %s:%d\n", cfg.mqtt_host, cfg.mqtt_port);
  _wifiClient.setInsecure();

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "fc-irrig-%04X",
           (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  if (_mqtt.connect(clientId, cfg.mqtt_user, cfg.mqtt_password,
                    mqttStatusTopic().c_str(), 0, true, "{\"online\":false}")) {
    Serial.printf("[MQTT] Connected — base topic: %s\n", cfg.mqtt_base_topic);

    for (int i = 1; i <= MAX_ZONES; i++) {
      _mqtt.subscribe(mqttZoneCmdTopic(i).c_str());
    }
    _mqtt.subscribe(mqttAllOffTopic().c_str());
    _mqtt.subscribe(mqttSyncTopic().c_str());
    _mqtt.subscribe(mqttOtaCmdTopic().c_str());

    mqttPublishStatus(_zones, 0);
    for (int i = 0; i < MAX_ZONES; i++) mqttPublishZone(i, _zones[i]);

    if (!_discoveryPublished) {
      mqttPublishDiscovery(_zones);
      _discoveryPublished = true;
    }
  } else {
    Serial.printf("[MQTT] Failed rc=%d\n", _mqtt.state());
  }
}

// ── PUBLIC ───────────────────────────────────────────────────
void mqttInit(Zone zones[MAX_ZONES]) {
  _zones = zones;
  if (cfg.mqtt_host[0] == '\0') {
    Serial.println("[MQTT] No host configured");
    return;
  }
  _mqtt.setServer(cfg.mqtt_host, cfg.mqtt_port);
  _mqtt.setCallback(onMessage);
  _mqtt.setBufferSize(1024);
  _mqtt.setKeepAlive(60);
  reconnect();
}

void mqttLoop(Zone zones[MAX_ZONES]) {
  _zones = zones;
  if (!_mqtt.connected()) reconnect();
  _mqtt.loop();
}

bool mqttIsConnected() {
  return _mqtt.connected();
}

void mqttPublishStatus(Zone zones[MAX_ZONES], float supplyPsi) {
  if (!_mqtt.connected()) return;
  StaticJsonDocument<1024> doc;
  doc["device"]     = DEVICE_ID;
  doc["device_id"]  = chipSerial();   // unique per ESP32 chip — used by dashboard for unclaimed-device discovery
  doc["base_topic"] = cfg.mqtt_base_topic;
  doc["fw"]         = FW_VERSION;
  doc["online"]     = true;
  doc["supply_psi"] = supplyPsi;
  doc["uptime"]     = millis() / 1000;
  doc["rssi"]       = WiFi.RSSI();
  doc["ip"]         = WiFi.localIP().toString();
  JsonArray arr = doc.createNestedArray("zones");
  for (int i = 0; i < MAX_ZONES; i++) {
    JsonObject z = arr.createNestedObject();
    z["id"]   = i + 1;
    z["name"] = zones[i].name;
    z["on"]   = zoneIsOn(i);
    ZoneState st = getZoneState(zones[i]);
    z["state"] = (st == ZONE_MANUAL)   ? "manual"   :
                 (st == ZONE_PROGRAM)  ? "program"  :
                 (st == ZONE_SCHEDULE) ? "schedule" : "off";
  }
  char buf[1024];
  serializeJson(doc, buf);
  _mqtt.publish(mqttStatusTopic().c_str(), buf, true);
}

void mqttPublishZone(int idx, const Zone& z) {
  if (!_mqtt.connected()) return;
  String topic = mqttZoneStateTopic(idx + 1);
  StaticJsonDocument<128> doc;
  doc["zone"]  = idx + 1;
  doc["name"]  = z.name;
  doc["on"]    = zoneIsOn(idx);
  ZoneState st = getZoneState(z);
  doc["state"] = (st == ZONE_MANUAL) ? "manual" :
                 (st == ZONE_SCHEDULE) ? "schedule" : "off";
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(topic.c_str(), buf, true);
}

void mqttPublishAlert(const char* message) {
  if (!_mqtt.connected()) return;
  StaticJsonDocument<128> doc;
  doc["alert"] = message;
  doc["ts"]    = millis() / 1000;
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(mqttAlertTopic().c_str(), buf);
  Serial.printf("[MQTT] Alert: %s\n", message);
}

void mqttPublishDiscovery(Zone zones[MAX_ZONES]) {
  if (!_mqtt.connected()) return;

  // Zone switches
  for (int i = 0; i < MAX_ZONES; i++) {
    char topic[128], payload[768];
    snprintf(topic, sizeof(topic),
             "%s/switch/%s_zone%d/config", MQTT_DISCOVERY, DEVICE_ID, i + 1);

    StaticJsonDocument<768> doc;
    char uid[32];   snprintf(uid,   sizeof(uid),   "%s_zone%d",   DEVICE_ID, i + 1);
    char name[32];  snprintf(name,  sizeof(name),  "%s Zone %d",  DEVICE_NAME, i + 1);
    String stTopic  = mqttZoneStateTopic(i + 1);
    String cmdTopic = mqttZoneCmdTopic(i + 1);

    doc["name"]           = name;
    doc["unique_id"]      = uid;
    doc["state_topic"]    = stTopic;
    doc["command_topic"]  = cmdTopic;
    doc["value_template"] = "{{ 'ON' if value_json.on else 'OFF' }}";
    doc["payload_on"]     = "{\"cmd\":\"on\",\"duration\":10}";
    doc["payload_off"]    = "{\"cmd\":\"off\"}";
    doc["icon"]           = "mdi:water";
    doc["availability_topic"] = mqttStatusTopic();
    doc["availability_template"] = "{{ 'online' if value_json.online else 'offline' }}";
    JsonObject dev = doc.createNestedObject("device");
    dev["identifiers"]  = DEVICE_ID;
    dev["name"]         = DEVICE_NAME;
    dev["model"]        = "KC868-A8v3";
    dev["manufacturer"] = "KinCony";
    dev["sw_version"]   = FW_VERSION;
    serializeJson(doc, payload);
    _mqtt.publish(topic, payload, true);
  }

  // Supply pressure sensor
  {
    char topic[128], payload[768];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_supply/config", MQTT_DISCOVERY, DEVICE_ID);
    StaticJsonDocument<768> doc;
    doc["name"]               = DEVICE_NAME " Supply PSI";
    doc["unique_id"]          = DEVICE_ID "_supply_psi";
    doc["state_topic"]        = mqttStatusTopic();
    doc["value_template"]     = "{{ value_json.supply_psi }}";
    doc["unit_of_measurement"]= "PSI";
    doc["device_class"]       = "pressure";
    doc["state_class"]        = "measurement";
    doc["icon"]               = "mdi:gauge";
    doc["availability_topic"] = mqttStatusTopic();
    doc["availability_template"] = "{{ 'online' if value_json.online else 'offline' }}";
    JsonObject dev = doc.createNestedObject("device");
    dev["identifiers"]  = DEVICE_ID;
    dev["name"]         = DEVICE_NAME;
    dev["model"]        = "KC868-A8v3";
    dev["manufacturer"] = "KinCony";
    dev["sw_version"]   = FW_VERSION;
    serializeJson(doc, payload);
    _mqtt.publish(topic, payload, true);
  }

  // Firmware version sensor
  {
    char topic[128], payload[768];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_firmware/config", MQTT_DISCOVERY, DEVICE_ID);
    StaticJsonDocument<768> doc;
    doc["name"]           = DEVICE_NAME " Firmware";
    doc["unique_id"]      = DEVICE_ID "_firmware";
    doc["state_topic"]    = mqttStatusTopic();
    doc["value_template"] = "{{ value_json.fw }}";
    doc["icon"]           = "mdi:chip";
    doc["entity_category"]= "diagnostic";
    doc["availability_topic"] = mqttStatusTopic();
    doc["availability_template"] = "{{ 'online' if value_json.online else 'offline' }}";
    JsonObject dev = doc.createNestedObject("device");
    dev["identifiers"]  = DEVICE_ID;
    dev["name"]         = DEVICE_NAME;
    dev["model"]        = "KC868-A8v3";
    dev["manufacturer"] = "KinCony";
    dev["sw_version"]   = FW_VERSION;
    serializeJson(doc, payload);
    _mqtt.publish(topic, payload, true);
  }

  // WiFi RSSI sensor
  {
    char topic[128], payload[768];
    snprintf(topic, sizeof(topic), "%s/sensor/%s_rssi/config", MQTT_DISCOVERY, DEVICE_ID);
    StaticJsonDocument<768> doc;
    doc["name"]               = DEVICE_NAME " WiFi RSSI";
    doc["unique_id"]          = DEVICE_ID "_rssi";
    doc["state_topic"]        = mqttStatusTopic();
    doc["value_template"]     = "{{ value_json.rssi }}";
    doc["unit_of_measurement"]= "dBm";
    doc["device_class"]       = "signal_strength";
    doc["state_class"]        = "measurement";
    doc["icon"]               = "mdi:wifi";
    doc["entity_category"]    = "diagnostic";
    doc["availability_topic"] = mqttStatusTopic();
    doc["availability_template"] = "{{ 'online' if value_json.online else 'offline' }}";
    JsonObject dev = doc.createNestedObject("device");
    dev["identifiers"]  = DEVICE_ID;
    dev["name"]         = DEVICE_NAME;
    dev["model"]        = "KC868-A8v3";
    dev["manufacturer"] = "KinCony";
    dev["sw_version"]   = FW_VERSION;
    serializeJson(doc, payload);
    _mqtt.publish(topic, payload, true);
  }

  Serial.printf("[MQTT] HA discovery published (%d zones + 3 sensors)\n", MAX_ZONES);
}
