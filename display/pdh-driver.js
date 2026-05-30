/* ============================================================
   pdh-sim.js — drives the prototype: physics-flavoured state
   machine + 10 Hz display refresh + serial log.
   ============================================================ */
(function () {
  'use strict';
  var screenEl = document.getElementById('screen');
  var serialEl = document.getElementById('serial');

  function rnd(a, b) { return a + Math.random() * (b - a); }

  // ---- channel sim object ----
  function mkCh(id, accent, name) {
    return {
      id: id, accent: accent, name: name,
      state: 'idle', ts: 0,
      tecTemp: 24.0, laserI: 138.0, setpoint: 50.0, errRms: 0.500,
      lockQual: 0, markerX: 0, peakX: 0.6, scanVal: '',
      _logState: 'idle'
    };
  }

  var model = {
    run: false, pllLock: true, refclk: '25.000', uptime: '00:00:00',
    screen: 'dash', selRow: 1,
    ch: [
      mkCh('CH1', 'var(--ch1)', '1550 nm DFB · Cavity A'),
      mkCh('CH2', 'var(--ch2)', '1560 nm DFB · Cavity B')
    ]
  };

  // ---- transitions ----
  function enter(c, s) {
    c.state = s; c.ts = 0;
    if (s === 'coarse') { c.peakX = rnd(0.5, 0.74); c.markerX = 0; }
    if (s === 'fine')   { c.peakX = rnd(0.42, 0.6); c.markerX = 0; }
  }

  var LBL = { idle:'IDLE', coarse:'COARSE', fine:'FINE', acquire:'ACQUIRE', locked:'LOCKED', hold:'HOLD', fault:'LOST', manual:'MANUAL' };

  // ---- per-channel physics step ----
  function step(c, dt) {
    c.ts += dt;
    switch (c.state) {
      case 'idle':
        c.lockQual = 0; c.scanVal = '';
        break;

      case 'coarse': {
        var p = Math.min(c.ts / 3.0, 1);
        c.markerX = p;
        c.tecTemp = 18.0 + 14.0 * p + Math.sin(c.ts * 9) * 0.004;
        c.setpoint = 20 + 60 * p;
        c.errRms = 0.4 + Math.random() * 0.12;
        c.lockQual = 0;
        c.scanVal = c.tecTemp.toFixed(2) + ' \u00b0C';
        if (p >= 1) { enter(c, 'fine'); }
        break;
      }
      case 'fine': {
        var pf = Math.min(c.ts / 2.4, 1);
        c.markerX = pf;
        c.laserI = 134.0 + 16.0 * pf;
        c.setpoint = 30 + 55 * pf;
        c.errRms = 0.28 + Math.random() * 0.1;
        c.scanVal = c.laserI.toFixed(1) + ' mA';
        if (pf >= 1) { enter(c, 'acquire'); }
        break;
      }
      case 'acquire': {
        var pa = Math.min(c.ts / 1.3, 1);
        c.markerX = c.peakX;
        c.laserI = 134.0 + 16.0 * c.peakX + Math.sin(c.ts * 30) * 0.3 * (1 - pa);
        c.setpoint = 30 + 55 * c.peakX;
        c.errRms = 0.22 * (1 - pa) + 0.05;
        c.lockQual = pa * 0.9;
        c.scanVal = '';
        if (pa >= 1) { enter(c, 'locked'); }
        break;
      }
      case 'locked':
        c.errRms = rnd(0.0028, 0.0065);
        c.lockQual = rnd(0.9, 0.99);
        c.tecTemp += (Math.random() - 0.5) * 0.0008;
        c.laserI += (Math.random() - 0.5) * 0.03;
        break;

      case 'hold':
        c.errRms = rnd(0.02, 0.05);
        c.lockQual = rnd(0.55, 0.7);
        break;

      case 'fault':
        c.errRms = rnd(0.3, 0.62);
        c.lockQual = 0;
        c.tecTemp += (Math.random() - 0.5) * 0.01;
        if (c.ts > 1.6) { enter(c, 'fine'); }  // back off, re-acquire
        break;

      case 'manual':
        c.lockQual = 0;
        c.markerX = 0.5 + Math.sin(c.ts * 0.6) * 0.28;
        c.scanVal = c.laserI.toFixed(1) + ' mA';
        c.laserI = 134.0 + 16.0 * c.markerX;
        break;
    }

    // log transitions
    if (c.state !== c._logState) {
      logTransition(c);
      c._logState = c.state;
    }
  }

  // ---- serial log ----
  var logLines = [];
  function pushLog(html) {
    logLines.push(html);
    if (logLines.length > 9) logLines.shift();
    serialEl.innerHTML = logLines.map(function (l) { return '<div>' + l + '</div>'; }).join('');
  }
  function ts() {
    var s = model._secs | 0;
    var hh = String((s / 3600 | 0) % 24).padStart(2, '0');
    var mm = String((s / 60 | 0) % 60).padStart(2, '0');
    var ss = String(s % 60).padStart(2, '0');
    return hh + ':' + mm + ':' + ss;
  }
  function logTransition(c) {
    var cc = c.id === 'CH1' ? 'c1' : 'c2';
    var t = '<span class="t">[' + ts() + ']</span> ';
    var msg = {
      coarse:  c.id + ' \u2192 SEARCH (coarse TEC sweep)',
      fine:    c.id + ' \u2192 fine laser-I sweep',
      acquire: c.id + ' peak found, ACQUIRE lock-in',
      locked:  '<span class="ok">' + c.id + ' LOCKED · servo engaged</span>',
      hold:    c.id + ' HOLD',
      fault:   '<span class="er">' + c.id + ' LOCK LOST \u2192 re-acquire</span>',
      manual:  c.id + ' manual tune',
      idle:    c.id + ' idle'
    }[c.state];
    pushLog(t + '<span class="' + cc + '">' + msg + '</span>');
  }

  // ---- loop ----
  var RENDER_MS = 80, last = performance.now(), acc = 0;
  model._secs = 0; var secAcc = 0;
  function frame(now) {
    var dt = Math.min((now - last) / 1000, 0.1); last = now;
    acc += dt * 1000; secAcc += dt;
    if (secAcc >= 1) { model._secs += Math.floor(secAcc); secAcc -= Math.floor(secAcc); }
    model.uptime = ts();

    if (acc >= RENDER_MS) {
      acc = 0;
      var anyRun = false;
      model.ch.forEach(function (c) {
        step(c, RENDER_MS / 1000);
        if (c.state !== 'idle') anyRun = true;
      });
      model.run = anyRun;
      if (model.screen !== 'settings') {
        screenEl.innerHTML = PDH.renderDevice(model);
      }
      document.getElementById('st1').textContent = LBL[model.ch[0].state];
      document.getElementById('st2').textContent = LBL[model.ch[1].state];
    }
    requestAnimationFrame(frame);
  }
  requestAnimationFrame(frame);

  // ---- initial render ----
  screenEl.innerHTML = PDH.renderDevice(model);

  // ---- controls ----
  document.getElementById('runBoth').addEventListener('click', function () {
    if (model.screen === 'settings') { model.screen = 'dash'; }
    enter(model.ch[0], 'coarse');
    model.ch[1].ts = -0.6;            // slight stagger
    enter(model.ch[1], 'coarse'); model.ch[1].ts = -0.6;
    pushLog('<span class="t">[' + ts() + ']</span> > run');
  });
  document.getElementById('stopAll').addEventListener('click', function () {
    model.ch.forEach(function (c) { enter(c, 'idle'); });
    pushLog('<span class="t">[' + ts() + ']</span> > stop all');
  });
  document.getElementById('setBtn').addEventListener('click', function () {
    model.screen = (model.screen === 'settings') ? 'dash' : 'settings';
    screenEl.innerHTML = PDH.renderDevice(model);
    var b = document.getElementById('setBtn');
    b.classList.toggle('primary', model.screen === 'settings');
    b.style.setProperty('--accent', 'var(--ch1)');
  });

  document.querySelectorAll('.btn[data-act]').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var c = model.ch[+btn.dataset.ch];
      var act = btn.dataset.act;
      if (model.screen === 'settings' && act !== 'stop') { model.screen = 'dash'; }
      if (act === 'sweep') { enter(c, 'coarse'); pushLog('<span class="t">[' + ts() + ']</span> > ' + c.id.toLowerCase() + ' sweep'); }
      else if (act === 'break') {
        if (c.state === 'locked' || c.state === 'acquire' || c.state === 'hold') enter(c, 'fault');
      }
      else if (act === 'stop') { enter(c, 'idle'); }
      else if (act === 'manual') { enter(c, 'manual'); }
    });
  });
})();
