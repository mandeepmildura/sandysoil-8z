# Sandy Soil 8Z — Irrigation Controller

8-zone automated irrigation controller for farm use.
Built on the **KC868-A8v3** (ESP32-S3, 8 MB flash, 8 MB PSRAM) with web dashboard, MQTT remote control, Home Assistant integration, OTA updates, and cloud logging.

---

## Overview

Sandy Soil 8Z is a standalone irrigation controller that runs on a KinCony KC868-A8v3 board. It drives 8 relay-controlled solenoid valves via a PCF8575 I2C expander, monitors supply line pressure through an analog sensor, and provides multiple interfaces for control and monitoring:

- **Web dashboard** — live zone control, pressure display, and settings from any browser on your network
- **MQTT** — JSON commands from Home Assistant, Node-RED, or any MQTT client
- **Schedules** — day-of-week + time-based automatic watering (NTP-synced AEST)
- **Internet OTA** — firmware updates from GitHub Releases via dashboard, MQTT, or auto-check on boot
- **Cloud logging** — optional Supabase integration for historical data

On first boot the device starts a WiFi hotspot for initial configuration. After that, all settings (WiFi, MQTT, pressure, Supabase) are editable from the web dashboard's Settings panel — no re-flashing needed.

---

## Hardware

| Component | Detail |
|-----------|--------|
| Board | KC868-A8v3 — ESP32-S3, 8 MB flash, 8 MB PSRAM |
| Relay expander | PCF8575 (I2C addr 0x22) — active-LOW, channels 0–7 |
| Pressure sensor | 0–5V ratiometric on GPIO1 (ADC1_CH0) |
| Display | SSD1306 0.96" OLED (I2C addr 0x3C, SDA=GPIO8, SCL=GPIO18) |
| Connectivity | WiFi 2.4 GHz, HiveMQ Cloud MQTT over TLS (port 8883) |

---

## Features

- **8 independent zones** — relay control with manual + scheduled modes
- **Web dashboard** at `http://<board-ip>/` — zone on/off, duration, pressure, settings
- **WiFi setup portal** — hotspot on first boot for zero-config provisioning
- **WiFi credential editing** — change SSID/password from the Settings panel at any time
- **Internet OTA updates** — check GitHub Releases and update from the dashboard or remotely via MQTT
- **Local OTA** — browser upload at `/update` or VS Code/PlatformIO over-the-air
- **MQTT control** — JSON commands, status publish, Home Assistant auto-discovery
- **Remote OTA via MQTT** — trigger firmware updates from anywhere during support sessions
- **Schedule-based watering** — per-zone day-of-week + time, NTP-synced (AEST UTC+10)
- **Pressure monitoring** — live PSI display, low-pressure MQTT alerts when zones are running
- **Pressure simulator** — set fake PSI values from the dashboard for testing
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
│   ├── ota_github.h / ota_github.cpp GitHub Releases OTA checker + downloader
│   ├── pressure.h / pressure.cpp     ADC pressure sensor + simulator
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
http://192.168.x.x/update    Firmware upload page
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

There are four ways to update the firmware:

### Option A — Internet OTA via GitHub Releases (recommended)

The device can check GitHub for new releases, download the firmware, and flash itself — no physical access needed.

**From the dashboard (local network):**

1. Open `http://<board-ip>/` in a browser
2. Click **"Check for Update"** in the footer
3. A modal shows the current and latest version
4. If an update is available, click **"Install Update"**
5. Progress bar shows download and flash progress
6. Device reboots automatically with the new firmware

**Remotely via MQTT (support sessions):**

Publish a JSON command to trigger OTA from anywhere in the world — ideal for customer support:

| Topic | Payload | Description |
|-------|---------|-------------|
| `farm/irrigation1/cmd/ota` | `{"action":"check"}` | Check for updates (result published to `farm/irrigation1/ota/status`) |
| `farm/irrigation1/cmd/ota` | `{"action":"update"}` | Check and install the latest firmware |

