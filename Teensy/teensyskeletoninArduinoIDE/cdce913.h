#pragma once
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// CDCE913 PLL clock synthesiser — I2C address 0x65
// Configured: 10 MHz input → 25 MHz output
// ============================================================

#define CDCE_ADDR  0x65

void    cdce_program_if_needed();
void    cdce_wait_for_lock();
bool    cdce_pll_locked();
bool    cdce_selftest_25mhz();
