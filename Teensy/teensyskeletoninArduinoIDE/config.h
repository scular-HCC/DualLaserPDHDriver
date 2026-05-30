#pragma once

// ============================================================
// Hardware pin mapping — matches Dual-PDH schematic
// ============================================================

// I2C
#define PIN_SDA           18
#define PIN_SCL           19

// SPI
#define PIN_SCK           13
#define PIN_MOSI          11

// AD9833 DDS chips
#define PIN_AD9833_1_CS    2    // DDS1_FSYNC
#define PIN_AD9833_2_CS    3    // DDS2_FSYNC
#define PIN_AD9833_1_RESET 5
#define PIN_AD9833_2_RESET 6

// AD5064 setpoint DAC
#define PIN_AD5064_CS     10    // CS_3V3
#define PIN_AD5064_LDAC    7
#define PIN_AD5064_CLR     8

// ILI9341 TFT — free SPI pins
#define PIN_TFT_CS        15
#define PIN_TFT_DC        14
#define PIN_TFT_RST        4
// Backlight: wire through 100 Ω to VIN; not software-controlled.

// Optional CDCE913 CLKOUT0 frequency self-test
#define PIN_FREQ_TEST      9

// Analog inputs
#define PIN_LOCK1_IN      A0    // PDH error CH1
#define PIN_LOCK2_IN      A1    // PDH error CH2
#define PIN_MPD1_MON      A2
#define PIN_MPD2_MON      A3
#define PIN_LAS1_IMON     A4    // Laser 1 current monitor
#define PIN_LAS2_IMON     A5    // Laser 2 current monitor
#define PIN_TEC1_IMON     A6
#define PIN_TEC2_IMON     A7
#define PIN_NTC1_MON      A8    // TEC thermistor CH1
#define PIN_NTC2_MON      A9    // TEC thermistor CH2

// ============================================================
// ADC / DAC constants
// ============================================================
#define ADC_BITS          12
#define ADC_MAX           4095
#define ADC_MID           (ADC_MAX / 2)
#define ADC_REF_V         3.3f
#define DAC_MID_CODE      0x8000u

// ============================================================
// NTC thermistor (beta equation) — tune to your parts
// ============================================================
#define NTC_R_SERIES      10000.0f  // series pull-up resistor (Ω)
#define NTC_R25           10000.0f  // NTC resistance at 25 °C
#define NTC_BETA          3950.0f

// ============================================================
// Laser IMON scaling — adjust to match schematic gain
// mA per ADC LSB so that full-scale (4095) = ~330 mA
// ============================================================
#define LASER_IMON_MA_PER_LSB  0.0806f

// ============================================================
// Dither / clock
// ============================================================
#define DITHER_FREQ_HZ    10000.0f
#define REFCLK_HZ         25000000.0f

// ============================================================
// Control / display timing
// ============================================================
#define CONTROL_PERIOD_US 1000UL    // 1 kHz
#define DISPLAY_PERIOD_MS  100UL    // 10 Hz
#define SCAN_PERIOD_MS    5000UL    // simulated sweep cycle for display
#define DEBUG_PERIOD_MS   1000UL
