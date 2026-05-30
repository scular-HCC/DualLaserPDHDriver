#pragma once
#include <stdint.h>

// ============================================================
// AD5064 4-channel 16-bit DAC — SPI, 24-bit frames
// Channel assignment:
//   0 — CH1 laser current setpoint
//   1 — CH2 laser current setpoint
//   2 — CH1 TEC setpoint (reserved)
//   3 — CH2 TEC setpoint (reserved)
// ============================================================

void ad5064_write_channel(uint8_t ch, uint16_t code);
void ad5064_set_midscale();
