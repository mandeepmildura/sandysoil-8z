#include "zones.h"

// Shadow register — tracks current 16-bit PCF8575 state
// P0–P7: relays (active LOW: 0=ON, 1=OFF)
// P8–P15: digital inputs (read only)
static uint16_t _pcfState = 0xFFFF;  // all HIGH = all relays OFF

// Dirty bitmask — bits 0–7 correspond to zones 0–7
static uint8_t _dirtyMask = 0;

// ── PCF8575 low-level ────────────────────────────────────────

static void pcfWrite(uint16_t val) {
  Wire.beginTransmission(PCF8575_ADDR);
  Wire.write(val & 0xFF);
  Wire.write((val >> 8) & 0xFF);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    Serial.printf("[PCF] I2C write to 0x%02X FAILED err=%d\n", PCF8575_ADDR, err);
  }
  _pcfState = val;
}

static uint16_t pcfRead() {
  Wire.requestFrom((uint8_t)PCF8575_ADDR, (uint8_t)2);
  uint16_t val = 0;
  if (Wire.available()) val  = Wire.read();
  if (Wire.available()) val |= ((uint16_t)Wire.read() << 8);
  return val;
}

void pcfInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  _pcfState = 0xFFFF;
  pcfWrite(_pcfState);
  Serial.println("[PCF] PCF8575 init — all relays OFF");
}

void pcfRelaySet(int idx, bool on) {
  if (idx < 0 || idx >= 8) return;
  if (on) _pcfState &= ~(1 << idx);   // clear bit → relay ON
  else    _pcfState |=  (1 << idx);   // set bit   → relay OFF
  pcfWrite(_pcfState);
}

bool pcfRelayGet(int idx) {
  if (idx < 0 || idx >= 8) return false;
  return !(_pcfState & (1 << idx));   // LOW = ON
}

uint16_t pcfReadAll() { return pcfRead(); }

// ── Zone logic ───────────────────────────────────────────────

void zonesInit(Zone zones[MAX_ZONES]) {
  pcfInit();
  for (int i = 0; i < MAX_ZONES; i++) {
    zones[i].manualOn       = false;
    zones[i].scheduleActive = false;
  }
  Serial.println("[Zones] Init — all OFF");
}

void zonesLoop(Zone zones[MAX_ZONES]) {
  uint32_t now = millis();
  for (int i = 0; i < MAX_ZONES; i++) {
    if (zones[i].manualOn && zones[i].manualDurMin > 0) {
      if (now - zones[i].manualOnAt >= (uint32_t)zones[i].manualDurMin * 60000UL) {
        Serial.printf("[Zones] Zone %d manual timeout\n", i + 1);
        zoneOff(zones, i);
        _dirtyMask |= (1 << i);
      }
    }
    if (zones[i].scheduleActive) {
      if (now - zones[i].scheduleOnAt >= (uint32_t)zones[i].scheduleDurMin * 60000UL) {
        Serial.printf("[Zones] Zone %d schedule complete\n", i + 1);
        zones[i].scheduleActive = false;
        if (!zones[i].manualOn) {
          pcfRelaySet(i, false);
          _dirtyMask |= (1 << i);
        }
      }
    }
  }
}

void zoneOnManual(Zone zones[MAX_ZONES], int idx, uint16_t durationMin) {
  if (idx < 0 || idx >= MAX_ZONES) return;
  if (!zones[idx].enabled) {
    Serial.printf("[Zones] Zone %d disabled\n", idx + 1);
    return;
  }
  if (durationMin > MAX_RUN_MINUTES) durationMin = MAX_RUN_MINUTES;
  zones[idx].manualOn     = true;
  zones[idx].manualOnAt   = millis();
  zones[idx].manualDurMin = durationMin;
  pcfRelaySet(idx, true);
  Serial.printf("[Zones] Zone %d ON manual — %d min\n", idx + 1, durationMin);
}

void zoneOff(Zone zones[MAX_ZONES], int idx) {
  if (idx < 0 || idx >= MAX_ZONES) return;
  zones[idx].manualOn       = false;
  zones[idx].scheduleActive = false;
  pcfRelaySet(idx, false);
  Serial.printf("[Zones] Zone %d OFF\n", idx + 1);
}

void zoneOffAll(Zone zones[MAX_ZONES]) {
  for (int i = 0; i < MAX_ZONES; i++) zoneOff(zones, i);
  Serial.println("[Zones] ALL OFF");
}

bool zoneIsOn(int idx) {
  if (idx < 0 || idx >= MAX_ZONES) return false;
  return pcfRelayGet(idx);
}

ZoneState zoneGetState(const Zone& z) { return getZoneState(z); }

uint8_t zonesGetDirty()  { return _dirtyMask; }
void    zonesClearDirty() { _dirtyMask = 0; }

void scheduleCheck(Zone zones[MAX_ZONES], int dayOfWeek, int hour, int minute) {
  uint8_t dayBit = (1 << dayOfWeek);
  for (int i = 0; i < MAX_ZONES; i++) {
    if (!zones[i].enabled || zones[i].manualOn) continue;
    for (int s = 0; s < MAX_SCHEDULES; s++) {
      Schedule& sched = zones[i].schedules[s];
      if (!sched.enabled) continue;
      if (!(sched.daysOfWeek & dayBit)) continue;
      if (sched.hour != hour || sched.minute != minute) continue;
      Serial.printf("[Zones] Schedule: Zone %d %02d:%02d\n", i + 1, hour, minute);
      zones[i].scheduleActive  = true;
      zones[i].scheduleOnAt    = millis();
      zones[i].scheduleDurMin  = sched.durationMin;
      pcfRelaySet(i, true);
    }
  }
}
