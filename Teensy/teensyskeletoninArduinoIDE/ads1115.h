#pragma once
#include <Arduino.h>

// ADS1115 — 4-channel 16-bit sigma-delta I2C ADC (one per CH card +
// one on the AFE card). Used NON-BLOCKING: start a single-shot
// conversion on one tick, collect the result on the next (conversion
// at 128 SPS finishes in ~8 ms, well inside the 100 ms telemetry tick).
// PGA fixed at ±4.096 V (monitors are 0–3.3 V) → 125 µV/LSB.

// Start a single-shot conversion of AINch (0..3, vs GND).
// Returns true on I2C ACK.
bool ads1115_start(uint8_t addr, uint8_t ch);

// Read the last completed conversion as VOLTS. Returns NAN on error.
float ads1115_read_volts(uint8_t addr);
