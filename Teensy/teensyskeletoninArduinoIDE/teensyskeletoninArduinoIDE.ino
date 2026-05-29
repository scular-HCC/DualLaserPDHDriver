/* Dual-PDH Controller for Teensy 4.1
   - Two PDH locks (Laser1, Laser2)
   - CDCE913 (I2C) safe init + PLL lock + 25MHz self-test
   - Two AD9833 DDS (SPI) for dither
   - AD5064 4-channel DAC (SPI) for setpoints
   - State machine: IDLE, SEARCH, ACQUIRE, LOCKED, HOLD, RELOCK
   - Runtime serial console for status and PID tuning
   - Non-blocking, production-style code
   Author: Generated for Stefan
*/

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FreqCount.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// -----------------------------
// Pin mapping (matches your schematic)
// -----------------------------
#define PIN_SDA        18
#define PIN_SCL        19

#define PIN_SCK        13
#define PIN_MOSI       11

#define PIN_AD9833_1_CS   2    // DDS1_FSYNC
#define PIN_AD9833_2_CS   3    // DDS2_FSYNC
#define PIN_AD5064_CS     10   // CS_3V3

#define PIN_AD9833_1_RESET 5
#define PIN_AD9833_2_RESET 6

#define PIN_AD5064_LDAC   7
#define PIN_AD5064_CLR    8

#define PIN_TFT_CS        15   // free SPI chip select for ILI9341
#define PIN_TFT_DC        14   // free data/command pin
#define PIN_TFT_RST       4    // free reset pin
// ILI9341 LED (backlight) pin must be connected to VIN through a 100 ohm resistor — not handled in software.

#define PIN_FREQ_TEST     9    // optional: CDCE913 CLKOUT0 -> pin 9 for FreqCount

// Analog inputs (from your schematic nets)
#define PIN_LOCK1_IN   A0
#define PIN_LOCK2_IN   A1
#define PIN_MPD1_MON   A2
#define PIN_MPD2_MON   A3
#define PIN_LAS1_IMON  A4
#define PIN_LAS2_IMON  A5
#define PIN_TEC1_IMON  A6
#define PIN_TEC2_IMON  A7
#define PIN_NTC1_MON   A8
#define PIN_NTC2_MON   A9

// -----------------------------
// CDCE913 config (10MHz -> 25MHz) with version byte
// -----------------------------
#define CDCE_ADDR 0x65
uint8_t cdce_image[32] = {
  // 0x00..0x1F
  // 0x00: control / output enable bits (we set PLL enable + outputs enabled)
  // 0x01: M
  // 0x02: N
  // 0x03: P (unused)
  // 0x04: Q (output divider for CLKOUT0..2; we'll set per-output dividers below)
  // 0x05..0x0B: fractional / reserved
  // 0x0C..0x0E: phase registers for CLKOUT0..2 (set to 0 for phase alignment)
  // 0x0F..0x1E: reserved / defaults
  // 0x1F: version byte
  0x00, // 0x00 control placeholder (we will set bits below)
  0x0A, // 0x01 M = 10
  0x01, // 0x02 N = 1
  0x00, // 0x03 P = 0
  0x04, // 0x04 Q = 4 (will be applied to outputs)
  0x00, // 0x05 fractional MSB
  0x00, // 0x06 fractional LSB
  0x00, // 0x07 SSC / reserved
  0x00, // 0x08
  0x00, // 0x09
  0x00, // 0x0A
  0x00, // 0x0B
  0x00, // 0x0C phase CLKOUT0 = 0
  0x00, // 0x0D phase CLKOUT1 = 0
  0x00, // 0x0E phase CLKOUT2 = 0
  0x00, // 0x0F
  0x00, // 0x10
  0x00, // 0x11
  0x00, // 0x12
  0x00, // 0x13
  0x00, // 0x14
  0x00, // 0x15
  0x00, // 0x16
  0x00, // 0x17
  0x00, // 0x18
  0x00, // 0x19
  0x00, // 0x1A
  0x00, // 0x1B
  0x00, // 0x1C
  0x00, // 0x1D
  0x00, // 0x1E
  0x51  // 0x1F VERSION BYTE (0x51 chosen; change when you change config)
};
// -----------------------------
// Control loop timing
// -----------------------------
const unsigned long CONTROL_PERIOD_US = 1000; // 1 kHz control loop
const unsigned long DISPLAY_PERIOD_MS = 100;  // 10 Hz display refresh
elapsedMicros loopTimer;

