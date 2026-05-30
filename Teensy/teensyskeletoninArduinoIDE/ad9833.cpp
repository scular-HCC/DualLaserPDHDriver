#include <Arduino.h>
#include <SPI.h>
#include "ad9833.h"

static void ad9833_write(uint8_t cs_pin, uint16_t word) {
  digitalWrite(cs_pin, LOW);
  SPI.transfer16(word);
  digitalWrite(cs_pin, HIGH);
}

void ad9833_reset(uint8_t cs_pin, uint8_t rst_pin) {
  digitalWrite(rst_pin, HIGH);
  delay(1);
  digitalWrite(rst_pin, LOW);
  delay(1);
  ad9833_write(cs_pin, 0x0100); // hold in reset
}

void ad9833_set_freq(uint8_t cs_pin, float freq_hz, float refclk_hz) {
  uint32_t word = (uint32_t)((freq_hz * (1ULL << 28)) / refclk_hz);
  uint16_t lsb  = 0x4000 | (word & 0x3FFF);
  uint16_t msb  = 0x4000 | ((word >> 14) & 0x3FFF);
  ad9833_write(cs_pin, 0x2100); // B28=1, RESET=1
  ad9833_write(cs_pin, lsb);
  ad9833_write(cs_pin, msb);
  ad9833_write(cs_pin, 0x2000); // clear RESET
}
