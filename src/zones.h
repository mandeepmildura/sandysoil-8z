#pragma once

// ─────────────────────────────────────────────
//  zones.h
//  FarmControl Irrigation Controller
//  8-relay control via PCF8575 I2C expander.
//  Active-LOW: bit=0 → relay ON, bit=1 → relay OFF
// ─────────────────────────────────────────────

#include "config.h"
#include <Wire.h>

// ── PCF8575 low-level ─────────────────────────
void     pcfInit();
void     pcfRelaySet(int relayIdx, bool on);   // relayIdx 0–7
bool     pcfRelayGet(int relayIdx);
uint16_t pcfReadAll();

// ── Zone control ──────────────────────────────
void    zonesInit(Zone zones[MAX_ZONES]);
void    zonesLoop(Zone zones[MAX_ZONES]);
void    zoneOnManual(Zone zones[MAX_ZONES], int idx, uint16_t durationMin);
void    zoneOnProgram(Zone zones[MAX_ZONES], int idx, uint16_t durationMin);
void    zoneOff(Zone zones[MAX_ZONES], int idx);
void    zoneOffAll(Zone zones[MAX_ZONES]);
bool    zoneIsOn(int idx);
ZoneState zoneGetState(const Zone& z);

// Schedule
void scheduleCheck(Zone zones[MAX_ZONES], int dayOfWeek, int hour, int minute);

// Dirty bitmask — set when a zone changes state due to timer expiry
// Main loop reads and publishes MQTT state, then clears it.
uint8_t zonesGetDirty();
void    zonesClearDirty();

// ── Legacy aliases used in main .ino ─────────────────────────
inline void    zones_init(Zone z[])                        { zonesInit(z); }
inline void    zones_loop(Zone z[])                        { zonesLoop(z); }
inline void    zone_on_manual(Zone z[], int i, uint16_t d) { zoneOnManual(z, i, d); }
inline void    zone_on_program(Zone z[], int i, uint16_t d){ zoneOnProgram(z, i, d); }
inline void    zone_off(Zone z[], int i)                   { zoneOff(z, i); }
inline void    zone_off_all(Zone z[])                      { zoneOffAll(z); }
inline bool    zone_is_on(int i)                           { return zoneIsOn(i); }
inline uint8_t zones_get_dirty()                           { return zonesGetDirty(); }
inline void    zones_clear_dirty()                         { zonesClearDirty(); }
inline void    schedule_check(Zone z[], int d, int h, int m){ scheduleCheck(z, d, h, m); }
