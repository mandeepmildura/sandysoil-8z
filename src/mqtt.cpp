#include "mqtt.h"
#include "zones.h"
#include "storage.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClientSecure _wifiClient;
static PubSubClient     _mqtt(_wifiClient);
static Zone*            _zones = nullptr;

static char _mqttHost[128] = "";
static char _mqttUser[64]  = "";
static char _mqttPass[64]  = "";
static int  _mqttPort      = 8883;

static uint32_t _lastReconnect = 0;
static bool     _discoveryPublished = false;

// ── INCOMING MESSAGE HANDLER ─────────────────────────────────
static void onMessage(char* topic, byte* payload, unsigned int length) {
  char msg[256] = {0};
  strncpy(msg, (char*)payload, min((unsigned int)255, length));
  Serial.printf("[MQTT] ← %s: %s\n", topic, msg);

  // Parse zone command: farm/irrigation1/zone/N/cmd
  int zoneNum = 0;
  char cmdTopic[64];
  for (int i = 0; i < MAX_ZONES; i++) {
    snprintf(cmdTopic, sizeof(cmdTopic), MQTT_ZONE_CMD, i + 1);
    if (strcmp(topic, cmdTopic) == 0) {
      zoneNum = i + 1;
      break;
    }
  }

  if (zoneNum > 0) {
    int idx = zoneNum - 1;
    StaticJsonDocument<128> doc;
    deserializeJson(doc, msg);

    const char* cmd = doc["cmd"] | msg;  // support both {"cmd":"on"} and plain "on"
    int dur = doc["duration"] | DEFAULT_RUN_MIN;

    if (strcmp(cmd, "on") == 0 || strcmp(cmd, "ON") == 0) {
      zone_on_manual(_zones, idx, dur);
      mqtt_publish_zone(idx, _zones[idx]);
    } else if (strcmp(cmd, "off") == 0 || strcmp(cmd, "OFF") == 0) {
      zone_off(_zones, idx);
      mqtt_publish_zone(idx, _zones[idx]);
    } else if (strcmp(cmd, "auto") == 0) {
      zone_off(_zones, idx);  // return to auto/schedule mode
      mqtt_publish_zone(idx, _zones[idx]);
    }
    return;
  }

  // Global all-off
  if (strcmp(topic, MQTT_BASE "/all/off") == 0) {
    zone_off_all(_zones);
    return;
  }

  // Sync request — app asks for a full state dump on (re)connect
  if (strcmp(topic, MQTT_BASE "/cmd/sync") == 0) {
    Serial.println("[MQTT] Sync requested — publishing all zone states");
    mqtt_publish_status(_zones, 0);  // supply PSI will be updated next pressure tick
    for (int i = 0; i < MAX_ZONES; i++) mqtt_publish_zone(i, _zones[i]);
  }
}

// ── RECONNECT ─────────────────────────────────────────────────
static void reconnect() {
  if (_mqttHost[0] == '\0') return;
  if (millis() - _lastReconnect < 5000) return;
  _lastReconnect = millis();

  Serial.printf("[MQTT] Connecting to %s:%d ...\n", _mqttHost, _mqttPort);
  _wifiClient.setInsecure();

  char clientId[32];
  snprintf(clientId, sizeof(clientId), "farmcontrol-%s-%04X",
           DEVICE_ID, (uint16_t)(ESP.getEfuseMac() & 0xFFFF));

  if (_mqtt.connect(clientId, _mqttUser, _mqttPass,
                    MQTT_STATUS, 0, true, "{\"online\":false}")) {
    Serial.println("[MQTT] Connected");

    // Subscribe to all zone commands
    char sub[64];
    for (int i = 1; i <= MAX_ZONES; i++) {
      snprintf(sub, sizeof(sub), MQTT_ZONE_CMD, i);
      _mqtt.subscribe(sub);
    }
    _mqtt.subscribe(MQTT_BASE "/all/off");
    _mqtt.subscribe(MQTT_BASE "/cmd/sync");
    _mqtt.subscribe(MQTT_BASE "/discovery/request");

    // Publish current state of all zones immediately on connect
    // so broker retained messages are always up-to-date
    mqtt_publish_status(_zones, 0);
    for (int i = 0; i < MAX_ZONES; i++) mqtt_publish_zone(i, _zones[i]);

    if (!_discoveryPublished) {
      mqtt_publish_discovery(_zones);
      _discoveryPublished = true;
    }
  } else {
    Serial.printf("[MQTT] Failed, rc=%d\n", _mqtt.state());
  }
}

// ── PUBLIC ────────────────────────────────────────────────────
void mqtt_init(Zone zones[MAX_ZONES]) {
  _zones = zones;
  storage_load_mqtt(_mqttHost, &_mqttPort, _mqttUser, _mqttPass);
  if (_mqttHost[0] == '\0') {
    Serial.println("[MQTT] No host configured");
    return;
  }
  _mqtt.setServer(_mqttHost, _mqttPort);
  _mqtt.setCallback(onMessage);
  _mqtt.setBufferSize(1024);
  _mqtt.setKeepAlive(60);
  reconnect();
}