// -----------------------------
// PID structure and defaults
// -----------------------------
struct PID {
  float kp;
  float ki;
  float kd;
  float integrator;
  float prev_err;
  float out_min;
  float out_max;
  float integrator_limit;
};

enum LockState {
  IDLE,
  SEARCH,
  ACQUIRE,
  LOCKED,
  HOLD,
  RELOCK
};

struct LockChannel;
void lock_enter_state(LockChannel &ch, LockState s);

PID pid1 = {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f};
PID pid2 = {0.08f, 0.5f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.5f};

// Map PID output (-1..1) to DAC code (0..65535)
inline uint16_t pid_to_dac(float u) {
  if (u > 1.0f) u = 1.0f;
  if (u < -1.0f) u = -1.0f;
  return (uint16_t)((u * 0.5f + 0.5f) * 65535.0f);
}

// -----------------------------
// Lock state machine
// -----------------------------
struct LockChannel {
  LockState state;
  unsigned long state_ts;      // timestamp when entered state (ms)
  unsigned int relock_attempts;
  unsigned long last_error_time;
  bool locked;
  // thresholds (tune for your PDH electronics)
  float lock_threshold;        // absolute error below which we consider locked
  float acquire_threshold;     // threshold used during acquire
  unsigned long acquire_timeout_ms;
  unsigned long relock_backoff_ms;
};

LockChannel ch1, ch2;
Adafruit_ILI9341 tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST);

uint16_t lastSet1 = 0;
uint16_t lastSet2 = 0;
int lastRead1 = 0;
int lastRead2 = 0;
float lastErr1 = 0.0f;
float lastErr2 = 0.0f;
float lastTemp1 = 0.0f;
float lastTemp2 = 0.0f;
uint16_t lastTecSet1 = 0x8000;
uint16_t lastTecSet2 = 0x8000;
int lastTecRead1 = 0;
int lastTecRead2 = 0;

// -----------------------------
// ADC scaling and offsets
// (adjust to your mixer and ADC front-end)
// -----------------------------
const float ADC_REF_V = 3.3f;
const int ADC_MAX = 4095; // Teensy 4.1 12-bit ADC returns 0..4095
const int ADC_MID = ADC_MAX / 2;
const uint16_t DAC_MID_CODE = 0x8000;
const float DITHER_FREQ_HZ = 10000.0f;
const float REFCLK_HZ = 25000000.0f;

// Convert raw ADC to normalized error (-1..1) assuming midscale carrier
inline float adc_to_error(int raw) {
  return (raw - ADC_MID) / (float)ADC_MID;
}

// NTC thermistor beta equation. NTC is the bottom leg of a voltage divider with
// NTC_R_SERIES as the pull-up to VREF. Tune these three constants to your parts.
const float NTC_R_SERIES = 10000.0f;  // series resistor (ohms)
const float NTC_R25      = 10000.0f;  // NTC resistance at 25 C (ohms)
const float NTC_BETA     = 3950.0f;   // NTC beta coefficient

inline float ntc_to_celsius(int raw) {
  if (raw <= 0 || raw >= ADC_MAX) return NAN;
  float ratio = raw / (float)ADC_MAX;
  float r = NTC_R_SERIES * ratio / (1.0f - ratio);
  float t_k = 1.0f / (1.0f / 298.15f + logf(r / NTC_R25) / NTC_BETA);
  return t_k - 273.15f;
}

// -----------------------------
// CDCE913 helpers (safe EEPROM programming)
// -----------------------------
uint8_t cdce_read_eeprom_byte(uint8_t offset) {
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x40 + offset);
  Wire.endTransmission();
  Wire.requestFrom(CDCE_ADDR, 1);
  if (Wire.available()) return Wire.read();
  return 0xFF;
}

bool cdce_needs_programming() {
  return cdce_read_eeprom_byte(0x1F) != cdce_image[31];
}

void cdce_program_eeprom() {
  // Write 0x00..0x1F to RAM
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x00);
  for (int i = 0; i < 32; ++i) Wire.write(cdce_image[i]);
  Wire.endTransmission();
  delay(5);
  // Commit to EEPROM
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x86);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(20);
}

