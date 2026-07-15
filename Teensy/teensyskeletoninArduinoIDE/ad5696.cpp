#include <Arduino.h>
#include <Wire.h>
#include "ad5696.h"

bool ad5696_write(uint8_t addr, uint8_t dac_mask, uint16_t code) {
  Wire.beginTransmission(addr);
  Wire.write(AD5696_CMD_WRITE_UPDATE | (dac_mask & 0x0F));
  Wire.write((uint8_t)(code >> 8));
  Wire.write((uint8_t)(code & 0xFF));
  return Wire.endTransmission() == 0;
}
