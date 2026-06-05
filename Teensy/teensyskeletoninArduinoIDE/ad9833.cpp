#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "ad9833.h"

// AD9833 control-word bits we use:
//   B28   (0x2000) — load full 28-bit freq word as two consecutive writes
//   RESET (0x0100) — hold DAC/phase accumulator at reset (output = mid-scale)
// Register address prefixes (D15..D14 / D15..D13):
//   FREQ0  : 0x4000   PHASE0 : 0xC000

static void ad9833_write(uint8_t cs_pin, uint16_t word) {
  digitalWrite(cs_pin, LOW);
  SPI.transfer16(word);
  digitalWrite(cs_pin, HIGH);
}

static inline uint32_t ad9833_freq_word(float freq_hz, float refclk_hz) {
  return (uint32_t)((freq_hz * (double)(1ULL << 28)) / refclk_hz);
}

static inline uint16_t ad9833_phase_word(float phase_deg) {
  // PHASE0 is 12-bit, 4096 codes per 360°.
  float w = fmodf(phase_deg, 360.0f);
  if (w < 0.0f) w += 360.0f;
  uint16_t p = (uint16_t)lroundf(w / 360.0f * 4096.0f) & 0x0FFF;
  return 0xC000 | p;   // PHASE0 register select
}

void ad9833_reset_sw(uint8_t cs_pin) {
  ad9833_write(cs_pin, 0x2100);  // B28=1, RESET=1
}

void ad9833_reset(uint8_t cs_pin, uint8_t rst_pin) {
  digitalWrite(rst_pin, HIGH);
  delay(1);
  digitalWrite(rst_pin, LOW);
  delay(1);
  ad9833_write(cs_pin, 0x0100); // hold in reset
}

// Write FREQ0 (two halves) and PHASE0 with RESET held; output stays parked.
void ad9833_load(uint8_t cs_pin, float freq_hz, float refclk_hz, float phase_deg) {
  uint32_t word = ad9833_freq_word(freq_hz, refclk_hz);
  uint16_t lsb  = 0x4000 | (word & 0x3FFF);
  uint16_t msb  = 0x4000 | ((word >> 14) & 0x3FFF);
  ad9833_write(cs_pin, 0x2100);                 // B28=1, RESET=1 (hold)
  ad9833_write(cs_pin, lsb);
  ad9833_write(cs_pin, msb);
  ad9833_write(cs_pin, ad9833_phase_word(phase_deg));
}

// Release RESET — accumulator starts running.
void ad9833_run(uint8_t cs_pin) {
  ad9833_write(cs_pin, 0x2000);                 // B28=1, RESET=0 (run)
}

void ad9833_set_freq_phase(uint8_t cs_pin, float freq_hz, float refclk_hz, float phase_deg) {
  ad9833_load(cs_pin, freq_hz, refclk_hz, phase_deg);
  ad9833_run(cs_pin);
}

void ad9833_set_freq(uint8_t cs_pin, float freq_hz, float refclk_hz) {
  ad9833_set_freq_phase(cs_pin, freq_hz, refclk_hz, 0.0f);
}

// Live phase update only — frequency and RESET untouched, output keeps running.
void ad9833_set_phase(uint8_t cs_pin, float phase_deg) {
  ad9833_write(cs_pin, ad9833_phase_word(phase_deg));
}
