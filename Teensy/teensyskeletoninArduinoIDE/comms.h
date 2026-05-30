#pragma once
#include <Arduino.h>
#include "lock.h"
#include "pid.h"
#include "display.h"

// ============================================================
// Unified command processor — shared by USB-serial, Telnet,
// and HTTP POST.  Every entry-point passes a Print& so
// responses go to the right sink.
// ============================================================

// Write full telemetry as a JSON object to `out`.
void comms_json_status(Print& out,
                       LockChannel ch[2], PID pid[2],
                       const DispModel& disp,
                       bool pll_lock, float refclk_mhz,
                       const char* ip_str,
                       unsigned long uptime_ms);

// Parse and execute one command line; write human-readable
// (or JSON) response to `out`.
void comms_process(const String& line, Print& out,
                   LockChannel ch[2], PID pid[2],
                   DispModel& disp);
