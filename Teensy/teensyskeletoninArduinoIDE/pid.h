#pragma once

// ============================================================
// PID controller — anti-windup, derivative on error
// ============================================================

struct PID {
  float kp, ki, kd;
  float integrator;
  float prev_err;
  float out_min, out_max;
  float integrator_limit;
};

// Step the PID; returns output clamped to [out_min, out_max].
float pid_step(PID &p, float err, float dt);

// Map PID output (-1..1) to 16-bit DAC code.
uint16_t pid_to_dac(float u);
