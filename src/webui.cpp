#include "webui.h"
#include "config.h"
#include "storage.h"
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <WiFi.h>

// ── AP hotspot credentials ─────────────────────────────────────
#define WEBUI_AP_SSID  "SandySoil-Setup"
#define WEBUI_AP_PASS  "irrigation"        // min 8 chars for WPA2

// ── ArduinoOTA password (used by VS Code / PlatformIO OTA) ───
#define OTA_PASSWORD   "irrigation8z"

static WebServer _server(80);
static bool      _apMode = false;

bool webui_in_ap_mode() { return _apMode; }

// ─────────────────────────────────────────────────────────────
// Shared HTML helpers
// ─────────────────────────────────────────────────────────────

static String htmlHead(const char* title) {
  String h = F("<!DOCTYPE html><html lang='en'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>");
  h += title;
  h += F("</title><style>"
    "body{font-family:sans-serif;max-width:520px;margin:40px auto;padding:0 16px;background:#f0f4f0}"
    "h1{color:#2e7d32;margin-bottom:4px}h2{color:#444;margin-top:0}"
    "p.sub{color:#666;font-size:.9em;margin-top:0}"
    "fieldset{border:1px solid #ccc;border-radius:8px;padding:16px;margin-bottom:16px;background:#fff}"
    "legend{font-weight:bold;color:#555;padding:0 6px}"
    "label{display:block;margin:10px 0 3px;font-size:.88em;color:#444;font-weight:500}"
    "input[type=text],input[type=password],input[type=number]{"
      "width:100%;box-sizing:border-box;padding:9px 10px;border:1px solid #bbb;"
      "border-radius:5px;font-size:1em}"
    "input:focus{outline:none;border-color:#2e7d32;box-shadow:0 0 0 2px #c8e6c9}"
    ".btn{display:inline-block;background:#2e7d32;color:#fff;border:none;"
      "padding:11px 28px;border-radius:5px;cursor:pointer;font-size:1em;text-decoration:none}"
    ".btn:hover{background:#1b5e20}"
    ".btn-sec{background:#555}.btn-sec:hover{background:#333}"
    ".msg{padding:12px 16px;border-radius:6px;margin-bottom:16px}"
    ".ok{background:#c8e6c9;color:#1b5e20;border:1px solid #a5d6a7}"
    ".err{background:#ffcdd2;color:#b71c1c;border:1px solid #ef9a9a}"
    ".row{display:flex;gap:12px;align-items:flex-end}"
    ".row>div{flex:1}"
    "nav{margin-bottom:20px}nav a{margin-right:12px;color:#2e7d32}"
    "</style></head><body>"
    "<h1>Sandy Soil 8Z</h1><p class='sub'>Irrigation Controller &mdash; Mildura</p>");
  return h;
}

static String htmlFoot() {
  return F("</body></html>");
}

// ─────────────────────────────────────────────────────────────
// GET /config  — show credentials form
// ─────────────────────────────────────────────────────────────

static void handleConfigGet() {
  char ssid[64]="", wpass[64]="", host[128]="", muser[64]="", mpass[64]="";
  int  port = 8883;
  storage_load_wifi(ssid, wpass);
  storage_load_mqtt(host, &port, muser, mpass);

  String html = htmlHead("Configuration");
  html += "<nav><a href='/config'>Config</a><a href='/update'>Firmware Update</a></nav>";
  html += "<form method='POST' action='/config'>";

  // WiFi
  html += "<fieldset><legend>WiFi</legend>";
  html += "<label>SSID (network name)</label>";
  html += "<input type='text' name='ssid' value='"; html += ssid; html += "' required>";
  html += "<label>Password</label>";
  html += "<input type='password' name='wpass' placeholder='(leave blank to keep current)'>";
  html += "</fieldset>";

  // MQTT
  html += "<fieldset><legend>MQTT Broker</legend>";
  html += "<div class='row'>";
  html += "<div><label>Host</label><input type='text' name='mhost' value='"; html += host; html += "'></div>";
  html += "<div style='max-width:90px'><label>Port</label><input type='number' name='mport' value='"; html += port; html += "'></div>";
  html += "</div>";
  html += "<label>Username</label>";
  html += "<input type='text' name='muser' value='"; html += muser; html += "'>";
  html += "<label>Password</label>";
  html += "<input type='password' name='mpass' placeholder='(leave blank to keep current)'>";
  html += "</fieldset>";

  html += "<button class='btn' type='submit'>Save &amp; Reboot</button>";
  html += "</form>";
  html += htmlFoot();
  _server.send(200, "text/html", html);
}

// ─────────────────────────────────────────────────────────────
// POST /config  — save and reboot
// ─────────────────────────────────────────────────────────────

