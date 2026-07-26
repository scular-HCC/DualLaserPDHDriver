#include <Arduino.h>
#include <math.h>
#include "comms.h"
#include "config.h"
#include "ad9833.h"
#include "cdce913.h"
#include "net_settings.h"
#include "network.h"
#include "demo.h"
#include "lock.h"
#include "chcard.h"

// Declared in teensyskeletoninArduinoIDE.ino
extern float g_dither_freq;
extern bool  g_demo_mode;
extern float g_rf_omega[2];
extern float g_rf_phase[2];
void rf_set_omega(int i, float hz);
void rf_set_phase(int i, float deg);
void rf_phase_cal(int i, Print& out);
extern uint16_t g_err_null[2];
void err_null_cal(int i, Print& out);
void err_null_set(int i, uint16_t code);

// ============================================================
// JSON status generator
// Streams directly to Print — no intermediate buffer needed.
// Includes network settings block for the web dashboard.
// ============================================================
void comms_json_status(Print& out,
                       LockChannel ch[2], PID pid[2],
                       const DispModel& disp,
                       bool pll_lock, float refclk_mhz,
                       const char* ip_str,
                       unsigned long uptime_ms) {
  // Top-level fields
  out.print(F("{\"t\":"));      out.print(uptime_ms);
  out.print(F(",\"pll\":"));    out.print(pll_lock ? 1 : 0);
  out.print(F(",\"refclk\":")); out.print(refclk_mhz, 3);
  out.print(F(",\"dither\":")); out.print(g_dither_freq, 0);
  out.print(F(",\"ip\":\""));   out.print(ip_str); out.print('"');

  out.print(F(",\"demo\":")); out.print(g_demo_mode ? 1 : 0);

  // Network settings block
  {
    NetSettings ns;
    net_settings_load(ns);
    char buf[16];
    out.print(F(",\"net\":{"));
    out.print(F("\"dhcp\":"));  out.print(ns.use_dhcp ? 1 : 0);
    net_fmt_ip(ns.static_ip, buf);
    out.print(F(",\"sip\":\"")); out.print(buf); out.print('"');
    net_fmt_ip(ns.subnet, buf);
    out.print(F(",\"sub\":\"")); out.print(buf); out.print('"');
    net_fmt_ip(ns.gateway, buf);
    out.print(F(",\"gw\":\"")); out.print(buf); out.print('"');
    out.print(F(",\"host\":\"")); out.print(ns.hostname); out.print('"');
    out.print('}');
  }

  // Channel array
  out.print(F(",\"ch\":["));
  for (int i = 0; i < 2; i++) {
    if (i) out.print(',');
    out.print(F("{\"n\":")); out.print(i + 1);
    out.print(F(",\"st\":\""));   out.print(lock_state_name(ch[i].state)); out.print('"');
    out.print(F(",\"rms\":"));    out.print(disp.ch[i].err_rms, 5);
    out.print(F(",\"temp\":"));
    if (isnan(disp.ch[i].tec_temp_c)) out.print(F("null"));
    else out.print(disp.ch[i].tec_temp_c, 2);
    out.print(F(",\"ima\":"));   out.print(disp.ch[i].laser_i_ma, 2);
    out.print(F(",\"teci\":"));  out.print(disp.ch[i].tec_i_a, 3);
    out.print(F(",\"tlim\":"));  out.print(disp.ch[i].tec_lim_fac, 3);
    out.print(F(",\"hbf\":"));   out.print(disp.hbridge_fault[i] ? 1 : 0);
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
    out.print(F(",\"omega\":")); out.print(g_rf_omega[i], 0);
    out.print(F(",\"phase\":")); out.print(g_rf_phase[i], 1);
    out.print(F(",\"null\":"));  out.print(g_err_null[i]);
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

  // ---- status -----------------------------------------------
  if (line == "s" || line == "status") {
    if (g_demo_mode) out.println(F("[DEMO MODE — type 'demo off' to switch to real hardware]"));
    for (int i = 0; i < 2; i++) {
      out.print(F("CH")); out.print(i + 1);
      out.print(' '); out.print(lock_state_name(ch[i].state));
      out.print(F("  rms="));    out.print(disp.ch[i].err_rms, 5);
      out.print(F("  temp="));   out.print(disp.ch[i].tec_temp_c, 1);
      out.print(F("°C  I="));    out.print(disp.ch[i].laser_i_ma, 1);
      out.print(F("mA  TEC="));  out.print(disp.ch[i].tec_i_a, 3);
      out.print(F("A(lim="));    out.print(disp.ch[i].tec_lim_fac, 2);
      out.print(F(")  setp=")); out.print(disp.ch[i].setpoint_pct, 1);
      if (disp.hbridge_fault[i]) out.print(F("  *** H-BRIDGE FAULT ***"));
      out.print(F("%  relk="));  out.print(ch[i].relock_attempts);
      out.println();
      out.print(F("  PID kp="));  out.print(pid[i].kp, 4);
      out.print(F(" ki="));       out.print(pid[i].ki, 4);
      out.print(F(" kd="));       out.print(pid[i].kd, 4);
      out.print(F("  thr lock=")); out.print(ch[i].lock_threshold, 4);
      out.print(F(" acq="));       out.print(ch[i].acquire_threshold, 4);
      out.println();
    }
    out.print(F("RF: Omega1=")); out.print(g_rf_omega[0], 0);
    out.print(F("Hz phi1="));    out.print(g_rf_phase[0], 0);
    out.print(F("deg  Omega2=")); out.print(g_rf_omega[1], 0);
    out.print(F("Hz phi2="));    out.print(g_rf_phase[1], 0);
    out.println(F("deg"));
    return;
  }

  // ---- lock / stop / hold / break / reset -------------------
  if (line == "lock1") { lock_enter(ch[0], LOCK_SEARCH); out.println(F("CH1 -> SEARCH")); return; }
  if (line == "lock2") { lock_enter(ch[1], LOCK_SEARCH); out.println(F("CH2 -> SEARCH")); return; }
  if (line == "stop1") { lock_enter(ch[0], LOCK_IDLE);   out.println(F("CH1 -> IDLE"));   return; }
  if (line == "stop2") { lock_enter(ch[1], LOCK_IDLE);   out.println(F("CH2 -> IDLE"));   return; }
  if (line == "hold1") { lock_enter(ch[0], LOCK_HOLD);   out.println(F("CH1 -> HOLD"));   return; }
  if (line == "hold2") { lock_enter(ch[1], LOCK_HOLD);   out.println(F("CH2 -> HOLD"));   return; }

  if (line == "break1") {
    if (ch[0].state==LOCK_LOCKED||ch[0].state==LOCK_ACQUIRE||ch[0].state==LOCK_HOLD)
    { lock_enter(ch[0],LOCK_RELOCK); out.println(F("CH1 lock broken -> RELOCK")); }
    else out.println(F("CH1 not locked")); return;
  }
  if (line == "break2") {
    if (ch[1].state==LOCK_LOCKED||ch[1].state==LOCK_ACQUIRE||ch[1].state==LOCK_HOLD)
    { lock_enter(ch[1],LOCK_RELOCK); out.println(F("CH2 lock broken -> RELOCK")); }
    else out.println(F("CH2 not locked")); return;
  }

  if (line == "reset1") { ch[0].relock_attempts = 0; out.println(F("CH1 relock counter reset")); return; }
  if (line == "reset2") { ch[1].relock_attempts = 0; out.println(F("CH2 relock counter reset")); return; }

  // ---- PID gains: p1 kp ki kd / p2 kp ki kd ----------------
  if (line.startsWith("p1 ") || line.startsWith("p2 ")) {
    int idx = line[1] - '1';
    float k1, k2, k3;
    if (sscanf(line.c_str() + 3, "%f %f %f", &k1, &k2, &k3) == 3) {
      pid[idx].kp = k1; pid[idx].ki = k2; pid[idx].kd = k3;
      pid[idx].integrator = 0.0f; pid[idx].prev_err = 0.0f;
      out.print(F("CH")); out.print(idx+1);
      out.print(F(" PID kp=")); out.print(k1,4);
      out.print(F(" ki="));     out.print(k2,4);
      out.print(F(" kd="));     out.println(k3,4);
    } else out.println(F("Usage: p1 <kp> <ki> <kd>"));
    return;
  }

  // ---- thresholds: thresh1 lock acq / thresh2 lock acq -----
  if (line.startsWith("thresh1 ") || line.startsWith("thresh2 ")) {
    int idx = line[6] - '1';
    float lt, at;
    if (sscanf(line.c_str() + 8, "%f %f", &lt, &at) == 2) {
      ch[idx].lock_threshold    = lt;
      ch[idx].acquire_threshold = at;
      out.print(F("CH")); out.print(idx+1);
      out.print(F(" lock_thr=")); out.print(lt,5);
      out.print(F(" acq_thr="));  out.println(at,5);
    } else out.println(F("Usage: thresh1 <lock> <acq>"));
    return;
  }

  // ---- RF modulation Ω: omega1 <hz> / omega2 <hz> ----------
  if (line.startsWith("omega1 ") || line.startsWith("omega2 ")) {
    int idx = line[5] - '1';
    float hz;
    if (sscanf(line.c_str()+7, "%f", &hz)==1 && hz>=100000.0f && hz<=3500000.0f) {
      rf_set_omega(idx, hz);
      out.print(F("CH")); out.print(idx+1);
      out.print(F(" Omega -> ")); out.print(hz,0); out.println(F(" Hz"));
    } else out.println(F("Usage: omega1 <hz>  (100000-3500000)"));
    return;
  }

  // ---- demod phase: phase1 <deg> / phase2 <deg> ------------
  if (line.startsWith("phase1 ") || line.startsWith("phase2 ")) {
    int idx = line[5] - '1';
    float d;
    if (sscanf(line.c_str()+7, "%f", &d)==1) {
      rf_set_phase(idx, d);
      out.print(F("CH")); out.print(idx+1);
      out.print(F(" demod phase -> ")); out.print(d,1); out.println(F(" deg"));
    } else out.println(F("Usage: phase1 <deg>"));
    return;
  }

  // ---- demod-phase calibration: cal1 / cal2 ----------------
  if (line == "cal1") { rf_phase_cal(0, out); return; }
  if (line == "cal2") { rf_phase_cal(1, out); return; }

  // ---- AFE ERR offset null: null1 / null2 ------------------
  // Nulls the demodulator pedestal (AD835 offset + LO feedthrough).
  // Block the light first — see err_null_cal().
  if (line == "null1") { err_null_cal(0, out); return; }
  if (line == "null2") { err_null_cal(1, out); return; }

  // ---- manual null code: nullset1 <code> / nullset2 <code> --
  if (line.startsWith("nullset1 ") || line.startsWith("nullset2 ")) {
    int idx = line[7] - '1';
    int c;
    if (sscanf(line.c_str()+9, "%d", &c)==1 && c>=0 && c<=65535) {
      err_null_set(idx, (uint16_t)c);
      out.print(F("CH")); out.print(idx+1);
      out.print(F(" ERR null code -> ")); out.println(c);
    } else out.println(F("Usage: nullset1 <0-65535>"));
    return;
  }

  // ---- legacy: set BOTH Ω the same (kept for compatibility) -
  if (line.startsWith("dither ")) {
    float hz;
    if (sscanf(line.c_str()+7, "%f", &hz)==1 && hz>=100000.0f && hz<=3500000.0f) {
      rf_set_omega(0, hz);
      rf_set_omega(1, hz);
      out.print(F("Both Omega -> ")); out.print(hz,0);
      out.println(F(" Hz  (use omega1/omega2 for split-frequency operation)"));
    } else out.println(F("Usage: dither <hz>  (100000-3500000)  [legacy: sets both Omega]"));
    return;
  }

  // ---- raw sensor dump --------------------------------------
  // LOCKn = Teensy ADC counts (ERRn_BUS); the rest are the latest
  // I2C telemetry values in VOLTS (per-card ADS1115, 10 Hz round robin).
  if (line == "d") {
    out.print(F("LOCK1=")); out.print(analogRead(PIN_LOCK1_IN));
    out.print(F(" LOCK2=")); out.print(analogRead(PIN_LOCK2_IN));
    for (int i = 0; i < 2; i++) {
      const ChCardMon& m = chcard_mon(i);
      out.print(F(" NTC"));  out.print(i+1); out.print('='); out.print(m.ntc_v, 3);
      out.print(F(" LAS"));  out.print(i+1); out.print('='); out.print(m.las_imon_v, 3);
      out.print(F(" TEC"));  out.print(i+1); out.print('='); out.print(m.tec_imon_v, 3);
      out.print(F(" MPD"));  out.print(i+1); out.print('='); out.print(m.mpd_mon_v, 3);
      out.print(F(" POPT")); out.print(i+1); out.print('=');
      out.print(mpd_to_optical_mw(m.mpd_mon_v, m.ntc_v), 2);
    }
    const AfeMon& a = afe_mon();
    out.print(F(" PD1=")); out.print(a.pd_lvl_v[0], 3);
    out.print(F(" PD2=")); out.print(a.pd_lvl_v[1], 3);
    out.println();
    // ERR in volts against the 2.5 V zero, plus the null DAC state.
    for (int i = 0; i < 2; i++) {
      int raw = analogRead(i == 0 ? PIN_LOCK1_IN : PIN_LOCK2_IN);
      out.print(F("  ERR")); out.print(i+1);
      out.print(F(" = ")); out.print(raw * (ADC_REF_V / ADC_MAX), 4);
      out.print(F(" V  (zero = ")); out.print(ERR_CENTER_V, 3);
      out.print(F(" V, offset ")); 
      out.print((raw - ERR_MID_COUNTS) * (ADC_REF_V / ADC_MAX) * 1e3f, 2);
      out.print(F(" mV)  null="));  out.print(g_err_null[i]);
      out.println(afe_null_present() ? F("") : F("  [DAC NOT PRESENT]"));
    }
    return;
  }

  // ---- CDCE913 force reprogram -----------------------------
  if (line == "r") {
    extern void cdce_program_eeprom_force();
    cdce_program_eeprom_force();
    out.println(F("CDCE913 EEPROM reprogrammed"));
    return;
  }

  // ============================================================
  // Network commands
  // ============================================================

  // ---- netinfo — show current settings ----------------------
  if (line == "netinfo") {
    NetSettings ns;
    net_settings_load(ns);
    char buf[16];
    out.print(F("Mode:      ")); out.println(ns.use_dhcp ? F("DHCP") : F("Static"));
    net_fmt_ip(ns.static_ip, buf); out.print(F("Static IP: ")); out.println(buf);
    net_fmt_ip(ns.subnet,    buf); out.print(F("Subnet:    ")); out.println(buf);
    net_fmt_ip(ns.gateway,   buf); out.print(F("Gateway:   ")); out.println(buf);
    out.print(F("Hostname:  ")); out.println(ns.hostname);
    out.print(F("Active IP: ")); out.println(network_ip());
    out.println(F("(Changes require 'netreset' to take effect)"));
    return;
  }

  // ---- netset <field> <value> --------------------------------
  if (line.startsWith("netset ")) {
    NetSettings ns;
    net_settings_load(ns);
    String args = line.substring(7);
    args.trim();

    if (args.startsWith("ip ")) {
      if (net_parse_ip(args.c_str() + 3, ns.static_ip)) {
        net_settings_save(ns);
        out.println(F("Static IP saved. Run 'netreset' to apply."));
      } else { out.println(F("Invalid IP address (use a.b.c.d)")); }

    } else if (args.startsWith("sub ")) {
      if (net_parse_ip(args.c_str() + 4, ns.subnet)) {
        net_settings_save(ns);
        out.println(F("Subnet saved. Run 'netreset' to apply."));
      } else { out.println(F("Invalid subnet mask")); }

    } else if (args.startsWith("gw ")) {
      if (net_parse_ip(args.c_str() + 3, ns.gateway)) {
        net_settings_save(ns);
        out.println(F("Gateway saved. Run 'netreset' to apply."));
      } else { out.println(F("Invalid gateway address")); }

    } else if (args.startsWith("host ")) {
      String h = args.substring(5);
      h.trim();
      // Hostname: alphanumeric + hyphens, 1-31 chars
      bool valid = (h.length() > 0 && h.length() < 32);
      for (unsigned int i = 0; i < h.length() && valid; i++) {
        char c = h[i];
        if (!isalnum(c) && c != '-') valid = false;
      }
      if (valid) {
        strncpy(ns.hostname, h.c_str(), 31);
        ns.hostname[31] = '\0';
        net_settings_save(ns);
        out.print(F("Hostname saved: ")); out.println(ns.hostname);
        out.println(F("Run 'netreset' to apply. mDNS: <hostname>.local"));
      } else { out.println(F("Invalid hostname (a-z 0-9 hyphens, max 31 chars)")); }

    } else if (args == "dhcp on" || args == "dhcp 1") {
      ns.use_dhcp = true;
      net_settings_save(ns);
      out.println(F("DHCP enabled. Run 'netreset' to apply."));

    } else if (args == "dhcp off" || args == "dhcp 0") {
      ns.use_dhcp = false;
      net_settings_save(ns);
      out.println(F("Static IP mode. Run 'netreset' to apply."));

    } else {
      out.println(F("Usage:  netset ip <a.b.c.d>"));
      out.println(F("        netset sub <a.b.c.d>"));
      out.println(F("        netset gw <a.b.c.d>"));
      out.println(F("        netset host <name>"));
      out.println(F("        netset dhcp on|off"));
    }
    return;
  }

  // ---- demo on/off — toggle simulation mode -----------------
  if (line == "demo on" || line == "demo off") {
    bool enable = (line == "demo on");
    if (enable == g_demo_mode) {
      out.print(F("Demo mode already ")); out.println(enable ? F("ON") : F("OFF"));
      return;
    }
    g_demo_mode = enable;

    // Persist to EEPROM
    NetSettings ns;
    net_settings_load(ns);
    ns.demo_mode = enable;
    net_settings_save(ns);

    if (enable) {
      // Enter demo: restart simulation from IDLE
      demo_init(ch, disp);
      out.println(F("Demo mode ON — simulation running."));
      out.println(F("Type 'demo off' to switch to real hardware (persists across reboot)."));
    } else {
      // Exit demo: safe DAC outputs, reset to IDLE
      chcard_set_midscale_all();
      lock_init(ch[0]);
      lock_init(ch[1]);
      out.println(F("Demo mode OFF — real hardware mode."));
      out.println(F("Type 'lock1' / 'lock2' to start sweep+lock."));
    }
    return;
  }

  // ---- netreset — apply saved settings immediately ----------
  if (line == "netreset") {
    out.println(F("Restarting Ethernet with saved settings..."));
    network_reinit();
    out.print(F("Active IP: ")); out.println(network_ip());
    return;
  }

  // ---- help -------------------------------------------------
  if (line == "help" || line == "?") {
    out.print(F("Mode: ")); out.println(g_demo_mode ? F("DEMO (simulation)") : F("REAL hardware"));
    out.println(F("PDH Commands:"));
    out.println(F("  s / status           — full telemetry"));
    out.println(F("  lock1 / lock2        — start sweep+lock"));
    out.println(F("  stop1 / stop2        — stop to IDLE"));
    out.println(F("  hold1 / hold2        — freeze outputs"));
    out.println(F("  break1 / break2      — break lock -> RELOCK"));
    out.println(F("  reset1 / reset2      — clear relock counter"));
    out.println(F("  omega1 <hz>          — CH1 modulation Ω (100k-3.5M)"));
    out.println(F("  omega2 <hz>          — CH2 modulation Ω (100k-3.5M)"));
    out.println(F("  phase1 <deg>         — CH1 demod phase (reference DDS)"));
    out.println(F("  phase2 <deg>         — CH2 demod phase"));
    out.println(F("  cal1 / cal2          — auto-calibrate demod phase"));
    out.println(F("  null1 / null2        — auto-null ERR offset (BLOCK THE LIGHT)"));
    out.println(F("  nullset1 <code>      — set ERR null DAC code by hand (0-65535)"));
    out.println(F("  nullset2 <code>      — same, CH2"));
    out.println(F("  thresh1 lock acq     — CH1 capture/loss thresholds"));
    out.println(F("  thresh2 lock acq     — CH2 thresholds"));
    out.println(F("  p1 kp ki kd          — CH1 PID gains (LEGACY: servo is analog)"));
    out.println(F("  p2 kp ki kd          — CH2 PID gains (LEGACY)"));
    out.println(F("  dither <hz>          — legacy: set both Ω the same"));
    out.println(F("  d                    — raw ADC dump"));
    out.println(F("  r                    — reprogram CDCE913 EEPROM"));
    out.println(F("Demo:"));
    out.println(F("  demo on              — enable simulation (saves to EEPROM)"));
    out.println(F("  demo off             — real hardware mode (saves to EEPROM)"));
    out.println(F("Network:"));
    out.println(F("  netinfo              — show network settings"));
    out.println(F("  netset ip <a.b.c.d>  — set static IP"));
    out.println(F("  netset sub <a.b.c.d> — set subnet mask"));
    out.println(F("  netset gw <a.b.c.d>  — set gateway"));
    out.println(F("  netset host <name>   — set mDNS hostname"));
    out.println(F("  netset dhcp on|off   — DHCP or static mode"));
    out.println(F("  netreset             — apply settings now"));
    return;
  }

  out.print(F("Unknown: ")); out.println(line);
}
