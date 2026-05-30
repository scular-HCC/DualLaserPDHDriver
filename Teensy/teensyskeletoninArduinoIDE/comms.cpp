#include <Arduino.h>
#include <math.h>
#include "comms.h"
#include "config.h"
#include "ad9833.h"
#include "cdce913.h"

// Declared in teensyskeletoninArduinoIDE.ino
extern float g_dither_freq;

// ============================================================
// JSON status generator
// Streams directly to Print — no intermediate buffer needed.
// ============================================================
void comms_json_status(Print& out,
                       LockChannel ch[2], PID pid[2],
                       const DispModel& disp,
                       bool pll_lock, float refclk_mhz,
                       const char* ip_str,
                       unsigned long uptime_ms) {
  out.print(F("{\"t\":"));      out.print(uptime_ms);
  out.print(F(",\"pll\":"));    out.print(pll_lock ? 1 : 0);
  out.print(F(",\"refclk\":")); out.print(refclk_mhz, 3);
  out.print(F(",\"dither\":")); out.print(g_dither_freq, 0);
  out.print(F(",\"ip\":\""));   out.print(ip_str);
  out.print(F("\",\"ch\":["));

  for (int i = 0; i < 2; i++) {
    if (i) out.print(',');
    out.print(F("{\"n\":")); out.print(i + 1);

    out.print(F(",\"st\":\""));
    out.print(lock_state_name(ch[i].state));
    out.print('"');

    out.print(F(",\"rms\":"));  out.print(disp.ch[i].err_rms, 5);

    out.print(F(",\"temp\":"));
    if (isnan(disp.ch[i].tec_temp_c)) out.print(F("null"));
    else out.print(disp.ch[i].tec_temp_c, 2);

    out.print(F(",\"ima\":"));  out.print(disp.ch[i].laser_i_ma, 2);
    out.print(F(",\"setp\":")); out.print(disp.ch[i].setpoint_pct, 2);
    out.print(F(",\"scan\":")); out.print(disp.ch[i].scan_phase, 3);
    out.print(F(",\"peak\":")); out.print(disp.ch[i].peak_phase, 3);
    out.print(F(",\"qual\":")); out.print(disp.ch[i].lock_qual, 3);
    out.print(F(",\"relk\":")); out.print(ch[i].relock_attempts);
    out.print(F(",\"kp\":"));   out.print(pid[i].kp, 5);
    out.print(F(",\"ki\":"));   out.print(pid[i].ki, 5);
    out.print(F(",\"kd\":"));   out.print(pid[i].kd, 5);
    out.print(F(",\"lthr\":")); out.print(ch[i].lock_threshold, 5);
    out.print(F(",\"athr\":")); out.print(ch[i].acquire_threshold, 5);
    out.print('}');
  }
  out.print(F("]}"));
}

