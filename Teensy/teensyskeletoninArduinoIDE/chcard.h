#pragma once
#include <Arduino.h>
#include <math.h>
#include "config.h"

// ============================================================
// CH / AFE card abstraction — v5 universal backplane.
// Replaces the v3 on-board AD5064 + Teensy analog monitor pins.
//
// Per CH card (i = 0 laser 1, i = 1 laser 2):
//   AD5696R  : DAC A = laser setpoint, DAC B = TEC setpoint
//   ADS1115  : AIN0 LAS_IMON, AIN1 TEC_IMON, AIN2 MPD_MON, AIN3 NTC
//   PCA9538  : IO0 LOCK_EN (out), IO1 HB_FAULT (in, active-low)
// AFE card:
//   ADS1115  : AIN0 PD1_LVL, AIN1 PD2_LVL
//
// DAC/expander writes are cached — repeated calls with the same value
// generate NO I2C traffic (backplane noise rule: bus static in lock).
// Telemetry is a non-blocking round robin: each chcard_poll() call
// collects the previous conversion and starts the next channel.
// ============================================================

struct ChCardMon {
  float las_imon_v;   // AIN0
  float tec_imon_v;   // AIN1
  float mpd_mon_v;    // AIN2
  float ntc_v;        // AIN3
  bool  valid;        // all four channels read at least once
};

struct AfeMon {
  float pd_lvl_v[2];  // AIN0 / AIN1
  bool  valid;
};

// Configure both CH-card expanders (LOCK_EN held LOW) and park the
// DACs (laser midscale, TEC at zero-current code). Returns a bitmask
// of cards that ACKed (bit0 = CH1, bit1 = CH2) for boot diagnostics.
uint8_t chcard_init_all();

// Setpoint DACs (16-bit codes, 5 V span — same codes as the v3 AD5064).
void chcard_write_laser_dac(int i, uint16_t code);
void chcard_write_tec_dac(int i, uint16_t code);

// Park both cards: laser midscale, TEC zero-current (safe state).
void chcard_set_midscale_all();

// Analog PI integrator hold/run via PCA9538 IO0. true = RUN.
// Write-on-change; safe to call every control tick.
void chcard_set_lock_en(int i, bool run);

// H-bridge fault on card i (PCA9538 IO1, active-low). Query only after
// the shared FAULT_n line asserts. Returns true = fault.
bool chcard_fault(int i);

// Non-blocking telemetry round robin — call at TELEM_PERIOD_MS.
// Collects the previous ADS1115 conversion and starts the next one.
void chcard_poll(int i);
void afe_poll();

const ChCardMon& chcard_mon(int i);
const AfeMon&    afe_mon();

// Monitor-PD optical power (mW). MPD_MON rides on the NTC_A node (the PD
// bias reference), so subtract NTC_A to strip that offset: the PD only
// pulls MPD_MON *below* NTC_A, hence NTC_A − MPD_MON. Tiny negative
// results (offset/noise at zero light) clamp to 0.
static inline float mpd_to_optical_mw(float mpd_v, float ntc_v) {
  if (isnan(mpd_v) || isnan(ntc_v)) return NAN;
  float dv = ntc_v - mpd_v;
  if (dv < 0.0f) dv = 0.0f;
  return dv * MPD_MW_PER_V;
}