void mqtt_loop(Zone zones[MAX_ZONES]) {
  _zones = zones;
  if (!_mqtt.connected()) reconnect();
  _mqtt.loop();
}

bool mqtt_connected() {
  return _mqtt.connected();
}

void mqtt_publish_status(Zone zones[MAX_ZONES], float supplyPsi) {
  if (!_mqtt.connected()) return;
  StaticJsonDocument<1024> doc;
  doc["device"]    = DEVICE_ID;
  doc["fw"]        = FW_VERSION;
  doc["online"]    = true;
  doc["supply_psi"] = supplyPsi;
  doc["uptime"]    = millis() / 1000;
  JsonArray zonesArr = doc.createNestedArray("zones");
  for (int i = 0; i < MAX_ZONES; i++) {
    JsonObject z = zonesArr.createNestedObject();
    z["id"]   = i + 1;
    z["name"] = zones[i].name;
    z["on"]   = zone_is_on(i);
    ZoneState st = getZoneState(zones[i]);
    z["state"] = (st == ZONE_MANUAL) ? "manual" : (st == ZONE_SCHEDULE) ? "schedule" : "off";
  }
  char buf[1024];
  serializeJson(doc, buf);
  _mqtt.publish(MQTT_STATUS, buf, true);
}

void mqtt_publish_zone(int idx, const Zone& z) {
  if (!_mqtt.connected()) return;
  char topic[64];
  snprintf(topic, sizeof(topic), MQTT_ZONE_STATE, idx + 1);
  StaticJsonDocument<128> doc;
  doc["zone"]  = idx + 1;
  doc["name"]  = z.name;
  doc["on"]    = zone_is_on(idx);
  ZoneState st = getZoneState(z);
  doc["state"] = (st == ZONE_MANUAL) ? "manual" : (st == ZONE_SCHEDULE) ? "schedule" : "off";
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(topic, buf, true);
}

void mqtt_publish_alert(const char* message) {
  if (!_mqtt.connected()) return;
  StaticJsonDocument<128> doc;
  doc["alert"] = message;
  doc["ts"]    = millis() / 1000;
  char buf[128];
  serializeJson(doc, buf);
  _mqtt.publish(MQTT_ALERT, buf);
  Serial.printf("[MQTT] Alert: %s\n", message);
}

void mqtt_publish_discovery(Zone zones[MAX_ZONES]) {
  if (!_mqtt.connected()) return;

  // Publish HA auto-discovery for each zone as a switch
  for (int i = 0; i < MAX_ZONES; i++) {
    char topic[128], payload[512];
    snprintf(topic, sizeof(topic),
             "%s/switch/%s_zone%d/config", MQTT_DISCOVERY, DEVICE_ID, i + 1);

    StaticJsonDocument<512> doc;
    char uid[32]; snprintf(uid, sizeof(uid), "%s_zone%d", DEVICE_ID, i + 1);
    char name[32]; snprintf(name, sizeof(name), "%s Zone %d", DEVICE_NAME, i + 1);
    char stateTopic[64]; snprintf(stateTopic, sizeof(stateTopic), MQTT_ZONE_STATE, i + 1);
    char cmdTopic[64];   snprintf(cmdTopic,   sizeof(cmdTopic),   MQTT_ZONE_CMD,   i + 1);

    doc["name"]         = name;
    doc["unique_id"]    = uid;
    doc["state_topic"]  = stateTopic;
    doc["command_topic"]= cmdTopic;
    doc["value_template"]   = "{{ 'ON' if value_json.on else 'OFF' }}";
    doc["payload_on"]       = "{\"cmd\":\"on\",\"duration\":10}";
    doc["payload_off"]      = "{\"cmd\":\"off\"}";
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
  snprintf(topic, sizeof(topic), "%s/sensor/%s_supply_psi/config", MQTT_DISCOVERY, DEVICE_ID);
  StaticJsonDocument<512> doc;
  doc["name"]               = DEVICE_NAME " Supply Pressure";
  doc["unique_id"]          = DEVICE_ID "_supply_psi";
  doc["state_topic"]        = MQTT_STATUS;
  doc["value_template"]     = "{{ value_json.supplyPsi }}";
  doc["unit_of_measurement"]= "PSI";
  doc["device_class"]       = "pressure";
  JsonObject dev = doc.createNestedObject("device");
  dev["identifiers"] = DEVICE_ID;
  dev["name"]        = DEVICE_NAME;
  serializeJson(doc, payload);
  _mqtt.publish(topic, payload, true);

  Serial.println("[MQTT] HA discovery published");
}
