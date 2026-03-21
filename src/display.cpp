#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ─────────────────────────────────────────────
//  display.cpp
//  SSD1306 OLED — rotates between screens:
//  • Status (supply PSI, zone count, WiFi/MQTT)
//  • Active zones list
// ─────────────────────────────────────────────

static Adafruit_SSD1306 oled(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);
static bool _ready = false;

bool displayInit() {
  // Wire already begun by zones_init (pcf_init)
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("[Display] SSD1306 not found");
    return false;
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  _ready = true;
  Serial.println("[Display] OK");
  return true;
}

void displayBoot(const char* message) {
  if (!_ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("FarmControl");
  oled.println("Irrigation v" FW_VERSION);
  oled.drawLine(0, 18, 127, 18, SSD1306_WHITE);
  oled.setCursor(0, 22);
  oled.println(message);
  oled.display();
}

void displayHotspot() {
  if (!_ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Setup Mode");
  oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
  oled.setCursor(0, 14);
  oled.println("WiFi: " HOTSPOT_SSID);
  oled.setCursor(0, 26);
  oled.println("Open: " HOTSPOT_IP);
  oled.display();
}

void displayWiFiConnecting(const char* ssid) {
  if (!_ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("Connecting WiFi...");
  oled.setCursor(0, 14);
  oled.println(ssid);
  oled.display();
}

void displayWiFiConnected(const char* ip) {
  if (!_ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("WiFi Connected");
  oled.setCursor(0, 14);
  oled.println(ip);
  oled.display();
}

void displayWiFiFailed() {
  if (!_ready) return;
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println("WiFi FAILED");
  oled.setCursor(0, 14);
  oled.println("Check credentials");
  oled.display();
}

void displayLoop(Zone zones[MAX_ZONES], float supplyPsi,
                 bool mqttConnected, bool wifiConnected) {
  if (!_ready) return;

  static uint32_t lastUpdate  = 0;
  static uint8_t  screen      = 0;
  static uint32_t lastFlip    = 0;

  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();

  // Flip between screens every 4s
  if (millis() - lastFlip > 4000) {
    lastFlip = millis();
    screen   = !screen;
  }

  oled.clearDisplay();
  oled.setTextSize(1);

  if (screen == 0) {
    // ── Screen 0: status ──
    oled.setCursor(0, 0);
    oled.printf("PSI: %.1f", supplyPsi);
    oled.setCursor(70, 0);
    oled.print(wifiConnected  ? "W:ok" : "W:--");
    oled.setCursor(100, 0);
    oled.print(mqttConnected  ? "M:ok" : "M:--");
    oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    int activeCount = 0;
    for (int i = 0; i < MAX_ZONES; i++) {
      if (zones[i].manualOn || zones[i].scheduleActive) activeCount++;
    }
    oled.setCursor(0, 14);
    oled.printf("%d zone%s active", activeCount, activeCount == 1 ? "" : "s");
    oled.setCursor(0, 26);
    oled.print(cfg.board_name);
  } else {
    // ── Screen 1: active zones ──
    oled.setCursor(0, 0);
    oled.println("Active Zones:");
    oled.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    int row = 0;
    for (int i = 0; i < MAX_ZONES && row < 4; i++) {
      if (zones[i].manualOn || zones[i].scheduleActive) {
        oled.setCursor(0, 14 + row * 12);
        const char* mode = zones[i].manualOn ? "M" : "S";
        oled.printf("Z%d %s %s", i + 1, zones[i].name, mode);
        row++;
      }
    }
    if (row == 0) {
      oled.setCursor(0, 14);
      oled.println("All zones off");
    }
  }

  oled.display();
}