bool cdce_pll_locked() {
  Wire.beginTransmission(CDCE_ADDR);
  Wire.write(0x82);
  Wire.endTransmission();
  Wire.requestFrom(CDCE_ADDR, 1);
  if (Wire.available()) {
    uint8_t s = Wire.read();
    return (s & 0x01);
  }
  return false;
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
  Serial.println("CDCE913: PLL LOCKED");
}

void cdce_program_if_needed() {
  if (!cdce_needs_programming()) {
    Serial.println("CDCE913: EEPROM OK, skipping program");
    return;
  }
  Serial.println("CDCE913: Programming EEPROM...");
  cdce_program_eeprom();
  Serial.println("CDCE913: EEPROM programmed");
}

bool cdce_selftest_25mhz() {
  // Requires CDCE913 CLKOUT0 wired to PIN_FREQ_TEST.
  // If the pin is not connected, report the failure explicitly instead of silently passing.
  FreqCount.begin(100); // 100 ms gate
  delay(150);
  if (!FreqCount.available()) {
    Serial.println("CDCE913: 25 MHz self-test unavailable (no measurement)");
    return false;
  }

  uint32_t f = FreqCount.read();
  Serial.print("CDCE913 measured: ");
  Serial.println(f);
  return (f > 24990000UL && f < 25010000UL);
}

// -----------------------------
// AD9833 helpers (SPI)
// -----------------------------
void ad9833_write(uint8_t cs_pin, uint16_t word) {
  digitalWrite(cs_pin, LOW);
  SPI.transfer16(word);
  digitalWrite(cs_pin, HIGH);
}

void ad9833_reset(uint8_t cs_pin, uint8_t rst_pin) {
  digitalWrite(rst_pin, HIGH);
  delay(1);
  digitalWrite(rst_pin, LOW);
  delay(1);
  // Put into reset state
  ad9833_write(cs_pin, 0x0100);
}

void ad9833_set_freq(uint8_t cs_pin, float freq_hz, float refclk_hz) {
  // 28-bit frequency word
  uint32_t word = (uint32_t)((freq_hz * (1ULL << 28)) / refclk_hz);
  uint16_t lsb = 0x4000 | (word & 0x3FFF);
  uint16_t msb = 0x4000 | ((word >> 14) & 0x3FFF);
  // B28=1, RESET=0
  ad9833_write(cs_pin, 0x2100);
  ad9833_write(cs_pin, lsb);
  ad9833_write(cs_pin, msb);
  ad9833_write(cs_pin, 0x2000); // clear RESET
}

// -----------------------------
// AD5064 helpers (24-bit frames)
// -----------------------------
void ad5064_write_channel(uint8_t channel, uint16_t code) {
  if (channel > 3) return;

  // Clamp to the 16-bit DAC code range.
  uint16_t value = code;
  if (value > 0xFFFF) value = 0xFFFF;

  uint8_t cmd = 0x3; // write & update
  uint8_t addr = channel & 0x3;
  uint8_t b0 = (cmd << 4) | addr;
  uint8_t b1 = (value >> 8) & 0xFF;
  uint8_t b2 = value & 0xFF;

  digitalWrite(PIN_AD5064_CS, LOW);
  SPI.transfer(b0);
  SPI.transfer(b1);
  SPI.transfer(b2);
  digitalWrite(PIN_AD5064_CS, HIGH);
}

void ad5064_set_midscale() {
  ad5064_write_channel(0, DAC_MID_CODE);
  ad5064_write_channel(1, DAC_MID_CODE);
  ad5064_write_channel(2, DAC_MID_CODE);
  ad5064_write_channel(3, DAC_MID_CODE);
}

// -----------------------------
// PID step (anti-windup, derivative on measurement)
// -----------------------------
float pid_step(PID &p, float err, float dt) {
  // Integrator with clamping
  p.integrator += err * p.ki * dt;
  if (p.integrator > p.integrator_limit) p.integrator = p.integrator_limit;
  if (p.integrator < -p.integrator_limit) p.integrator = -p.integrator_limit;

  float deriv = 0.0f;
  if (dt > 0.0f) deriv = (err - p.prev_err) / dt;

  float out = p.kp * err + p.integrator + p.kd * deriv;

  // Output clamp
  if (out > p.out_max) out = p.out_max;
  if (out < p.out_min) out = p.out_min;

  p.prev_err = err;
  return out;
}

