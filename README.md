# Sandy Soil 8Z — Irrigation Controller

8-zone automated irrigation controller for farm use.
Built on the **KC868-A8v3** (ESP32-S3) with MQTT remote control, Home Assistant integration, and cloud logging.

---

## Hardware

| Component | Detail |
|-----------|--------|
| Microcontroller | KC868-A8v3 — ESP32-S3, 8 MB flash |
| Relay expander | PCF8575 (I2C, 16-bit) — active-LOW, channels 0–7 |
| Pressure sensor | Analog input via ADC |
| Display | Local LCD/OLED |
| Connectivity | Wi-Fi (2.4 GHz), HiveMQ Cloud MQTT over TLS |

---

## Features

- **8 independent irrigation zones** via relay control
- **Schedule-based watering** — day-of-week + time, NTP-synced (AEST UTC+10)
- **Manual override** — on/off with optional duration timer
- **Water pressure monitoring** — alerts via MQTT when supply drops below 5 PSI
- **MQTT control** — JSON commands from any client (Home Assistant, Node-RED, custom apps)
- **Home Assistant auto-discovery** — zones appear as switches, pressure as sensor
- **Supabase cloud logging** — historical pressure and zone activity
- **Web config UI** — update Wi-Fi and MQTT credentials via browser (no re-flash needed)
- **OTA firmware updates** — upload new firmware via browser or VS Code

---

## Project Structure

```
sandysoil-8z/
├── platformio.ini              ← VS Code / PlatformIO build config
├── src/
│   ├── FarmControl_Irrigation.ino  ← Main setup/loop
│   ├── zones.h / zones.cpp         ← Relay control, timers, schedules
│   ├── mqtt.cpp                    ← MQTT client, HA discovery, commands
│   ├── webui.h / webui.cpp         ← Web config page + OTA updates
│   │
│   │   (gitignored — create from examples below)
│   ├── config.h                    ← Pin definitions, intervals, constants
│   ├── secrets.h                   ← Wi-Fi / MQTT / Supabase credentials
│   ├── storage.h / storage.cpp     ← NVS read/write
│   ├── wifi_setup.h / .cpp         ← Wi-Fi init and reconnect
│   ├── pressure.h / .cpp           ← ADC pressure reading
│   ├── display.h / .cpp            ← LCD/OLED rendering
│   └── supabase.h / .cpp           ← Cloud logging
```

---

## Setup — VS Code + PlatformIO

### 1. Install tools

