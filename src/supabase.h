#pragma once

// ─────────────────────────────────────────────
//  supabase.h
//  FarmControl Irrigation Controller
//  Logs supply pressure and zone activity to
//  Supabase via HTTP REST API.
//  Only runs when WiFi is connected.
// ─────────────────────────────────────────────

#include <Arduino.h>
#include "config.h"

// Initialise — call once after WiFi connects
void supabaseInit();

// Log pressure + zone states — call periodically from loop
void supabaseLog(float supplyPsi, Zone zones[MAX_ZONES]);

// Pull schedule rows from Supabase and rewrite Zone.schedules[]
void supabaseSyncSchedules(Zone zones[MAX_ZONES]);

// Legacy aliases used in main .ino
inline void supabase_init()                              { supabaseInit(); }
inline void supabase_log(float p, Zone z[MAX_ZONES])    { supabaseLog(p, z); }
inline void supabase_sync_schedules(Zone z[])            { supabaseSyncSchedules(z); }
