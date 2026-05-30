#pragma once

// ============================================================
// Web dashboard — served at HTTP GET /
// Single self-contained HTML/CSS/JS file embedded as a string.
// Uses raw-string literal (C++11) to avoid escaping.
// ============================================================

static const char WEBPAGE_HTML[] = R"HTMLEND(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dual-PDH Controller</title>
<style>
:root{
  --bg:#070b11;--panel:#0e141e;--p2:#121a26;--rule:#1b2533;
  --ink:#e8eef6;--dim:#8696aa;--mu:#4a586b;
  --c1:#2dd4cf;--c2:#f5a524;
  --lk:#34e0a1;--sw:#5aa9ff;--ac:#fbbf24;--ho:#a78bfa;--fa:#ff5a6a;--id:#64748b;
  --f:system-ui,-apple-system,sans-serif;--m:'Cascadia Code','Courier New',monospace;
}
*{box-sizing:border-box;margin:0;padding:0}
body{background:var(--bg);color:var(--ink);font-family:var(--f);min-height:100vh;padding:14px}
/* ---- header ---- */
.hdr{display:flex;align-items:center;gap:10px;padding:9px 14px;background:var(--panel);border:1px solid var(--rule);border-radius:8px;margin-bottom:12px;flex-wrap:wrap}
.logo{font-weight:700;font-size:15px;letter-spacing:1.2px;color:var(--ink)}
.logo b{color:var(--c1)}
.dot{width:8px;height:8px;border-radius:50%;flex-shrink:0}
.chip{font-family:var(--m);font-size:10px;color:var(--dim);padding:2px 7px;border:1px solid var(--rule);border-radius:4px;white-space:nowrap}
.sp{flex:1}
#poll{font-size:11px;font-family:var(--m);transition:color .3s}
/* ---- global controls ---- */
.gctrl{display:flex;gap:8px;margin-bottom:12px}
.gbtn{flex:1;appearance:none;border:1px solid var(--rule);border-radius:8px;cursor:pointer;font-family:var(--f);font-weight:700;font-size:13px;letter-spacing:.6px;padding:10px;color:var(--ink);background:var(--p2);transition:.12s}
.gbtn:hover{transform:translateY(-1px)}
.gbtn.run{color:var(--lk);border-color:color-mix(in srgb,var(--lk) 45%,transparent);background:color-mix(in srgb,var(--lk) 12%,var(--p2))}
/* ---- channel grid ---- */
.chs{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px}
@media(max-width:720px){.chs{grid-template-columns:1fr}}
.ch{background:var(--panel);border:1px solid var(--rule);border-radius:10px;overflow:hidden}
.ch-rail{height:4px}
.ch-in{padding:10px}
.ch-head{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.ch-id{font-weight:700;font-size:14px;letter-spacing:.4px}
.badge{display:inline-flex;align-items:center;gap:5px;font-size:10px;font-weight:600;padding:2px 8px 2px 6px;border-radius:999px;letter-spacing:.6px;white-space:nowrap}
.bdot{width:5px;height:5px;border-radius:50%}
/* ---- plot ---- */
.plot-wrap{background:#080d14;border:1px solid var(--rule);border-radius:4px;margin-bottom:8px;overflow:hidden;position:relative}
.plot-wrap svg{display:block;width:100%;height:80px}
.plot-wrap svg .g{stroke:#16202e;stroke-width:1;vector-effect:non-scaling-stroke}
.plot-wrap svg .flat{stroke-width:1;stroke-dasharray:2 3;vector-effect:non-scaling-stroke;opacity:.6}
.plot-wrap svg .fill{stroke:none;opacity:.16}
.plot-wrap svg .tr{fill:none;stroke-width:1.6;vector-effect:non-scaling-stroke}
.plot-wrap svg .scan{stroke-width:1.5;vector-effect:non-scaling-stroke;opacity:.85}
.plot-wrap svg .eline{fill:none;stroke:var(--c2);stroke-width:1.3;vector-effect:non-scaling-stroke}
.plot-wrap svg .ez{stroke:var(--mu);stroke-width:1;stroke-dasharray:1 4;vector-effect:non-scaling-stroke;opacity:.7}
.plot-wrap svg .etime{fill:none;stroke-width:1.3;vector-effect:non-scaling-stroke}
.plot-wrap svg .erail{fill:none;stroke:var(--fa);stroke-width:1.4;vector-effect:non-scaling-stroke}
.plot-wrap svg .setl{stroke:var(--lk);stroke-width:1;stroke-dasharray:3 2;vector-effect:non-scaling-stroke;opacity:.8}
.plot-wrap svg .setp{fill:var(--lk)}
.plot-lbl{position:absolute;left:6px;bottom:4px;font-family:var(--m);font-size:9px;letter-spacing:.7px;color:var(--mu);text-transform:uppercase;pointer-events:none}
.plot-val{position:absolute;right:6px;bottom:4px;font-family:var(--m);font-size:9px;color:inherit;pointer-events:none}
/* ---- stats ---- */
.stats{display:grid;grid-template-columns:1fr 1fr;gap:4px;margin-bottom:5px}
.stat{background:var(--p2);border:1px solid var(--rule);border-radius:3px;padding:3px 7px}
.stat .k{font-size:9px;letter-spacing:.7px;color:var(--mu);text-transform:uppercase;line-height:1.2}
.stat .v{font-family:var(--m);font-size:16px;line-height:1.1;color:var(--ink)}
.stat .u{font-size:9px;color:var(--dim)}
.stat.hl .v{color:inherit}
/* ---- servo bar ---- */
.srv{background:var(--p2);border:1px solid var(--rule);border-radius:3px;padding:4px 7px;display:flex;align-items:center;gap:7px;margin-bottom:8px}
.srv-l{font-size:9px;letter-spacing:.6px;color:var(--mu);text-transform:uppercase}
.srv-t{flex:1;height:5px;background:#0a1019;border-radius:3px;overflow:hidden}
.srv-f{height:100%;border-radius:3px;transition:width .25s}
.srv-p{font-family:var(--m);font-size:9px}
/* ---- channel buttons ---- */
.btns{display:flex;gap:5px;flex-wrap:wrap}
.btn{appearance:none;border:1px solid var(--rule);background:var(--p2);color:var(--ink);font-family:var(--f);font-size:11px;font-weight:600;padding:5px 9px;border-radius:6px;cursor:pointer;transition:.12s;white-space:nowrap}
.btn:hover{transform:translateY(-1px);background:#16202d}
.btn.pri{border-color:color-mix(in srgb,var(--ac) 55%,transparent);color:var(--ac);background:color-mix(in srgb,var(--ac) 14%,var(--p2))}
.btn.danger{color:var(--fa);border-color:color-mix(in srgb,var(--fa) 45%,transparent)}
.btn.ghost{color:var(--dim)}
/* ---- settings ---- */
details.settings{background:var(--panel);border:1px solid var(--rule);border-radius:8px;margin-bottom:12px}
details.settings summary{padding:10px 14px;cursor:pointer;font-size:11px;letter-spacing:1.2px;text-transform:uppercase;color:var(--dim);font-weight:600;list-style:none;user-select:none}
details.settings summary::marker{display:none}
details.settings summary::before{content:"▸  "}
details.settings[open] summary::before{content:"▾  "}
.set-grid{padding:0 14px 14px;display:grid;grid-template-columns:1fr 1fr;gap:16px}
@media(max-width:600px){.set-grid{grid-template-columns:1fr}}
.set-ch h4{font-size:11px;letter-spacing:.8px;text-transform:uppercase;margin-bottom:9px;font-weight:700}
.fr{display:flex;gap:6px;align-items:center;margin-bottom:5px}
.fr label{font-size:10px;color:var(--dim);width:56px;flex-shrink:0;font-family:var(--m)}
.fr input{flex:1;background:var(--p2);border:1px solid var(--rule);color:var(--ink);font-family:var(--m);font-size:12px;padding:4px 7px;border-radius:4px;outline:none;min-width:0}
.fr input:focus{border-color:var(--c1)}
.fr-apply{appearance:none;background:none;border:1px solid var(--rule);color:var(--dim);font-family:var(--f);font-size:10px;padding:3px 8px;border-radius:4px;cursor:pointer}
.fr-apply:hover{color:var(--ink);border-color:var(--ink)}
.set-divider{border:none;border-top:1px solid var(--rule);margin:9px 0}
/* ---- command box ---- */
.cmd-box{background:var(--panel);border:1px solid var(--rule);border-radius:8px;padding:12px 14px}
.cmd-box h3{font-size:11px;letter-spacing:1.2px;text-transform:uppercase;color:var(--dim);margin-bottom:8px;font-weight:600}
.cmd-row{display:flex;gap:6px}
#cmd-in{flex:1;background:var(--p2);border:1px solid var(--rule);color:var(--ink);font-family:var(--m);font-size:12px;padding:7px 10px;border-radius:5px;outline:none;min-width:0}
#cmd-in:focus{border-color:var(--c1)}
#cmd-send{appearance:none;background:color-mix(in srgb,var(--c1) 14%,var(--p2));border:1px solid color-mix(in srgb,var(--c1) 50%,transparent);color:var(--c1);font-family:var(--f);font-weight:600;font-size:12px;padding:7px 14px;border-radius:5px;cursor:pointer;white-space:nowrap}
#cmd-out{font-family:var(--m);font-size:11px;color:var(--dim);margin-top:8px;min-height:18px;line-height:1.6;white-space:pre-wrap;max-height:200px;overflow-y:auto}
</style>
</head>
<body>
<div class="hdr">
  <span class="logo">DUAL<b>PDH</b></span>
  <span class="dot" id="pll-dot" style="background:var(--mu)"></span>
  <span class="chip" id="pll-txt">CLK ---</span>
  <span class="chip" id="ip-chip">connecting...</span>
  <span class="chip" id="up-chip">--:--:--</span>
  <span class="sp"></span>
  <span id="poll" style="color:var(--mu)">●  connecting</span>
</div>
<div class="gctrl">
  <button class="gbtn run" onclick="both('lock')">▶  RUN BOTH</button>
  <button class="gbtn" onclick="both('stop')">■  STOP ALL</button>
</div>
<div class="chs" id="chs">
  <div class="ch" id="ch1"><div class="ch-rail" style="background:var(--c1)"></div><div class="ch-in" style="text-align:center;padding:30px;color:var(--mu)">Connecting…</div></div>
  <div class="ch" id="ch2"><div class="ch-rail" style="background:var(--c2)"></div><div class="ch-in" style="text-align:center;padding:30px;color:var(--mu)">Connecting…</div></div>
</div>
<details class="settings" id="settings">
  <summary>Settings &amp; PID Tuning</summary>
  <div class="set-grid" id="set-grid"></div>
</details>
<div class="cmd-box">
  <h3>Serial Console</h3>
  <div class="cmd-row">
    <input id="cmd-in" placeholder="lock1  p1 0.08 0.5 0  thresh1 0.02 0.1  dither 10000  help" onkeydown="if(event.key==='Enter')sendCmd()">
    <button id="cmd-send" onclick="sendCmd()">Send</button>
  </div>
  <div id="cmd-out"></div>
</div>
<script>
// ---- colour helpers ----
const CC=['#2dd4cf','#f5a524'];
const SC={IDLE:'#64748b',SEARCH:'#5aa9ff',ACQUIRE:'#fbbf24',LOCKED:'#34e0a1',HOLD:'#a78bfa',RELOCK:'#ff5a6a'};
const AXES={IDLE:'STANDBY',SEARCH:'TEC / LAS-I SWEEP',ACQUIRE:'PEAK LOCK-IN',LOCKED:'PDH ERROR IN-LOCK',HOLD:'PDH ERROR HOLD',RELOCK:'ERROR RE-ACQUIRE'};

// ---- trace math (port of pdh-screen.js) ----
const PW=200,PH=80,BASE=70;
function clamp(v,a,b){return v<a?a:v>b?b:v;}
function lor(x,x0,w,amp){const u=(x-x0)/w;return BASE-amp/(1+u*u);}

function makeSVG(st,scan,peak,accent,sc,rms){
  let b=`<svg viewBox="0 0 ${PW} ${PH}" preserveAspectRatio="none">`;
  // grid
  [40,80,120,160].forEach(x=>b+=`<line x1="${x}" y1="0" x2="${x}" y2="${PH}" class="g"/>`);
  [20,40,60].forEach(y=>b+=`<line x1="0" y1="${y}" x2="${PW}" y2="${y}" class="g"/>`);

  if(st==='IDLE'){
    b+=`<line x1="0" y1="72" x2="${PW}" y2="72" class="flat" style="stroke:${accent}"/>`;
  } else if(st==='SEARCH'){
    const x0=peak*PW,w=24,amp=46;
    let l='',a=`M0,${PH} `;
    for(let x=0;x<=PW;x+=2){
      const y=lor(x,x0,w,amp).toFixed(1);
      l+=(x?'L':'M')+x+','+y+' '; a+='L'+x+','+y+' ';
    }
    a+=`L${PW},${PH} Z`;
    b+=`<path d="${a}" class="fill" style="fill:${accent}"/>`;
    b+=`<path d="${l}" class="tr" style="stroke:${accent}"/>`;
    const mx=(scan*PW).toFixed(1);
    b+=`<line x1="${mx}" y1="0" x2="${mx}" y2="${PH}" class="scan" style="stroke:${sc}"/>`;
  } else if(st==='ACQUIRE'){
    const x0=peak*PW,w=12,amp=56;
    let l='',a=`M0,${PH} `;
    for(let x=0;x<=PW;x+=2){
      const y=lor(x,x0,w,amp).toFixed(1);
      l+=(x?'L':'M')+x+','+y+' '; a+='L'+x+','+y+' ';
    }
    a+=`L${PW},${PH} Z`;
    b+=`<path d="${a}" class="fill" style="fill:${accent}"/>`;
    b+=`<path d="${l}" class="tr" style="stroke:${accent}"/>`;
    b+=`<line x1="0" y1="38" x2="${PW}" y2="38" class="ez"/>`;
    // S-curve
    let e='';
    const d=1.9;
    for(let x=0;x<=PW;x+=2){
      const u=(x-x0)/w,d2=1+u*u,c=-2*u/(d2*d2);
      const sL=0.8*(-(u+d)/(1+(u+d)**2)),sR=0.8*(-(u-d)/(1+(u-d)**2));
      const y=clamp(38-24*c-5*(sL+sR),4,PH-4).toFixed(1);
      e+=(x?'L':'M')+x+','+y+' ';
    }
    b+=`<path d="${e}" class="eline"/>`;
    const spx=(peak*PW).toFixed(1),spy=lor(peak*PW,x0,w,amp).toFixed(1);
    b+=`<line x1="${spx}" y1="0" x2="${spx}" y2="${PH}" class="setl"/>`;
    b+=`<circle cx="${spx}" cy="${spy}" r="2.5" class="setp"/>`;
  } else if(st==='LOCKED'||st==='HOLD'){
    const amp=st==='HOLD'?10:clamp(6-(rms||0)*40,1.5,6);
    b+=`<line x1="0" y1="40" x2="${PW}" y2="40" class="ez"/>`;
    let p='',y=40;
    for(let x=0;x<=PW;x+=4){
      y=40+(Math.random()-.5)*amp+Math.sin(x/9)*amp*.12;
      p+=(x?'L':'M')+x+','+clamp(y,4,PH-4).toFixed(1)+' ';
    }
    b+=`<path d="${p}" class="etime" style="stroke:${sc}"/>`;
  } else if(st==='RELOCK'){
    b+=`<line x1="0" y1="40" x2="${PW}" y2="40" class="ez"/>`;
    let p='',y=40;
    for(let x=0;x<=PW;x+=4){
      y+=(Math.random()-.5)*34;
      if(Math.random()>.78)y=Math.random()>.5?8:PH-8;
      p+=(x?'L':'M')+x+','+clamp(y,6,PH-6).toFixed(1)+' ';
    }
    b+=`<path d="${p}" class="erail"/>`;
  }
  b+='</svg>';
  return b;
}

function ff(v,d){return(v==null||isNaN(v))?'---':v.toFixed(d);}

// ---- channel render ----
function renderCh(c){
  const el=document.getElementById('ch'+c.n);
  if(!el) return;
  const acc=CC[c.n-1],sc=SC[c.st]||'#64748b';
  const axLabel=AXES[c.st]||'';
  let pval='';
  if(c.st==='SEARCH')pval=ff(c.scan*100,0)+'%';
  else if(c.st==='LOCKED'||c.st==='HOLD')pval='±'+ff(c.rms,4);

  el.innerHTML=`
<div class="ch-rail" style="background:${acc}"></div>
<div class="ch-in">
  <div class="ch-head">
    <span class="ch-id" style="color:${acc}">CH${c.n}</span>
    <span class="badge" style="color:${sc};background:${sc}22;border:1px solid ${sc}55">
      <span class="bdot" style="background:${sc};box-shadow:0 0 5px ${sc}"></span>${c.st}
    </span>
    <span class="sp"></span>
    <span style="font-family:var(--m);font-size:9px;color:var(--mu)">${c.relk?c.relk+' relock':''}</span>
  </div>
  <div class="plot-wrap" style="color:${acc}">
    ${makeSVG(c.st,c.scan,c.peak,acc,sc,c.rms)}
    <span class="plot-lbl">${axLabel}</span>
    ${pval?`<span class="plot-val">${pval}</span>`:''}
  </div>
  <div class="stats">
    <div class="stat"><div class="k">TEC TEMP</div><div class="v">${ff(c.temp,2)}<span class="u"> °C</span></div></div>
    <div class="stat"><div class="k">LASER I</div><div class="v">${ff(c.ima,1)}<span class="u"> mA</span></div></div>
    <div class="stat hl" style="color:${acc}"><div class="k">SETPOINT</div><div class="v">${ff(c.setp,1)}<span class="u"> %</span></div></div>
    <div class="stat"><div class="k">ERR RMS</div><div class="v" style="font-size:13px">${ff(c.rms,5)}</div></div>
  </div>
  <div class="srv">
    <span class="srv-l">SERVO</span>
    <div class="srv-t"><div class="srv-f" style="width:${(c.qual*100).toFixed(0)}%;background:${sc}"></div></div>
    <span class="srv-p" style="color:${sc}">${(c.qual*100).toFixed(0)}%</span>
  </div>
  <div class="btns">
    <button class="btn pri" style="--ac:${acc}" onclick="cmd('lock${c.n}')">↻ Sweep+Lock</button>
    <button class="btn" onclick="cmd('hold${c.n}')">⏸ Hold</button>
    <button class="btn danger" onclick="cmd('break${c.n}')">⚡ Break</button>
    <button class="btn ghost" onclick="cmd('stop${c.n}')">■ Stop</button>
  </div>
</div>`;
}

// ---- settings render (only on first open or explicit refresh) ----
let settingsInited=false;
function renderSettings(data){
  const el=document.getElementById('set-grid');
  el.innerHTML=data.ch.map(c=>`
<div class="set-ch">
  <h4 style="color:${CC[c.n-1]}">CH${c.n} — PID Gains</h4>
  <div class="fr"><label>Kp</label><input id="kp${c.n}" value="${c.kp.toFixed(5)}"></div>
  <div class="fr"><label>Ki</label><input id="ki${c.n}" value="${c.ki.toFixed(5)}"></div>
  <div class="fr"><label>Kd</label><input id="kd${c.n}" value="${c.kd.toFixed(5)}">
    <button class="fr-apply" onclick="applyPID(${c.n})">Apply</button></div>
  <hr class="set-divider">
  <h4 style="color:${CC[c.n-1]};margin-top:6px">CH${c.n} — Thresholds</h4>
  <div class="fr"><label>Lock thr</label><input id="lt${c.n}" value="${c.lthr.toFixed(5)}"></div>
  <div class="fr"><label>Acq thr</label><input id="at${c.n}" value="${c.athr.toFixed(5)}">
    <button class="fr-apply" onclick="applyThr(${c.n})">Apply</button></div>
</div>`).join('')+`
<div class="set-ch">
  <h4 style="color:var(--dim)">Global</h4>
  <div class="fr"><label>Dither</label><input id="dHz" value="${data.dither||10000}">
    <button class="fr-apply" onclick="applyDither()">Apply</button></div>
</div>`;
}

function applyPID(n){
  const kp=V('kp'+n),ki=V('ki'+n),kd=V('kd'+n);
  cmd(`p${n} ${kp} ${ki} ${kd}`);
}
function applyThr(n){cmd(`thresh${n} ${V('lt'+n)} ${V('at'+n)}`);}
function applyDither(){cmd(`dither ${V('dHz')}`);}
function V(id){return document.getElementById(id).value.trim();}

// ---- command send ----
async function cmd(c){
  try{
    const r=await fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'text/plain'},body:c});
    const t=await r.text();
    const out=document.getElementById('cmd-out');
    out.textContent=(out.textContent?out.textContent+'\n':'')+('> '+c+'\n'+t.trim());
    out.scrollTop=out.scrollHeight;
  }catch(e){
    const out=document.getElementById('cmd-out');
    out.textContent+='Error: '+e.message+'\n';
  }
}
async function sendCmd(){
  const inp=document.getElementById('cmd-in');
  const v=inp.value.trim();
  if(!v)return;
  await cmd(v);
  inp.value='';
}
async function both(action){
  await cmd(action+'1');
  await cmd(action+'2');
}

// ---- polling ----
let fails=0;
async function poll(){
  const ind=document.getElementById('poll');
  try{
    const r=await fetch('/api/status');
    const d=await r.json();
    fails=0;
    ind.textContent='●  '+new Date().toLocaleTimeString();
    ind.style.color='#34e0a1';

    // Top bar
    const dot=document.getElementById('pll-dot');
    dot.style.background=d.pll?'var(--lk)':'var(--ac)';
    dot.style.boxShadow='0 0 7px '+(d.pll?'var(--lk)':'var(--ac)');
    document.getElementById('pll-txt').textContent=(d.pll?'CLK PLL':'CLK UNLK')+' · '+d.refclk.toFixed(3)+' MHz';
    document.getElementById('ip-chip').textContent=d.ip||'--';
    const s=Math.floor(d.t/1000);
    document.getElementById('up-chip').textContent=
      String(s/3600|0).padStart(2,'0')+':'+String(s/60%60|0).padStart(2,'0')+':'+String(s%60).padStart(2,'0');

    // Channels
    d.ch.forEach(c=>renderCh(c));

    // Settings — fill on first open, don't stomp mid-edit
    const det=document.getElementById('settings');
    if(!settingsInited){renderSettings(d);settingsInited=true;}
    else if(det.open&&!document.activeElement.matches('.set-ch input')){renderSettings(d);}
  }catch(e){
    fails++;
    ind.style.color=fails>3?'var(--fa)':'var(--ac)';
    ind.textContent='●  offline ('+fails+')';
  }
}
setInterval(poll,500);
poll();
</script>
</body>
</html>
)HTMLEND";
