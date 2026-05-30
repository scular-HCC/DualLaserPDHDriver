#include <Arduino.h>
#include <SPI.h>
#include "ad5064.h"
#include "config.h"

void ad5064_write_channel(uint8_t ch, uint16_t code) {
  if (ch > 3) return;
  uint8_t b0 = (0x3 << 4) | (ch & 0x3); // cmd=write&update
  uint8_t b1 = code >> 8;
  uint8_t b2 = code & 0xFF;
  digitalWrite(PIN_AD5064_CS, LOW);
  SPI.transfer(b0);
  SPI.transfer(b1);
  SPI.transfer(b2);
  digitalWrite(PIN_AD5064_CS, HIGH);
}

void ad5064_set_midscale() {
  for (uint8_t i = 0; i < 4; i++)
    ad5064_write_channel(i, DAC_MID_CODE);
}
