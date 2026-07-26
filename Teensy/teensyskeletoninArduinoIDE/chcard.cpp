#include <Arduino.h>
#include "config.h"
#include "chcard.h"
#include "ad5696.h"
#include "ads1115.h"
#include "pca9538.h"

static const uint8_t ADS_ADDR[2] = { I2C_ADS1115_CH1, I2C_ADS1115_CH2 };
static const uint8_t DAC_ADDR[2] = { I2C_AD5696_CH1,  I2C_AD5696_CH2  };
static const uint8_t EXP_ADDR[2] = { I2C_PCA9538_CH1, I2C_PCA9538_CH2 };

static const uint8_t AFE_NULL_DAC[2] = { AFE_NULL_DAC_CH1, AFE_NULL_DAC_CH2 };

// Cached last-written values → no I2C traffic unless something changes.
static int32_t s_las_code[2] = { -1, -1 };
static int32_t s_tec_code[2] = { -1, -1 };
static int8_t  s_lock_en[2]  = { -1, -1 };

// AFE ERR offset-null DAC. Seeded to the RSTSEL power-up state so the
// cached value matches the hardware before the first write.
static uint16_t s_null_code[2] = { AFE_NULL_CODE_MID, AFE_NULL_CODE_MID };
static int32_t  s_null_sent[2] = { -1, -1 };
static bool     s_null_ok      = false;

static ChCardMon s_mon[2] = {{NAN, NAN, NAN, NAN, false}, {NAN, NAN, NAN, NAN, false}};
static AfeMon    s_afe    = {{NAN, NAN}, false};

// Round-robin state: channel whose conversion is in flight (-1 = none).
static int8_t s_pend[2]    = { -1, -1 };
static int8_t s_afe_pend   = -1;
static uint8_t s_seen[2]   = { 0, 0 };
static uint8_t s_afe_seen  = 0;

uint8_t chcard_init_all() {
  uint8_t ok = 0;
  for (int i = 0; i < 2; i++) {
    // IO0 = LOCK_EN output (init LOW = integrator HELD), rest inputs.
    bool a = pca9538_write_reg(EXP_ADDR[i], PCA9538_REG_OUTPUT, 0x00);
    bool b = pca9538_write_reg(EXP_ADDR[i], PCA9538_REG_CONFIG, 0xFE);
    s_lock_en[i] = 0;
    if (a && b) ok |= (1 << i);
  }
  chcard_set_midscale_all();
  return ok;
}

void chcard_write_laser_dac(int i, uint16_t code) {
  if (s_las_code[i] == (int32_t)code) return;
  if (ad5696_write(DAC_ADDR[i], AD5696_DAC_A, code)) s_las_code[i] = code;
}

void chcard_write_tec_dac(int i, uint16_t code) {
  if (s_tec_code[i] == (int32_t)code) return;
  if (ad5696_write(DAC_ADDR[i], AD5696_DAC_B, code)) s_tec_code[i] = code;
}

void chcard_set_midscale_all() {
  for (int i = 0; i < 2; i++) {
    chcard_write_laser_dac(i, DAC_MID_CODE);
    chcard_write_tec_dac(i, TEC_DAC_ZERO_CODE);   // 0 A, not midscale
  }
}

// ---- AFE ERR offset-null DAC --------------------------------------
bool afe_null_init(const uint16_t code[2]) {
  s_null_ok = true;                 // afe_write_null_dac() clears it on NAK
  for (int i = 0; i < 2; i++) {
    s_null_sent[i] = -1;            // force the write even if the code matches
    afe_write_null_dac(i, code[i]);
  }
  return s_null_ok;
}

void afe_write_null_dac(int i, uint16_t code) {
  if (s_null_sent[i] == (int32_t)code) return;
  if (ad5696_write(I2C_AD5696_AFE, AFE_NULL_DAC[i], code)) {
    s_null_sent[i] = code;
    s_null_code[i] = code;
  } else {
    s_null_ok = false;
  }
}

uint16_t afe_null_code(int i) { return s_null_code[i]; }
bool     afe_null_present()   { return s_null_ok;      }

void chcard_set_lock_en(int i, bool run) {
  if (s_lock_en[i] == (int8_t)run) return;
  // IO0 = LOCK_EN; other outputs unused (register bit 0 only).
  if (pca9538_write_reg(EXP_ADDR[i], PCA9538_REG_OUTPUT, run ? 0x01 : 0x00))
    s_lock_en[i] = (int8_t)run;
}

bool chcard_fault(int i) {
  int16_t v = pca9538_read_reg(EXP_ADDR[i], PCA9538_REG_INPUT);
  if (v < 0) return false;          // unreachable card ≠ fault here
  return (v & 0x02) == 0;           // IO1 active-low
}

void chcard_poll(int i) {
  if (s_pend[i] >= 0) {
    float v = ads1115_read_volts(ADS_ADDR[i]);
    switch (s_pend[i]) {
      case 0: s_mon[i].las_imon_v = v; break;
      case 1: s_mon[i].tec_imon_v = v; break;
      case 2: s_mon[i].mpd_mon_v  = v; break;
      case 3: s_mon[i].ntc_v      = v; break;
    }
    s_seen[i] |= (1 << s_pend[i]);
    if (s_seen[i] == 0x0F) s_mon[i].valid = true;
  }
  int8_t next = (s_pend[i] + 1) & 3;
  s_pend[i] = ads1115_start(ADS_ADDR[i], next) ? next : -1;
}

void afe_poll() {
  if (s_afe_pend >= 0) {
    float v = ads1115_read_volts(I2C_ADS1115_AFE);
    s_afe.pd_lvl_v[s_afe_pend] = v;
    s_afe_seen |= (1 << s_afe_pend);
    if (s_afe_seen == 0x03) s_afe.valid = true;
  }
  int8_t next = (s_afe_pend + 1) & 1;
  s_afe_pend = ads1115_start(I2C_ADS1115_AFE, next) ? next : -1;
}

const ChCardMon& chcard_mon(int i) { return s_mon[i]; }
const AfeMon&    afe_mon()         { return s_afe;    }
