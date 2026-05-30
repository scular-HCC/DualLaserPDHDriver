/* Dual-PDH Controller — Teensy 4.1
   Two independent PDH frequency locks (CH1: 1550 nm, CH2: 1560 nm DFB lasers)
   Hardware : CDCE913 clock, dual AD9833 dither DDS, AD5064 setpoint DAC,
              ILI9341 320×240 TFT, built-in Ethernet (QNEthernet)
   Interfaces: USB-serial CLI | Ethernet Telnet CLI (port 23) | HTTP dashboard (port 80)
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
#include "network.h"

// ---- Hardware ----
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

// ---- Control state (accessed by comms.cpp via extern) ----
LockChannel ch[2];

PID pid[2] = {
  {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f},
  {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f}
};

float g_dither_freq = DITHER_FREQ_HZ;  // mutable, accessible by comms.cpp

elapsedMicros loopTimer;

// ---- Sensor readings (updated every control tick) ----
static float s_err[2]      = {0.0f, 0.0f};
static float s_laser_i[2]  = {0.0f, 0.0f};
static float s_tec_temp[2] = {NAN,  NAN};
static uint16_t s_dac[2]   = {DAC_MID_CODE, DAC_MID_CODE};

// ---- RMS accumulators (flushed each display tick) ----
static float s_rms_sum[2] = {0.0f, 0.0f};
static int   s_rms_cnt[2] = {0, 0};
static float s_rms_val[2] = {0.5f, 0.5f};

// ---- Display model ----
DispModel disp;

// ---- USB serial line buffer ----
static String usb_line;

// ============================================================
// ADC helpers
// ============================================================

static inline float adc_to_error(int raw) {
  return (raw - ADC_MID) / (float)ADC_MID;
}

static float ntc_to_celsius(int raw) {
  if (raw <= 0 || raw >= ADC_MAX) return NAN;
  float ratio = raw / (float)ADC_MAX;
  float r = NTC_R_SERIES * ratio / (1.0f - ratio);
  float t_k = 1.0f / (1.0f / 298.15f + logf(r / NTC_R25) / NTC_BETA);
  return t_k - 273.15f;
}

// ============================================================
// Update display model (called at display rate)
// ============================================================

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

    disp.ch[i].tec_temp_c  = s_tec_temp[i];
    disp.ch[i].laser_i_ma  = s_laser_i[i];
    disp.ch[i].setpoint_pct = s_dac[i] * 100.0f / 65535.0f;
    disp.ch[i].err_rms     = s_rms_val[i];

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
// Per-channel control step (1 kHz)
// ============================================================

static void control_step(int i, float e, float dt) {
  switch (ch[i].state) {
    case LOCK_IDLE:
      break;

    case LOCK_SEARCH:
      if (fabsf(e) > ch[i].acquire_threshold) {
        disp.ch[i].peak_phase = disp.ch[i].scan_phase;
        lock_enter(ch[i], LOCK_ACQUIRE);
        Serial.print(F("CH")); Serial.print(i + 1); Serial.println(F(" peak -> ACQUIRE"));
      }
      break;

    case LOCK_ACQUIRE: {
      uint16_t code = pid_to_dac(pid_step(pid[i], -e, dt));
      ad5064_write_channel(i, code);
      s_dac[i] = code;
      if (fabsf(e) < ch[i].lock_threshold) {
        ch[i].locked = true;
        lock_enter(ch[i], LOCK_LOCKED);
        Serial.print(F("CH")); Serial.print(i + 1); Serial.println(F(" LOCKED"));
      } else if (millis() - ch[i].state_ts > ch[i].acquire_timeout_ms) {
        ch[i].relock_attempts++;
        lock_enter(ch[i], LOCK_RELOCK);
      }
      break;
    }

    case LOCK_LOCKED: {
      uint16_t code = pid_to_dac(pid_step(pid[i], -e, dt));
      ad5064_write_channel(i, code);
      s_dac[i] = code;
      if (fabsf(e) > ch[i].acquire_threshold) {
        ch[i].locked = false;
        lock_enter(ch[i], LOCK_RELOCK);
        Serial.print(F("CH")); Serial.print(i + 1); Serial.println(F(" LOCK LOST"));
      }
      break;
    }

    case LOCK_HOLD:
      break;

    case LOCK_RELOCK:
      if (millis() - ch[i].state_ts > ch[i].relock_backoff_ms * (1 + ch[i].relock_attempts))
        lock_enter(ch[i], LOCK_SEARCH);
      break;
  }

  s_rms_sum[i] += e * e;
  s_rms_cnt[i]++;
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  Wire.begin();
  SPI.begin();

  analogReadResolution(ADC_BITS);
  analogReadAveraging(8);

  const uint8_t out_pins[] = {
    PIN_AD9833_1_CS, PIN_AD9833_2_CS, PIN_AD5064_CS,
    PIN_AD9833_1_RESET, PIN_AD9833_2_RESET,
    PIN_AD5064_LDAC, PIN_AD5064_CLR,
    PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST
  };
  for (uint8_t p : out_pins) pinMode(p, OUTPUT);
  pinMode(PIN_FREQ_TEST, INPUT);

  digitalWrite(PIN_AD9833_1_CS,  HIGH);
  digitalWrite(PIN_AD9833_2_CS,  HIGH);
  digitalWrite(PIN_AD5064_CS,    HIGH);
  digitalWrite(PIN_TFT_CS,       HIGH);
  digitalWrite(PIN_TFT_DC,       HIGH);
  digitalWrite(PIN_TFT_RST,      HIGH);
  digitalWrite(PIN_AD5064_LDAC,  LOW);
  digitalWrite(PIN_AD5064_CLR,   HIGH);

  delay(50);

  display_init(tft);

  // CDCE913
  cdce_program_if_needed();
  cdce_wait_for_lock();
  disp.pll_lock   = cdce_pll_locked();
  disp.refclk_mhz = 25.0f;
  if (cdce_selftest_25mhz()) Serial.println(F("CDCE913: 25 MHz OK"));
  else                       Serial.println(F("CDCE913: 25 MHz FAIL"));

  // AD9833 dither
  ad9833_reset(PIN_AD9833_1_CS, PIN_AD9833_1_RESET);
  ad9833_reset(PIN_AD9833_2_CS, PIN_AD9833_2_RESET);
  ad9833_set_freq(PIN_AD9833_1_CS, g_dither_freq, REFCLK_HZ);
  ad9833_set_freq(PIN_AD9833_2_CS, g_dither_freq, REFCLK_HZ);

  ad5064_set_midscale();

  lock_init(ch[0]);
  lock_init(ch[1]);
  disp.ch[0].peak_phase = 0.6f;
  disp.ch[1].peak_phase = 0.6f;

  // Ethernet (non-fatal — system works without it)
  network_init();

  loopTimer = 0;
  Serial.println(F("Ready. Type 'help' for commands."));
}

// ============================================================
// Main loop
// ============================================================

void loop() {
  // ---- USB serial (line-buffered) ----
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      usb_line.trim();
      if (usb_line.length()) {
        comms_process(usb_line, Serial, ch, pid, disp);
      }
      usb_line = "";
    } else {
      usb_line += c;
    }
  }

  // ---- 1 kHz control loop ----
  if (loopTimer < CONTROL_PERIOD_US) return;
  float dt = loopTimer / 1e6f;
  loopTimer = 0;

  s_err[0]      = adc_to_error(analogRead(PIN_LOCK1_IN));
  s_err[1]      = adc_to_error(analogRead(PIN_LOCK2_IN));
  s_laser_i[0]  = analogRead(PIN_LAS1_IMON) * LASER_IMON_MA_PER_LSB;
  s_laser_i[1]  = analogRead(PIN_LAS2_IMON) * LASER_IMON_MA_PER_LSB;
  s_tec_temp[0] = ntc_to_celsius(analogRead(PIN_NTC1_MON));
  s_tec_temp[1] = ntc_to_celsius(analogRead(PIN_NTC2_MON));

  control_step(0, s_err[0], dt);
  control_step(1, s_err[1], dt);

  // ---- 10 Hz display + network tick ----
  static unsigned long last_disp = 0;
  if (millis() - last_disp >= DISPLAY_PERIOD_MS) {
    last_disp = millis();
    update_disp_model();
    display_update(tft, disp);
    // Service Ethernet — runs after display to avoid jitter on control loop
    network_poll(ch, pid, disp, disp.pll_lock, disp.refclk_mhz);
  }

  // ---- 1 Hz debug print ----
  static unsigned long last_dbg = 0;
  if (millis() - last_dbg >= DEBUG_PERIOD_MS) {
    last_dbg = millis();
    for (int i = 0; i < 2; i++) {
      Serial.print(F("CH")); Serial.print(i + 1);
      Serial.print(' '); Serial.print(lock_state_name(ch[i].state));
      Serial.print(F(" err=")); Serial.print(s_err[i], 4);
      Serial.print(F(" rms=")); Serial.print(s_rms_val[i], 5);
      if (!isnan(s_tec_temp[i])) {
        Serial.print(F(" T=")); Serial.print(s_tec_temp[i], 1);
      }
      Serial.print(F(" I=")); Serial.print(s_laser_i[i], 1);
      Serial.println(F(" mA"));
    }
    Serial.print(F("IP: ")); Serial.println(network_ip());
  }
}
