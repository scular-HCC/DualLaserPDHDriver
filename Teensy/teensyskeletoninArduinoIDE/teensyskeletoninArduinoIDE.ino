/* Dual-PDH Controller — Teensy 4.1   (Rev 2.0 — RF analog-derivative lock)
   Two independent frequency locks for 1550 nm DFB lasers.

   Rev 2.0: the lock-in demod (AD831) and PI servo are ANALOG hardware; the
   Teensy is a SUPERVISOR. Per channel it programs a DRIVE + a phase-set
   REFERENCE AD9833 (dual-Ω, phase-coherent via shared 25 MHz MCLK), sweeps the
   DC setpoint to acquire, hands the loop to the analog PI via LOCKn_EN
   (integrator hold/run), auto-calibrates the demod phase, and monitors the
   post-mix baseband error on LOCKn_IN. See TeensyFirmwareAssumptions.txt and
   docs/firmware_reference.html (Rev 2.0).

   Hardware : CDCE913 clock, 4x AD9833 DDS (2 drive + 2 ref), AD831 demod,
              analog PI + ADG419 hold, AD5064 setpoint DAC,
              ILI9341 320×240 TFT, built-in Ethernet (QNEthernet)
   Interfaces: USB-serial CLI | Ethernet Telnet CLI (port 23) | HTTP dashboard (port 80)

   Demo mode is a RUNTIME setting — no recompile needed:
     'demo on'   — switch to hardware simulation (Teensy + TFT only)
     'demo off'  — switch to real PDH hardware
   The setting persists in EEPROM across power cycles.
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FreqCount.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

#include "config.h"
#include "cdce913.h"
#include "ad9833.h"
#include "ad5064.h"
#include "pid.h"
#include "lock.h"
#include "display.h"
#include "comms.h"
#include "net_settings.h"
#include "network.h"
#include "demo.h"   // always compiled; only driven when g_demo_mode == true

// ============================================================
// Hardware
// ============================================================
// SPI1 hardware bus: MOSI=26, SCK=27 — explicit constructor required
Adafruit_ILI9341 tft(&SPI1, PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_RST);

// ============================================================
// Control state
// ============================================================
LockChannel ch[2];

PID pid[2] = {
  {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f},
  {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f}
};

float g_dither_freq = DITHER_FREQ_HZ;   // legacy/JSON display (= CH1 Ω)

// Per-channel RF modulation frequency and demod (reference) phase.
float g_rf_omega[2] = { RF_OMEGA1_HZ, RF_OMEGA2_HZ };
float g_rf_phase[2] = { RF_PHASE1_DEG, RF_PHASE2_DEG };

// Runtime demo flag — loaded from EEPROM, toggled by 'demo on/off'
bool g_demo_mode = false;

elapsedMicros loopTimer;

// ============================================================
// Display model
// ============================================================
DispModel disp;

// ============================================================
// Real-hardware sensor values (always compiled; only used when
// g_demo_mode == false so they don't shadow the demo values)
// ============================================================
static float    s_err[2]     = {0.0f, 0.0f};
static float    s_laser_i[2] = {0.0f, 0.0f};
static float    s_tec_temp[2]= {NAN,  NAN};
static uint16_t s_dac[2]     = {DAC_MID_CODE, DAC_MID_CODE};
static float    s_rms_sum[2] = {0.0f, 0.0f};
static int      s_rms_cnt[2] = {0, 0};
static float    s_rms_val[2] = {0.5f, 0.5f};

// TEC current monitoring and software current limit
static float    s_tec_i[2]       = {0.0f, 0.0f};
static uint16_t s_tec_dac[2]     = {TEC_DAC_ZERO_CODE, TEC_DAC_ZERO_CODE}; // desired setpoint (default 0 A)
static float    s_tec_lim_fac[2] = {1.0f, 1.0f};                 // 1.0 = no limiting

static inline float adc_to_error(int raw) {
  return (raw - ADC_MID) / (float)ADC_MID;
}

static inline float adc_to_tec_amps(int raw) {
  float v = raw * (ADC_REF_V / ADC_MAX);
  return (v - TEC_IMON_OFFSET_V) / TEC_IMON_V_PER_A;
}

// Smoothly clamp the TEC setpoint DAC when current exceeds TEC_ILIMIT_A.
// s_tec_dac[i] holds the desired setpoint; this function derives the
// limited output and writes it to DAC channel 2+i each control tick.
static void tec_limit_step(int i) {
  if (s_tec_i[i] > TEC_ILIMIT_A) {
    s_tec_lim_fac[i] -= TEC_ILIMIT_STEP;
    if (s_tec_lim_fac[i] < 0.0f) s_tec_lim_fac[i] = 0.0f;
  } else {
    s_tec_lim_fac[i] += TEC_ILIMIT_RESTORE;
    if (s_tec_lim_fac[i] > 1.0f) s_tec_lim_fac[i] = 1.0f;
  }
  // Scale the excursion from zero-current code by the limit factor.
  // This preserves the direction (cooling or heating) while clamping magnitude.
  int32_t zero  = (int32_t)TEC_DAC_ZERO_CODE;
  int32_t raw   = (int32_t)s_tec_dac[i];
  int32_t delta = (int32_t)((raw - zero) * s_tec_lim_fac[i]);
  int32_t out   = constrain(zero + delta, 0, 65535);
  ad5064_write_channel(2 + i, (uint16_t)out);
}

static float ntc_to_celsius(int raw) {
  if (raw <= 0 || raw >= ADC_MAX) return NAN;
  float ratio = raw / (float)ADC_MAX;
  float r = NTC_R_SERIES * ratio / (1.0f - ratio);
  float t_k = 1.0f / (1.0f / 298.15f + logf(r / NTC_R25) / NTC_BETA);
  return t_k - 273.15f;
}

static void update_disp_model() {
  for (int i = 0; i < 2; i++) {
    disp.state[i] = ch[i].state;

    if (s_rms_cnt[i] > 0) {
      s_rms_val[i] = sqrtf(s_rms_sum[i] / s_rms_cnt[i]);
      s_rms_sum[i] = 0.0f;
      s_rms_cnt[i] = 0;
    }

    if (ch[i].state == LOCK_SEARCH) {
      unsigned long elapsed = millis() - ch[i].state_ts;
      disp.ch[i].scan_phase = fmodf(elapsed / (float)SCAN_PERIOD_MS, 1.0f);
    }

    disp.ch[i].tec_temp_c   = s_tec_temp[i];
    disp.ch[i].laser_i_ma   = s_laser_i[i];
    disp.ch[i].tec_i_a      = s_tec_i[i];
    disp.ch[i].tec_lim_fac  = s_tec_lim_fac[i];
    disp.ch[i].setpoint_pct = s_dac[i] * 100.0f / 65535.0f;
    disp.ch[i].err_rms      = s_rms_val[i];

    if (ch[i].state == LOCK_LOCKED) {
      float q = 1.0f - s_rms_val[i] / ch[i].acquire_threshold;
      disp.ch[i].lock_qual = constrain(q, 0.0f, 1.0f);
    } else if (ch[i].state == LOCK_HOLD) {
      disp.ch[i].lock_qual = 0.5f;
    } else {
      disp.ch[i].lock_qual = 0.0f;
    }
  }
  disp.pll_lock   = cdce_pll_locked();
  disp.refclk_mhz = 25.0f;
}

// ============================================================
// RF analog-derivative lock — supervisor
//
// The lock-in demod (AD831) and PI servo are ANALOG. The Teensy:
//   • programs the drive + reference DDS (dual-Ω, phase-coherent),
//   • sweeps the DC setpoint (DAC) to drag the laser across resonance,
//   • on capture, RELEASES the analog integrator (LOCKn_EN = RUN) so the
//     analog PI holds the lock,
//   • monitors the post-mix baseband error (LOCKn_IN) and, optionally,
//     off-loads the analog integrator by trimming the DC setpoint.
// The digital PID is no longer the lock servo (pid[] is retained only for
// the legacy CLI/JSON and the optional centering gain).
// ============================================================

// Analog integrator: HIGH = RUN (loop closed), LOW = HOLD/RESET (acquiring).
static inline void set_integrator(int i, bool run) {
  digitalWrite(i == 0 ? PIN_LOCK1_EN : PIN_LOCK2_EN, run ? HIGH : LOW);
}

// Persist the current Ω / demod-phase to EEPROM (via net_settings) so they
// survive a power cycle. Cheap — EEPROM emulation only rewrites changed bytes.
static void persist_rf() {
  NetSettings ns;
  net_settings_load(ns);
  ns.rf_omega[0] = g_rf_omega[0];  ns.rf_omega[1] = g_rf_omega[1];
  ns.rf_phase[0] = g_rf_phase[0];  ns.rf_phase[1] = g_rf_phase[1];
  net_settings_save(ns);
}

// Reprogram one channel's drive+reference DDS at a new Ω, released together
// so the pair restarts phase-coherent. Preserves the calibrated demod phase.
void rf_set_omega(int i, float hz) {
  g_rf_omega[i] = hz;
  uint8_t drv = (i == 0) ? PIN_AD9833_1_CS : PIN_AD9833_2_CS;
  uint8_t ref = (i == 0) ? PIN_AD9833_3_CS : PIN_AD9833_4_CS;
  ad9833_load(drv, hz, REFCLK_HZ, 0.0f);
  ad9833_load(ref, hz, REFCLK_HZ, g_rf_phase[i]);
  ad9833_run(drv);  ad9833_run(ref);     // release back-to-back → coherent
  if (i == 0) g_dither_freq = hz;
  persist_rf();
}

// Live demod-phase update (reference DDS PHASE0).
void rf_set_phase(int i, float deg) {
  g_rf_phase[i] = deg;
  ad9833_set_phase(i == 0 ? PIN_AD9833_3_CS : PIN_AD9833_4_CS, deg);
  persist_rf();
}

// Demod-phase calibration: step the reference phase over 0–360° and pick the
// phase that maximises the discriminator signal. Park the laser so there is a
// non-zero error (e.g. on a resonance shoulder, or 'hold') before running.
void rf_phase_cal(int i, Print& out) {
  uint8_t ref_cs = (i == 0) ? PIN_AD9833_3_CS : PIN_AD9833_4_CS;
  uint8_t in_pin = (i == 0) ? PIN_LOCK1_IN    : PIN_LOCK2_IN;
  set_integrator(i, false);                 // hold while sweeping phase
  out.print(F("CH")); out.print(i + 1); out.println(F(" demod-phase calibration:"));
  float best_mag = -1.0f; int best_p = 0;
  for (int p = 0; p < 360; p += PHASE_CAL_STEP_DEG) {
    ad9833_set_phase(ref_cs, (float)p);
    delay(PHASE_CAL_SETTLE_MS);
    long acc = 0;
    for (int k = 0; k < PHASE_CAL_SAMPLES; k++) acc += analogRead(in_pin);
    float mean = (float)acc / PHASE_CAL_SAMPLES;
    float mag  = fabsf(mean - ADC_MID);
    out.print(F("  ")); out.print(p); out.print(F(" deg  |e|=")); out.println(mag, 1);
    if (mag > best_mag) { best_mag = mag; best_p = p; }
  }
  rf_set_phase(i, (float)best_p);
  out.print(F("CH")); out.print(i + 1);
  out.print(F(" demod phase -> ")); out.print(best_p); out.println(F(" deg (verify lock polarity)"));
}

// Optional slow integrator off-load (anti-windup). Trims the DC setpoint to
// absorb the analog-PI correction, keeping the analog integrator near centre.
// Requires the optional CORR taps (see config.h); otherwise a no-op.
static void center_step(int i) {
#ifdef PIN_LAS1_CORR_MON
  static uint16_t cnt[2] = {0, 0};
  if (++cnt[i] < 50) return;                 // ~20 Hz: far slower than the analog PI
  cnt[i] = 0;
  uint8_t pin = (i == 0) ? PIN_LAS1_CORR_MON : PIN_LAS2_CORR_MON;
  int corr = analogRead(pin) - ADC_MID;      // analog-PI output vs centre
  int32_t code = (int32_t)s_dac[i] - (corr >> 6);  // small step; sign per wiring — FLAG
  s_dac[i] = (uint16_t)constrain(code, 0, 65535);
  ad5064_write_channel(i, s_dac[i]);
#else
  (void)i;
#endif
}

// Per-channel acquisition state
static int       s_sweep_dir[2]  = {1, 1};
static bool      s_armed[2]      = {false, false};
static float     s_prev_e[2]     = {0.0f, 0.0f};
static LockState s_prev_state[2] = {LOCK_IDLE, LOCK_IDLE};

static void control_step(int i, float e, float dt) {
  (void)dt;
  // Hand the loop to / take it from the analog integrator by state.
  set_integrator(i, ch[i].state == LOCK_ACQUIRE || ch[i].state == LOCK_LOCKED);

  // State-entry hook
  if (ch[i].state != s_prev_state[i]) {
    if (ch[i].state == LOCK_SEARCH) s_armed[i] = false;
    s_prev_state[i] = ch[i].state;
  }

  switch (ch[i].state) {
    case LOCK_IDLE:
      break;

    case LOCK_SEARCH: {
      // Sweep the DC setpoint to drag the laser across resonance (bounce at rails).
      int32_t code = (int32_t)s_dac[i] + s_sweep_dir[i] * SWEEP_STEP_CODE;
      if      (code >= 65535) { code = 65535; s_sweep_dir[i] = -1; }
      else if (code <= 0)     { code = 0;     s_sweep_dir[i] =  1; }
      s_dac[i] = (uint16_t)code;
      ad5064_write_channel(i, s_dac[i]);
      // Arm on a discriminator shoulder; capture the next zero-crossing (= resonance).
      if (fabsf(e) > ch[i].acquire_threshold) s_armed[i] = true;
      bool zero_cross = (e == 0.0f) || (e * s_prev_e[i] < 0.0f);
      if (s_armed[i] && zero_cross) {
        disp.ch[i].peak_phase = disp.ch[i].scan_phase;
        lock_enter(ch[i], LOCK_ACQUIRE);     // integrator released next tick
        Serial.print(F("CH")); Serial.print(i+1); Serial.println(F(" resonance -> ACQUIRE"));
      }
      break;
    }

    case LOCK_ACQUIRE:
      // Analog PI now closing the loop; DC setpoint held at the capture code.
      if (fabsf(e) < ch[i].lock_threshold) {
        ch[i].locked = true;
        lock_enter(ch[i], LOCK_LOCKED);
        Serial.print(F("CH")); Serial.print(i+1); Serial.println(F(" LOCKED"));
      } else if (millis() - ch[i].state_ts > ch[i].acquire_timeout_ms) {
        ch[i].relock_attempts++;
        lock_enter(ch[i], LOCK_RELOCK);
      }
      break;

    case LOCK_LOCKED:
      if (fabsf(e) > ch[i].acquire_threshold) {
        ch[i].locked = false;
        lock_enter(ch[i], LOCK_RELOCK);
        Serial.print(F("CH")); Serial.print(i+1); Serial.println(F(" LOCK LOST"));
      } else {
        center_step(i);                      // optional slow integrator off-load
      }
      break;

    case LOCK_HOLD:
      break;

    case LOCK_RELOCK:
      if (millis() - ch[i].state_ts > ch[i].relock_backoff_ms * (1 + ch[i].relock_attempts))
        lock_enter(ch[i], LOCK_SEARCH);
      break;
  }
  s_prev_e[i]   = e;
  s_rms_sum[i] += e * e;
  s_rms_cnt[i]++;
}

// ============================================================
// USB serial line buffer
// ============================================================
static String usb_line;

// ============================================================
// Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();   // SPI0 — signal-path chips
  SPI1.begin();  // SPI1 — TFT display

  analogReadResolution(ADC_BITS);
  analogReadAveraging(8);

  const uint8_t out_pins[] = {
    PIN_AD9833_1_CS, PIN_AD9833_2_CS, PIN_AD9833_3_CS, PIN_AD9833_4_CS,
    PIN_AD5064_CS,
    PIN_AD9833_1_RESET, PIN_AD9833_2_RESET,
    PIN_AD5064_CLR,
    PIN_LOCK1_EN, PIN_LOCK2_EN,
    PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST
  };
  for (uint8_t p : out_pins) pinMode(p, OUTPUT);
  pinMode(PIN_FREQ_TEST, INPUT);

  // H-bridge fault inputs: OPA551 Flag is open-drain active-low
  pinMode(PIN_HBRIDGE_FAULT1, INPUT_PULLUP);
  pinMode(PIN_HBRIDGE_FAULT2, INPUT_PULLUP);

  digitalWrite(PIN_AD9833_1_CS,  HIGH);
  digitalWrite(PIN_AD9833_2_CS,  HIGH);
  digitalWrite(PIN_AD9833_3_CS,  HIGH);
  digitalWrite(PIN_AD9833_4_CS,  HIGH);
  digitalWrite(PIN_AD5064_CS,    HIGH);
  digitalWrite(PIN_LOCK1_EN,     LOW);   // integrator HELD until a lock is acquired
  digitalWrite(PIN_LOCK2_EN,     LOW);
  digitalWrite(PIN_TFT_CS,       HIGH);
  digitalWrite(PIN_TFT_DC,       HIGH);
  digitalWrite(PIN_TFT_RST,      HIGH);
  digitalWrite(PIN_AD5064_CLR,   HIGH);

  delay(50);
  display_init(tft);

  // ── Load runtime settings from EEPROM ─────────────────────
  {
    NetSettings ns;
    net_settings_load(ns);
    g_demo_mode = ns.demo_mode;
    // Restore persisted RF Ω / demod-phase (sanity-clamped against corruption).
    for (int i = 0; i < 2; i++) {
      float w = ns.rf_omega[i];
      g_rf_omega[i] = (w >= 100000.0f && w <= 3500000.0f) ? w
                      : (i == 0 ? RF_OMEGA1_HZ : RF_OMEGA2_HZ);
      float p = ns.rf_phase[i];
      g_rf_phase[i] = (isfinite(p) && p >= -360.0f && p <= 360.0f) ? p : 0.0f;
    }
    g_dither_freq = g_rf_omega[0];
  }

  // ── Hardware init (always runs; safe even without hardware) ─
  // In demo mode the peripherals are initialised but never driven.
  cdce_program_if_needed();
  cdce_wait_for_lock();
  disp.pll_lock   = cdce_pll_locked();
  disp.refclk_mhz = 25.0f;
  if (cdce_selftest_25mhz()) Serial.println(F("CDCE913: 25 MHz OK"));
  else                       Serial.println(F("CDCE913: 25 MHz FAIL / not connected"));

  // Reset all four DDS, then bring up each channel's DRIVE+REFERENCE pair
  // released together so they start phase-coherent (shared 25 MHz MCLK).
  ad9833_reset(PIN_AD9833_1_CS, PIN_AD9833_1_RESET);
  ad9833_reset(PIN_AD9833_2_CS, PIN_AD9833_2_RESET);
  ad9833_reset_sw(PIN_AD9833_3_CS);
  ad9833_reset_sw(PIN_AD9833_4_CS);
  // CH1: drive (Ω1) + reference (Ω1, demod phase)
  ad9833_load(PIN_AD9833_1_CS, g_rf_omega[0], REFCLK_HZ, 0.0f);
  ad9833_load(PIN_AD9833_3_CS, g_rf_omega[0], REFCLK_HZ, g_rf_phase[0]);
  ad9833_run(PIN_AD9833_1_CS);  ad9833_run(PIN_AD9833_3_CS);
  // CH2: drive (Ω2) + reference (Ω2, demod phase)
  ad9833_load(PIN_AD9833_2_CS, g_rf_omega[1], REFCLK_HZ, 0.0f);
  ad9833_load(PIN_AD9833_4_CS, g_rf_omega[1], REFCLK_HZ, g_rf_phase[1]);
  ad9833_run(PIN_AD9833_2_CS);  ad9833_run(PIN_AD9833_4_CS);
  ad5064_set_midscale();

  // ── Mode-specific channel init ─────────────────────────────
  if (g_demo_mode) {
    demo_init(ch, disp);
    Serial.println(F("*** DEMO MODE — type 'demo off' for real hardware ***"));
  } else {
    lock_init(ch[0]);
    lock_init(ch[1]);
    disp.ch[0].peak_phase = 0.6f;
    disp.ch[1].peak_phase = 0.6f;
    Serial.println(F("Real hardware mode. Type 'demo on' to enable simulation."));
  }

  network_init();
  loopTimer = 0;
  Serial.println(F("Ready. Type 'help' for commands."));
}

// ============================================================
// Main loop
// ============================================================
void loop() {

  // ---- USB serial (line-buffered) ----------------------------
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      usb_line.trim();
      if (usb_line.length())
        comms_process(usb_line, Serial, ch, pid, disp);
      usb_line = "";
    } else {
      usb_line += c;
    }
  }

  // ---- 1 kHz control / simulation tick ----------------------
  if (loopTimer < CONTROL_PERIOD_US) return;
  float dt = loopTimer / 1e6f;
  loopTimer = 0;

  if (g_demo_mode) {
    // Demo: drive state machine and fill DispModel from simulation
    demo_update(ch, disp, dt);
  } else {
    // Real hardware: read sensors, run PID control loops
    s_err[0]      = adc_to_error(analogRead(PIN_LOCK1_IN));
    s_err[1]      = adc_to_error(analogRead(PIN_LOCK2_IN));
    s_laser_i[0]  = analogRead(PIN_LAS1_IMON) * LASER_IMON_MA_PER_LSB;
    s_laser_i[1]  = analogRead(PIN_LAS2_IMON) * LASER_IMON_MA_PER_LSB;
    s_tec_temp[0] = ntc_to_celsius(analogRead(PIN_NTC1_MON));
    s_tec_temp[1] = ntc_to_celsius(analogRead(PIN_NTC2_MON));
    s_tec_i[0]    = adc_to_tec_amps(analogRead(PIN_TEC1_IMON));
    s_tec_i[1]    = adc_to_tec_amps(analogRead(PIN_TEC2_IMON));
    tec_limit_step(0);
    tec_limit_step(1);

    // H-bridge fault: OPA551 Flag is open-drain active-low → LOW = fault
    bool fault0 = (digitalRead(PIN_HBRIDGE_FAULT1) == LOW);
    bool fault1 = (digitalRead(PIN_HBRIDGE_FAULT2) == LOW);
    if (fault0 != disp.hbridge_fault[0]) {
      disp.hbridge_fault[0] = fault0;
      Serial.print(F("CH1 H-BRIDGE FAULT: ")); Serial.println(fault0 ? F("ASSERTED") : F("cleared"));
    }
    if (fault1 != disp.hbridge_fault[1]) {
      disp.hbridge_fault[1] = fault1;
      Serial.print(F("CH2 H-BRIDGE FAULT: ")); Serial.println(fault1 ? F("ASSERTED") : F("cleared"));
    }
    control_step(0, s_err[0], dt);
    control_step(1, s_err[1], dt);
  }

  // ---- 10 Hz display + network tick -------------------------
  static unsigned long last_disp = 0;
  if (millis() - last_disp >= DISPLAY_PERIOD_MS) {
    last_disp = millis();
    if (!g_demo_mode) update_disp_model();  // demo fills disp each tick
    display_update(tft, disp);
    network_poll(ch, pid, disp, disp.pll_lock, disp.refclk_mhz);
  }

  // ---- 1 Hz debug print -------------------------------------
  static unsigned long last_dbg = 0;
  if (millis() - last_dbg >= DEBUG_PERIOD_MS) {
    last_dbg = millis();
    if (g_demo_mode) Serial.print(F("[DEMO] "));
    for (int i = 0; i < 2; i++) {
      Serial.print(F("CH")); Serial.print(i+1);
      Serial.print(' '); Serial.print(lock_state_name(ch[i].state));
      Serial.print(F(" rms="));  Serial.print(disp.ch[i].err_rms, 5);
      if (!isnan(disp.ch[i].tec_temp_c)) {
        Serial.print(F(" T="));  Serial.print(disp.ch[i].tec_temp_c, 1);
      }
      Serial.print(F(" I="));   Serial.print(disp.ch[i].laser_i_ma, 1);
      Serial.print(F(" mA"));
      if (disp.hbridge_fault[i]) Serial.print(F(" *** H-BRIDGE FAULT ***"));
      Serial.println();
    }
    Serial.print(F("IP: ")); Serial.println(network_ip());
  }
}
