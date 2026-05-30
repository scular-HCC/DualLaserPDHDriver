#pragma once
#include "lock.h"
#include "pid.h"

// ============================================================
// Serial console — 115200 baud, line-oriented commands
//
//   s           — status dump
//   p1 kp ki kd — set CH1 PID gains
//   p2 kp ki kd — set CH2 PID gains
//   lock1       — force CH1 into SEARCH
//   lock2       — force CH2 into SEARCH
//   stop1       — force CH1 IDLE
//   stop2       — force CH2 IDLE
//   hold1/hold2 — force HOLD
//   r           — reprogram CDCE913 EEPROM (force)
//   d           — dump raw ADC readings
// ============================================================

void serial_process(LockChannel &ch1, LockChannel &ch2, PID &pid1, PID &pid2);
