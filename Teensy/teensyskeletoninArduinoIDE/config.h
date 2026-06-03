#pragma once

// ============================================================
// DEMO MODE DEFAULT (first-boot only)
// Demo mode is now a runtime setting stored in EEPROM.
// Use the command  'demo on'  or  'demo off'  via USB serial,
// Telnet, or the web dashboard — no recompile required.
//
// This value is only written to EEPROM when the device boots
// with a blank or invalid EEPROM (first flash after firmware
// update). Change it here if you want demo OFF as the factory
// default. After the first boot it has no effect.
// ============================================================
#define DEMO_MODE_DEFAULT  1   // 1 = demo on, 0 = real hardware

// ============================================================
// Hardware pin mapping — Teensy 4.1
//
// SPI0 (pins 11/12/13): AD5064 + AD9833 x2  (signal-path chips)
// SPI1 (pins 26/27/39): ILI9341 TFT          (dedicated display bus)
// I2C0 (pins 18/19):    CDCE913 clock gen
// ============================================================

// --- I2C0 ---------------------------------------------------
#define PIN_SDA           18
#define PIN_SCL           19

// --- SPI0 (signal-path) — MOSI=11, MISO=12, SCK=13 ---------
#define PIN_SCK           13
#define PIN_MOSI          11
#define PIN_MISO          12

// AD9833 DDS chips
#define PIN_AD9833_1_CS    2    // DDS1_FSYNC
#define PIN_AD9833_2_CS    3    // DDS2_FSYNC
#define PIN_AD9833_1_RESET 4
#define PIN_AD9833_2_RESET 5

// AD5064 setpoint DAC  (pin 10 = hardware CS0 on SPI0)
#define PIN_AD5064_CS     10
#define PIN_AD5064_CLR     7    // active-low async zero; driven HIGH normally

// CDCE913 clock-output frequency self-test
// FreqCount library on Teensy 4.1 is hardwired to pin 9 — do not change
#define PIN_FREQ_TEST      9

// --- SPI1 (display) — MOSI=26, MISO=39, SCK=27 -------------
// All five TFT signals are on adjacent pins for clean PCB routing.
#define PIN_TFT_MOSI      26   // SPI1 MOSI
#define PIN_TFT_SCK       27   // SPI1 SCK
#define PIN_TFT_CS        28
#define PIN_TFT_DC        29   // data/command select — required
#define PIN_TFT_RST       30
// Backlight: wire through 100 Ω to 3.3 V; not software-controlled.

// --- Analog inputs (pins 14-25 = A0-A11) --------------------
#define PIN_NTC1_MON      A0   // pin 14 — TEC thermistor CH1
#define PIN_TEC1_IMON     A1   // pin 15
#define PIN_LAS1_IMON     A2   // pin 16 — Laser 1 current monitor
#define PIN_MPD1_MON      A3   // pin 17
// pins 18/19: I2C (A4/A5 not available while Wire is active)
#define PIN_NTC2_MON      A6   // pin 20 — TEC thermistor CH2
#define PIN_TEC2_IMON     A7   // pin 21
#define PIN_LAS2_IMON     A8   // pin 22 — Laser 2 current monitor
#define PIN_MPD2_MON      A9   // pin 23
#define PIN_LOCK1_IN      A10  // pin 24 — PDH error CH1
#define PIN_LOCK2_IN      A11  // pin 25 — PDH error CH2

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
// TEC current monitor scaling
// Hardware: INA826 (G=7.2, Rg=8.06k), shunt R=0.1Ω
//   V_IMON = TEC_IMON_OFFSET_V + TEC_IMON_V_PER_A * I_amps
//   At 0 A  → ~1.20 V  (INA826 REF pin)
//   At 2.5 A → ~3.00 V  (Vmax from schematic annotation)
// DAC zero-current code from TEC_VZERO=1.875V annotation:
//   code 24576 = 0 A, code 0 = +2.5 A cooling, code 65535 = -1.5 A heating
// ============================================================
#define TEC_IMON_OFFSET_V    1.20f   // V_IMON at 0 A
#define TEC_IMON_V_PER_A     0.72f   // INA826 gain × shunt = 7.2 × 0.1
#define TEC_ILIMIT_A         2.5f    // software current limit (amps)
#define TEC_DAC_ZERO_CODE    24576u  // DAC code for 0 A TEC current
#define TEC_ILIMIT_STEP      0.05f   // limit-factor reduction per tick when over-current
#define TEC_ILIMIT_RESTORE   0.005f  // limit-factor restoration per tick when under-current

// ============================================================
// Dither / clock
// ============================================================
#define DITHER_FREQ_HZ    10000.0f
#define REFCLK_HZ         25000000.0f

// ============================================================
// H-Bridge fault inputs (OPA551 Flag pin — open-drain active-low)
// Pulled HIGH internally; LOW = fault asserted.
// Pins 31/32 are free GPIO on Teensy 4.1, interrupt-capable, not CAN-in-use.
// ============================================================
#define PIN_HBRIDGE_FAULT1   31
#define PIN_HBRIDGE_FAULT2   32

// ============================================================
// Control / display timing
// ============================================================
#define CONTROL_PERIOD_US 1000UL    // 1 kHz
#define DISPLAY_PERIOD_MS  100UL    // 10 Hz
#define SCAN_PERIOD_MS    5000UL    // simulated sweep cycle for display
#define DEBUG_PERIOD_MS   1000UL
