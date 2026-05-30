// serial_console.cpp — thin wrapper; all logic lives in comms.cpp
#include <Arduino.h>
#include "serial_console.h"
#include "comms.h"

void serial_process(LockChannel &ch1, LockChannel &ch2, PID &pid1, PID &pid2) {
  // Handled directly in the main loop via comms_process.
  // This stub keeps the old header present for compatibility.
  (void)ch1; (void)ch2; (void)pid1; (void)pid2;
}
