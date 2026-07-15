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
// Hardware pin mapping — Teensy 4.1 on the v5 DIG eurocard
// (universal DIN 41612 Type M backplane, one pinout per slot)
//
// SPI0 (pins 11/12/13): AD9833 x4 (2 drive DDS + 2 reference DDS)
// SPI1 (pins 26/27/39): ILI9341 TFT          (dedicated display bus)
// I2C0 (pins 18/19):    SYSTEM BUS — CDCE913 (on-card) + per-card
//                        AD5696R DAC / ADS1115 ADC / PCA9538 expander
//                        on the CH and AFE cards via backplane b18/b19.
//                        Teensy is the only master; pullups R112/R113
//                        (4.7k, Clock section) are the only bus pullups.
//
// v5 CHANGES vs the v3 single-board map:
//   AD5064 SPI DAC        -> REMOVED (per-CH-card AD5696R over I2C)
//   analog monitor inputs -> REMOVED (per-card ADS1115 over I2C)
//   LOCKn_EN GPIOs        -> REMOVED (PCA9538 IO0 on each CH card)
//   HBRIDGE_FAULT1/2      -> replaced by ONE shared open-drain FAULT_n
//   GA0..GA2 slot straps  -> read on freed pins 14/15/16
//   ERRn_BUS stays ANALOG on A10/A11 (1 kHz supervisor sampling)
//
// I2C NOISE RULES (see Backplane_Slots + AFE/CH sheets):
//   telemetry poll <= 10 Hz, round-robin, and NEVER while a channel
//   is in ACQUIRE. The bus is static between transactions.
// ============================================================

// --- I2C0 (system bus) --------------------------------------
#define PIN_SDA           18
#define PIN_SCL           19

// --- Backplane geographic address (slot ID straps) ----------
// Freed analog pins re-used as digital inputs (schematic: DIG_Card).
#define PIN_GA0           14   // was A0 / NTC1 monitor
#define PIN_GA1           15   // was A1 / TEC1 current monitor
#define PIN_GA2           16   // was A2 / LAS1 current monitor

// --- Shared fault line (open-drain wired-OR, active-low) ----
// Any card may assert; source identified via that card's PCA9538.
// External 10k pullup R305 on the DIG card; internal pullup as backup.
#define PIN_FAULT_N       31   // was HBRIDGE_FAULT1

// ============================================================
// I2C address map (7-bit) — from the backplane GA strap table:
//   slot 6 = CH laser 1 (GA2..0 = 101), slot 7 = CH laser 2 (110),
//   slot 5 = AFE (100).  ADS1115 ADDR pin <- GA0 on CH cards;
//   the AFE ADS1115 straps ADDR->SDA on-card (0x4A) to avoid the
//   CH-card 0x48/0x49 range.  VERIFY AD5696R/PCA9538 base addresses
//   against the datasheets before bring-up.
// ============================================================
#define I2C_CDCE913_ADDR     0x65          // fixed, on the DIG card
#define I2C_ADS1115_CH1      0x49          // CH laser-1 card (GA0 = 1)
#define I2C_ADS1115_CH2      0x48          // CH laser-2 card (GA0 = 0)
#define I2C_ADS1115_AFE      0x4A          // AFE card (ADDR -> SDA)
#define I2C_AD5696_CH1       0x0D          // base 0x0C | GA0
#define I2C_AD5696_CH2       0x0C
#define I2C_PCA9538_CH1      0x71          // base 0x70 | GA1<<1 | GA0 (slot 6: 01)
#define I2C_PCA9538_CH2      0x72          // (slot 7: 10)

// PCA9538 IO assignment on each CH card (see CH_Card schematic):
//   IO0 = LOCK_EN (output: HIGH = integrator RUN)
//   IO1 = HB_FAULT (input, active-low from OPA551 Flag)
//   IO2 = GA2 readback, IO3..IO7 spare inputs