// -----------------------------
// Lock state machine helpers
// -----------------------------
void lock_enter_state(LockChannel &ch, LockState s) {
  ch.state = s;
  ch.state_ts = millis();
}

const char* state_name(LockState s) {
  switch (s) {
    case IDLE: return "IDLE";
    case SEARCH: return "SEARCH";
    case ACQUIRE: return "ACQUIRE";
    case LOCKED: return "LOCKED";
    case HOLD: return "HOLD";
    case RELOCK: return "RELOCK";
    default: return "?";
  }
}

void update_display() {
  tft.fillScreen(ILI9341_BLACK);
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.println("Dual-PDH Status");
  tft.println("10 Hz refresh");
  tft.println();

  tft.setTextColor(ILI9341_CYAN);
  tft.print("L1 set:"); tft.print(lastSet1); tft.print(" / "); tft.print(lastSet1 * 100.0f / 65535.0f, 1); tft.println("% ");
  tft.print("L1 read:"); tft.print(lastRead1); tft.print(" ADC"); tft.println();
  tft.print("L1 err:"); tft.print(lastErr1, 3); tft.println();
  tft.print("State:"); tft.println(state_name(ch1.state));
  tft.print("Temp1:");
  if (isnan(lastTemp1)) tft.println(" --- C");
  else { tft.print(lastTemp1, 1); tft.println(" C"); }
  tft.print("TEC1 set:"); tft.print(lastTecSet1 * 100.0f / 65535.0f, 1);
  tft.print("% rd:"); tft.println(lastTecRead1);
  tft.println();

  tft.setTextColor(ILI9341_YELLOW);
  tft.print("L2 set:"); tft.print(lastSet2); tft.print(" / "); tft.print(lastSet2 * 100.0f / 65535.0f, 1); tft.println("% ");
  tft.print("L2 read:"); tft.print(lastRead2); tft.print(" ADC"); tft.println();
  tft.print("L2 err:"); tft.print(lastErr2, 3); tft.println();
  tft.print("State:"); tft.println(state_name(ch2.state));
  tft.print("Temp2:");
  if (isnan(lastTemp2)) tft.println(" --- C");
  else { tft.print(lastTemp2, 1); tft.println(" C"); }
  tft.print("TEC2 set:"); tft.print(lastTecSet2 * 100.0f / 65535.0f, 1);
  tft.print("% rd:"); tft.println(lastTecRead2);
}

void lock_init_channels() {
  ch1.state = IDLE;
  ch1.state_ts = millis();
  ch1.relock_attempts = 0;
  ch1.locked = false;
  ch1.lock_threshold = 0.02f;      // tune: normalized error threshold
  ch1.acquire_threshold = 0.1f;
  ch1.acquire_timeout_ms = 2000;
  ch1.relock_backoff_ms = 500;

  ch2 = ch1; // same defaults for channel 2
}

// -----------------------------
// Serial console: simple commands
// - 's' : status
// - 'p1 kp ki kd' : set pid1 gains
// - 'p2 kp ki kd' : set pid2 gains
// - 'r' : reprogram CDCE913 EEPROM (force)
// - 'd' : dump ADC readings
// -----------------------------
void serial_process() {
  if (!Serial.available()) return;
  String line = Serial.readStringUntil('\n');
  line.trim();
  if (line.length() == 0) return;

  if (line == "s") {
    Serial.print("CH1 state: "); Serial.print((int)ch1.state);
    Serial.print(" locked="); Serial.print(ch1.locked);
    Serial.print(" relock_attempts="); Serial.println(ch1.relock_attempts);
    Serial.print("CH2 state: "); Serial.print((int)ch2.state);
    Serial.print(" locked="); Serial.print(ch2.locked);
    Serial.print(" relock_attempts="); Serial.println(ch2.relock_attempts);
    return;
  }

  if (line.startsWith("p1 ")) {
    float k1,k2,k3;
    if (sscanf(line.c_str()+3, "%f %f %f", &k1,&k2,&k3) == 3) {
      pid1.kp = k1; pid1.ki = k2; pid1.kd = k3;
      Serial.println("PID1 updated");
    }
    return;
  }

  if (line.startsWith("p2 ")) {
    float k1,k2,k3;
    if (sscanf(line.c_str()+3, "%f %f %f", &k1,&k2,&k3) == 3) {
      pid2.kp = k1; pid2.ki = k2; pid2.kd = k3;
      Serial.println("PID2 updated");
    }
    return;
  }

  if (line == "r") {
    Serial.println("Forcing CDCE913 EEPROM reprogram");
    cdce_program_eeprom();
    return;
  }

  if (line == "d") {
    Serial.print("ADC LOCK1="); Serial.print(analogRead(PIN_LOCK1_IN));
    Serial.print(" LOCK2="); Serial.println(analogRead(PIN_LOCK2_IN));
    return;
  }

  Serial.println("Unknown command");
}

