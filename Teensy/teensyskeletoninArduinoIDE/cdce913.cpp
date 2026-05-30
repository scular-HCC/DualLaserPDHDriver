#include <Arduino.h>
#include <Wire.h>
#include <FreqCount.h>
#include "cdce913.h"
#include "config.h"

// EEPROM image: 10 MHz → 25 MHz (M=10, N=1, PLL enabled)
// Version byte 0x51 at offset 0x1F — bump when config changes.
static const uint8_t CDCE_IMAGE[32] = {
  0x00, 0x0A, 0x01, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x51
};

static uint8_t cdce_read_byte(uint8_t offset) {
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x40 + offset);
  Wire.endTransmission();
  Wire.requestFrom(CDCE_ADDR, 1);
  return Wire.available() ? Wire.read() : 0xFF;
}

static void cdce_program_eeprom() {
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x00);
  for (int i = 0; i < 32; i++) Wire.write(CDCE_IMAGE[i]);
  Wire.endTransmission();
  delay(5);
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x86);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(20);
}

void cdce_program_eeprom_force() {
  cdce_program_eeprom();
}

void cdce_program_if_needed() {
  if (cdce_read_byte(0x1F) == CDCE_IMAGE[31]) {
    Serial.println("CDCE913: EEPROM OK");
    return;
  }
  Serial.println("CDCE913: programming EEPROM...");
  cdce_program_eeprom();
  Serial.println("CDCE913: EEPROM programmed");
}

bool cdce_pll_locked() {
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x82);
  Wire.endTransmission();
  Wire.requestFrom(CDCE_ADDR, 1);
  return Wire.available() && (Wire.read() & 0x01);
}

void cdce_wait_for_lock() {
  unsigned long t0 = millis();
  while (!cdce_pll_locked()) {
    if (millis() - t0 > 200) {
      Serial.println("CDCE913: PLL NOT LOCKED");
      return;
    }
    delay(5);
  }
  Serial.println("CDCE913: PLL locked");
}

bool cdce_selftest_25mhz() {
  FreqCount.begin(100);
  delay(150);
  if (!FreqCount.available()) {
    Serial.println("CDCE913: self-test unavailable (pin not connected?)");
    return false;
  }
  uint32_t f = FreqCount.read();
  Serial.print("CDCE913 measured: "); Serial.println(f);
  return (f > 24990000UL && f < 25010000UL);
}
