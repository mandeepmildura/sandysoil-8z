#pragma once

// -----------------------------------------
//  webui.h
//  Sandy Soil 8Z — Irrigation Controller
//  Single-file HTML/CSS/JS dashboard served
//  from PROGMEM — no SD card or SPIFFS needed.
//
//  Route: GET /
//  Polls: /api/status, /api/zones, /api/pressure
//  Every 2 seconds for live updates.
//
//  Settings modal includes WiFi credentials,
//  MQTT, pressure sensor, and board config.
//
//  /update route provides HTTP OTA firmware upload.
// -----------------------------------------

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

static const char WEBUI_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sandy Soil 8Z — Irrigation</title>
<style>
:root{--bg:#1a1f2e;--card:#252b3b;--border:#2d3548;--green:#4ade80;--red:#ef4444;--yellow:#f59e0b;--text:#e2e8f0;--muted:#94a3b8}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
header{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:8px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--border)}
header h1{font-size:15px;font-weight:600;white-space:nowrap}
.badges{display:flex;gap:6px;flex-wrap:wrap;align-items:center}
.badge{font-size:11px;padding:3px 8px;border-radius:99px;border:1px solid var(--border);color:var(--muted)}
.badge.ok{color:var(--green);border-color:var(--green)}
.badge.err{color:var(--red);border-color:var(--red)}
.pbar{display:flex;align-items:center;gap:12px;padding:9px 16px;background:var(--card);border-bottom:1px solid var(--border);font-size:13px;color:var(--muted)}
.pbar .pval{color:var(--text);font-size:18px;font-weight:700}
.pbar.low .pval{color:var(--red)}
.zones{display:grid;grid-template-columns:repeat(auto-fill,minmax(155px,1fr));gap:10px;padding:12px}
.zone{background:var(--card);border:1px solid var(--border);border-radius:8px;padding:12px;transition:border-color .2s}
.zone.on{border-color:var(--green)}
.zone.disabled{opacity:.45}
.zname{font-size:13px;font-weight:600;margin-bottom:3px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.zstate{font-size:11px;color:var(--muted);margin-bottom:10px;min-height:14px}
.zstate.on{color:var(--green)}
.zstate.sched{color:var(--yellow)}
.zbtns{display:flex;gap:5px;margin-bottom:8px}
.btn{flex:1;padding:6px 2px;border:none;border-radius:5px;font-size:12px;font-weight:600;cursor:pointer;transition:opacity .15s}
.btn:active{opacity:.7}
.btn-off{background:#374151;color:var(--text)}
.btn-on{background:var(--green);color:#000}
.btn-on:disabled,.btn-off:disabled{opacity:.35;cursor:default}
.dur{display:flex;align-items:center;gap:5px;font-size:11px;color:var(--muted)}
.dur input{width:44px;padding:3px 5px;background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-size:12px;text-align:center}
footer{padding:12px 16px;display:flex;gap:8px;flex-wrap:wrap;border-top:1px solid var(--border)}
.btn-alloff{background:var(--red);color:#fff;padding:10px 18px;border:none;border-radius:6px;font-weight:700;cursor:pointer;font-size:13px}
.btn-settings{background:#374151;color:var(--text);padding:10px 18px;border:none;border-radius:6px;cursor:pointer;font-size:13px}
.btn-restart{background:transparent;color:var(--muted);padding:10px 14px;border:1px solid var(--border);border-radius:6px;cursor:pointer;font-size:12px}
.btn-ota{background:#6366f1;color:#fff;padding:10px 18px;border:none;border-radius:6px;cursor:pointer;font-size:13px;font-weight:600}
.overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.65);z-index:50;overflow-y:auto;padding:20px 16px}
.overlay.open{display:flex;align-items:flex-start;justify-content:center}
.modal{background:var(--card);border-radius:10px;width:100%;max-width:460px;padding:20px}
.mtitle{font-size:15px;font-weight:600;margin-bottom:16px}
.msec{font-size:10px;text-transform:uppercase;letter-spacing:1px;color:var(--muted);margin:14px 0 6px}
.mrow{margin-bottom:10px}
.mrow label{display:block;font-size:11px;color:var(--muted);margin-bottom:4px}
.mrow input,.mrow select{width:100%;padding:8px 10px;background:var(--bg);border:1px solid var(--border);border-radius:5px;color:var(--text);font-size:13px}
.mbtns{display:flex;gap:8px;margin-top:16px}
.btn-save{flex:1;background:var(--green);color:#000;padding:10px;border:none;border-radius:6px;font-weight:700;cursor:pointer}
.btn-cancel{padding:10px 18px;background:#374151;color:var(--text);border:none;border-radius:6px;cursor:pointer}
.toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#374151;color:var(--text);padding:8px 18px;border-radius:20px;font-size:13px;opacity:0;transition:opacity .3s;pointer-events:none;z-index:99}
.toast.show{opacity:1}
</style>
</head>
<body>

<header>
  <h1 id="boardName">Sandy Soil 8Z — Irrigation</h1>
  <div class="badges">
    <span class="badge" id="ipBadge">--</span>
    <span class="badge" id="mqttBadge">MQTT</span>
    <span class="badge" id="uptimeBadge">--</span>
  </div>
</header>

<div class="pbar" id="pbar">
  Supply Pressure: <span class="pval" id="pval">--</span> PSI
  <span id="simBadge" style="display:none;font-size:10px;color:var(--yellow);margin-left:8px;border:1px solid var(--yellow);border-radius:99px;padding:2px 8px">SIM</span>
  <span style="margin-left:auto;display:flex;gap:5px;align-items:center">
    <input type="number" id="simPsi" step="0.1" min="0" max="200" placeholder="PSI" style="width:60px;padding:4px 6px;background:var(--bg);border:1px solid var(--border);border-radius:4px;color:var(--text);font-size:12px">
    <button onclick="simSet()" style="padding:4px 8px;background:#6366f1;color:#fff;border:none;border-radius:4px;font-size:11px;cursor:pointer">Sim</button>
    <button onclick="simClear()" style="padding:4px 8px;background:#374151;color:var(--text);border:none;border-radius:4px;font-size:11px;cursor:pointer">Real</button>
  </span>
</div>

<div class="zones" id="zones"></div>

<footer>
  <button class="btn-alloff" onclick="allOff()">All Zones OFF</button>
  <button class="btn-settings" onclick="openSettings()">Settings</button>
  <button class="btn-ota" onclick="checkOTA()">Check for Update</button>
  <button class="btn-settings" onclick="location.href='/update'" style="font-size:12px">Upload .bin</button>
  <button class="btn-restart" onclick="doRestart()">Restart</button>
</footer>

<!-- Settings modal -->
<div class="overlay" id="overlay">
  <div class="modal">
    <div class="mtitle">Settings</div>

    <div class="msec">WiFi</div>
    <div class="mrow"><label>SSID (Network Name)</label><input id="s_wifi_ssid" type="text"></div>
    <div class="mrow"><label>Password (leave blank to keep current)</label><input id="s_wifi_pass" type="password" placeholder="(unchanged)"></div>

    <div class="msec">Board</div>
    <div class="mrow"><label>Board Name</label><input id="s_board_name" type="text"></div>

    <div class="msec">MQTT</div>
    <div class="mrow"><label>Host</label><input id="s_mqtt_host" type="text" placeholder="xxxx.s1.eu.hivemq.cloud"></div>
    <div class="mrow"><label>Port</label><input id="s_mqtt_port" type="number" value="8883"></div>
    <div class="mrow"><label>Username</label><input id="s_mqtt_user" type="text"></div>
    <div class="mrow"><label>Password</label><input id="s_mqtt_pass" type="password"></div>
    <div class="mrow"><label>Base Topic</label><input id="s_mqtt_topic" type="text" placeholder="farm/irrigation1"></div>

    <div class="msec">Pressure Sensor</div>
    <div class="mrow"><label>Full Scale PSI</label><input id="s_pres_max" type="number" step="0.1"></div>
    <div class="mrow"><label>Low Pressure Alert (PSI)</label><input id="s_pres_low" type="number" step="0.1"></div>
    <div class="mrow"><label>Zero Offset (PSI)</label><input id="s_pres_offset" type="number" step="0.1"></div>

    <div class="msec">Supabase (optional)</div>
    <div class="mrow"><label>Supabase URL</label><input id="s_sb_url" type="text" placeholder="https://xxxx.supabase.co"></div>
    <div class="mrow"><label>Supabase API Key</label><input id="s_sb_key" type="password"></div>

    <div class="mbtns">
      <button class="btn-save" onclick="saveSettings()">Save</button>
      <button class="btn-cancel" onclick="closeSettings()">Cancel</button>
    </div>
  </div>
</div>

<!-- OTA update modal -->
<div class="overlay" id="otaOverlay">
  <div class="modal">
    <div class="mtitle">Firmware Update</div>
    <p style="font-size:13px;color:var(--muted);margin-bottom:12px">Current: <span class="fw" style="color:var(--green);font-weight:700" id="otaCurVer">...</span></p>
    <div id="otaCheckResult" style="margin-bottom:12px"></div>
    <div id="otaProgressWrap" style="display:none;margin-bottom:12px">
      <div style="width:100%;height:22px;background:var(--bg);border-radius:11px;overflow:hidden;border:1px solid var(--border)">
        <div id="otaFill" style="height:100%;width:0;background:var(--green);transition:width .3s;border-radius:11px"></div>
      </div>
      <div id="otaProgText" style="text-align:center;font-size:12px;color:var(--muted);margin-top:4px">...</div>
    </div>
    <div class="mbtns">
      <button class="btn-save" id="otaInstallBtn" style="display:none" onclick="installOTA()">Install Update</button>
      <button class="btn-cancel" onclick="closeOTA()">Close</button>
    </div>
  </div>
</div>

<div class="toast" id="toast"></div>

<script>
const dur = {};
for (let i = 1; i <= 8; i++) dur[i] = 10;

function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.classList.add('show');
  setTimeout(() => el.classList.remove('show'), 2200);
}

function fmtUptime(s) {
  if (s < 60)   return s + 's';
  if (s < 3600) return Math.floor(s/60) + 'm';
  return Math.floor(s/3600) + 'h ' + Math.floor((s%3600)/60) + 'm';
}

async function poll() {
  try {
    const [st, pr, zn] = await Promise.all([
      fetch('/api/status').then(r=>r.json()),
      fetch('/api/pressure').then(r=>r.json()),
      fetch('/api/zones').then(r=>r.json())
    ]);

    document.getElementById('boardName').textContent = st.board_name || 'Sandy Soil 8Z';
    document.getElementById('ipBadge').textContent = st.ip || '--';

    const mb = document.getElementById('mqttBadge');
    mb.textContent = st.mqtt_connected ? 'MQTT OK' : 'MQTT --';
    mb.className = 'badge ' + (st.mqtt_connected ? 'ok' : 'err');
    document.getElementById('uptimeBadge').textContent = fmtUptime(st.uptime_sec || 0);

    const psi = pr.supply_psi;
    document.getElementById('pval').textContent = psi != null ? psi.toFixed(1) : '--';
    document.getElementById('simBadge').style.display = pr.simulated ? 'inline' : 'none';

    renderZones(zn);
  } catch(e) { /* offline */ }
}

function renderZones(zones) {
  const el = document.getElementById('zones');
  el.innerHTML = zones.map(z => {
    const isOn = z.on;
    const dis  = !z.enabled;
    let stateLabel = 'OFF', stateCls = '';
    if (z.state === 'manual') {
      stateLabel = 'Manual ON';
      if (z.remaining_sec != null) stateLabel += ' (' + Math.ceil(z.remaining_sec/60) + 'm left)';
      stateCls = 'on';
    } else if (z.state === 'schedule') {
      stateLabel = 'Scheduled ON';
      stateCls = 'sched';
    }
    return `<div class="zone${isOn?' on':''}${dis?' disabled':''}">
      <div class="zname" title="${z.name}">${z.name}</div>
      <div class="zstate ${stateCls}">${stateLabel}</div>
      <div class="zbtns">
        <button class="btn btn-off" onclick="zoneOff(${z.zone})"${dis?' disabled':''}>OFF</button>
        <button class="btn btn-on"  onclick="zoneOn(${z.zone})"${dis?' disabled':''}>ON</button>
      </div>
      <div class="dur">
        <span>Dur:</span>
        <input type="number" id="dur_${z.zone}" value="${dur[z.zone]||10}" min="1" max="120"
               onchange="dur[${z.zone}]=this.valueAsNumber">
        <span>min</span>
      </div>
    </div>`;
  }).join('');
}

async function zoneOn(n) {
  const d = dur[n] || 10;
  await fetch('/api/zone/'+n+'/on', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({duration: d})
  });
  toast('Zone ' + n + ' ON - ' + d + ' min');
  poll();
}

async function zoneOff(n) {
  await fetch('/api/zone/'+n+'/off', {method:'POST'});
  toast('Zone ' + n + ' OFF');
  poll();
}

async function allOff() {
  if (!confirm('Turn off all zones?')) return;
  await fetch('/api/zones/off', {method:'POST'});
  toast('All zones OFF');
  poll();
}

async function doRestart() {
  if (!confirm('Restart the board?')) return;
  await fetch('/api/restart', {method:'POST'});
  toast('Restarting...');
}

async function openSettings() {
  const cfg = await fetch('/api/config').then(r=>r.json());
  document.getElementById('s_wifi_ssid').value   = cfg.wifi_ssid || '';
  document.getElementById('s_wifi_pass').value   = '';
  document.getElementById('s_board_name').value  = cfg.board_name || '';
  document.getElementById('s_mqtt_host').value   = cfg.mqtt_host || '';
  document.getElementById('s_mqtt_port').value   = cfg.mqtt_port || 8883;
  document.getElementById('s_mqtt_user').value   = cfg.mqtt_user || '';
  document.getElementById('s_mqtt_pass').value   = cfg.mqtt_password || '';
  document.getElementById('s_mqtt_topic').value  = cfg.mqtt_base_topic || '';
  document.getElementById('s_pres_max').value    = cfg.pressure_max_psi || 100;
  document.getElementById('s_pres_low').value    = cfg.low_pressure_psi || 5;
  document.getElementById('s_pres_offset').value = cfg.pressure_zero_offset || 0;
  document.getElementById('s_sb_url').value      = cfg.supabase_url || '';
  document.getElementById('s_sb_key').value      = cfg.supabase_key || '';
  document.getElementById('overlay').classList.add('open');
}

function closeSettings() {
  document.getElementById('overlay').classList.remove('open');
}

async function saveSettings() {
  const body = {
    wifi_ssid:            document.getElementById('s_wifi_ssid').value,
    board_name:           document.getElementById('s_board_name').value,
    mqtt_host:            document.getElementById('s_mqtt_host').value,
    mqtt_port:            parseInt(document.getElementById('s_mqtt_port').value),
    mqtt_user:            document.getElementById('s_mqtt_user').value,
    mqtt_password:        document.getElementById('s_mqtt_pass').value,
    mqtt_base_topic:      document.getElementById('s_mqtt_topic').value,
    pressure_max_psi:     parseFloat(document.getElementById('s_pres_max').value),
    low_pressure_psi:     parseFloat(document.getElementById('s_pres_low').value),
    pressure_zero_offset: parseFloat(document.getElementById('s_pres_offset').value),
    supabase_url:         document.getElementById('s_sb_url').value,
    supabase_key:         document.getElementById('s_sb_key').value
  };
  const wp = document.getElementById('s_wifi_pass').value;
  if (wp.length > 0) body.wifi_password = wp;
  await fetch('/api/config', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify(body)
  });
  closeSettings();
  toast('Settings saved');
  poll();
}

async function simSet() {
  const v = parseFloat(document.getElementById('simPsi').value);
  if (isNaN(v)) { toast('Enter a PSI value'); return; }
  await fetch('/api/pressure/simulate', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({psi: v})
  });
  toast('Simulating ' + v + ' PSI');
  poll();
}

async function simClear() {
  await fetch('/api/pressure/simulate', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({clear: true})
  });
  toast('Real sensor active');
  poll();
}

let otaDlUrl = '';

async function checkOTA() {
  document.getElementById('otaOverlay').classList.add('open');
  document.getElementById('otaCheckResult').innerHTML = '<span style="color:var(--muted)">Checking GitHub...</span>';
  document.getElementById('otaInstallBtn').style.display = 'none';
  document.getElementById('otaProgressWrap').style.display = 'none';
  try {
    const st = await fetch('/api/status').then(r=>r.json());
    document.getElementById('otaCurVer').textContent = st.firmware || '?';
    const r = await fetch('/api/ota/check').then(r=>r.json());
    if (r.update_available) {
      otaDlUrl = r.download_url;
      document.getElementById('otaCheckResult').innerHTML =
        '<span style="color:var(--green);font-weight:600">Update available: v' + r.latest_version + '</span>';
      document.getElementById('otaInstallBtn').style.display = 'block';
    } else {
      document.getElementById('otaCheckResult').innerHTML =
        '<span style="color:var(--muted)">Up to date (v' + (r.latest_version||r.current_version) + ')</span>';
    }
  } catch(e) {
    document.getElementById('otaCheckResult').innerHTML =
      '<span style="color:var(--red)">Check failed — offline?</span>';
  }
}

function closeOTA() {
  document.getElementById('otaOverlay').classList.remove('open');
}

async function installOTA() {
  if (!otaDlUrl) return;
  document.getElementById('otaInstallBtn').style.display = 'none';
  document.getElementById('otaProgressWrap').style.display = 'block';
  document.getElementById('otaProgText').textContent = 'Starting download...';
  await fetch('/api/ota/update', {
    method:'POST', headers:{'Content-Type':'application/json'},
    body: JSON.stringify({url: otaDlUrl})
  });
  const iv = setInterval(async () => {
    try {
      const s = await fetch('/api/ota/status').then(r=>r.json());
      document.getElementById('otaFill').style.width = s.progress + '%';
      document.getElementById('otaProgText').textContent = s.status + ' ' + s.progress + '%';
      if (s.status === 'done') {
        clearInterval(iv);
        document.getElementById('otaProgText').textContent = 'Done — rebooting...';
        setTimeout(()=>location.reload(), 8000);
      }
      if (s.status.startsWith('error')) {
        clearInterval(iv);
        document.getElementById('otaCheckResult').innerHTML =
          '<span style="color:var(--red)">' + s.status + '</span>';
      }
    } catch(e) {
      clearInterval(iv);
      document.getElementById('otaProgText').textContent = 'Rebooting...';
      setTimeout(()=>location.reload(), 8000);
    }
  }, 1500);
}

poll();
setInterval(poll, 2000);
</script>
</body>
</html>
)HTML";