// -----------------------------
// Setup
// -----------------------------
void setup() {
  Serial.begin(115200);
  Wire.begin();
  SPI.begin();

  // Teensy 4.1 ADC is more stable when set explicitly.
  analogReadResolution(12);
  analogReadAveraging(8);

  // Pin modes
  pinMode(PIN_AD9833_1_CS, OUTPUT);
  pinMode(PIN_AD9833_2_CS, OUTPUT);
  pinMode(PIN_AD5064_CS, OUTPUT);
  pinMode(PIN_AD9833_1_RESET, OUTPUT);
  pinMode(PIN_AD9833_2_RESET, OUTPUT);
  pinMode(PIN_AD5064_LDAC, OUTPUT);
  pinMode(PIN_AD5064_CLR, OUTPUT);
  pinMode(PIN_TFT_CS, OUTPUT);
  pinMode(PIN_TFT_DC, OUTPUT);
  pinMode(PIN_TFT_RST, OUTPUT);
  pinMode(PIN_FREQ_TEST, INPUT);

  digitalWrite(PIN_AD9833_1_CS, HIGH);
  digitalWrite(PIN_AD9833_2_CS, HIGH);
  digitalWrite(PIN_AD5064_CS, HIGH);
  digitalWrite(PIN_TFT_CS, HIGH);
  digitalWrite(PIN_TFT_DC, HIGH);
  digitalWrite(PIN_TFT_RST, HIGH);
  digitalWrite(PIN_AD5064_LDAC, LOW);
  digitalWrite(PIN_AD5064_CLR, HIGH);

  delay(50);

  // CDCE913 bring-up (safe)
  cdce_program_if_needed();
  cdce_wait_for_lock();
  if (cdce_selftest_25mhz()) Serial.println("CDCE913: 25 MHz OK");
  else Serial.println("CDCE913: 25 MHz FAIL");

  // AD9833 init (use CDCE913 output as refclk)
  ad9833_reset(PIN_AD9833_1_CS, PIN_AD9833_1_RESET);
  ad9833_reset(PIN_AD9833_2_CS, PIN_AD9833_2_RESET);
  ad9833_set_freq(PIN_AD9833_1_CS, DITHER_FREQ_HZ, REFCLK_HZ);
  ad9833_set_freq(PIN_AD9833_2_CS, DITHER_FREQ_HZ, REFCLK_HZ);

  // AD5064 init: put all outputs at midscale before enabling control loops.
  ad5064_set_midscale();

  // ILI9341 display init (uses the existing SPI bus on SCK/MOSI, with free CS/DC/RST pins)
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.println("Display ready");

  // Initialize lock channels
  lock_init_channels();

  Serial.println("Dual-PDH controller initialized");
}

