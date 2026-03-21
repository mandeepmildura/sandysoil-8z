# Sandy Soil Automation

Farm IoT platform by Sandy Soil Automations (Mildura). Two ESP32 devices, cloud dashboard, Home Assistant integration.

## Quick Start

Tell me what you want to work on:
- "dual MQTT for HA" — connect devices to local Mosquitto
- "release filter v1.3.0" — tag + upload firmware
- "update dashboard" — modify cloud UI at sandysoils.pages.dev
- "add sensor to irrigation" — use unused hardware (DI, AI-2, 1-Wire, RS485)
- "check supabase" — verify data flow through bridge

## Repos

| Repo | What | Deploy |
|------|------|--------|
| [sandysoil-8z](https://github.com/mandeepmildura/sandysoil-8z) | Irrigation firmware (8 zones) | PlatformIO → COM5 |
| [farmcontrol-filter](https://github.com/mandeepmildura/farmcontrol-filter) | Filter firmware (pressure + backwash) | PlatformIO → COM6 |
| [farmcontrol-app](https://github.com/mandeepmildura/farmcontrol-app) | Cloud dashboard + bridge | Cloudflare Pages (auto on push) |

## Devices

### Irrigation Controller — KC868-A8v3
- ESP32-S3, 8 relays (PCF8575), supply pressure (AI-1), OLED
- Firmware: v2.3.1 | MQTT: `farm/irrigation1`
- HA Discovery: 8 zone switches + supply PSI + firmware + RSSI = 11 entities
- **Unused:** 8x DI, AI-2, Ethernet, RS485, RF433, SD card, RTC, 1-Wire, NTC

### Filter Station — ALR-V13
- ESP32-S3, 1 relay, inlet pressure (AI-1), outlet pressure (AI-2), OLED
- Firmware: v1.2.0 | MQTT: `farm/filter1`
- HA Discovery: 3 pressure + backwash switch + state + fault = 6 entities
- **Unused:** LoRa SX1278, A3, A4, D1, 1-Wire, I2C expansion

## Cloud Stack

| Service | Purpose | URL/Host |
|---------|---------|----------|
| Cloudflare Pages | Dashboard | sandysoils.pages.dev |
| Railway | MQTT↔Supabase bridge | auto-deployed |
| HiveMQ Cloud | MQTT broker TLS:8883 | eb65c13ec...s1.eu.hivemq.cloud |
| Supabase | Database + Realtime | project dashboard |

## Local (Home Assistant)

| Service | Address |
|---------|---------|
| HA | nnpmmcmlamrrnjted3667u31rbnfxuy3.ui.nabu.casa |
| Mosquitto | 192.168.1.30:1883 |
| Node-RED | HA add-on (brokers: "Hive", "Local MQTT (192.168.1.30)") |

## MQTT Topics

**Irrigation:** `farm/irrigation1/status`, `.../zone/N/state`, `.../zone/N/cmd`, `.../all/off`, `.../cmd/sync`, `.../cmd/ota`, `.../alert`

**Filter:** `farm/filter1/status`, `.../pressure`, `.../backwash/state`, `.../backwash/start`, `.../backwash/stop`, `.../backwash/reset`, `.../cmd/ota`, `.../alerts`

## Data Flow

```
ESP32 → HiveMQ (TLS 8883) → Railway bridge → Supabase → Cloudflare dashboard
                                                ↕ Realtime
```

## Rules

- Customers are farmers — UI must use plain language, large text
- Python path: `/c/Users/msgil/AppData/Local/Programs/Python/Python311/python`
- Use Python scripts (not heredocs/sed) for C++ file modifications on Windows
- Never use wildcard MQTT bridges between brokers — causes relay loops
- Firmware hosted on GitHub Releases for OTA updates

## Pending

1. **Dual MQTT for HA** — add local Mosquitto connection to both firmwares
2. **Release filter v1.3.0** — tag + firmware.bin upload
3. **GitHub Project board** — https://github.com/users/mandeepmildura/projects/1
