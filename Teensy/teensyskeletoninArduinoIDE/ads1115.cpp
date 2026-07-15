#include <Arduino.h>
#include <Wire.h>
#include "ads1115.h"

// Config register: OS=1 (start), MUX=100+ch (AINch vs GND),
// PGA=001 (±4.096 V), MODE=1 (single-shot); LSB = 128 SPS, comparator off.
bool ads1115_start(uint8_t addr, uint8_t ch) {
  uint16_t cfg = 0x8000                       // OS: begin conversion
               | ((uint16_t)(0x4 + (ch & 3)) << 12)  // MUX
               | (0x1 << 9)                   // PGA ±4.096 V
               | (0x1 << 8)                   // single-shot
               | (0x4 << 5)                   // 128 SPS
               | 0x3;                         // comparator disabled
  Wire.beginTransmission(addr);
  Wire.write(0x01);                           // config register
  Wire.write((uint8_t)(cfg >> 8));
  Wire.write((uint8_t)(cfg & 0xFF));
  return Wire.endTransmission() == 0;
}

float ads1115_read_volts(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x00);                           // conversion register
  if (Wire.endTransmission() != 0) return NAN;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return NAN;
  int16_t raw = ((int16_t)Wire.read() << 8) | Wire.read();
  return raw * (4.096f / 32768.0f);
}
