#pragma once
#include <Adafruit_ILI9341.h>
#include "lock.h"

// ============================================================
// Display model — filled by the main loop, consumed by display
// ============================================================

struct DispChannel {
  float scan_phase;    // 0..1 — scan marker position during SEARCH
  float peak_phase;    // 0..1 — saved peak position (set on ACQUIRE entry)
  float tec_temp_c;    // NTC temperature (°C), NAN if unavailable
  float laser_i_ma;    // laser current monitor (mA)
  float setpoint_pct;  // DAC setpoint 0..100 %
  float err_rms;       // RMS of PDH error signal (normalised 0..1)
  float lock_qual;     // 0..1 servo health (0=unlocked, →1=tight lock)
};

struct DispModel {
  bool       pll_lock;
  float      refclk_mhz;
  LockState  state[2];
  DispChannel ch[2];
};

void display_init(Adafruit_ILI9341 &tft);
void display_update(Adafruit_ILI9341 &tft, const DispModel &m);
