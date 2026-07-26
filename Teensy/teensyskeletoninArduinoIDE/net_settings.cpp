#include <Arduino.h>
#include <EEPROM.h>
#include "net_settings.h"

static void fill_defaults(NetSettings& s) {
  s.magic     = NET_MAGIC;
  s.use_dhcp  = NET_DEFAULT_DHCP;
  uint8_t ip[]  = NET_DEFAULT_IP;
  uint8_t sub[] = NET_DEFAULT_SUBNET;
  uint8_t gw[]  = NET_DEFAULT_GATEWAY;
  memcpy(s.static_ip, ip,  4);
  memcpy(s.subnet,    sub, 4);
  memcpy(s.gateway,   gw,  4);
  strncpy(s.hostname, NET_DEFAULT_HOSTNAME, sizeof(s.hostname) - 1);
  s.hostname[sizeof(s.hostname) - 1] = '\0';
  s.demo_mode = NET_DEFAULT_DEMO;
  s.rf_omega[0] = RF_OMEGA1_HZ;  s.rf_omega[1] = RF_OMEGA2_HZ;
  s.rf_phase[0] = RF_PHASE1_DEG; s.rf_phase[1] = RF_PHASE2_DEG;
  s.err_null[0] = NET_DEFAULT_ERR_NULL; s.err_null[1] = NET_DEFAULT_ERR_NULL;
}

void net_settings_load(NetSettings& s) {
  EEPROM.get(NET_EEPROM_ADDR, s);
  if (s.magic != NET_MAGIC) {
    Serial.println(F("net_settings: EEPROM blank, writing defaults"));
    fill_defaults(s);
    net_settings_save(s);
  }
  // Safety: ensure hostname is null-terminated
  s.hostname[sizeof(s.hostname) - 1] = '\0';
}

void net_settings_save(const NetSettings& s) {
  EEPROM.put(NET_EEPROM_ADDR, s);
}

bool net_parse_ip(const char* str, uint8_t ip[4]) {
  int a, b, c, d;
  if (sscanf(str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) return false;
  if (a < 0 || a > 255 || b < 0 || b > 255 ||
      c < 0 || c > 255 || d < 0 || d > 255)  return false;
  ip[0] = (uint8_t)a; ip[1] = (uint8_t)b;
  ip[2] = (uint8_t)c; ip[3] = (uint8_t)d;
  return true;
}

void net_fmt_ip(const uint8_t ip[4], char* buf) {
  snprintf(buf, 16, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}
