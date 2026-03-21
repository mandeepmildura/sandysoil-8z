#include "pressure.h"
#include "config.h"

// ─────────────────────────────────────────────
//  pressure.cpp
//  Single-channel supply pressure via ADC.
//  Averages PRESSURE_SAMPLES readings to reduce
//  ADC noise on the ESP32-S3.
//
//  Voltage divider: sensor outputs 0–5V but the
//  ESP32 ADC input is 3.3V max. The KC868-A8v3
//  includes a 2:3 voltage divider on A0, so the
//  effective input range is 0–3.3V ≡ 0–5V sensor.
// ─────────────────────────────────────────────

static float _lastPsi = 0.0f;
static bool  _simulated = false;
static float _simPsi    = 0.0f;

void pressureInit() {
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);  // 0–3.3V input range
  Serial.printf("[Pressure] ADC init on GPIO%d\n", PIN_PRESSURE);
}

void pressureSimulate(float psi) { _simulated = true; _simPsi = psi; }
void pressureSimulateClear()    { _simulated = false; _simPsi = 0.0f; }
bool pressureIsSimulated()      { return _simulated; }

float pressureGetSupplyPsi() {
  if (_simulated) { _lastPsi = _simPsi; return _simPsi; }
  // Average multiple samples
  long sum = 0;
  for (int i = 0; i < PRESSURE_SAMPLES; i++) {
    sum += analogRead(PIN_PRESSURE);
    delay(2);
  }
  float raw = sum / (float)PRESSURE_SAMPLES;

  // Convert ADC count → voltage (0–3.3V)
  float voltage = (raw / ADC_MAX) * ADC_VREF;

  // Scale back to sensor range (0–5V) assuming 2:3 divider on board
  float sensorV = voltage * (5.0f / 3.3f);

  // Convert voltage → PSI using sensor range (0.5–4.5V = 0–max PSI)
  float psi = 0.0f;
  if (sensorV >= SENSOR_V_MIN) {
    psi = ((sensorV - SENSOR_V_MIN) / (SENSOR_V_MAX - SENSOR_V_MIN))
          * cfg.pressure_max_psi
          - cfg.pressure_zero_offset;
    if (psi < 0.0f) psi = 0.0f;
  }

  _lastPsi = psi;
  return psi;
}

bool pressureIsLow(float psi, float threshold) {
  return psi < threshold;
}