// ============================================================
// Command processor
// ============================================================
void comms_process(const String& line, Print& out,
                   LockChannel ch[2], PID pid[2],
                   DispModel& disp) {
  if (line.length() == 0) return;

  // ---- status (JSON) ----
  if (line == "s" || line == "status") {
    // Caller is responsible for JSON wrapping if needed.
    // For CLI, we print a compact multi-line status.
    for (int i = 0; i < 2; i++) {
      out.print(F("CH")); out.print(i + 1);
      out.print(' '); out.print(lock_state_name(ch[i].state));
      out.print(F("  rms="));    out.print(disp.ch[i].err_rms, 5);
      out.print(F("  temp="));   out.print(disp.ch[i].tec_temp_c, 1);
      out.print(F("°C  I="));    out.print(disp.ch[i].laser_i_ma, 1);
      out.print(F("mA  setp=")); out.print(disp.ch[i].setpoint_pct, 1);
      out.print(F("%  relk="));  out.print(ch[i].relock_attempts);
      out.println();
      out.print(F("  PID kp="));  out.print(pid[i].kp, 4);
      out.print(F(" ki="));       out.print(pid[i].ki, 4);
      out.print(F(" kd="));       out.print(pid[i].kd, 4);
      out.print(F("  thr lock=")); out.print(ch[i].lock_threshold, 4);
      out.print(F(" acq="));       out.print(ch[i].acquire_threshold, 4);
      out.println();
    }
    out.print(F("dither="));  out.print(g_dither_freq, 0);
    out.println(F(" Hz"));
    return;
  }

  // ---- lock/start sweep ----
  if (line == "lock1") { lock_enter(ch[0], LOCK_SEARCH); out.println(F("CH1 -> SEARCH")); return; }
  if (line == "lock2") { lock_enter(ch[1], LOCK_SEARCH); out.println(F("CH2 -> SEARCH")); return; }

  // ---- stop to idle ----
  if (line == "stop1") { lock_enter(ch[0], LOCK_IDLE); out.println(F("CH1 -> IDLE")); return; }
  if (line == "stop2") { lock_enter(ch[1], LOCK_IDLE); out.println(F("CH2 -> IDLE")); return; }

  // ---- hold ----
  if (line == "hold1") { lock_enter(ch[0], LOCK_HOLD); out.println(F("CH1 -> HOLD")); return; }
  if (line == "hold2") { lock_enter(ch[1], LOCK_HOLD); out.println(F("CH2 -> HOLD")); return; }

  // ---- break lock ----
  if (line == "break1") {
    if (ch[0].state == LOCK_LOCKED || ch[0].state == LOCK_ACQUIRE || ch[0].state == LOCK_HOLD) {
      lock_enter(ch[0], LOCK_RELOCK);
      out.println(F("CH1 lock broken -> RELOCK"));
    } else {
      out.println(F("CH1 not locked"));
    }
    return;
  }
  if (line == "break2") {
    if (ch[1].state == LOCK_LOCKED || ch[1].state == LOCK_ACQUIRE || ch[1].state == LOCK_HOLD) {
      lock_enter(ch[1], LOCK_RELOCK);
      out.println(F("CH2 lock broken -> RELOCK"));
    } else {
      out.println(F("CH2 not locked"));
    }
    return;
  }

  // ---- PID gains: p1 kp ki kd ----
  if (line.startsWith("p1 ") || line.startsWith("p2 ")) {
    int idx = line[1] - '1';
    float k1, k2, k3;
    if (sscanf(line.c_str() + 3, "%f %f %f", &k1, &k2, &k3) == 3) {
      pid[idx].kp = k1; pid[idx].ki = k2; pid[idx].kd = k3;
      pid[idx].integrator = 0.0f; pid[idx].prev_err = 0.0f;
      out.print(F("CH")); out.print(idx + 1);
      out.print(F(" PID kp=")); out.print(k1, 4);
      out.print(F(" ki="));     out.print(k2, 4);
      out.print(F(" kd="));     out.println(k3, 4);
    } else {
      out.println(F("Usage: p1 <kp> <ki> <kd>"));
    }
    return;
  }

  // ---- thresholds: thresh1 lock acq ----
  if (line.startsWith("thresh1 ") || line.startsWith("thresh2 ")) {
    int idx = line[6] - '1';
    float lt, at;
    if (sscanf(line.c_str() + 8, "%f %f", &lt, &at) == 2) {
      ch[idx].lock_threshold    = lt;
      ch[idx].acquire_threshold = at;
      out.print(F("CH")); out.print(idx + 1);
      out.print(F(" lock_thr=")); out.print(lt, 5);
      out.print(F(" acq_thr="));  out.println(at, 5);
    } else {
      out.println(F("Usage: thresh1 <lock> <acq>"));
    }
    return;
  }

  // ---- dither frequency ----
  if (line.startsWith("dither ")) {
    float hz;
    if (sscanf(line.c_str() + 7, "%f", &hz) == 1 && hz > 100.0f && hz < 500000.0f) {
      g_dither_freq = hz;
      ad9833_set_freq(PIN_AD9833_1_CS, hz, REFCLK_HZ);
      ad9833_set_freq(PIN_AD9833_2_CS, hz, REFCLK_HZ);
      out.print(F("Dither -> ")); out.print(hz, 0); out.println(F(" Hz"));
    } else {
      out.println(F("Usage: dither <hz>  (100–500000)"));
    }
    return;
  }

  // ---- raw ADC dump ----
  if (line == "d") {
    out.print(F("LOCK1=")); out.print(analogRead(PIN_LOCK1_IN));
    out.print(F(" LOCK2=")); out.print(analogRead(PIN_LOCK2_IN));
    out.print(F(" NTC1="));  out.print(analogRead(PIN_NTC1_MON));
    out.print(F(" NTC2="));  out.print(analogRead(PIN_NTC2_MON));
    out.print(F(" LAS1="));  out.print(analogRead(PIN_LAS1_IMON));
    out.print(F(" LAS2="));  out.println(analogRead(PIN_LAS2_IMON));
    return;
  }

  // ---- force CDCE EEPROM reprogram ----
  if (line == "r") {
    extern void cdce_program_eeprom_force();
    cdce_program_eeprom_force();
    out.println(F("CDCE913 EEPROM reprogrammed"));
    return;
  }

  // ---- reset relock counter ----
  if (line == "reset1") { ch[0].relock_attempts = 0; out.println(F("CH1 relock counter reset")); return; }
  if (line == "reset2") { ch[1].relock_attempts = 0; out.println(F("CH2 relock counter reset")); return; }

  // ---- help ----
  if (line == "help" || line == "?") {
    out.println(F("Commands:"));
    out.println(F("  s / status           — telemetry"));
    out.println(F("  lock1 / lock2        — start sweep+lock"));
    out.println(F("  stop1 / stop2        — stop to IDLE"));
    out.println(F("  hold1 / hold2        — enter HOLD"));
    out.println(F("  break1 / break2      — break lock -> RELOCK"));
    out.println(F("  reset1 / reset2      — clear relock counter"));
    out.println(F("  p1 kp ki kd          — set CH1 PID gains"));
    out.println(F("  p2 kp ki kd          — set CH2 PID gains"));
    out.println(F("  thresh1 lock acq     — set CH1 thresholds"));
    out.println(F("  thresh2 lock acq     — set CH2 thresholds"));
    out.println(F("  dither <hz>          — set dither frequency"));
    out.println(F("  d                    — raw ADC dump"));
    out.println(F("  r                    — reprogram CDCE913"));
    return;
  }

  out.print(F("Unknown: ")); out.println(line);
}
