#pragma once
#include <stdint.h>

// ============================================================
// AD9833 DDS — SPI, used for PDH dither sine generation
// ============================================================

void ad9833_reset(uint8_t cs_pin, uint8_t rst_pin);
void ad9833_set_freq(uint8_t cs_pin, float freq_hz, float refclk_hz);
