#pragma once
#include "lock.h"
#include "display.h"

// ============================================================
// Demo mode — hardware simulation for Teensy + TFT only.
// Drives a realistic sweep-to-lock animation on both channels
// without any real PDH electronics connected.
//
// Enable / disable via DEMO_MODE in config.h.
//
// Switching to real hardware:
//   1. Set  #define DEMO_MODE 0  in config.h
//   2. Recompile and flash — nothing else changes.
//
// Sequence per channel (loops):
//   IDLE (0.5 s) → SEARCH (10 s TEC sweep)
//                → ACQUIRE (2 s PID convergence)
//                → LOCKED  (40 s in-lock)
//                → RELOCK  (1.5 s fault)
//                → SEARCH  …
//
// CH2 is staggered by 5 s so states are never in sync.
//
// Command interaction:
//   stop1 / stop2  — pauses demo on that channel (stays IDLE)
//   lock1 / lock2  — resumes demo from SEARCH
//   hold1 / hold2  — freezes output (demo does not override)
// ============================================================

// Call once from setup() instead of lock_init().
void demo_init(LockChannel ch[2], DispModel& disp);

// Call every 1 kHz control tick instead of real ADC + PID.
// Fills ch[].state and the entire DispModel.
void demo_update(LockChannel ch[2], DispModel& disp, float dt);
