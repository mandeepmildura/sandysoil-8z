#pragma once

// -----------------------------------------
//  ota_github.h
//  Sandy Soil 8Z — Internet OTA Updates
//  Checks GitHub Releases for new firmware,
//  downloads and flashes via HTTPUpdate.
// -----------------------------------------

#include <Arduino.h>

// Check GitHub Releases API for a newer version
// Returns true if update available, fills newVersion and downloadUrl
bool otaCheckForUpdate(String& newVersion, String& downloadUrl);

// Download firmware from URL and flash (BLOCKING — takes 30-60s)
// Returns true on success (device will reboot)
bool otaStartUpdate(const String& downloadUrl);

// Status tracking for dashboard polling
String otaGetStatus();
int    otaGetProgress();

// Deferred update — set from API handler, executed from loop()
void   otaSetPending(const String& url);
bool   otaIsPending();
void   otaRunPending();
