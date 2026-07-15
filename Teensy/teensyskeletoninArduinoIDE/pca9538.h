#pragma once
#include <Arduino.h>

// PCA9538 — 8-bit I2C IO expander (one per CH card, v5 universal bus).
// Address 0x70 | A1<<1 | A0 (A0<-GA0, A1<-GA1 on-card).
// Registers: 0=input, 1=output, 2=polarity, 3=config (1=input).
// ~INT (open-drain) drives the shared backplane FAULT_n line.

#define PCA9538_REG_INPUT   0
#define PCA9538_REG_OUTPUT  1
#define PCA9538_REG_CONFIG  3

bool    pca9538_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
// Returns register value, or -1 on I2C error.
int16_t pca9538_read_reg(uint8_t addr, uint8_t reg);