- [VS Code](https://code.visualstudio.com/)
- PlatformIO extension (search "PlatformIO IDE" in VS Code extensions)

### 2. Open the project

```
File → Open Folder → select the sandysoil-8z directory
```

PlatformIO detects `platformio.ini` automatically.

### 3. Create your local config files

These files are gitignored (contain credentials). Create them in `src/`:

**`src/config.h`**
```cpp
#pragma once
#define FW_VERSION        "1.0.0"
#define DEVICE_ID         "sandysoil8z"
#define DEVICE_NAME       "Sandy Soil 8Z"

// I2C pins (KC868-A8v3)
#define I2C_SDA           8
#define I2C_SCL           9
#define PCF8575_ADDR      0x20

// Zone limits
#define MAX_ZONES         8
#define MAX_SCHEDULES     4
#define MAX_RUN_MINUTES   240
#define DEFAULT_RUN_MIN   10

// Polling intervals (ms)
#define PRESSURE_INTERVAL_MS   10000   // 10 s
#define STATUS_INTERVAL_MS     30000   // 30 s
#define SUPABASE_INTERVAL_MS   60000   // 60 s
#define SCHEDULE_CHECK_MS       5000   //  5 s

// MQTT topics
#define MQTT_BASE         "farm/irrigation1"
#define MQTT_STATUS       MQTT_BASE "/status"
#define MQTT_ALERT        MQTT_BASE "/alert"
#define MQTT_ZONE_STATE   MQTT_BASE "/zone/%d/state"
#define MQTT_ZONE_CMD     MQTT_BASE "/zone/%d/cmd"
#define MQTT_DISCOVERY    "homeassistant"
```

**`src/secrets.h`**
```cpp
#pragma once
// Wi-Fi
#define DEFAULT_WIFI_SSID   "YourSSID"
#define DEFAULT_WIFI_PASS   "YourPassword"

// MQTT (HiveMQ Cloud example)
#define DEFAULT_MQTT_HOST   "xxxx.s1.eu.hivemq.cloud"
#define DEFAULT_MQTT_PORT   8883
#define DEFAULT_MQTT_USER   "username"
#define DEFAULT_MQTT_PASS   "password"

// Supabase
#define DEFAULT_SB_URL      "https://xxxx.supabase.co"
#define DEFAULT_SB_KEY      "your-anon-key"
```

> **Note:** Credentials saved here are only used on first boot. After that they are stored in device NVS and can be updated via the web config page without re-flashing.

### 4. Install dependencies

PlatformIO downloads all libraries automatically on first build.
Manual install if needed:
```
pio pkg install
```

### 5. Build and flash (USB cable)

```
pio run --target upload
```

Or use the **PlatformIO toolbar** at the bottom of VS Code:
- ✓ (Build) — compile only
- → (Upload) — compile and flash
- 🔌 (Serial Monitor) — open serial console at 115200 baud

---

## Wi-Fi Configuration via Web Page

### First boot / Wi-Fi failure

If Wi-Fi credentials are missing or wrong the device starts a hotspot:

```
SSID:     SandySoil-Setup
Password: irrigation
```

1. Connect your phone or laptop to that network
2. Open **http://192.168.4.1** in a browser
3. Enter your Wi-Fi SSID and password, plus MQTT broker details
4. Click **Save & Reboot** — the device restarts and connects

### Updating credentials when already connected

Browse to the device's local IP (printed in Serial Monitor on boot):

```
http://192.168.x.x/config
```

The config page shows current settings (passwords hidden). Fill in new values and save.

---

## OTA Firmware Updates

### Option A — Browser upload

1. Browse to **http://192.168.x.x/update**
2. Build the firmware in VS Code: **PlatformIO → Build**
3. Find the binary: `.pio/build/sandysoil-8z/firmware.bin`
4. Upload the file and click **Upload & Flash**

### Option B — VS Code / PlatformIO direct OTA

1. Find the device IP in the Serial Monitor
2. Edit `platformio.ini` — uncomment and set:
   ```ini
   upload_protocol = espota
   upload_port     = 192.168.x.x   ; ← your device IP
   upload_flags    = --auth=irrigation8z
   ```
3. Click → (Upload) in VS Code — firmware uploads over Wi-Fi

OTA password: **`irrigation8z`**

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
// Turn zone 3 on for 20 minutes
{ "cmd": "on", "duration": 20 }

// Turn off
{ "cmd": "off" }

// Return to automatic schedule
{ "cmd": "auto" }
```

Plain string payloads also accepted: `on`, `off`, `auto`.

---

## Home Assistant Integration

The device publishes MQTT auto-discovery messages on connect.
In Home Assistant:
- Each zone appears as a **switch** entity
- Supply pressure appears as a **pressure sensor** entity

No manual YAML configuration needed — just ensure your MQTT integration is set up and the discovery prefix is `homeassistant`.

---

## Zone State Machine

```
            ┌─────────┐
  cmd:on ──▶│ MANUAL  │──── timer expires / cmd:off ──▶┐
            └─────────┘                                  │
                                                         ▼
            ┌──────────┐                            ┌─────────┐
  schedule ▶│ SCHEDULE │──── duration expires ─────▶│   OFF   │
            └──────────┘                            └─────────┘
```

- Manual overrides schedule — schedule resumes when manual off
- `cmd:auto` cancels manual, returns to schedule mode

---

## Architecture Notes

| Module | Responsibility |
|--------|---------------|
| `zones.cpp` | PCF8575 I2C relay driver, zone state, timers, schedule evaluation |
| `mqtt.cpp` | PubSubClient wrapper, command parsing, HA discovery, state publish |
| `webui.cpp` | WebServer config page, HTTP OTA upload, ArduinoOTA handler |
| `storage.cpp` | ESP32 NVS (non-volatile) read/write for all credentials and zone config |
| `wifi_setup.cpp` | Wi-Fi init, reconnect loop |
| `pressure.cpp` | ADC read + PSI conversion |
| `supabase.cpp` | REST POST to Supabase for cloud logging |
| `display.cpp` | Local display rendering |
| `.ino` | Main orchestration loop — ties all modules together |

### Dirty mask

`zones.cpp` maintains a `uint8_t _dirtyMask` (bit per zone). When a timer expires the bit is set. The main loop checks this every iteration, publishes MQTT state for changed zones, then clears the mask. This avoids polling zone state on every MQTT tick.

---

## Security Notes

- `WiFiClientSecure.setInsecure()` is used for MQTT TLS — certificate pinning should be added for production use
- OTA and web config are password-protected but served over plain HTTP — only use on a trusted local network
- `secrets.h` and `config.h` are gitignored; never commit them

---

## Partition Layout

`default_8MB.csv` (8 MB flash):

| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20 KB | Wi-Fi / MQTT / zone credentials |
| otadata | 8 KB | OTA boot selection |
| app0 | ~3.4 MB | Running firmware |
| app1 | ~3.4 MB | OTA staging |
| spiffs | ~1 MB | Reserved |