// --- SPI0 (signal-path) — MOSI=11, MISO=12, SCK=13 ---------
#define PIN_SCK           13
#define PIN_MOSI          11
#define PIN_MISO          12

// AD9833 DDS chips
#define PIN_AD9833_1_CS    2    // DDS1_FSYNC — CH1 DRIVE (Ω, to bias-tee injection)
#define PIN_AD9833_2_CS    3    // DDS2_FSYNC — CH2 DRIVE (Ω)
#define PIN_AD9833_1_RESET 4
#define PIN_AD9833_2_RESET 5
// Reference DDS (Option B): phase-set demod LO, one per channel.
// All four AD9833 share the one 25 MHz MCLK → drive/reference phase-coherent.
#define PIN_AD9833_3_CS    6    // DDS3_FSYNC — CH1 REFERENCE (demod LO → LO1_REF)
#define PIN_AD9833_4_CS    8    // DDS4_FSYNC — CH2 REFERENCE (demod LO → LO2_REF)

// Analog PI integrator hold/run (lock enable) — one per channel.
// v5: no longer a Teensy GPIO. Driven via each CH card's PCA9538 IO0
// (chcard_set_lock_en). HIGH = RUN (loop closed), LOW = HOLD/RESET.
// (v3 pins 33/34 are now free GPIO.)

// OPTIONAL — dedicated analog-PI-output taps for integrator centering
// (anti-windup off-load loop). Leave undefined to disable centering;
// firmware then leaves the DC setpoint fixed after acquisition.
// #define PIN_LAS1_CORR_MON  A14   // pin 38
// #define PIN_LAS2_CORR_MON  A16   // pin 40

// Setpoint DACs — v5: one AD5696R per CH card over I2C (see address
// map above). Channel A = laser current setpoint, channel B = TEC
// setpoint (RC-filtered on-card). The v3 AD5064 (SPI, pins 10/7) is
// gone; pins 10 and 7 are free GPIO.

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

// --- Analog inputs ------------------------------------------
// v5: all slow monitors (NTC, TEC/laser IMON, MPD, PD level) moved to
// the per-card ADS1115s — read via chcard_poll()/afe_poll(). Only the
// fast post-mix baseband error stays on the Teensy ADC (ERRn_BUS,
// backplane c13/c14):
#define PIN_LOCK1_IN      A10  // pin 24 — CH1 baseband error (ERR1_BUS)
#define PIN_LOCK2_IN      A11  // pin 25 — CH2 baseband error (ERR2_BUS)
// FLAG: ERRn_BUS still reaches these pins unconditioned, as in v3 —
// verify level/offset against the 3V3 ADC range during bring-up.

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
// v5: read as VOLTS from the CH-card ADS1115 (AIN3); the 10k pullup
// (R_NTC1) to 3V3 lives on the CH card.
// ============================================================
#define NTC_R_SERIES      10000.0f  // series pull-up resistor (Ω)
#define NTC_R25           10000.0f  // NTC resistance at 25 °C
#define NTC_BETA          3950.0f
#define NTC_VSUP          3.3f      // pullup supply on the CH card

// ============================================================
// Laser IMON scaling — adjust to match schematic gain
// v5: monitors arrive as VOLTS from the ADS1115.
// (v3 was 0.0806 mA/LSB at 3.3V/4095 → 100 mA per volt.)
// ============================================================
#define LASER_IMON_MA_PER_V   100.0f

// ============================================================
// Telemetry (I2C) cadence — noise rules from the backplane sheet:
// round-robin one ADS1115 channel per card per display tick (10 Hz),
// full 4-channel refresh every 400 ms; polling is SUSPENDED while
// either channel is in ACQUIRE.
// ============================================================
#define TELEM_PERIOD_MS   100UL

