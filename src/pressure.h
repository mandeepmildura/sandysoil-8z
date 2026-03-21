#pragma once

// ─────────────────────────────────────────────
//  pressure.h
//  FarmControl Irrigation Controller
//  Reads supply pressure via ADC.
//
//  Sensor: 0–5V ratiometric (3-wire)
//  Pin:    GPIO1 (ADC1_CH0 on ESP32-S3)
//  Range:  cfg.pressure_max_psi
// ─────────────────────────────────────────────

#include <Arduino.h>

// Initialise ADC pin — call once in setup()
void pressureInit();

// Returns latest supply pressure in PSI
float pressureGetSupplyPsi();

// Returns true if last reading is below threshold
bool pressureIsLow(float psi, float threshold);

// Pressure simulator — set a fake PSI value for testing
void pressureSimulate(float psi);
void pressureSimulateClear();
bool pressureIsSimulated();

// Legacy aliases used in main .ino
inline void  pressure_init()                                { pressureInit(); }
inline float pressure_get_supply_psi()                      { return pressureGetSupplyPsi(); }
inline bool  pressure_is_low(float psi, float thr)         { return pressureIsLow(psi, thr); }
