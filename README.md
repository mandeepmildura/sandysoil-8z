# Sandy Soil 8Z — Irrigation Controller

8-zone automated irrigation controller for farm use.
Built on the **KC868-A8v3** (ESP32-S3, 16 MB flash) with web dashboard, MQTT remote control, Home Assistant integration, OTA updates, and cloud logging.

---

## Overview

Sandy Soil 8Z is a standalone irrigation controller that runs on a KinCony KC868-A8v3 board. It drives 8 relay-controlled solenoid valves via a PCF8575 I2C expander, monitors supply line pressure through an analog sensor, and provides multiple interfaces for control and monitoring:

- **Web dashboard** — live zone control, pressure display, and settings from any browser on your network
- **MQTT** — JSON commands from Home Assistant, Node-RED, or any MQTT client
- **Schedules** — day-of-week + time-based automatic watering (NTP-synced AEST)
- **Cloud logging** — optional Supabase integration for historical data

On first boot the device starts a WiFi hotspot for initial configuration. After that, all settings (WiFi, MQTT, pressure, Supabase) are editable from the web dashboard's Settings panel — no re-flashing needed.

---

## Hardware

| Component | Detail |
|-----------|--------|
| Board | KC868-A8v3 — ESP32-S3, 16 MB flash, PSRAM |
| Relay expander | PCF8575 (I2C addr 0x21) — active-LOW, channels 0–7 |
| Pressure sensor | 0–5V ratiometric on GPIO1 (ADC1_CH0) |
| Display | SSD1306 0.96" OLED (I2C addr 0x3C, SDA=GPIO8, SCL=GPIO18) |
| Connectivity | WiFi 2.4 GHz, HiveMQ Cloud MQTT over TLS (port 8883) |

---

## Features

- **8 independent zones** — relay control with manual + scheduled modes
- **Web dashboard** at `http://<board-ip>/` — zone on/off, duration, pressure, settings
- **WiFi setup portal** — hotspot on first boot for zero-config provisioning
- **WiFi credential editing** — change SSID/password from the Settings panel at any time
- **OTA firmware updates** — browser upload at `/update` or VS Code/PlatformIO over-the-air
- **MQTT control** — JSON commands, status publish, Home Assistant auto-discovery
- **Schedule-based watering** — per-zone day-of-week + time, NTP-synced (AEST UTC+10)
- **Pressure monitoring** — low-pressure MQTT alerts when zones are running
- **Supabase cloud logging** — periodic telemetry POST (optional)
- **OLED display** — rotating status screens (PSI, active zones, WiFi/MQTT state)

---

## Project Structure

```
sandysoil-8z/
├── platformio.ini                    Build config (VS Code + PlatformIO)
├── README.md
├── src/
│   ├── FarmControl_Irrigation.ino    Main setup/loop
│   ├── config.h                      Pin defs, constants, Config/Zone/Schedule structs
│   ├── secrets.h                     Placeholder (credentials via web UI, not hardcoded)
│   ├── secrets.h.example             Reference for credential defines
│   ├── storage.h / storage.cpp       NVS persistence (config + zone names)
│   ├── wifi_setup.h / wifi_setup.cpp WiFi connect + first-boot setup portal
│   ├── zones.h / zones.cpp           PCF8575 relay driver, timers, schedules
│   ├── mqtt.h / mqtt.cpp             MQTT client, HA discovery, command handling
│   ├── api.h / api.cpp               REST API endpoints (ESPAsyncWebServer)
│   ├── webui.h                       Dashboard HTML + OTA upload page (PROGMEM)
│   ├── pressure.h / pressure.cpp     ADC pressure sensor
│   ├── display.h / display.cpp       SSD1306 OLED rendering
│   └── supabase.h / supabase.cpp     Cloud logging via REST
```

---

## Setup — VS Code + PlatformIO

### 1. Install tools

