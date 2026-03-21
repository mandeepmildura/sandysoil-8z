#pragma once

// ─────────────────────────────────────────────
//  mqtt.h
//  FarmControl Irrigation Controller
//  HiveMQ Cloud MQTT via TLS port 8883
//
//  Published topics (base: cfg.mqtt_base_topic):
//    ../status          — online/offline (LWT) + full state
//    ../zone/{n}/state  — individual zone on/off
//    ../alert           — low pressure / fault alerts
//
//  Subscribed topics:
//    ../zone/{n}/cmd    — {"cmd":"on","duration":10} or "off"
//    ../all/off         — emergency all-zones-off
//    ../cmd/sync        — request full state dump
// ─────────────────────────────────────────────

#include <Arduino.h>
#include "config.h"

// Initialise MQTT — call once in setup() after WiFi connects
void mqttInit(Zone zones[MAX_ZONES]);

// Call from main loop — handles reconnection and incoming messages
void mqttLoop(Zone zones[MAX_ZONES]);

// Publish full device status
void mqttPublishStatus(Zone zones[MAX_ZONES], float supplyPsi);

// Publish single zone state
void mqttPublishZone(int idx, const Zone& z);

// Publish alert message
void mqttPublishAlert(const char* message);

// Publish Home Assistant MQTT discovery
void mqttPublishDiscovery(Zone zones[MAX_ZONES]);

// Returns true if connected to broker
bool mqttIsConnected();

// Legacy aliases used in main .ino
inline void mqtt_init(Zone z[])                       { mqttInit(z); }
inline void mqtt_loop(Zone z[])                       { mqttLoop(z); }
inline void mqtt_publish_status(Zone z[], float p)    { mqttPublishStatus(z, p); }
inline void mqtt_publish_zone(int i, const Zone& z)   { mqttPublishZone(i, z); }
inline void mqtt_publish_alert(const char* m)         { mqttPublishAlert(m); }
inline void mqtt_publish_discovery(Zone z[])          { mqttPublishDiscovery(z); }
inline bool mqtt_connected()                          { return mqttIsConnected(); }
