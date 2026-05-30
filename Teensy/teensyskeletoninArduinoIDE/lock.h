#pragma once
#include <stdint.h>
#include <stdbool.h>

// ============================================================
// PDH lock state machine
// ============================================================

enum LockState : uint8_t {
  LOCK_IDLE   = 0,
  LOCK_SEARCH,
  LOCK_ACQUIRE,
  LOCK_LOCKED,
  LOCK_HOLD,
  LOCK_RELOCK
};

struct LockChannel {
  LockState     state;
  unsigned long state_ts;          // millis() when state was entered
  unsigned int  relock_attempts;
  bool          locked;
  float         lock_threshold;    // |error| below → locked
  float         acquire_threshold; // |error| above → start acquire
  unsigned long acquire_timeout_ms;
  unsigned long relock_backoff_ms;
};

void          lock_init(LockChannel &ch);
void          lock_enter(LockChannel &ch, LockState s);
const char*   lock_state_name(LockState s);