// ============================================================
// TEC current monitor scaling
// Hardware: INA826 (G=7.2, Rg=8.06k), shunt R=0.1Ω
//   V_IMON = TEC_IMON_OFFSET_V + TEC_IMON_V_PER_A * I_amps
//   At 0 A  → ~1.20 V  (INA826 REF pin)
//   At 2.5 A → ~3.00 V  (Vmax from schematic annotation)
// DAC zero-current code for the ≈1.20V virtual-zero. v5: AD5696R with
// internal 2.5V ref × gain 2 → 5V span, so the code is unchanged from
// the AD5064 (5V ref) value — VERIFY the GAIN pin strap on the CH card:
//   code 15728 = 0 A  (1.20 / 5.0 × 65535 ≈ 15728)
// The +2.5 A (cooling) / -1.5 A (heating) endpoint codes are NOT asserted
// here: they depend on the R44/R45 and R40/R43 scaler around the 1.20 V
// virtual zero and must be calibrated in firmware against measured TEC
// current (see volts_to_tec_amps / IMON readback). Regenerate the
// current↔code table for VZERO=1.20V; the old table assumed VZERO=1.875V.
// ============================================================
#define TEC_IMON_OFFSET_V    1.20f   // V_IMON at 0 A
#define TEC_IMON_V_PER_A     0.72f   // INA826 gain × shunt = 7.2 × 0.1
#define TEC_ILIMIT_A         2.5f    // software current limit (amps)
#define TEC_DAC_ZERO_CODE    15728u  // DAC code for 0 A TEC current (VOUTC≈1.20V, 5V ref)
#define TEC_ILIMIT_STEP      0.05f   // limit-factor reduction per tick when over-current
#define TEC_ILIMIT_RESTORE   0.005f  // limit-factor restoration per tick when under-current

// ============================================================
// RF modulation / clock
//
// Drive + reference DDS run at the per-channel modulation frequency Ω
// (just above the DFB thermal/carrier FM crossover, ~1–3 MHz). Channels
// use slightly different Ω so cross-channel tones stay out of the beat.
// Final Ω set after measuring the per-device crossover. The reference
// DDS phase (RF_PHASEn_DEG) is the demod phase — calibrate per channel.
// ============================================================
#define RF_OMEGA1_HZ      2000000.0f   // CH1 modulation/demod Ω
#define RF_OMEGA2_HZ      2500000.0f   // CH2 modulation/demod Ω
#define RF_PHASE1_DEG     0.0f         // CH1 demod phase (run 'cal1' to set)
#define RF_PHASE2_DEG     0.0f         // CH2 demod phase (run 'cal2' to set)
#define REFCLK_HZ         25000000.0f

// Legacy: kept for the web/JSON "dither" field and the 'dither' test
// command. Now holds the representative (CH1) modulation frequency.
#define DITHER_FREQ_HZ    RF_OMEGA1_HZ

// Acquisition sweep + lock supervisor
#define SWEEP_STEP_CODE   24      // DAC code increment per control tick during SEARCH
#define PHASE_CAL_STEP_DEG 15     // reference-phase step for 'caln'
#define PHASE_CAL_SETTLE_MS 5     // settle before sampling at each phase
#define PHASE_CAL_SAMPLES  64     // error samples averaged per phase step

// ============================================================
// H-Bridge / card faults — v5: the OPA551 Flag lands on each CH
// card's PCA9538 IO1; the expander's ~INT drives the shared FAULT_n
// line (PIN_FAULT_N above). On FAULT_n LOW the supervisor queries
// each card's expander to identify the source (chcard_fault()).
// (v3 PIN_HBRIDGE_FAULT1/2 on 31/32 are gone; 32 is free GPIO.)
// ============================================================

// ============================================================
// Control / display timing
// ============================================================
#define CONTROL_PERIOD_US 1000UL    // 1 kHz
#define DISPLAY_PERIOD_MS  100UL    // 10 Hz
#define SCAN_PERIOD_MS    5000UL    // simulated sweep cycle for display
#define DEBUG_PERIOD_MS   1000UL
