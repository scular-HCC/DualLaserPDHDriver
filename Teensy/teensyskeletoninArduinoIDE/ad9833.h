#pragma once
#include <stdint.h>

// ============================================================
// AD9833 DDS — SPI
//
// Used for the RF analog-derivative lock: two DRIVE DDS (modulate the
// laser current at Ω) and two REFERENCE DDS (phase-set demod LO). All
// four share the one 25 MHz MCLK, so within a channel the drive and
// reference stay phase-coherent once their accumulators are released
// together; the demod phase is then set by the reference PHASE0 register.
// ============================================================

// Legacy single-call helpers (still used for the 'dither' test command).
void ad9833_reset(uint8_t cs_pin, uint8_t rst_pin);
void ad9833_set_freq(uint8_t cs_pin, float freq_hz, float refclk_hz);

// Software reset (no GPIO): holds the output at mid-scale (RESET=1).
void ad9833_reset_sw(uint8_t cs_pin);

// Set frequency (FREQ0) + phase (PHASE0) and run, in one call.
void ad9833_set_freq_phase(uint8_t cs_pin, float freq_hz, float refclk_hz, float phase_deg);

// Live phase update only (PHASE0) — does not touch frequency or RESET.
// Use during demod-phase calibration.
void ad9833_set_phase(uint8_t cs_pin, float phase_deg);

// Phase-coherent pair start:
//   1. ad9833_load(...)  on every chip   — writes FREQ0/PHASE0 with RESET held
//   2. ad9833_run(...)   on every chip   — clears RESET (release together)
// Releasing the RESET of two chips back-to-back (shared MCLK, identical
// freq word) starts their accumulators aligned; the constant write skew
// is absorbed by the reference PHASE0 calibration.
void ad9833_load(uint8_t cs_pin, float freq_hz, float refclk_hz, float phase_deg);
void ad9833_run(uint8_t cs_pin);
