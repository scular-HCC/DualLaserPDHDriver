#pragma once
#include <Arduino.h>
#include "lock.h"
#include "pid.h"
#include "display.h"
#include "net_settings.h"

// ============================================================
// Ethernet interface — Teensy 4.1 built-in PHY via QNEthernet
//
// Services:
//   port 80  — HTTP  (GET / → dashboard, GET /api/status → JSON,
//                     POST /api/cmd → command, OPTIONS → CORS)
//   port 23  — Telnet CLI (single client, line-buffered)
//
// Settings (IP, subnet, gateway, hostname, DHCP flag) are loaded
// from EEPROM via net_settings. Changes written to EEPROM with
// netset commands persist across power cycles; call network_reinit()
// to apply them without rebooting.
//
// Library required: QNEthernet by Luni64
//   Install via Arduino Library Manager: "QNEthernet"
// ============================================================

// Initialise Ethernet using saved EEPROM settings.
// Call once from setup(), after Serial/SPI/Wire are ready.
void network_init();

// Re-initialise Ethernet with current EEPROM settings.
// Drops all active connections. Call after netset changes.
void network_reinit();

// Service all pending HTTP and Telnet traffic.
// Call from loop() — non-blocking.
void network_poll(LockChannel ch[2], PID pid[2],
                  DispModel& disp,
                  bool pll_lock, float refclk_mhz);

// Returns dotted-decimal current IP string (valid after network_init).
const char* network_ip();
