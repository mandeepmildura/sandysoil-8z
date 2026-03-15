#pragma once
#include "config.h"
#include <Wire.h>

// PCF8575 relay control (I2C expander)
void pcf_init();
void pcf_relay_set(int relayIdx, bool on);   // relayIdx 0-7
bool pcf_relay_get(int relayIdx);
uint16_t pcf_read_all();                      // raw 16-bit port state

void zones_init(Zone zones[MAX_ZONES]);
void zones_loop(Zone zones[MAX_ZONES]);
void zone_on_manual(Zone zones[MAX_ZONES], int idx, uint16_t durationMin);
void zone_off(Zone zones[MAX_ZONES], int idx);
void zone_off_all(Zone zones[MAX_ZONES]);
bool zone_is_on(int idx);
ZoneState zone_get_state(const Zone& z);
void schedule_check(Zone zones[MAX_ZONES], int dayOfWeek, int hour, int minute);

// Dirty bitmask — set when a zone changes state unexpectedly (timer expiry, etc.)
// Main loop reads this and publishes updated MQTT zone state, then clears it.
uint8_t  zones_get_dirty();
void     zones_clear_dirty();