static void handleConfigPost() {
  // Load current so blank password fields keep existing value
  char curSsid[64]="", curWpass[64]="", curHost[128]="", curMuser[64]="", curMpass[64]="";
  int  curPort = 8883;
  storage_load_wifi(curSsid, curWpass);
  storage_load_mqtt(curHost, &curPort, curMuser, curMpass);

  String ssid  = _server.arg("ssid");
  String wpass = _server.arg("wpass");
  String host  = _server.arg("mhost");
  int    port  = _server.arg("mport").toInt();
  String muser = _server.arg("muser");
  String mpass = _server.arg("mpass");

  // Validation: SSID must not be empty
  if (ssid.length() == 0) {
    String html = htmlHead("Configuration");
    html += "<div class='msg err'>SSID cannot be empty.</div>";
    html += "<a class='btn btn-sec' href='/config'>Back</a>";
    html += htmlFoot();
    _server.send(400, "text/html", html);
    return;
  }

  storage_save_wifi(ssid.c_str(), wpass.length() ? wpass.c_str() : curWpass);
  storage_save_mqtt(
    host.length() ? host.c_str() : curHost,
    port          ? port         : curPort,
    muser.c_str(),
    mpass.length() ? mpass.c_str() : curMpass
  );

  Serial.printf("[WebUI] Credentials saved — SSID: %s, MQTT: %s:%d\n",
                ssid.c_str(), host.c_str(), port ? port : curPort);

  String html = htmlHead("Configuration");
  html += "<div class='msg ok'>Settings saved. Rebooting in 2 seconds&hellip;</div>";
  html += htmlFoot();
  _server.send(200, "text/html", html);
  delay(2000);
  ESP.restart();
}

// ─────────────────────────────────────────────────────────────
// GET /update  — OTA firmware upload page
// ─────────────────────────────────────────────────────────────

static void handleOtaGet() {
  String html = htmlHead("Firmware Update");
  html += "<nav><a href='/config'>Config</a><a href='/update'>Firmware Update</a></nav>";
  html += "<h2>Firmware Update</h2>";
  html += "<p>Upload a compiled <code>.bin</code> firmware file to update over-the-air.</p>";
  html += "<p>Current firmware: <strong>" FW_VERSION "</strong></p>";
  html += "<form method='POST' action='/update' enctype='multipart/form-data'>"
          "<fieldset><legend>Upload Firmware</legend>"
          "<label>Firmware file (.bin)</label>"
          "<input type='file' name='firmware' accept='.bin' required>"
          "<br><br>"
          "<button class='btn' type='submit'>Upload &amp; Flash</button>"
          "</fieldset></form>";
  html += htmlFoot();
  _server.send(200, "text/html", html);
}

// ─────────────────────────────────────────────────────────────
// POST /update  — handle binary upload (called after upload completes)
// ─────────────────────────────────────────────────────────────

static void handleOtaPost() {
  bool ok = !Update.hasError();
  String html = htmlHead("Firmware Update");
  if (ok) {
    html += "<div class='msg ok'>Firmware updated successfully. Rebooting&hellip;</div>";
  } else {
    html += "<div class='msg err'>Firmware update FAILED. Check your .bin file and try again.</div>";
    html += "<a class='btn btn-sec' href='/update'>Try again</a>";
  }
  html += htmlFoot();
  _server.send(200, "text/html", html);
  if (ok) { delay(1500); ESP.restart(); }
}

// ─────────────────────────────────────────────────────────────
// Upload handler — streams binary data into the OTA partition
// ─────────────────────────────────────────────────────────────

static void handleOtaUpload() {
  HTTPUpload& upload = _server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("[WebUI/OTA] Upload start: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("[WebUI/OTA] Upload complete: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

// ─────────────────────────────────────────────────────────────
// Root redirect
// ─────────────────────────────────────────────────────────────

static void handleRoot() {
  _server.sendHeader("Location", "/config", true);
  _server.send(302, "text/plain", "");
}

// ─────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────

void webui_start_ap() {
  _apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WEBUI_AP_SSID, WEBUI_AP_PASS);
  Serial.printf("[WebUI] AP started — SSID: %s  IP: %s\n",
                WEBUI_AP_SSID, WiFi.softAPIP().toString().c_str());
  Serial.println("[WebUI] Connect to the hotspot and open http://192.168.4.1");
}

void webui_init() {
  // ── HTTP routes ───────────────────────────────────────────
  _server.on("/",       HTTP_GET,  handleRoot);
  _server.on("/config", HTTP_GET,  handleConfigGet);
  _server.on("/config", HTTP_POST, handleConfigPost);
  _server.on("/update", HTTP_GET,  handleOtaGet);
  _server.on("/update", HTTP_POST, handleOtaPost, handleOtaUpload);
  _server.begin();

  if (_apMode) {
    Serial.println("[WebUI] Config server on http://192.168.4.1");
  } else {
    Serial.printf("[WebUI] Config server on http://%s\n",
                  WiFi.localIP().toString().c_str());
  }

  // ── ArduinoOTA — VS Code / PlatformIO OTA upload ─────────
  // To use: in platformio.ini set upload_protocol = espota
  //         and upload_port = <device IP shown above>
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
}

void webui_loop() {
  _server.handleClient();
  ArduinoOTA.handle();
}
