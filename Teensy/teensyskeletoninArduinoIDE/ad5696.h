#pragma once
#include <Arduino.h>

// AD5696R — quad 16-bit I2C DAC (one per CH card, v5 universal bus).
// Internal 2.5 V reference, gain 2 → 5 V span (matches the old AD5064
// 5 V-ref codes). Address = 0x0C | A0, A0 strapped from GA0 on-card.
// Command 0b0011 = write-and-update; DAC select is a 4-bit mask
// (A=1, B=2, C=4, D=8). VERIFY addressing against the datasheet.

#define AD5696_CMD_WRITE_UPDATE  0x30
#define AD5696_DAC_A  0x1   // laser current setpoint
#define AD5696_DAC_B  0x2   // TEC setpoint
#define AD5696_DAC_C  0x4   // spare
#define AD5696_DAC_D  0x8   // spare

// Returns true on I2C ACK.
bool ad5696_write(uint8_t addr, uint8_t dac_mask, uint16_t code);
