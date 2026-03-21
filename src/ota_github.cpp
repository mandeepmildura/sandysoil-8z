#include "ota_github.h"
#include "config.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

// -----------------------------------------
//  ota_github.cpp
//  Queries GitHub Releases API for latest
//  release, compares semver, downloads .bin
//  asset via HTTPUpdate with redirect support.
// -----------------------------------------

static String  _otaStatus   = "idle";
static int     _otaProgress = 0;
static bool    _otaPending  = false;
static String  _otaPendingUrl;

String otaGetStatus()   { return _otaStatus; }
int    otaGetProgress()  { return _otaProgress; }

void otaSetPending(const String& url) {
  _otaPendingUrl = url;
  _otaPending = true;
  _otaStatus = "pending";
}

bool otaIsPending() { return _otaPending; }

// ── Semver comparison ────────────────────────
// Returns >0 if a is newer, 0 if equal, <0 if b is newer
static int semverCompare(const char* a, const char* b) {
  int aMaj = 0, aMin = 0, aPat = 0;
  int bMaj = 0, bMin = 0, bPat = 0;
  sscanf(a, "%d.%d.%d", &aMaj, &aMin, &aPat);
  sscanf(b, "%d.%d.%d", &bMaj, &bMin, &bPat);
  if (aMaj != bMaj) return aMaj - bMaj;
  if (aMin != bMin) return aMin - bMin;
  return aPat - bPat;
}

// ── Check GitHub for update ──────────────────
bool otaCheckForUpdate(String& newVersion, String& downloadUrl) {
  _otaStatus = "checking";
  _otaProgress = 0;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.github.com/repos/";
  url += GITHUB_OWNER;
  url += "/";
  url += GITHUB_REPO;
  url += "/releases/latest";

  http.begin(client, url);
  http.addHeader("User-Agent", "sandysoil-8z");
  http.setTimeout(10000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("[OTA-GH] GitHub API returned %d\n", code);
    _otaStatus = "idle";
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  // Parse only what we need
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[OTA-GH] JSON parse error: %s\n", err.c_str());
    _otaStatus = "idle";
    return false;
  }

  const char* tagName = doc["tag_name"];
  if (!tagName) {
    Serial.println("[OTA-GH] No tag_name in response");
    _otaStatus = "idle";
    return false;
  }

  // Strip leading 'v' if present
  const char* remoteVer = tagName;
  if (remoteVer[0] == 'v' || remoteVer[0] == 'V') remoteVer++;

  Serial.printf("[OTA-GH] Current: %s  Remote: %s\n", FW_VERSION, remoteVer);

  if (semverCompare(remoteVer, FW_VERSION) <= 0) {
    Serial.println("[OTA-GH] Already up to date");
    _otaStatus = "idle";
    newVersion = remoteVer;
    return false;
  }

  // Find firmware.bin asset
  JsonArray assets = doc["assets"];
  for (JsonObject asset : assets) {
    const char* name = asset["name"];
    if (name && strcmp(name, GITHUB_FW_ASSET) == 0) {
      const char* dlUrl = asset["browser_download_url"];
      if (dlUrl) {
        newVersion = remoteVer;
        downloadUrl = dlUrl;
        Serial.printf("[OTA-GH] Update available: %s -> %s\n", FW_VERSION, remoteVer);
        Serial.printf("[OTA-GH] URL: %s\n", dlUrl);
        _otaStatus = "idle";
        return true;
      }
    }
  }

  Serial.println("[OTA-GH] No firmware.bin asset found in release");
  _otaStatus = "idle";
  return false;
}

// ── Download and flash ───────────────────────
bool otaStartUpdate(const String& downloadUrl) {
  _otaStatus = "downloading";
  _otaProgress = 0;

  Serial.printf("[OTA-GH] Downloading: %s\n", downloadUrl.c_str());

  WiFiClientSecure client;
  client.setInsecure();

  // Follow GitHub's 302 redirect to objects.githubusercontent.com
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  httpUpdate.onStart([]() {
    _otaStatus = "flashing";
    Serial.println("[OTA-GH] Flashing...");
  });

  httpUpdate.onProgress([](int cur, int total) {
    if (total > 0) {
      _otaProgress = (cur * 100) / total;
    }
    Serial.printf("[OTA-GH] %d%%\r", _otaProgress);
  });

  httpUpdate.onEnd([]() {
    _otaStatus = "done";
    _otaProgress = 100;
    Serial.println("[OTA-GH] Flash complete — rebooting");
  });

  httpUpdate.onError([](int err) {
    _otaStatus = "error: " + httpUpdate.getLastErrorString();
    Serial.printf("[OTA-GH] Error %d: %s\n", err, httpUpdate.getLastErrorString().c_str());
  });

  HTTPUpdateResult result = httpUpdate.update(client, downloadUrl);

  switch (result) {
    case HTTP_UPDATE_OK:
      delay(500);
      ESP.restart();
      return true;  // never reached

    case HTTP_UPDATE_FAILED:
      Serial.printf("[OTA-GH] Failed: %s\n", httpUpdate.getLastErrorString().c_str());
      return false;

    case HTTP_UPDATE_NO_UPDATES:
      _otaStatus = "idle";
      return false;
  }

  return false;
}

// ── Deferred update (called from loop) ───────
void otaRunPending() {
  if (!_otaPending) return;
  _otaPending = false;
  String url = _otaPendingUrl;
  _otaPendingUrl = "";
  otaStartUpdate(url);
}