// -----------------------------
// Main loop: non-blocking state machines and control
// -----------------------------
void loop() {
  serial_process(); // handle serial commands

  // Control loop at CONTROL_PERIOD_US
  if (loopTimer < CONTROL_PERIOD_US) return;
  float dt = loopTimer / 1e6f;
  loopTimer = 0;

  // Read PDH error signals and normalize
  int raw1 = analogRead(PIN_LOCK1_IN);
  int raw2 = analogRead(PIN_LOCK2_IN);
  float err1 = adc_to_error(raw1);
  float err2 = adc_to_error(raw2);
  lastRead1 = raw1;
  lastRead2 = raw2;
  lastErr1 = err1;
  lastErr2 = err2;
  lastTemp1  = ntc_to_celsius(analogRead(PIN_NTC1_MON));
  lastTemp2  = ntc_to_celsius(analogRead(PIN_NTC2_MON));
  lastTecRead1 = analogRead(PIN_TEC1_IMON);
  lastTecRead2 = analogRead(PIN_TEC2_IMON);

  // --- Channel 1 state machine ---
  switch (ch1.state) {
    case IDLE:
      // Wait for user or automatic start; here we auto-start SEARCH
      lock_enter_state(ch1, SEARCH);
      break;

    case SEARCH:
      // In SEARCH we enable dither and look for error signal above acquire threshold
      // If error magnitude exceeds acquire threshold, go to ACQUIRE
      if (fabs(err1) > ch1.acquire_threshold) {
        lock_enter_state(ch1, ACQUIRE);
      }
      break;

    case ACQUIRE:
      // Try to reduce error using PID for a limited time
      {
        float u = pid_step(pid1, -err1, dt); // negative because error sign convention
        uint16_t dac = pid_to_dac(u);
        ad5064_write_channel(0, dac); // channel 0 controls laser1 current setpoint
        lastSet1 = dac;

        if (fabs(err1) < ch1.lock_threshold) {
          ch1.locked = true;
          lock_enter_state(ch1, LOCKED);
        } else if (millis() - ch1.state_ts > ch1.acquire_timeout_ms) {
          // failed to acquire
          ch1.relock_attempts++;
          lock_enter_state(ch1, RELOCK);
        }
      }
      break;

    case LOCKED:
      // Maintain lock with PID; monitor for loss
      {
        float u = pid_step(pid1, -err1, dt);
        uint16_t dac = pid_to_dac(u);
        ad5064_write_channel(0, dac);
        lastSet1 = dac;

        if (fabs(err1) > ch1.acquire_threshold) {
          // lost lock
          ch1.locked = false;
          lock_enter_state(ch1, RELOCK);
        }
      }
      break;

    case RELOCK:
      // Backoff and attempt search again after delay
      if (millis() - ch1.state_ts > ch1.relock_backoff_ms * (1 + ch1.relock_attempts)) {
        lock_enter_state(ch1, SEARCH);
      }
      break;

    case HOLD:
      // Optional: hold outputs, do nothing
      break;
  }

  // --- Channel 2 state machine (mirror of channel 1) ---
  switch (ch2.state) {
    case IDLE:
      lock_enter_state(ch2, SEARCH);
      break;

    case SEARCH:
      if (fabs(err2) > ch2.acquire_threshold) {
        lock_enter_state(ch2, ACQUIRE);
      }
      break;

    case ACQUIRE:
      {
        float u = pid_step(pid2, -err2, dt);
        uint16_t dac = pid_to_dac(u);
        ad5064_write_channel(1, dac); // channel 1 controls laser2
        lastSet2 = dac;

        if (fabs(err2) < ch2.lock_threshold) {
          ch2.locked = true;
          lock_enter_state(ch2, LOCKED);
        } else if (millis() - ch2.state_ts > ch2.acquire_timeout_ms) {
          ch2.relock_attempts++;
          lock_enter_state(ch2, RELOCK);
        }
      }
      break;

    case LOCKED:
      {
        float u = pid_step(pid2, -err2, dt);
        uint16_t dac = pid_to_dac(u);
        ad5064_write_channel(1, dac);
        lastSet2 = dac;

        if (fabs(err2) > ch2.acquire_threshold) {
          ch2.locked = false;
          lock_enter_state(ch2, RELOCK);
        }
      }
      break;

    case RELOCK:
      if (millis() - ch2.state_ts > ch2.relock_backoff_ms * (1 + ch2.relock_attempts)) {
        lock_enter_state(ch2, SEARCH);
      }
      break;

    case HOLD:
      break;
  }

  // Optional: update other DAC channels (TEC, offsets) here
  // Example: keep TEC channel midscale for now
  // ad5064_write_channel(2, 0x8000);

  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay >= DISPLAY_PERIOD_MS) {
    lastDisplay = millis();
    update_display();
  }

  // Periodic debug print at low rate
  static unsigned long lastDbg = 0;
  if (millis() - lastDbg > 1000) {
    lastDbg = millis();
    Serial.print("E1="); Serial.print(err1, 4);
    Serial.print(" S1="); Serial.print((int)ch1.state);
    Serial.print(" E2="); Serial.print(err2, 4);
    Serial.print(" S2="); Serial.println((int)ch2.state);
  }
}