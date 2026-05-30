#include <Arduino.h>
#include "lock.h"

void lock_init(LockChannel &ch) {
  ch.state              = LOCK_IDLE;
  ch.state_ts           = millis();
  ch.relock_attempts    = 0;
  ch.locked             = false;
  ch.lock_threshold     = 0.02f;
  ch.acquire_threshold  = 0.10f;
  ch.acquire_timeout_ms = 2000;
  ch.relock_backoff_ms  = 500;
}

void lock_enter(LockChannel &ch, LockState s) {
  ch.state    = s;
  ch.state_ts = millis();
}

const char* lock_state_name(LockState s) {
  switch (s) {
    case LOCK_IDLE:    return "IDLE";
    case LOCK_SEARCH:  return "SEARCH";
    case LOCK_ACQUIRE: return "ACQUIRE";
    case LOCK_LOCKED:  return "LOCKED";
    case LOCK_HOLD:    return "HOLD";
    case LOCK_RELOCK:  return "RELOCK";
    default:           return "?";
  }
}