The device publishes progress to `farm/irrigation1/ota/status`:
```json
{"status":"downloading","version":"2.3.1"}
{"status":"up_to_date","version":"2.3.0"}
{"update_available":true,"current":"2.3.0","latest":"2.3.1"}
```

**Auto-check on boot:**

Every time the device boots with WiFi connected, it checks GitHub for a newer release and logs the result. It does **not** auto-flash — you must trigger the update via the dashboard or MQTT.

**Creating a new release:**

1. Bump `FW_VERSION` in `src/config.h` (e.g., `"2.4.0"`)
2. Build: `pio run`
3. Commit and push to GitHub
4. Create a GitHub Release with a tag matching the version (e.g., `v2.4.0`)
5. Attach `.pio/build/sandysoil-8z/firmware.bin` as a release asset named `firmware.bin`

Using the GitHub CLI:
```bash
gh release create v2.4.0 .pio/build/sandysoil-8z/firmware.bin \
  --title "v2.4.0 — Description" \
  --notes "Release notes here"
```

### Option B — Browser upload (local network)

1. Open `http://<board-ip>/update` in a browser
2. Build firmware in VS Code: PlatformIO **Build** (checkmark icon)
3. The binary is at `.pio/build/sandysoil-8z/firmware.bin`
4. Select the file and click **Upload & Flash**
5. The board reboots automatically with the new firmware

### Option C — VS Code / PlatformIO OTA (local network)

1. Note the device IP from the Serial Monitor
2. Edit `platformio.ini` — uncomment and set:
   ```ini
   upload_protocol = espota
   upload_port     = 192.168.x.x   ; your device IP
   upload_flags    = --auth=irrigation8z
   ```
3. Click the Upload arrow in VS Code — firmware uploads over WiFi

**OTA password:** `irrigation8z`

### Option D — USB cable

Plug in via USB and flash directly:
```bash
pio run --target upload --upload-port COM5
```

---

## Pressure Simulator

The dashboard includes a pressure simulator for testing without a physical sensor connected.

**To simulate pressure:**
1. Enter a PSI value in the input box next to "Supply Pressure"
2. Click **Sim** — the display shows the simulated value with a yellow **SIM** badge
3. All pressure-based logic (low-pressure alerts, MQTT status, Supabase logging) uses the simulated value

**To return to the real sensor:**
- Click **Real** — the badge disappears and the ADC sensor is used again

**Via API:**
```bash
# Set simulated pressure
curl -X POST http://<board-ip>/api/pressure/simulate \
  -H "Content-Type: application/json" \
  -d '{"psi": 45.0}'

# Clear simulation (return to real sensor)
curl -X POST http://<board-ip>/api/pressure/simulate \
  -H "Content-Type: application/json" \
  -d '{"clear": true}'
```

---

## REST API

