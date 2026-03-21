#include "wifi_setup.h"
#include "config.h"
#include "storage.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// ─────────────────────────────────────────────
//  wifi_setup.cpp
//  Setup portal uses a simple blocking WebServer
//  (not AsyncWebServer — the async server only
//  starts after WiFi is connected).
// ─────────────────────────────────────────────

static uint32_t _lastReconnect = 0;

// ── SETUP PORTAL ─────────────────────────────

static const char SETUP_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FarmControl — WiFi Setup</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1f2e;color:#e2e8f0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#252b3b;border-radius:10px;padding:24px;width:100%;max-width:420px}
h1{font-size:18px;margin-bottom:4px}
p{font-size:12px;color:#94a3b8;margin-bottom:20px}
.sec{font-size:11px;text-transform:uppercase;letter-spacing:1px;color:#94a3b8;margin:16px 0 8px}
label{display:block;font-size:12px;color:#94a3b8;margin-bottom:4px}
input{width:100%;padding:9px 11px;background:#1a1f2e;border:1px solid #2d3548;border-radius:5px;color:#e2e8f0;font-size:13px;margin-bottom:12px}
button{width:100%;padding:12px;background:#4ade80;color:#000;border:none;border-radius:6px;font-weight:700;font-size:14px;cursor:pointer;margin-top:4px}
</style></head><body>
<div class="card">
  <h1>FarmControl Setup</h1>
  <p>Irrigation Controller — First Boot</p>
  <form method="POST" action="/save">
    <div class="sec">WiFi</div>
    <label>Network Name (SSID)</label>
    <input name="ssid" type="text" required autocomplete="off">
    <label>Password</label>
    <input name="pass" type="password" autocomplete="off">
    <div class="sec">MQTT (optional — configure later via web UI)</div>
    <label>Host</label>
    <input name="mqtt_host" type="text" placeholder="xxxx.s1.eu.hivemq.cloud">
    <label>Port</label>
    <input name="mqtt_port" type="number" value="8883">
    <label>Username</label>
    <input name="mqtt_user" type="text">
    <label>Password</label>
    <input name="mqtt_pass" type="password">
    <button type="submit">Save &amp; Connect</button>
  </form>
</div></body></html>
)HTML";

static const char SAVED_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved</title>
<style>body{font-family:-apple-system,sans-serif;background:#1a1f2e;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;text-align:center}
.card{background:#252b3b;border-radius:10px;padding:30px;max-width:340px}
h1{color:#4ade80;margin-bottom:8px}p{color:#94a3b8;font-size:13px}</style></head>
<body><div class="card"><h1>✓ Saved</h1>
<p>Board is rebooting and connecting to your WiFi.<br><br>
Find it on your network and open its IP address in a browser.</p></div></body></html>
)HTML";

void wifiStartSetupPortal() {
  Serial.println("[WiFi] Starting setup portal...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(HOTSPOT_SSID);
  Serial.printf("[WiFi] Hotspot: %s  IP: %s\n", HOTSPOT_SSID, HOTSPOT_IP);

  DNSServer dns;
  dns.start(53, "*", WiFi.softAPIP());

  WebServer server(80);

  server.on("/", HTTP_GET, [&]() {
    server.send_P(200, "text/html", SETUP_HTML);
  });
  server.on("/save", HTTP_POST, [&]() {
    String ssid  = server.arg("ssid");
    String pass  = server.arg("pass");
    String mhost = server.arg("mqtt_host");
    int    mport = server.arg("mqtt_port").toInt();
    String muser = server.arg("mqtt_user");
    String mpass = server.arg("mqtt_pass");

    strlcpy(cfg.wifi_ssid,     ssid.c_str(),  sizeof(cfg.wifi_ssid));
    strlcpy(cfg.wifi_password, pass.c_str(),  sizeof(cfg.wifi_password));
    strlcpy(cfg.mqtt_host,     mhost.c_str(), sizeof(cfg.mqtt_host));
    cfg.mqtt_port = mport > 0 ? mport : 8883;
    strlcpy(cfg.mqtt_user,     muser.c_str(), sizeof(cfg.mqtt_user));
    strlcpy(cfg.mqtt_password, mpass.c_str(), sizeof(cfg.mqtt_password));
    storageSaveConfig();

    server.send_P(200, "text/html", SAVED_HTML);
    delay(1500);
    ESP.restart();
  });
  server.onNotFound([&]() {
    server.sendHeader("Location", "http://192.168.4.1");
    server.send(302, "text/plain", "");
  });
  server.begin();

  Serial.println("[WiFi] Portal running — waiting for setup...");
  while (true) {
    dns.processNextRequest();
    server.handleClient();
    delay(5);
  }
}

// ── NORMAL CONNECT ────────────────────────────

bool wifiConnect() {
  if (strlen(cfg.wifi_ssid) == 0) return false;

  WiFi.mode(WIFI_STA);
  if (strlen(cfg.static_ip) > 0) {
    IPAddress ip, gw, sn;
    ip.fromString(cfg.static_ip);
    gw.fromString(cfg.gateway);
    sn.fromString(cfg.subnet);
    WiFi.config(ip, gw, sn);
  }

  WiFi.begin(cfg.wifi_ssid, cfg.wifi_password);
  Serial.printf("[WiFi] Connecting to %s", cfg.wifi_ssid);

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected — IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("[WiFi] Connection failed");
  return false;
}

String wifiGetIP() {
  return WiFi.localIP().toString();
}

bool wifiIsConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void wifiMaintain() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - _lastReconnect > 30000) {
      _lastReconnect = millis();
      Serial.println("[WiFi] Reconnecting...");
      WiFi.reconnect();
    }
  }
}
