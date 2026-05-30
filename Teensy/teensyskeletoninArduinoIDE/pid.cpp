#include <Arduino.h>
#include "pid.h"

float pid_step(PID &p, float err, float dt) {
  p.integrator += err * p.ki * dt;
  if (p.integrator >  p.integrator_limit) p.integrator =  p.integrator_limit;
  if (p.integrator < -p.integrator_limit) p.integrator = -p.integrator_limit;

  float deriv = (dt > 0.0f) ? (err - p.prev_err) / dt : 0.0f;
  float out = p.kp * err + p.integrator + p.kd * deriv;

  if (out >  p.out_max) out =  p.out_max;
  if (out <  p.out_min) out =  p.out_min;

  p.prev_err = err;
  return out;
}

uint16_t pid_to_dac(float u) {
  if (u >  1.0f) u =  1.0f;
  if (u < -1.0f) u = -1.0f;
  return (uint16_t)((u * 0.5f + 0.5f) * 65535.0f);
}