All endpoints return JSON with CORS headers.

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/status` | Board info, WiFi RSSI, MQTT state, uptime, firmware version |
| GET | `/api/zones` | All zone states with remaining time |
| GET | `/api/pressure` | Current supply pressure PSI + simulated flag |
| GET | `/api/config` | Full configuration (WiFi, MQTT, pressure, Supabase) |
| POST | `/api/config` | Update configuration (JSON body, partial updates OK) |
| POST | `/api/zone/{n}/on` | Turn zone on — `{"duration": 10}` (minutes) |
| POST | `/api/zone/{n}/off` | Turn zone off |
| POST | `/api/zones/off` | All zones off |
| POST | `/api/pressure/simulate` | Set simulated PSI — `{"psi": 45.0}` or clear — `{"clear": true}` |
| GET | `/api/ota/check` | Check GitHub for firmware updates |
| POST | `/api/ota/update` | Start OTA update — `{"url": "https://...firmware.bin"}` |
| GET | `/api/ota/status` | OTA progress — `{"status": "downloading", "progress": 42}` |
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
| `farm/irrigation1/cmd/ota` | Subscribe | Trigger OTA check or update |
| `farm/irrigation1/ota/status` | Publish | OTA progress and result |
| `farm/irrigation1/alert` | Publish | Pressure or error alerts |

### Zone command examples

```json
{"cmd": "on", "duration": 20}
{"cmd": "off"}
{"cmd": "auto"}
```

Plain string payloads also accepted: `on`, `off`, `auto`.

### OTA command examples

```json
{"action": "check"}
{"action": "update"}
```

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
| `mqtt.cpp` | PubSubClient TLS, command parsing, HA discovery, OTA commands |
| `api.cpp` | ESPAsyncWebServer REST endpoints + OTA upload handler |
| `webui.h` | PROGMEM HTML dashboard + OTA upload page |
| `ota_github.cpp` | GitHub Releases API check, semver compare, HTTPUpdate download |
| `pressure.cpp` | ADC multi-sample read + voltage-to-PSI conversion + simulator |
| `display.cpp` | SSD1306 OLED rotating status screens |
| `supabase.cpp` | HTTPS POST telemetry to Supabase REST API |

### Boot sequence

```
1. storageInit → storageLoadConfig (if no config → setup portal)
2. storageLoadZones → zonesInit (Wire/I2C + PCF8575)
3. I2C bus scan (diagnostic)
4. displayInit → displayBoot
5. pressureInit (ADC)
6. wifiConnect (or fail → display error)
7. NTP sync (AEST UTC+10)
8. HTTP server (apiInit + webuiInit + server.begin)
9. ArduinoOTA (password: irrigation8z)
10. mqttInit (TLS to HiveMQ, subscribe to commands)
11. supabaseInit
12. OTA auto-check (checks GitHub, logs result, does not auto-flash)
```

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

### GitHub OTA flow

```
Dashboard "Check for Update"
  └─> GET /api/ota/check
        └─> otaCheckForUpdate()
              ├─ HTTPS GET api.github.com/repos/.../releases/latest
              ├─ Parse tag_name, compare semver with FW_VERSION
              └─ Find "firmware.bin" asset → return download URL

Dashboard "Install Update"
  └─> POST /api/ota/update {url: "..."}
        └─> otaSetPending(url)  ← sets flag, returns immediately

Main loop()
  └─> otaRunPending()
        └─> otaStartUpdate(url)
              ├─ HTTPUpdate with redirect following (GitHub 302)
              ├─ Progress callbacks update status/percentage
              └─> ESP.restart() on success
```

MQTT follows the same flow — `cmd/ota` with `{"action":"update"}` calls `otaCheckForUpdate()` then `otaSetPending()`.

---

## Security Notes

- `WiFiClientSecure.setInsecure()` is used for MQTT TLS and GitHub API — add certificate pinning for production
- Web UI and OTA are served over plain HTTP — use only on trusted local networks
- `secrets.h` is gitignored and not committed
- ArduinoOTA is password-protected (`irrigation8z`)
- The OTA MQTT command has no authentication beyond MQTT broker credentials — ensure your MQTT broker is secured

---

## Related Repos

| Repo | Description |
|------|-------------|
| [sandysoil-8z](https://github.com/mandeepmildura/sandysoil-8z) | **This repo** — KC868-A8v3 irrigation controller (8 zones) |
| [farmcontrol-filter](https://github.com/mandeepmildura/farmcontrol-filter) | ALR-V13 filtration controller (auto-backwash) |
| [farmcontrol-app](https://github.com/mandeepmildura/farmcontrol-app) | Web dashboard + MQTT-Supabase bridge (Vercel/Railway) |

---

## Partition Layout

`default_8MB.csv` (8 MB flash):

| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20 KB | WiFi / MQTT / zone config |
| otadata | 8 KB | OTA boot selection |
| app0 | ~3.3 MB | Running firmware |
| app1 | ~3.3 MB | OTA staging |
| spiffs | ~1.5 MB | Reserved |
