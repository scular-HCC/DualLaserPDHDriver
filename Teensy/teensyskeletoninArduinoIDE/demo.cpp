#include <Arduino.h>
#include <math.h>
#include "demo.h"
#include "config.h"

// ============================================================
// Timing constants
// ============================================================
static const float SWEEP_S   = 10.0f;  // SEARCH  (TEC sweep duration)
static const float ACQUIRE_S =  2.0f;  // ACQUIRE (PID convergence)
static const float LOCKED_S  = 40.0f;  // LOCKED  (before simulated fault)
static const float RELOCK_S  =  1.5f;  // RELOCK  (backoff before retry)

// CH2 initial phase offset — starts SEARCH 5 s after CH1
static const float CH2_DELAY = -5.0f;

// ============================================================
// Per-channel demo state
// ============================================================
struct DemoCh {
  float     t;            // seconds elapsed in current state
  float     peak_ph;      // peak position [0..1] (randomised each sweep)
  float     tec_lock;     // TEC temp settled at lock [°C]
  float     las_lock;     // laser I settled at lock [mA]
  LockState last_state;   // state set by demo on previous tick
  bool      paused;       // true = user issued stop, demo won't auto-advance
};

static DemoCh dc[2];

// ============================================================
// Helpers
// ============================================================

// Float in [lo, hi] using Arduino random()
static float randf(float lo, float hi) {
  return lo + (float)(random(100000)) / 100000.0f * (hi - lo);
}

// Simple noise: pseudo-random ± amp around zero
static float noise(float amp) {
  return ((float)(random(20001)) / 10000.0f - 1.0f) * amp;
}

// Smoothstep easing for nicer convergence curves
static float smoothstep(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * (3.0f - 2.0f * t);
}

// ============================================================
// demo_init
// ============================================================
void demo_init(LockChannel ch[2], DispModel& disp) {
  randomSeed(analogRead(A6));   // seed from floating pin

  for (int i = 0; i < 2; i++) {
    lock_init(ch[i]);             // sets LOCK_IDLE + defaults

    dc[i].t          = (i == 0) ? 0.0f : CH2_DELAY;
    dc[i].peak_ph    = (i == 0) ? 0.62f : 0.55f;
    dc[i].tec_lock   = (i == 0) ? 24.5f : 23.8f;
    dc[i].las_lock   = (i == 0) ? 140.5f : 139.2f;
    dc[i].last_state = LOCK_IDLE;
    dc[i].paused     = false;

    // Initial display values
    disp.ch[i].scan_phase   = 0.0f;
    disp.ch[i].peak_phase   = dc[i].peak_ph;
    disp.ch[i].tec_temp_c   = 24.0f;
    disp.ch[i].laser_i_ma   = 138.0f;
    disp.ch[i].setpoint_pct = 0.0f;
    disp.ch[i].err_rms      = 0.0f;
    disp.ch[i].lock_qual    = 0.0f;
    disp.state[i]           = LOCK_IDLE;
  }

  disp.pll_lock   = true;
  disp.refclk_mhz = 25.0f;
}

