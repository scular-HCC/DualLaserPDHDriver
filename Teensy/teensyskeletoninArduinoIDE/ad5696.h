#pragma once
#include <Arduino.h>

// AD5696R — quad 16-bit I2C DAC. Three in the crate: one per CH card
// plus one on the AFE card. Address = 0x0C | A1<<1 | A0 (base 00011,
// verified vs ADI Rev E datasheet Table 9).
// Command 0b0011 = write-and-update; DAC select is a 4-bit mask
// (A=1, B=2, C=4, D=8).
//
// NOTE — the two cards strap the GAIN pin DIFFERENTLY, so a code does
// NOT mean the same voltage on both:
//   CH cards (0x0C/0x0D): GAIN -> VLOGIC, gain 2 -> 0..5 V span
//                         (matches the old AD5064 5 V-ref codes).
//   AFE card (0x0E)     : GAIN -> GND,    gain 1 -> 0..2.5 V span.
// Do not copy CH-card code words to the AFE expecting the same volts.

#define AD5696_CMD_WRITE_UPDATE  0x30
#define AD5696_DAC_A  0x1   // CH: laser current setpoint | AFE: ERR1 offset null
#define AD5696_DAC_B  0x2   // CH: TEC setpoint           | AFE: ERR2 offset null
#define AD5696_DAC_C  0x4   // spare
#define AD5696_DAC_D  0x8   // spare

// Returns true on I2C ACK.
bool ad5696_write(uint8_t addr, uint8_t dac_mask, uint16_t code);
