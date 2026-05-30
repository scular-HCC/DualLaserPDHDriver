/* ============================================================
   pdh-screen.js  —  Shared 320x240 TFT renderer (pure functions)
   Visualises a `model` for the dual-PDH controller. No state, no
   side effects: used by the live prototype AND the screen board.
   window.PDH = { renderDevice, STATE, channel, fmt }
   ============================================================ */
(function () {
  'use strict';

  // ---- plot coordinate space ----
  var PW = 200, PH = 80, BASE = 70;   // transmission baseline (px from top)

  // ---- per-state presentation ----
  var STATE = {
    idle:    { label: 'IDLE',          color: 'var(--st-idle)',  mode: 'flat',    axis: 'STANDBY',                pulse: false },
    coarse:  { label: 'COARSE · TEC',  color: 'var(--st-sweep)', mode: 'sweep',   axis: 'TEC SWEEP · \u00b0C',     pulse: true  },
    fine:    { label: 'FINE · LAS-I',  color: 'var(--st-sweep)', mode: 'sweep',   axis: 'LASER-I SWEEP · mA',     pulse: true  },
    acquire: { label: 'ACQUIRE',       color: 'var(--st-acq)',   mode: 'capture', axis: 'PEAK LOCK-IN',           pulse: true  },
    locked:  { label: 'LOCKED',        color: 'var(--st-lock)',  mode: 'errtime', axis: 'PDH ERROR · IN-LOCK',    pulse: false },
    hold:    { label: 'HOLD',          color: 'var(--st-hold)',  mode: 'errtime', axis: 'PDH ERROR · HOLD',       pulse: false },
    fault:   { label: 'LOCK LOST',     color: 'var(--st-fault)', mode: 'errrail', axis: 'ERROR · RE-ACQUIRE',     pulse: true  },
    manual:  { label: 'MANUAL',        color: 'var(--st-hold)',  mode: 'sweep',   axis: 'MANUAL TUNE',            pulse: false }
  };

  function clamp(v, a, b) { return v < a ? a : v > b ? b : v; }
  function fmt(v, d) { return (v == null || isNaN(v)) ? '--' : v.toFixed(d); }

  // ---------- trace geometry ----------
  function lorentz(x, x0, w, amp) { var u = (x - x0) / w; return BASE - amp / (1 + u * u); }

  function transmissionPaths(x0, w, amp) {
    var line = '', area = 'M0,' + PH + ' ';
    for (var x = 0; x <= PW; x += 2) {
      var y = lorentz(x, x0, w, amp);
      line += (x === 0 ? 'M' : 'L') + x + ',' + y.toFixed(1) + ' ';
      area += 'L' + x + ',' + y.toFixed(1) + ' ';
    }
    area += 'L' + PW + ',' + PH + ' Z';
    return { line: line, area: area };
  }

  // dispersive PDH error signal: central S-curve + small sideband bumps
  function errorCurve(x0, w) {
    var mid = 38, A = 24, d = 1.9;
    var p = '';
    for (var x = 0; x <= PW; x += 2) {
      var u = (x - x0) / w;
      var central = -2 * u / Math.pow(1 + u * u, 2);
      var sbL = 0.8 * (-(u + d) / (1 + (u + d) * (u + d)));
      var sbR = 0.8 * (-(u - d) / (1 + (u - d) * (u - d)));
      var y = mid - A * (central) - 5 * (sbL + sbR);
      p += (x === 0 ? 'M' : 'L') + x + ',' + clamp(y, 4, PH - 4).toFixed(1) + ' ';
    }
    return p;
  }

  // time-domain error noise around centre (locked / hold)
  function noiseTrace(amp, mid) {
    mid = mid || 40;
    var p = '', y = mid;
    for (var x = 0; x <= PW; x += 4) {
      y = mid + (Math.random() - 0.5) * amp + Math.sin(x / 9) * amp * 0.12;
      p += (x === 0 ? 'M' : 'L') + x + ',' + clamp(y, 4, PH - 4).toFixed(1) + ' ';
    }
    return p;
  }

  // railing / diverging error (fault)
  function railTrace() {
    var p = '', y = 40;
    for (var x = 0; x <= PW; x += 4) {
      y += (Math.random() - 0.5) * 34;
      if (Math.random() > 0.78) y = Math.random() > 0.5 ? 8 : PH - 8;
      p += (x === 0 ? 'M' : 'L') + x + ',' + clamp(y, 6, PH - 6).toFixed(1) + ' ';
    }
    return p;
  }

  function grid() {
    var g = '';
    [40, 80, 120, 160].forEach(function (x) { g += '<line x1="' + x + '" y1="0" x2="' + x + '" y2="' + PH + '" class="gln"/>'; });
    [20, 40, 60].forEach(function (y) { g += '<line x1="0" y1="' + y + '" x2="' + PW + '" y2="' + y + '" class="gln"/>'; });
    return g;
  }

  // ---------- plot SVG for one channel ----------
  function plotSVG(ch, st) {
    var mx = clamp((ch.markerX == null ? 0.5 : ch.markerX) * PW, 0, PW);
    var px = clamp((ch.peakX == null ? 0.65 : ch.peakX) * PW, 0, PW);
    var body = '<svg viewBox="0 0 ' + PW + ' ' + PH + '" preserveAspectRatio="none">';
    body += grid();

    if (st.mode === 'flat') {
      body += '<line x1="0" y1="' + (PH - 8) + '" x2="' + PW + '" y2="' + (PH - 8) + '" class="tflat"/>';
    } else if (st.mode === 'sweep' || st.mode === 'capture') {
      var w = ch.state === 'coarse' ? 24 : 12;
      var amp = ch.state === 'coarse' ? 46 : 56;
      var tp = transmissionPaths(px, w, amp);
      body += '<path d="' + tp.area + '" class="tarea"/>';
      body += '<path d="' + tp.line + '" class="tline"/>';
      if (st.mode === 'capture') {
        body += '<path d="' + errorCurve(px, w) + '" class="eline"/>';
        body += '<line x1="0" y1="38" x2="' + PW + '" y2="38" class="ezero"/>';
      }
      // setpoint (found peak) marker
      body += '<line x1="' + px.toFixed(1) + '" y1="0" x2="' + px.toFixed(1) + '" y2="' + PH + '" class="setln"/>';
      body += '<circle cx="' + px.toFixed(1) + '" cy="' + lorentz(px, px, w, amp).toFixed(1) + '" r="2.4" class="setpt"/>';
      // live scan marker
      if (ch.state === 'coarse' || ch.state === 'fine' || ch.state === 'manual') {
        body += '<line x1="' + mx.toFixed(1) + '" y1="0" x2="' + mx.toFixed(1) + '" y2="' + PH + '" class="scanln"/>';
      }
    } else if (st.mode === 'errtime') {
      var a = ch.state === 'hold' ? 10 : (6 - clamp((ch.lockQual || 0.9), 0, 1) * 3);
      body += '<line x1="0" y1="40" x2="' + PW + '" y2="40" class="ezero"/>';
      body += '<path d="' + noiseTrace(a) + '" class="etime" style="stroke:' + st.color + '"/>';
    } else if (st.mode === 'errrail') {
      body += '<line x1="0" y1="40" x2="' + PW + '" y2="40" class="ezero"/>';
      body += '<path d="' + railTrace() + '" class="erail"/>';
    }
    body += '</svg>';
    return body;
  }

  // ---------- one channel panel ----------
  function channel(ch) {
    var st = STATE[ch.state] || STATE.idle;
    var pulse = st.pulse ? ' pulse' : '';
    var live = (ch.state === 'coarse' || ch.state === 'fine') ? ch.scanVal : null;

    var stats =
      stat('TEC TEMP', fmt(ch.tecTemp, 3), '\u00b0C', false) +
      stat('LASER I',  fmt(ch.laserI, 1), 'mA', false) +
      stat('SETPOINT', fmt(ch.setpoint, 1), '%', true) +
      stat('ERR RMS',  fmt(ch.errRms, 4), '', false);

    var q = clamp(ch.lockQual == null ? 0 : ch.lockQual, 0, 1);
    var health =
      '<div class="pdh-health" style="--sc:' + st.color + '">' +
        '<span class="hlab">SERVO</span>' +
        '<span class="bar"><i style="width:' + (q * 100).toFixed(0) + '%"></i></span>' +
        '<span class="hval">' + (q * 100).toFixed(0) + '%</span>' +
      '</div>';

    var pval = '';
    if (st.mode === 'sweep') pval = live != null ? live : '';
    else if (st.mode === 'errtime') pval = '\u00b1' + fmt(ch.errRms, 4);

    return '' +
      '<section class="pdh-chan" style="--accent:' + ch.accent + ';--sc:' + st.color + '">' +
        '<div class="pdh-rail"></div>' +
        '<div class="pdh-chan-in">' +
          '<div class="pdh-body">' +
            '<div class="pdh-plot">' +
              '<span class="pov-id">' + ch.id + '</span>' +
              '<span class="pdh-badge pov-badge' + pulse + '"><span class="bdot"></span>' + st.label + '</span>' +
              '<span class="plab">' + st.axis + '</span>' +
              (pval ? '<span class="pval">' + pval + '</span>' : '') +
              plotSVG(ch, st) +
            '</div>' +
            '<div class="pdh-reads">' + stats + health + '</div>' +
          '</div>' +
        '</div>' +
      '</section>';
  }

  function stat(k, v, unit, accent) {
    return '<div class="pdh-stat' + (accent ? ' accent' : '') + '">' +
      '<span class="k">' + k + '</span>' +
      '<span class="v">' + v + (unit ? '<small>' + unit + '</small>' : '') + '</span>' +
    '</div>';
  }

  // ---------- top bar ----------
  function topbar(m) {
    var pll = m.pllLock ? '' : ' warn';
    var running = m.run;
    return '' +
      '<div class="pdh-top">' +
        '<span class="pdh-chip"><span class="dot' + pll + '"></span>CLK ' + (m.pllLock ? 'PLL' : 'UNLK') + '</span>' +
        '<span class="pdh-chip">' + (m.refclk || '25.000') + ' MHz</span>' +
        '<span class="sp"></span>' +
      '</div>';
  }

  // ---------- settings / calibration screen ----------
  function settings(m) {
    var sel = m.selRow == null ? 1 : m.selRow;
    var rows = (m.rows || defaultSettingsRows()).map(function (r, i) {
      return '<div class="pdh-row' + (i === sel ? ' sel' : '') + '">' +
        '<span class="ico">' + r.ico + '</span>' +
        '<span class="lab">' + r.lab + '</span>' +
        '<span class="val">' + r.val + '</span>' +
      '</div>';
    }).join('');
    return '<div class="pdh-set">' +
      '<div class="pdh-set-h">Settings · Calibration</div>' +
      '<div class="pdh-rows">' + rows + '</div>' +
      '<div class="pdh-set-foot">' +
        '<span class="pdh-key">\u2191\u2193</span> select' +
        '<span class="pdh-key">\u21B5</span> edit' +
        '<span class="sp" style="flex:1"></span>' +
        '<span>serial · 115200</span>' +
      '</div>' +
    '</div>';
  }
  function defaultSettingsRows() {
    return [
      { ico: 'f', lab: 'Dither frequency', val: '10.000 kHz' },
      { ico: 'P', lab: 'CH1 servo  Kp / Ki', val: '0.080 / 0.50' },
      { ico: 'P', lab: 'CH2 servo  Kp / Ki', val: '0.080 / 0.50' },
      { ico: 'T', lab: 'TEC sweep range', val: '18.0 – 32.0 \u00b0C' },
      { ico: 'I', lab: 'Laser-I sweep span', val: '\u00b1 6.0 mA' },
      { ico: 'L', lab: 'Lock threshold', val: '0.020' },
      { ico: 'C', lab: 'CDCE913 EEPROM', val: 'v0x51 OK' }
    ];
  }

  // ---------- full device ----------
  function renderDevice(m) {
    var inner;
    if (m.screen === 'settings') {
      inner = settings(m);
    } else {
      inner = channel(m.ch[0]) + channel(m.ch[1]);
    }
    return '<div class="pdh">' + inner + '</div>';
  }

  window.PDH = { renderDevice: renderDevice, STATE: STATE, channel: channel, fmt: fmt, clamp: clamp };
})();