// ============================================================
// demo_update  — called at 1 kHz from loop()
// ============================================================
void demo_update(LockChannel ch[2], DispModel& disp, float dt) {

  for (int i = 0; i < 2; i++) {

    // ----------------------------------------------------------
    // Detect external state changes (from serial / web commands)
    // ----------------------------------------------------------
    if (ch[i].state == LOCK_IDLE && dc[i].last_state != LOCK_IDLE) {
      // User issued stop — pause demo on this channel
      dc[i].paused = true;
    }
    if ((ch[i].state == LOCK_SEARCH || ch[i].state == LOCK_ACQUIRE) && dc[i].paused) {
      // User issued lock — resume demo
      dc[i].paused = false;
      dc[i].t = 0.0f;
      dc[i].peak_ph = randf(0.44f, 0.76f);
    }

    // If paused, just hold IDLE display and skip simulation
    if (dc[i].paused) {
      disp.state[i]         = LOCK_IDLE;
      disp.ch[i].err_rms    = 0.0f;
      disp.ch[i].lock_qual  = 0.0f;
      dc[i].last_state      = LOCK_IDLE;
      continue;
    }

    // ----------------------------------------------------------
    // Accumulate time in current state
    // ----------------------------------------------------------
    dc[i].t += dt;

    // ----------------------------------------------------------
    // State machine
    // ----------------------------------------------------------
    switch (ch[i].state) {

      // ---- IDLE ------------------------------------------------
      case LOCK_IDLE:
        if (dc[i].t > 0.5f) {
          // Auto-start sweep after brief pause (or initial delay for CH2)
          lock_enter(ch[i], LOCK_SEARCH);
          dc[i].t = 0.0f;
          dc[i].peak_ph = randf(0.44f, 0.76f);
        }
        disp.ch[i].tec_temp_c   = 24.0f;
        disp.ch[i].laser_i_ma   = 138.0f;
        disp.ch[i].setpoint_pct = 0.0f;
        disp.ch[i].err_rms      = 0.0f;
        disp.ch[i].lock_qual    = 0.0f;
        disp.ch[i].scan_phase   = 0.0f;
        break;

      // ---- SEARCH (10-second TEC sweep) -------------------------
      case LOCK_SEARCH: {
        float p = dc[i].t / SWEEP_S;  // 0 → 1 over SWEEP_S seconds

        if (p >= 1.0f) {
          lock_enter(ch[i], LOCK_ACQUIRE);
          dc[i].t = 0.0f;
          // Choose settled temperatures/currents near the peak
          dc[i].tec_lock = (i == 0 ? 20.0f : 22.0f)
                         + dc[i].peak_ph * (i == 0 ? 12.0f : 8.0f)
                         + randf(-0.3f, 0.3f);
          dc[i].las_lock = 134.0f + dc[i].peak_ph * 16.0f + randf(-0.5f, 0.5f);
          break;
        }

        // TEC sweeps across a temperature range to find the cavity mode
        float t_start = (i == 0) ? 18.0f : 20.0f;
        float t_end   = (i == 0) ? 32.0f : 30.0f;

        disp.ch[i].scan_phase   = p;
        disp.ch[i].peak_phase   = dc[i].peak_ph;
        disp.ch[i].tec_temp_c   = t_start + (t_end - t_start) * p
                                 + sinf(dc[i].t * 3.7f) * 0.015f;
        disp.ch[i].laser_i_ma   = 138.0f + sinf(dc[i].t * 0.8f) * 0.12f;
        disp.ch[i].setpoint_pct = p * 100.0f;
        disp.ch[i].err_rms      = 0.42f + sinf(dc[i].t * 4.1f) * 0.05f
                                 + noise(0.02f);
        disp.ch[i].lock_qual    = 0.0f;
        break;
      }

      // ---- ACQUIRE (PID convergence over 2 s) ------------------
      case LOCK_ACQUIRE: {
        float p = dc[i].t / ACQUIRE_S;  // 0 → 1

        if (p >= 1.0f) {
          lock_enter(ch[i], LOCK_LOCKED);
          ch[i].locked = true;
          dc[i].t = 0.0f;
          break;
        }

        float sp = smoothstep(p);

        disp.ch[i].scan_phase   = 0.0f;
        disp.ch[i].peak_phase   = dc[i].peak_ph;
        disp.ch[i].tec_temp_c   = dc[i].tec_lock + (1.0f - sp) * 2.0f;
        disp.ch[i].laser_i_ma   = dc[i].las_lock + (1.0f - sp) * 4.0f;
        disp.ch[i].setpoint_pct = 30.0f + dc[i].peak_ph * 55.0f;
        // Error RMS decays exponentially from ~0.22 to ~0.003
        disp.ch[i].err_rms      = 0.22f * expf(-p * 5.0f) + 0.003f
                                 + noise(0.005f);
        disp.ch[i].lock_qual    = sp * 0.88f;
        break;
      }

      // ---- LOCKED ----------------------------------------------
      case LOCK_LOCKED: {
        if (dc[i].t > LOCKED_S) {
          // Simulate a lock loss after LOCKED_S seconds
          ch[i].locked = false;
          ch[i].relock_attempts++;
          lock_enter(ch[i], LOCK_RELOCK);
          dc[i].t = 0.0f;
          break;
        }

        float nt = dc[i].t;  // time base for organic drift

        disp.ch[i].scan_phase   = 0.0f;
        disp.ch[i].peak_phase   = dc[i].peak_ph;
        disp.ch[i].tec_temp_c   = dc[i].tec_lock
                                 + sinf(nt * 0.23f) * 0.004f
                                 + noise(0.001f);
        disp.ch[i].laser_i_ma   = dc[i].las_lock
                                 + sinf(nt * 0.41f) * 0.08f
                                 + noise(0.02f);
        disp.ch[i].setpoint_pct = 30.0f + dc[i].peak_ph * 55.0f
                                 + sinf(nt * 0.13f) * 0.1f;
        // Tight in-lock noise — RMS ~0.003 with small variations
        disp.ch[i].err_rms      = 0.0028f + (float)(random(4000)) / 2000000.0f;
        // Servo quality 0.92–0.97
        disp.ch[i].lock_qual    = 0.92f + sinf(nt * 0.31f) * 0.04f
                                 + noise(0.005f);
        if (disp.ch[i].lock_qual > 1.0f) disp.ch[i].lock_qual = 1.0f;
        if (disp.ch[i].lock_qual < 0.0f) disp.ch[i].lock_qual = 0.0f;
        break;
      }

      // ---- HOLD (manual — demo does not override) --------------
      case LOCK_HOLD:
        // Keep last display values; user controls this state
        disp.ch[i].lock_qual = 0.5f;
        break;

      // ---- RELOCK (fault — random-walk error signal) ----------
      case LOCK_RELOCK: {
        if (dc[i].t > RELOCK_S) {
          lock_enter(ch[i], LOCK_SEARCH);
          dc[i].t = 0.0f;
          dc[i].peak_ph = randf(0.44f, 0.76f);  // fresh sweep target
          break;
        }

        float diverge = dc[i].t / RELOCK_S;  // 0→1
        disp.ch[i].tec_temp_c   = dc[i].tec_lock
                                 + diverge * randf(-2.0f, 2.0f);
        disp.ch[i].laser_i_ma   = dc[i].las_lock + noise(1.5f * diverge);
        disp.ch[i].err_rms      = 0.35f + diverge * 0.25f + noise(0.05f);
        disp.ch[i].lock_qual    = 0.0f;
        break;
      }
    }

    // Propagate state and remember what the demo set
    disp.state[i]    = ch[i].state;
    dc[i].last_state = ch[i].state;
  }

  // PLL and refclk are always healthy in demo
  disp.pll_lock   = true;
  disp.refclk_mhz = 25.0f;
}
