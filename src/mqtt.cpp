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

// ── INCOMING MESSAGE HANDLER ─────────────────────────────────
static void onMessage(char* topic, byte* payload, unsigned int length) {
  char msg[256] = {0};
  strncpy(msg, (char*)payload, min((unsigned int)255, length));
  Serial.printf("[MQTT] <- %s: %s\n", topic, msg);

  // Zone command: ../zone/N/cmd
  char cmdTopic[64];
  for (int i = 0; i < MAX_ZONES; i++) {
    snprintf(cmdTopic, sizeof(cmdTopic), MQTT_ZONE_CMD, i + 1);
    if (strcmp(topic, cmdTopic) == 0) {
      StaticJsonDocument<128> doc;
      deserializeJson(doc, msg);
      const char* cmd = doc["cmd"] | msg;
      int dur = doc["duration"] | DEFAULT_RUN_MIN;

      if (strcmp(cmd, "on") == 0 || strcmp(cmd, "ON") == 0) {
        zoneOnManual(_zones, i, dur);
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
  if (strcmp(topic, MQTT_BASE "/all/off") == 0) {
    zoneOffAll(_zones);
    return;
  }

  // Sync request
  if (strcmp(topic, MQTT_BASE "/cmd/sync") == 0) {
    Serial.println("[MQTT] Sync requested");
    mqttPublishStatus(_zones, 0);
    for (int i = 0; i < MAX_ZONES; i++) mqttPublishZone(i, _zones[i]);
    return;
  }

  // OTA command: .../cmd/ota
  if (strcmp(topic, MQTT_BASE "/cmd/ota") == 0) {
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
      _mqtt.publish(MQTT_BASE "/ota/status", resp);
    } else if (strcmp(action, "update") == 0) {
      Serial.println("[MQTT] OTA update requested via MQTT");
      String newVer, dlUrl;
      if (otaCheckForUpdate(newVer, dlUrl)) {
        char resp[128];
        snprintf(resp, sizeof(resp),
          "{\"status\":\"downloading\",\"version\":\"%s\"}", newVer.c_str());
        _mqtt.publish(MQTT_BASE "/ota/status", resp);
        otaSetPending(dlUrl);
      } else {
        _mqtt.publish(MQTT_BASE "/ota/status",
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
                    MQTT_STATUS, 0, true, "{\"online\":false}")) {
    Serial.println("[MQTT] Connected");

    char sub[64];
    for (int i = 1; i <= MAX_ZONES; i++) {
      snprintf(sub, sizeof(sub), MQTT_ZONE_CMD, i);
      _mqtt.subscribe(sub);
    }
    _mqtt.subscribe(MQTT_BASE "/all/off");
    _mqtt.subscribe(MQTT_BASE "/cmd/sync");
    _mqtt.subscribe(MQTT_BASE "/cmd/ota");

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
  doc["fw"]         = FW_VERSION;
  doc["online"]     = true;
  doc["supply_psi"] = supplyPsi;
  doc["uptime"]     = millis() / 1000;
  JsonArray arr = doc.createNestedArray("zones");
  for (int i = 0; i < MAX_ZONES; i++) {
    JsonObject z = arr.createNestedObject();
    z["id"]   = i + 1;
    z["name"] = zones[i].name;
    z["on"]   = zoneIsOn(i);
    ZoneState st = getZoneState(zones[i]);
    z["state"] = (st == ZONE_MANUAL) ? "manual" :
                 (st == ZONE_SCHEDULE) ? "schedule" : "off";
  }
  char buf[1024];
  serializeJson(doc, buf);
  _mqtt.publish(MQTT_STATUS, buf, true);
}

void mqttPublishZone(int idx, const Zone& z) {
  if (!_mqtt.connected()) return;
  char topic[64];
  snprintf(topic, sizeof(topic), MQTT_ZONE_STATE, idx + 1);
  StaticJsonDocument<128> doc;
  doc["zone"]  = idx + 1;
  doc["name"]  = z.name;
  doc["on"]    = zoneIsOn(idx);
  ZoneState st = getZoneState(z);
  doc["state"] = (st == ZONE_MANUAL) ? "manual" :
                 (st == ZONE_SCHEDULE) ? "schedule" : "off";
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(topic, buf, true);
}

void mqttPublishAlert(const char* message) {
  if (!_mqtt.connected()) return;
  StaticJsonDocument<128> doc;
  doc["alert"] = message;
  doc["ts"]    = millis() / 1000;
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(MQTT_ALERT, buf);
  Serial.printf("[MQTT] Alert: %s\n", message);
}

void mqttPublishDiscovery(Zone zones[MAX_ZONES]) {
  if (!_mqtt.connected()) return;
  for (int i = 0; i < MAX_ZONES; i++) {
    char topic[128], payload[512];
    snprintf(topic, sizeof(topic),
             "%s/switch/%s_zone%d/config", MQTT_DISCOVERY, DEVICE_ID, i + 1);

    StaticJsonDocument<512> doc;
    char uid[32];   snprintf(uid,   sizeof(uid),   "%s_zone%d",   DEVICE_ID, i + 1);
    char name[32];  snprintf(name,  sizeof(name),  "%s Zone %d",  DEVICE_NAME, i + 1);
    char stTopic[64]; snprintf(stTopic, sizeof(stTopic), MQTT_ZONE_STATE, i + 1);
    char cmdTopic[64]; snprintf(cmdTopic, sizeof(cmdTopic), MQTT_ZONE_CMD, i + 1);

    doc["name"]           = name;
    doc["unique_id"]      = uid;
    doc["state_topic"]    = stTopic;
    doc["command_topic"]  = cmdTopic;
    doc["value_template"] = "{{ 'ON' if value_json.on else 'OFF' }}";
    doc["payload_on"]     = "{\"cmd\":\"on\",\"duration\":10}";
    doc["payload_off"]    = "{\"cmd\":\"off\"}";
    JsonObject dev = doc.createNestedObject("device");
    dev["identifiers"] = DEVICE_ID;
    dev["name"]        = DEVICE_NAME;
    dev["model"]       = "KC868-A8v3";
    dev["manufacturer"]= "KinCony / Sandy Soil Automations";
    serializeJson(doc, payload);
    _mqtt.publish(topic, payload, true);
  }

  // Supply pressure sensor
  char topic[128], payload[512];
  snprintf(topic, sizeof(topic), "%s/sensor/%s_supply/config", MQTT_DISCOVERY, DEVICE_ID);
  StaticJsonDocument<512> doc;
  doc["name"]               = DEVICE_NAME " Supply PSI";
  doc["unique_id"]          = DEVICE_ID "_supply_psi";
  doc["state_topic"]        = MQTT_STATUS;
  doc["value_template"]     = "{{ value_json.supply_psi }}";
  doc["unit_of_measurement"]= "PSI";
  doc["device_class"]       = "pressure";
  JsonObject dev = doc.createNestedObject("device");
  dev["identifiers"] = DEVICE_ID;
  dev["name"]        = DEVICE_NAME;
  serializeJson(doc, payload);
  _mqtt.publish(topic, payload, true);

  Serial.println("[MQTT] Discovery published");
}