// ---- OTA firmware update page ----
static const char OTA_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Sandy Soil 8Z — Firmware Update</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#1a1f2e;color:#e2e8f0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:20px}
.card{background:#252b3b;border-radius:10px;padding:28px;width:100%;max-width:440px}
h1{font-size:18px;margin-bottom:4px}
p{font-size:12px;color:#94a3b8;margin-bottom:20px}
.fw{color:#4ade80;font-weight:700}
label{display:block;font-size:12px;color:#94a3b8;margin-bottom:6px}
input[type=file]{width:100%;padding:10px;background:#1a1f2e;border:1px solid #2d3548;border-radius:5px;color:#e2e8f0;font-size:13px;margin-bottom:16px}
button{width:100%;padding:12px;background:#6366f1;color:#fff;border:none;border-radius:6px;font-weight:700;font-size:14px;cursor:pointer}
button:hover{background:#4f46e5}
button:disabled{opacity:.5;cursor:default}
.progress{display:none;margin-top:16px}
.progress-bar{width:100%;height:24px;background:#1a1f2e;border-radius:12px;overflow:hidden;border:1px solid #2d3548}
.progress-fill{height:100%;width:0;background:#4ade80;transition:width .2s;border-radius:12px}
.progress-text{text-align:center;font-size:12px;color:#94a3b8;margin-top:6px}
.msg{margin-top:16px;padding:12px;border-radius:6px;font-size:13px;display:none}
.msg.ok{display:block;background:#064e3b;color:#4ade80;border:1px solid #065f46}
.msg.err{display:block;background:#450a0a;color:#ef4444;border:1px solid #7f1d1d}
a{color:#94a3b8;font-size:12px;display:inline-block;margin-top:16px}
</style>
</head>
<body>
<div class="card">
  <h1>Firmware Update</h1>
  <p>Sandy Soil 8Z &mdash; Current: <span class="fw" id="fwVer">...</span></p>
  <form id="otaForm">
    <label>Upload compiled .bin firmware file</label>
    <input type="file" id="fwFile" accept=".bin" required>
    <button type="submit" id="uploadBtn">Upload &amp; Flash</button>
  </form>
  <div class="progress" id="progress">
    <div class="progress-bar"><div class="progress-fill" id="progressFill"></div></div>
    <div class="progress-text" id="progressText">Uploading...</div>
  </div>
  <div class="msg" id="msg"></div>
  <a href="/">&larr; Back to Dashboard</a>
</div>
<script>
fetch('/api/status').then(r=>r.json()).then(d=>{
  document.getElementById('fwVer').textContent=d.firmware||'?';
}).catch(()=>{});

document.getElementById('otaForm').addEventListener('submit', async function(e) {
  e.preventDefault();
  const file = document.getElementById('fwFile').files[0];
  if (!file) return;
  const btn = document.getElementById('uploadBtn');
  const prog = document.getElementById('progress');
  const fill = document.getElementById('progressFill');
  const ptxt = document.getElementById('progressText');
  const msg = document.getElementById('msg');

  btn.disabled = true;
  prog.style.display = 'block';
  msg.style.display = 'none';
  msg.className = 'msg';

  const xhr = new XMLHttpRequest();
  xhr.open('POST', '/api/update', true);
  xhr.upload.onprogress = function(ev) {
    if (ev.lengthComputable) {
      const pct = Math.round(ev.loaded/ev.total*100);
      fill.style.width = pct+'%';
      ptxt.textContent = pct+'% uploaded';
    }
  };
  xhr.onload = function() {
    if (xhr.status === 200) {
      msg.className = 'msg ok';
      msg.textContent = 'Firmware updated! Rebooting...';
      msg.style.display = 'block';
      ptxt.textContent = 'Done - rebooting';
      setTimeout(()=>location.href='/', 5000);
    } else {
      msg.className = 'msg err';
      msg.textContent = 'Update failed: ' + xhr.responseText;
      msg.style.display = 'block';
      btn.disabled = false;
    }
  };
  xhr.onerror = function() {
    msg.className = 'msg err';
    msg.textContent = 'Upload error - check connection';
    msg.style.display = 'block';
    btn.disabled = false;
  };

  const formData = new FormData();
  formData.append('firmware', file);
  xhr.send(formData);
});
</script>
</div>
</body>
</html>
)HTML";

// Register the web UI routes
inline void webuiInit(AsyncWebServer& server) {
  // Dashboard at /
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res = req->beginResponse_P(200, "text/html",
        (const uint8_t*)WEBUI_HTML, strlen_P(WEBUI_HTML));
    res->addHeader("Cache-Control", "no-cache");
    req->send(res);
  });

  // OTA update page at /update
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncWebServerResponse* res = req->beginResponse_P(200, "text/html",
        (const uint8_t*)OTA_HTML, strlen_P(OTA_HTML));
    res->addHeader("Cache-Control", "no-cache");
    req->send(res);
  });

  Serial.println("[WebUI] Dashboard at /  |  OTA at /update");
}