1. Install [VS Code](https://code.visualstudio.com/)
2. Open VS Code and install the **PlatformIO IDE** extension (search "PlatformIO IDE" in the Extensions panel)
3. Wait for PlatformIO to finish installing its core tools (progress shown in the bottom status bar)

### 2. Clone and open the project

```bash
git clone https://github.com/mandeepmildura/sandysoil-8z.git
```

In VS Code: **File > Open Folder** > select the `sandysoil-8z` directory.
PlatformIO detects `platformio.ini` automatically and installs dependencies.

### 3. Connect the board

Plug the KC868-A8v3 into your computer via USB. Check which COM port it appears on:
- **Windows:** Device Manager > Ports (COM & LPT)
- **Linux/Mac:** `ls /dev/ttyUSB*` or `ls /dev/ttyACM*`

The default in `platformio.ini` is `COM5`. Update `upload_port` if yours differs.

### 4. Build and flash

**Using VS Code toolbar** (bottom of screen):
- **checkmark** — Build (compile only)
- **arrow** — Upload (compile + flash)
- **plug** — Serial Monitor (115200 baud)

**Using terminal:**
```bash
pio run --target upload --upload-port COM5
```

### 5. First boot

After flashing, the board starts a WiFi hotspot:

```
SSID: FarmControl-Irrigation-Setup
```

1. Connect your phone/laptop to that network
2. A captive portal opens automatically (or browse to `http://192.168.4.1`)
3. Enter your WiFi SSID, password, and optionally MQTT broker details
4. Click **Save & Connect** — the board reboots and joins your network

### 6. Access the dashboard

Find the board's IP in the Serial Monitor output, then open:

```
http://192.168.x.x/          Dashboard (zone control, pressure, settings)
http://192.168.x.x/update    Firmware update page
```

---

## WiFi Configuration

### First boot / WiFi failure

If no WiFi credentials are saved (or connection fails), the device starts a setup hotspot. Connect and configure via the captive portal at `http://192.168.4.1`.

### Changing WiFi credentials later

1. Open the dashboard at `http://<board-ip>/`
2. Click **Settings**
3. Update the WiFi SSID and password fields
4. Click **Save**
5. Restart the board from the dashboard footer (or power cycle)

All settings are stored in NVS (non-volatile storage) and persist across reboots.

---

## OTA Firmware Updates

### Option A — Browser upload

1. Open `http://<board-ip>/update` in a browser
2. Build firmware in VS Code: PlatformIO **Build** (checkmark icon)
3. The binary is at `.pio/build/sandysoil-8z/firmware.bin`
4. Select the file and click **Upload & Flash**
5. The board reboots automatically with the new firmware

### Option B — VS Code / PlatformIO OTA

1. Note the device IP from the Serial Monitor
2. Edit `platformio.ini` — uncomment and set:
   ```ini
   upload_protocol = espota
   upload_port     = 192.168.x.x   ; your device IP
   upload_flags    = --auth=irrigation8z
   ```
3. Click the Upload arrow in VS Code — firmware uploads over WiFi

**OTA password:** `irrigation8z`

---

## REST API

All endpoints return JSON with CORS headers.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Board info, WiFi RSSI, MQTT state, uptime, firmware version |
| GET | `/api/zones` | All zone states with remaining time |
| GET | `/api/pressure` | Current supply pressure PSI |
| GET | `/api/config` | Full configuration (WiFi, MQTT, pressure, Supabase) |
| POST | `/api/config` | Update configuration (JSON body, partial updates OK) |
| POST | `/api/zone/{n}/on` | Turn zone on — `{"duration": 10}` (minutes) |
| POST | `/api/zone/{n}/off` | Turn zone off |
| POST | `/api/zones/off` | All zones off |
| POST | `/api/update` | Upload firmware binary (multipart form) |
| POST | `/api/restart` | Reboot the board |

---

## MQTT Topics

| Topic | Direction | Description |
|-------|-----------|-------------|
| `farm/irrigation1/status` | Publish | Full status JSON (all zones, PSI, uptime) |
| `farm/irrigation1/zone/N/state` | Publish | Single zone state |
| `farm/irrigation1/zone/N/cmd` | Subscribe | Zone command |
| `farm/irrigation1/all/off` | Subscribe | Turn all zones off |
| `farm/irrigation1/cmd/sync` | Subscribe | Request full state dump |
| `farm/irrigation1/alert` | Publish | Pressure or error alerts |

### Zone command examples

```json
{"cmd": "on", "duration": 20}
{"cmd": "off"}
{"cmd": "auto"}
```

Plain string payloads also accepted: `on`, `off`, `auto`.

---

## Home Assistant

The device publishes MQTT auto-discovery messages on connect. Each zone appears as a **switch** entity and supply pressure appears as a **sensor** entity. No manual YAML needed — just ensure your MQTT integration uses the `homeassistant` discovery prefix.

---

## Architecture

| Module | Responsibility |
|--------|---------------|
| `FarmControl_Irrigation.ino` | Main orchestration — setup, loop, timer ticks |
| `config.h` | Pin definitions, constants, Config/Zone/Schedule structs |
| `storage.cpp` | NVS read/write for all config and zone names |
| `wifi_setup.cpp` | WiFi STA connect + AP setup portal (first boot) |
| `zones.cpp` | PCF8575 I2C relay driver, zone state machine, timers, schedules |
| `mqtt.cpp` | PubSubClient TLS, command parsing, HA discovery, state publish |
| `api.cpp` | ESPAsyncWebServer REST endpoints + OTA upload handler |
| `webui.h` | PROGMEM HTML dashboard + OTA upload page |
| `pressure.cpp` | ADC multi-sample read + voltage-to-PSI conversion |
| `display.cpp` | SSD1306 OLED rotating status screens |
| `supabase.cpp` | HTTPS POST telemetry to Supabase REST API |

### Zone state machine

```
            ┌─────────┐
  cmd:on ──>│ MANUAL  │──── timer expires / cmd:off ──>┐
            └─────────┘                                 │
                                                        v
            ┌──────────┐                           ┌─────────┐
  schedule >│ SCHEDULE │──── duration expires ─────>│   OFF   │
            └──────────┘                           └─────────┘
```

Manual overrides schedule. `cmd:auto` cancels manual and returns to schedule mode.

### Dirty mask

`zones.cpp` maintains a `uint8_t _dirtyMask` (one bit per zone). When a timer expires, the corresponding bit is set. The main loop checks this every iteration, publishes MQTT state for changed zones, then clears the mask.

---

## Security Notes

- `WiFiClientSecure.setInsecure()` is used for MQTT TLS — add certificate pinning for production
- Web UI and OTA are served over plain HTTP — use only on trusted local networks
- `secrets.h` is gitignored and not committed
- ArduinoOTA is password-protected (`irrigation8z`)

---

## Partition Layout

`default_16MB.csv` (16 MB flash):

| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20 KB | WiFi / MQTT / zone config |
| otadata | 8 KB | OTA boot selection |
| app0 | ~6.5 MB | Running firmware |
| app1 | ~6.5 MB | OTA staging |
| spiffs | ~3 MB | Reserved |
