#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "config.h"   // for DEMO_MODE_DEFAULT

// ============================================================
// Network settings — persisted in Teensy EEPROM emulation.
// A magic number detects first-boot; defaults are written then.
// Changes written with net_settings_save() survive power cycles.
// ============================================================

#define NET_MAGIC       0x50444833UL   // 'P','D','H','3' — bump when struct changes
#define NET_EEPROM_ADDR 0              // byte offset in EEPROM

struct NetSettings {
  uint32_t magic;
  bool     use_dhcp;
  uint8_t  static_ip[4];
  uint8_t  subnet[4];
  uint8_t  gateway[4];
  char     hostname[32];
  bool     demo_mode;    // runtime demo flag — toggled by 'demo on/off'
  float    rf_omega[2];  // per-channel modulation Ω (Hz)  — survives power cycle
  float    rf_phase[2];  // per-channel demod phase (deg)  — set by phaseN / calN
};

// Default values used when EEPROM is blank or magic is wrong.
#define NET_DEFAULT_DHCP      true
#define NET_DEFAULT_IP        {192, 168, 1, 200}
#define NET_DEFAULT_SUBNET    {255, 255, 255, 0}
#define NET_DEFAULT_GATEWAY   {192, 168, 1, 1}
#define NET_DEFAULT_HOSTNAME  "dual-pdh"
#define NET_DEFAULT_DEMO      DEMO_MODE_DEFAULT

// Load from EEPROM; fills defaults and saves if magic invalid.
void net_settings_load(NetSettings& s);

// Write to EEPROM.
void net_settings_save(const NetSettings& s);

// Parse "a.b.c.d" into ip[4]. Returns true on success.
bool net_parse_ip(const char* str, uint8_t ip[4]);

// Format ip[4] as "a.b.c.d" into buf (must be ≥ 16 bytes).
void net_fmt_ip(const uint8_t ip[4], char* buf);
