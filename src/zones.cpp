#include "zones.h"

// Shadow register — tracks current state of all 16 PCF8575 ports
// P0-P7 = relays (bit=1 means relay OFF, bit=0 means relay ON — active LOW)
// P8-P15 = digital inputs (read only)
static uint16_t _pcfState = 0xFFFF;  // all HIGH on startup = all relays OFF

// Dirty bitmask — bits 0-7 correspond to zones 0-7
// Set when a zone turns off due to timer expiry so main loop can publish MQTT
static uint8_t _dirtyMask = 0;

// ── PCF8575 low-level I2C ─────────────────────────────────────

static void pcf_write(uint16_t val) {
  Wire.beginTransmission(PCF8575_ADDR);
  Wire.write(val & 0xFF);         // low byte: P0-P7 (relays)
  Wire.write((val >> 8) & 0xFF);  // high byte: P8-P15 (inputs)
  Wire.endTransmission();
  _pcfState = val;
}

static uint16_t pcf_read() {
  Wire.requestFrom((uint8_t)PCF8575_ADDR, (uint8_t)2);
  uint16_t val = 0;
  if (Wire.available()) val  = Wire.read();
  if (Wire.available()) val |= (Wire.read() << 8);
  return val;
}

void pcf_init() {
  Wire.begin(I2C_SDA, I2C_SCL);
  _pcfState = 0xFFFF;   // all relays OFF (active LOW — HIGH = off)
  pcf_write(_pcfState);
  Serial.println("[PCF] PCF8575 initialised — all relays OFF");
}

void pcf_relay_set(int idx, bool on) {
  if (idx < 0 || idx >= 8) return;
  // Relay is active LOW: on=true → bit=0, on=false → bit=1
  if (on) {
    _pcfState &= ~(1 << idx);   // clear bit
  } else {
    _pcfState |=  (1 << idx);   // set bit
  }
  pcf_write(_pcfState);
}

bool pcf_relay_get(int idx) {
  if (idx < 0 || idx >= 8) return false;
  return !(_pcfState & (1 << idx));  // LOW = on
}

uint16_t pcf_read_all() {
  return pcf_read();
}

// ── Zone logic ────────────────────────────────────────────────

void zones_init(Zone zones[MAX_ZONES]) {
  pcf_init();
  for (int i = 0; i < MAX_ZONES; i++) {
    zones[i].manualOn       = false;
    zones[i].scheduleActive = false;
  }
  Serial.println("[Zones] Initialised — all relays OFF");
}

void zones_loop(Zone zones[MAX_ZONES]) {
  uint32_t now = millis();

  for (int i = 0; i < MAX_ZONES; i++) {
    if (zones[i].manualOn && zones[i].manualDurMin > 0) {
      uint32_t elapsed = now - zones[i].manualOnAt;
      if (elapsed >= (uint32_t)zones[i].manualDurMin * 60000UL) {
        Serial.printf("[Zones] Zone %d manual timeout\n", i + 1);
        zone_off(zones, i);
        _dirtyMask |= (1 << i);  // signal main loop to publish MQTT state
        continue;
      }
    }

    if (zones[i].scheduleActive) {
      uint32_t elapsed = now - zones[i].scheduleOnAt;
      if (elapsed >= (uint32_t)zones[i].scheduleDurMin * 60000UL) {
        Serial.printf("[Zones] Zone %d schedule complete\n", i + 1);
        zones[i].scheduleActive = false;
        if (!zones[i].manualOn) {
          pcf_relay_set(i, false);
          _dirtyMask |= (1 << i);  // signal main loop to publish MQTT state
        }
      }
    }
  }
}

void zone_on_manual(Zone zones[MAX_ZONES], int idx, uint16_t durationMin) {
  if (idx < 0 || idx >= MAX_ZONES) return;
  if (!zones[idx].enabled) {
    Serial.printf("[Zones] Zone %d disabled, ignoring\n", idx + 1);
    return;
  }
  if (durationMin > MAX_RUN_MINUTES) durationMin = MAX_RUN_MINUTES;

  zones[idx].manualOn     = true;
  zones[idx].manualOnAt   = millis();
  zones[idx].manualDurMin = durationMin;
  pcf_relay_set(idx, true);
  Serial.printf("[Zones] Zone %d ON manual — %d min\n", idx + 1, durationMin);
}

void zone_off(Zone zones[MAX_ZONES], int idx) {
  if (idx < 0 || idx >= MAX_ZONES) return;
  zones[idx].manualOn       = false;
  zones[idx].scheduleActive = false;
  pcf_relay_set(idx, false);
  Serial.printf("[Zones] Zone %d OFF\n", idx + 1);
}

void zone_off_all(Zone zones[MAX_ZONES]) {
  for (int i = 0; i < MAX_ZONES; i++) zone_off(zones, i);
  Serial.println("[Zones] ALL zones OFF");
}

bool zone_is_on(int idx) {
  if (idx < 0 || idx >= MAX_ZONES) return false;
  return pcf_relay_get(idx);
}

ZoneState zone_get_state(const Zone& z) {
  return getZoneState(z);
}

uint8_t zones_get_dirty()  { return _dirtyMask; }
void    zones_clear_dirty() { _dirtyMask = 0; }

void schedule_check(Zone zones[MAX_ZONES], int dayOfWeek, int hour, int minute) {
  uint8_t dayBit = (1 << dayOfWeek);

  for (int i = 0; i < MAX_ZONES; i++) {
    if (!zones[i].enabled) continue;
    if (zones[i].manualOn) continue;

    for (int s = 0; s < MAX_SCHEDULES; s++) {
      Schedule& sched = zones[i].schedules[s];
      if (!sched.enabled) continue;
      if (!(sched.daysOfWeek & dayBit)) continue;
      if (sched.hour != hour || sched.minute != minute) continue;

      Serial.printf("[Zones] Schedule triggered: Zone %d, %02d:%02d\n", i + 1, hour, minute);
      zones[i].scheduleActive  = true;
      zones[i].scheduleOnAt    = millis();
      zones[i].scheduleDurMin  = sched.durationMin;
      pcf_relay_set(i, true);
    }
  }
}
