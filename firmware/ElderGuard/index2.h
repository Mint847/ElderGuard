const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ElderGuard — Live Floor Plan Dashboard</title>
<style>
:root {
--bg: #090d16;
--card-bg: rgba(17, 24, 39, 0.85);
--card-border: rgba(255, 255, 255, 0.08);
--primary: #38bdf8;
--primary-glow: rgba(56, 189, 248, 0.25);
--accent: #10b981;
--steam: #06b6d4;
--steam-glow: rgba(6, 182, 212, 0.35);
--danger: #f43f5e;
--warning: #f59e0b;
--radar: #a78bfa;
--text: #f8fafc;
--muted: #94a3b8;
}

* { box-sizing: border-box; margin: 0; padding: 0; }

body {
font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
background: radial-gradient(circle at top center, #131d31 0%, var(--bg) 70%);
color: var(--text);
min-height: 100vh;
padding: 24px 16px;
display: flex;
justify-content: center;
line-height: 1.5;
}

.container { max-width: 900px; width: 100%; }

header { text-align: center; margin-bottom: 28px; }

.brand-title {
font-size: 2.4rem; font-weight: 800; letter-spacing: -0.5px;
background: linear-gradient(135deg, #60a5fa 0%, #38bdf8 50%, #34d399 100%);
-webkit-background-clip: text; -webkit-text-fill-color: transparent;
display: inline-block; margin-bottom: 6px;
}
.subtitle { color: var(--muted); font-size: 0.95rem; }
.badge-bar { display: flex; justify-content: center; gap: 10px; margin-top: 14px; flex-wrap: wrap; }
.pill {
display: inline-flex; align-items: center; gap: 6px; padding: 4px 12px;
border-radius: 9999px; font-size: 0.8rem; font-weight: 600;
background: rgba(255,255,255,0.05); border: 1px solid var(--card-border); color: var(--muted);
}
.pill.green  { color: #34d399; border-color: rgba(52,211,153,0.3); }
.pill.purple { color: var(--radar); border-color: rgba(167,139,250,0.3); }
.pill.amber  { color: #f59e0b; border-color: rgba(245,158,11,0.3); }

.card {
background: var(--card-bg); backdrop-filter: blur(14px);
-webkit-backdrop-filter: blur(14px); border-radius: 18px;
padding: 22px 24px; margin-bottom: 20px;
border: 1px solid var(--card-border);
box-shadow: 0 10px 25px -5px rgba(0,0,0,0.5);
}
.card-header {
display: flex; justify-content: space-between; align-items: center;
margin-bottom: 18px; border-bottom: 1px solid rgba(255,255,255,0.06); padding-bottom: 12px;
}
.card-title { font-size: 1.2rem; font-weight: 700; color: #e2e8f0; display: flex; align-items: center; gap: 8px; }

.status-badge {
display: inline-flex; align-items: center; padding: 6px 16px;
border-radius: 20px; font-weight: 700; font-size: 0.95rem; letter-spacing: 0.5px;
background: #334155; color: #cbd5e1; text-transform: uppercase;
box-shadow: 0 0 12px rgba(0,0,0,0.3); transition: all 0.3s;
}
.status-badge.active   { background: #0284c7; color: #fff; box-shadow: 0 0 16px var(--primary-glow); }
.status-badge.waiting  { background: #475569; color: #94a3b8; }
.status-badge.checking { background: #d97706; color: #fff; box-shadow: 0 0 16px rgba(217,119,6,0.5); }
.status-badge.danger   { background: var(--danger); color: #fff; animation: alert-pulse 1.2s infinite; }

@keyframes alert-pulse {
0%   { box-shadow: 0 0 0 0 rgba(244,63,94,0.8); }
70%  { box-shadow: 0 0 0 18px rgba(244,63,94,0); }
100% { box-shadow: 0 0 0 0 rgba(244,63,94,0); }
}

.fall-countdown-wrap { margin-top: 16px; display: none; }
.fall-countdown-wrap.visible { display: block; }
.fall-countdown-label {
font-size: 0.85rem; color: var(--warning); font-weight: 700;
margin-bottom: 6px; display: flex; justify-content: space-between;
}
.fall-bar-track { width: 100%; height: 10px; background: #1e293b; border-radius: 6px; overflow: hidden; }
.fall-bar-fill  { height: 100%; border-radius: 6px; background: linear-gradient(90deg, #f59e0b, #f43f5e); transition: width 0.3s linear; }

.env-grid { display: grid; grid-template-columns: 1fr 1fr 1.4fr; gap: 14px; }
@media (max-width: 680px) { .env-grid { grid-template-columns: 1fr; } }
.env-metric { background: rgba(15,23,42,0.6); border: 1px solid var(--card-border); border-radius: 14px; padding: 16px; text-align: center; }
.env-label { font-size: 0.8rem; text-transform: uppercase; letter-spacing: 0.8px; color: var(--muted); margin-bottom: 6px; display: flex; align-items: center; justify-content: center; gap: 6px; }
.env-value { font-size: 2rem; font-weight: 800; color: #f1f5f9; letter-spacing: -0.5px; }
.env-unit  { font-size: 1rem; color: var(--muted); font-weight: 500; margin-left: 2px; }

.steam-banner { background: rgba(15,23,42,0.6); border: 1px solid var(--card-border); border-radius: 14px; padding: 16px; display: flex; flex-direction: column; justify-content: center; align-items: center; transition: all 0.4s; text-align: center; }
.steam-banner.active { background: linear-gradient(135deg, rgba(6,182,212,0.2) 0%, rgba(14,165,233,0.1) 100%); border-color: rgba(6,182,212,0.6); box-shadow: 0 0 20px var(--steam-glow); }
.steam-pill { display: inline-flex; align-items: center; gap: 6px; padding: 5px 14px; border-radius: 20px; font-weight: 700; font-size: 0.9rem; background: rgba(255,255,255,0.06); color: #94a3b8; }
.steam-banner.active .steam-pill { background: var(--steam); color: #042f2e; animation: steam-glow 2s infinite ease-in-out; }
@keyframes steam-glow { 0%,100%{box-shadow:0 0 10px rgba(6,182,212,0.5);}50%{box-shadow:0 0 22px rgba(6,182,212,0.9);} }
.steam-desc { font-size: 0.78rem; color: var(--muted); margin-top: 8px; }

/* ── FLOOR PLAN ── */
.fp-svg-wrap { width: 100%; max-width: 760px; margin: 0 auto; }
#fp-svg { width: 100%; border-radius: 14px; background: #07101f; border: 1px solid rgba(255,255,255,0.07); box-shadow: inset 0 0 40px rgba(0,0,0,0.6); display: block; }

.sensor-readout-grid {
display: grid;
grid-template-columns: repeat(5, 1fr);
gap: 12px;
margin-top: 16px;
}
@media (max-width: 720px) {
.sensor-readout-grid { grid-template-columns: repeat(2, 1fr); }
.sensor-card.pir-card { grid-column: span 2; }
}
.sensor-card {
background: rgba(15, 23, 42, 0.65);
border: 1px solid var(--card-border);
border-radius: 14px;
padding: 12px 14px;
display: flex;
flex-direction: column;
justify-content: space-between;
transition: all 0.25s ease;
}
.sensor-card-header {
display: flex;
justify-content: space-between;
align-items: center;
margin-bottom: 8px;
}
.sensor-name {
font-size: 0.78rem;
font-weight: 700;
color: #94a3b8;
letter-spacing: 0.3px;
}
.mini-badge {
font-size: 0.68rem;
font-weight: 800;
padding: 2px 7px;
border-radius: 999px;
letter-spacing: 0.4px;
text-transform: uppercase;
}
.mini-badge.clear {
background: rgba(16, 185, 129, 0.15);
color: #34d399;
border: 1px solid rgba(16, 185, 129, 0.3);
}
.mini-badge.blocked {
background: rgba(244, 63, 94, 0.18);
color: #fda4af;
border: 1px solid rgba(244, 63, 94, 0.4);
animation: alert-pulse 1.2s infinite;
}
.mini-badge.waiting {
background: rgba(100, 116, 139, 0.2);
color: #94a3b8;
border: 1px solid rgba(100, 116, 139, 0.3);
}
.mini-badge.triggered {
background: rgba(245, 158, 11, 0.2);
color: #fde68a;
border: 1px solid rgba(245, 158, 11, 0.5);
box-shadow: 0 0 10px rgba(245, 158, 11, 0.4);
}
.sensor-dist {
font-size: 1.35rem;
font-weight: 800;
color: #f1f5f9;
letter-spacing: -0.5px;
}
.dist-unit {
font-size: 0.8rem;
font-weight: 500;
color: var(--muted);
margin-left: 2px;
}
.sensor-state {
font-size: 0.95rem;
font-weight: 700;
color: #f1f5f9;
margin-top: 4px;
}

.fp-legend { display: flex; flex-wrap: wrap; gap: 14px; margin-top: 14px; font-size: 0.78rem; color: var(--muted); border-top: 1px solid rgba(255,255,255,0.05); padding-top: 12px; }
.fp-legend span { display: flex; align-items: center; gap: 6px; }

/* ── RADAR CARD (same as index1) ── */
.radar-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 14px; margin-bottom: 16px; }
@media (max-width: 600px) { .radar-grid { grid-template-columns: 1fr 1fr; } }
.radar-metric { background: rgba(15,23,42,0.6); border: 1px solid rgba(167,139,250,0.15); border-radius: 14px; padding: 14px 12px; text-align: center; }
.radar-metric.highlight { border-color: rgba(167,139,250,0.5); background: rgba(167,139,250,0.07); }
.radar-label { font-size: 0.75rem; text-transform: uppercase; letter-spacing: 0.8px; color: var(--muted); margin-bottom: 5px; }
.radar-val  { font-size: 1.8rem; font-weight: 800; color: var(--radar); letter-spacing: -0.5px; }
.radar-unit { font-size: 0.9rem; color: var(--muted); font-weight: 500; }

.radar-posture { display: flex; align-items: center; justify-content: center; gap: 10px; padding: 12px 18px; border-radius: 14px; border: 1px solid var(--card-border); background: rgba(15,23,42,0.6); transition: all 0.35s; }
.radar-posture.standing { border-color: rgba(52,211,153,0.5); background: rgba(52,211,153,0.08); box-shadow: 0 0 16px rgba(52,211,153,0.2); }
.radar-posture.floor    { border-color: rgba(244,63,94,0.4);  background: rgba(244,63,94,0.08);  box-shadow: 0 0 16px rgba(244,63,94,0.2); }
.radar-posture.none { border-color: var(--card-border); }
.posture-icon  { font-size: 2rem; }
.posture-label { font-size: 0.95rem; font-weight: 700; color: #e2e8f0; }
.posture-sub   { font-size: 0.78rem; color: var(--muted); margin-top: 2px; }

.radar-energy-bar-wrap { margin-top: 14px; }
.radar-energy-row { display: flex; align-items: center; gap: 10px; margin-bottom: 8px; }
.radar-energy-label { font-size: 0.78rem; color: var(--muted); width: 70px; flex-shrink: 0; }
.radar-bar-track { flex: 1; height: 8px; background: #1e293b; border-radius: 4px; overflow: hidden; }
.radar-bar-fill  { height: 100%; border-radius: 4px; transition: width 0.3s ease; }
.radar-bar-fill.stat { background: linear-gradient(90deg, #a78bfa, #7c3aed); }
.radar-bar-fill.move { background: linear-gradient(90deg, #38bdf8, #0284c7); }
.radar-energy-pct { font-size: 0.78rem; font-weight: 700; color: #e2e8f0; width: 32px; text-align: right; flex-shrink: 0; }

.radar-offline-banner { display: none; text-align: center; padding: 14px; border-radius: 12px; background: rgba(100,116,139,0.15); border: 1px dashed rgba(100,116,139,0.4); color: var(--muted); font-size: 0.9rem; margin-bottom: 14px; }
.radar-offline-banner.visible { display: block; }

.radar-override-pill { display: none; align-items: center; gap: 6px; padding: 5px 14px; border-radius: 20px; font-weight: 700; font-size: 0.82rem; margin-top: 12px; background: rgba(167,139,250,0.15); border: 1px solid rgba(167,139,250,0.4); color: var(--radar); }
.radar-override-pill.visible { display: inline-flex; }

/* ── SETTINGS ── */
.slider-row { margin-bottom: 20px; }
.slider-info { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; }
.slider-info label { font-size: 0.95rem; font-weight: 600; color: #e2e8f0; }
.slider-val-badge { font-size: 0.95rem; font-weight: 700; color: var(--primary); background: rgba(56,189,248,0.1); padding: 2px 10px; border-radius: 8px; border: 1px solid rgba(56,189,248,0.2); }

input[type=range] { -webkit-appearance: none; width: 100%; height: 8px; border-radius: 4px; background: #1e293b; outline: none; }
input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%; background: var(--primary); cursor: pointer; box-shadow: 0 0 10px var(--primary-glow); }

.btn { background: linear-gradient(135deg, #0284c7 0%, #0369a1 100%); color: white; border: none; padding: 12px 24px; border-radius: 10px; cursor: pointer; font-size: 1rem; font-weight: 700; transition: all 0.2s; box-shadow: 0 4px 14px rgba(2,132,199,0.4); display: inline-flex; align-items: center; gap: 8px; }
.btn:hover { transform: translateY(-1px); box-shadow: 0 6px 20px rgba(2,132,199,0.6); }
.btn:active { transform: translateY(1px); }
.btn-danger { background: linear-gradient(135deg, #e11d48 0%, #be123c 100%); box-shadow: 0 4px 14px rgba(225,29,72,0.4); }
.btn-danger:hover { box-shadow: 0 6px 20px rgba(225,29,72,0.6); }

.override-btn-group { display:flex; gap:6px; }
.override-btn { background: rgba(255,255,255,0.05); border:1px solid var(--card-border); color:var(--muted); padding:6px 12px; border-radius:8px; font-size:0.8rem; font-weight:600; cursor:pointer; transition:all 0.2s; }
.override-btn:hover { background: rgba(255,255,255,0.1); }
.override-btn.active { background: var(--primary); color:#fff; border-color: var(--primary); }

.save-feedback { color: var(--accent); font-weight: 700; margin-left: 14px; font-size: 0.95rem; display: none; }
.footer { text-align: center; color: #64748b; font-size: 0.8rem; margin-top: 25px; padding-bottom: 20px; }

.conn-banner {
display: none; position: sticky; top: 0; z-index: 50;
background: var(--danger); color: #fff; font-weight: 700;
text-align: center; padding: 10px; border-radius: 10px; margin-bottom: 16px;
box-shadow: 0 4px 14px rgba(244,63,94,0.4);
}
.conn-banner.visible { display: block; }

.event-log { list-style: none; font-size: 0.85rem; color: var(--muted); max-height: 180px; overflow-y: auto; }
.event-log li { padding: 6px 2px; border-bottom: 1px solid rgba(255,255,255,0.05); }
.event-log li:last-child { border-bottom: none; }
.event-log-empty { color: #64748b; font-style: italic; padding: 6px 2px; }
</style>
</head>
<body>
<div class="container">

<div id="conn-banner" class="conn-banner">
&#9888;&#65039; Connection Lost — data may be stale. Last update: <span id="conn-banner-time">--</span>
</div>

<header>
<div class="brand-title">ElderGuard</div>
<div class="subtitle">Multi-Sensor Fusion Fall Detection &mdash; Live Floor Plan</div>
<div class="badge-bar">
<span class="pill green">&#9679; ESP32-S3</span>
<span class="pill">4x VL53L0X ToF</span>
<span class="pill amber">&#9679; PIR Entry</span>
<span class="pill">DHT11 Steam</span>
<span class="pill purple">&#9679; LD2410C Radar</span>
</div>
</header>

<div class="card">
<div class="card-header">
<div class="card-title">&#9889; System Status</div>
<span id="sys-state" class="status-badge waiting">CONNECTING...</span>
</div>
<div id="fall-countdown-wrap" class="fall-countdown-wrap">
<div class="fall-countdown-label">
<span>&#9888;&#65039; Fall Confirmation Timer</span>
<span id="fall-countdown-text">5.0s</span>
</div>
<div class="fall-bar-track"><div id="fall-bar-fill" class="fall-bar-fill" style="width:0%"></div></div>
</div>
<div class="env-grid" style="margin-top:18px;">
<div class="env-metric">
<div class="env-label">&#127777;&#65039; Temperature</div>
<div class="env-value"><span id="temp-val">--</span><span class="env-unit">&deg;C</span></div>
</div>
<div class="env-metric">
<div class="env-label">&#128167; Humidity</div>
<div class="env-value"><span id="hum-val">--</span><span class="env-unit">%</span></div>
</div>
<div id="steam-card" class="steam-banner">
<span id="steam-badge" class="steam-pill">&#129703; Air Clear / Dry</span>
<div id="steam-text" class="steam-desc">No steam detected. Optical ToF beams reliable.</div>
</div>
</div>
<div style="margin-top:14px; display:flex; align-items:center; gap:10px; flex-wrap:wrap;">
<span style="font-size:0.78rem; color:var(--muted); text-transform:uppercase; letter-spacing:0.5px;">Demo: Shower Override</span>
<div class="override-btn-group">
<button id="ov-auto" class="override-btn" onclick="setShowerOverride('auto')">Auto</button>
<button id="ov-on" class="override-btn" onclick="setShowerOverride('on')">Force Shower</button>
<button id="ov-off" class="override-btn" onclick="setShowerOverride('off')">Force Dry</button>
</div>
<span id="override-badge" class="pill amber" style="display:none;"></span>
</div>
</div>

<div class="card">
<div class="card-header">
<div class="card-title">&#127968; Live Bathroom Floor Plan</div>
<span class="pill" style="font-size:0.72rem;">30 &times; 25 cm Room Replica</span>
</div>

<div class="fp-svg-wrap">
<svg id="fp-svg" viewBox="-28 -14 496 300" xmlns="http://www.w3.org/2000/svg">
<defs>
<filter id="glow-g" x="-60%" y="-60%" width="220%" height="220%">
<feGaussianBlur stdDeviation="3" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
</filter>
<filter id="glow-r" x="-60%" y="-60%" width="220%" height="220%">
<feGaussianBlur stdDeviation="4" result="b"/><feMerge><feMergeNode in="b"/><feMergeNode in="SourceGraphic"/></feMerge>
</filter>
</defs>

<rect x="30" y="25" width="380" height="230" fill="#0c1829" rx="4"/>

<rect x="22" y="17" width="8" height="246" fill="#2d3748" rx="2"/>
<rect x="22" y="255" width="396" height="8" fill="#2d3748" rx="2"/>
<rect x="410" y="17" width="8" height="246" fill="#2d3748" rx="2"/>
<rect x="22" y="17" width="158" height="8" fill="#2d3748" rx="2"/>
<rect x="250" y="17" width="168" height="8" fill="#2d3748" rx="2"/>

<line x1="180" y1="21" x2="250" y2="21" stroke="#374151" stroke-width="2"/>

<line id="beam-n1" x1="157" y1="25" x2="157" y2="120"
stroke="#10b981" stroke-width="2.5" stroke-linecap="round" opacity="0.75"
style="transition: stroke 0.25s;"/>
<circle id="hit-n1" cx="157" cy="120" r="3.5" fill="#10b981" opacity="0.8"
style="transition: fill 0.25s, cy 0.2s;"/>

<line id="beam-n2" x1="283" y1="25" x2="283" y2="120"
stroke="#10b981" stroke-width="2.5" stroke-linecap="round" opacity="0.75"
style="transition: stroke 0.25s;"/>
<circle id="hit-n2" cx="283" cy="120" r="3.5" fill="#10b981" opacity="0.8"
style="transition: fill 0.25s, cy 0.2s;"/>

<line id="beam-w1" x1="30" y1="99" x2="160" y2="99"
stroke="#10b981" stroke-width="2.5" stroke-linecap="round" opacity="0.75"
style="transition: stroke 0.25s;"/>
<circle id="hit-w1" cx="160" cy="99" r="3.5" fill="#10b981" opacity="0.8"
style="transition: fill 0.25s, cx 0.2s;"/>

<line id="beam-w2" x1="30" y1="181" x2="160" y2="181"
stroke="#10b981" stroke-width="2.5" stroke-linecap="round" opacity="0.75"
style="transition: stroke 0.25s;"/>
<circle id="hit-w2" cx="160" cy="181" r="3.5" fill="#10b981" opacity="0.8"
style="transition: fill 0.25s, cx 0.2s;"/>

<circle id="dot-n1" cx="157" cy="21" r="7" fill="#065f46" stroke="#10b981" stroke-width="2" filter="url(#glow-g)" style="transition: all 0.25s;"/>

<circle id="dot-n2" cx="283" cy="21" r="7" fill="#065f46" stroke="#10b981" stroke-width="2" filter="url(#glow-g)" style="transition: all 0.25s;"/>

<circle id="dot-w1" cx="26" cy="99" r="7" fill="#065f46" stroke="#10b981" stroke-width="2" filter="url(#glow-g)" style="transition: all 0.25s;"/>

<circle id="dot-w2" cx="26" cy="181" r="7" fill="#065f46" stroke="#10b981" stroke-width="2" filter="url(#glow-g)" style="transition: all 0.25s;"/>

<rect id="pir-body" x="202" y="14" width="26" height="12" rx="3"
fill="#451a03" stroke="#f59e0b" stroke-width="1.5"
style="transition: stroke 0.3s, fill 0.3s;"/>

<text x="157" y="5" text-anchor="middle" font-size="9" font-weight="700"
fill="#34d399" font-family="monospace" letter-spacing="0.5">N1</text>

<text x="283" y="5" text-anchor="middle" font-size="9" font-weight="700"
fill="#34d399" font-family="monospace" letter-spacing="0.5">N2</text>

<text x="13" y="103" text-anchor="end" font-size="9" font-weight="700"
fill="#34d399" font-family="monospace" letter-spacing="0.5">W1</text>

<text x="13" y="185" text-anchor="end" font-size="9" font-weight="700"
fill="#34d399" font-family="monospace" letter-spacing="0.5">W2</text>

<text x="215" y="5" text-anchor="middle" font-size="9" font-weight="700"
fill="#fbbf24" font-family="monospace" letter-spacing="0.5">PIR</text>
</svg>
</div>

<div class="sensor-readout-grid">
<div class="sensor-card">
<div class="sensor-card-header">
<span class="sensor-name">N1 &bull; North</span>
<span id="badge-n1" class="mini-badge clear">CLEAR</span>
</div>
<div class="sensor-dist"><span id="val-n1">--</span><span class="dist-unit">mm</span></div>
</div>
<div class="sensor-card">
<div class="sensor-card-header">
<span class="sensor-name">N2 &bull; North</span>
<span id="badge-n2" class="mini-badge clear">CLEAR</span>
</div>
<div class="sensor-dist"><span id="val-n2">--</span><span class="dist-unit">mm</span></div>
</div>
<div class="sensor-card">
<div class="sensor-card-header">
<span class="sensor-name">W1 &bull; West</span>
<span id="badge-w1" class="mini-badge clear">CLEAR</span>
</div>
<div class="sensor-dist"><span id="val-w1">--</span><span class="dist-unit">mm</span></div>
</div>
<div class="sensor-card">
<div class="sensor-card-header">
<span class="sensor-name">W2 &bull; West</span>
<span id="badge-w2" class="mini-badge clear">CLEAR</span>
</div>
<div class="sensor-dist"><span id="val-w2">--</span><span class="dist-unit">mm</span></div>
</div>
<div class="sensor-card pir-card">
<div class="sensor-card-header">
<span class="sensor-name">PIR &bull; Entry</span>
<span id="badge-pir" class="mini-badge waiting">WAITING</span>
</div>
<div class="sensor-state" id="val-pir">No Motion</div>
</div>
</div>

<div class="fp-legend">
<span><svg width="14" height="14"><circle cx="7" cy="7" r="5" fill="#10b981"/></svg> ToF Clear</span>
<span><svg width="14" height="14"><circle cx="7" cy="7" r="5" fill="#f43f5e"/></svg> ToF Blocked</span>
<span><svg width="14" height="14"><rect x="2" y="4" width="10" height="6" rx="2" fill="#451a03" stroke="#f59e0b" stroke-width="1.5"/></svg> PIR Entry Sensor</span>
</div>
</div>

<div class="card">
<div class="card-header">
<div class="card-title">&#128225; LD2410C 24GHz Radar</div>
<span id="radar-link-badge" class="status-badge waiting">CONNECTING...</span>
</div>

<div id="radar-offline-banner" class="radar-offline-banner">
&#9888;&#65039; Radar module not responding. Check 5V power and GPIO 17/18 connections.
</div>

<div class="radar-grid">
<div class="radar-metric highlight">
<div class="radar-label">&#128207; Distance</div>
<div class="radar-val"><span id="radar-dist-val">--</span><span class="radar-unit"> cm</span></div>
<div style="font-size:0.68rem; color:var(--muted); margin-top:4px;">Standing: &le; <span id="radar-stand-max">28</span>cm</div>
</div>
<div class="radar-metric">
<div class="radar-label">&#128226; Est. Gate (0.2m)</div>
<div class="radar-val" id="radar-gate-val">--</div>
</div>
<div class="radar-metric">
<div class="radar-label">&#9889; Peak Energy</div>
<div class="radar-val"><span id="radar-energy-val">--</span><span class="radar-unit">%</span></div>
</div>
</div>

<div id="radar-posture" class="radar-posture none">
<div class="posture-icon" id="radar-posture-icon">&#128225;</div>
<div>
<div class="posture-label" id="radar-posture-label">Waiting for data...</div>
<div class="posture-sub" id="radar-posture-sub">Radar will cross-reference ToF during shower mode</div>
</div>
</div>

<div style="margin-top:14px; display:flex; align-items:center; gap:10px; flex-wrap:wrap;">
<span style="font-size:0.78rem; color:var(--muted); text-transform:uppercase; letter-spacing:0.5px;">Demo: Radar Posture Override</span>
<div class="override-btn-group">
<button id="rov-auto" class="override-btn" onclick="setRadarOverride('auto')">Auto</button>
<button id="rov-standing" class="override-btn" onclick="setRadarOverride('standing')">Force Standing</button>
<button id="rov-floor" class="override-btn" onclick="setRadarOverride('floor')">Force Floor</button>
</div>
<span id="radar-demo-badge" class="pill amber" style="display:none;"></span>
</div>

<div style="display:flex; flex-wrap:wrap; gap:10px; margin-top:10px; align-items:center;">
<div id="radar-override-pill" class="radar-override-pill">
&#128737;&#65039; Radar Override Active &mdash; Suppressing ToF false alarm (Shower Mode)
</div>
<div id="radar-confirmed-pill" class="radar-override-pill"
style="background:rgba(244,63,94,0.12); border-color:rgba(244,63,94,0.4); color:#fda4af;">
&#128308; Radar confirmed fall through steam
</div>
</div>

<div class="radar-energy-bar-wrap">
<div class="radar-energy-row">
<div class="radar-energy-label">&#129485; Stationary</div>
<div class="radar-bar-track"><div id="radar-stat-bar" class="radar-bar-fill stat" style="width:0%"></div></div>
<div class="radar-energy-pct" id="radar-stat-pct">0%</div>
</div>
<div class="radar-energy-row">
<div class="radar-energy-label">&#127939; Moving</div>
<div class="radar-bar-track"><div id="radar-mov-bar" class="radar-bar-fill move" style="width:0%"></div></div>
<div class="radar-energy-pct" id="radar-mov-pct">0%</div>
</div>
</div>

<div style="display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-top:14px;">
<div class="radar-metric" style="padding:10px 12px;">
<div class="radar-label">Static Target</div>
<div style="font-size:1.3rem; font-weight:800; color:#a78bfa;">
<span id="radar-stat-dist">--</span><span class="radar-unit"> cm</span>
</div>
</div>
<div class="radar-metric" style="padding:10px 12px;">
<div class="radar-label">Moving Target</div>
<div style="font-size:1.3rem; font-weight:800; color:#38bdf8;">
<span id="radar-mov-dist">--</span><span class="radar-unit"> cm</span>
</div>
</div>
</div>
</div>

<div class="card">
<div class="card-header">
<div class="card-title">&#9881;&#65039; Sensor &amp; Timing Parameters</div>
</div>
<div class="slider-row">
<div class="slider-info"><label for="pir-warmup">PIR Sensor Warmup Time</label><span id="lbl-warmup" class="slider-val-badge">30s</span></div>
<input type="range" id="pir-warmup" min="5" max="60" value="30" oninput="updateLbl('lbl-warmup',this.value,'s')">
</div>
<div class="slider-row">
<div class="slider-info"><label for="pir-debounce">PIR Doorway Debounce (Entry Confirmation)</label><span id="lbl-debounce" class="slider-val-badge">1.5s</span></div>
<input type="range" id="pir-debounce" min="0.5" max="5.0" step="0.5" value="1.5" oninput="updateLbl('lbl-debounce',this.value,'s')">
</div>
<div class="slider-row">
<div class="slider-info"><label for="fall-timer">ToF Fall Confirmation Timer</label><span id="lbl-fall" class="slider-val-badge">5s</span></div>
<input type="range" id="fall-timer" min="1" max="15" value="5" oninput="updateLbl('lbl-fall',this.value,'s')">
</div>
<div class="slider-row">
<div class="slider-info"><label for="shower-thresh">Shower Steam Humidity Threshold</label><span id="lbl-shower" class="slider-val-badge">82%</span></div>
<input type="range" id="shower-thresh" min="70" max="95" step="1" value="82" oninput="updateLbl('lbl-shower',this.value,'%')">
</div>
<div style="display:flex; align-items:center; margin-top:10px;">
<button class="btn" onclick="saveSettings()">&#128190; Save Configuration</button>
<span id="save-msg" class="save-feedback">&#10003; Parameters Saved &amp; Applied!</span>
</div>
</div>

<div class="card">
<div style="display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:16px;">
<div>
<div class="card-title" style="margin-bottom:4px;">&#128680; Caretaker Emergency Alert</div>
<p style="color:var(--muted); font-size:0.9rem;">Manually trigger a test alert from the dashboard.</p>
</div>
<div style="display:flex; align-items:center; gap:4px;">
<button class="btn btn-danger" onclick="testAlert()">&#9888; Send Test Alert</button>
<span id="test-alert-msg" class="save-feedback"></span>
</div>
</div>
</div>

<div class="card">
<div class="card-header">
<div class="card-title">&#128220; Recent Events</div>
</div>
<ul id="event-log" class="event-log">
<li class="event-log-empty">No events yet this session.</li>
</ul>
</div>

<div class="footer">ElderGuard System &bull; University CSE Final Year Project &bull; Privacy-First Ambient Care</div>
</div>

<script>
var fallTimerTotal = 5;
var sawCheckingThisCycle = false;
var prevSysState = null;

/* Shared scale constants — fallback defaults match main2.cpp's own
   defaults, overwritten live from /api/status as soon as it responds, so
   the floor plan and radar gate never silently drift out of sync with the
   firmware's actual configured values. */
var scaleClearN = 250;
var scaleClearW = 300;
var radarGateWidthCm = 20;

/* Connection-lost tracking */
var consecutiveFailures = 0;
var lastSuccessTime = Date.now();
var CONN_LOST_THRESHOLD = 3; // ~2.4s at 800ms/poll

function updateLbl(id, val, unit) { document.getElementById(id).innerText = val + unit; }

/* ── Shared toast helper (replaces native alert() popups) ── */
function showToast(elId, msg, isError) {
var el = document.getElementById(elId);
el.innerText = msg;
el.style.color = isError ? 'var(--danger)' : 'var(--accent)';
el.style.display = 'inline';
clearTimeout(el._toastTimer);
el._toastTimer = setTimeout(function(){ el.style.display = 'none'; }, 3000);
}

/* ── Recent-events log (client-side only, resets on page reload) ── */
var MAX_EVENTS = 20;
function logEvent(text) {
var ul = document.getElementById('event-log');
var empty = ul.querySelector('.event-log-empty');
if (empty) empty.remove();
var li = document.createElement('li');
var t = new Date().toLocaleTimeString();
li.textContent = '[' + t + '] ' + text;
ul.insertBefore(li, ul.firstChild);
while (ul.children.length > MAX_EVENTS) ul.removeChild(ul.lastChild);
}

/* ── Floor plan: ToF beam & external sensor readout updater ── */
function setToF(beamId, hitId, badgeId, valId, dotId, blocked, distMm, axis) {
var beam  = document.getElementById(beamId);
var hit   = document.getElementById(hitId);
var badge = document.getElementById(badgeId);
var valEl = document.getElementById(valId);
var dot   = document.getElementById(dotId);

var col = blocked ? '#f43f5e' : '#10b981';
var df  = blocked ? '#7f1d1d' : '#065f46';
var gf  = blocked ? 'url(#glow-r)' : 'url(#glow-g)';

if (beam) {
beam.setAttribute('stroke', col);
beam.setAttribute('opacity', blocked ? '1' : '0.75');
}
if (hit) hit.setAttribute('fill', col);
if (dot) {
dot.setAttribute('fill', df);
dot.setAttribute('stroke', col);
dot.setAttribute('filter', gf);
}

if (badge) {
badge.className = 'mini-badge ' + (blocked ? 'blocked' : 'clear');
badge.innerText = blocked ? 'BLOCKED' : 'CLEAR';
}
if (valEl) valEl.innerText = (distMm !== undefined) ? distMm : '--';

if (axis === 'south') {
/* Room depth: scaleClearN mm -> 230px (y=25 to y=255) */
var px = Math.max(6, Math.min(230, Math.round((distMm / scaleClearN) * 230)));
var ty = 25 + px;
if (beam) beam.setAttribute('y2', ty);
if (hit) hit.setAttribute('cy', ty);
} else {
/* Room width: scaleClearW mm -> 380px (x=30 to x=410) */
var px2 = Math.max(6, Math.min(380, Math.round((distMm / scaleClearW) * 380)));
var tx = 30 + px2;
if (beam) beam.setAttribute('x2', tx);
if (hit) hit.setAttribute('cx', tx);
}
}

/* ── PIR indicator ── */
function updatePIR(active) {
var b = document.getElementById('pir-body');
var badge = document.getElementById('badge-pir');
var valEl = document.getElementById('val-pir');
if (active) {
if (b) { b.setAttribute('stroke','#fbbf24'); b.setAttribute('fill','#78350f'); }
if (badge) { badge.className = 'mini-badge triggered'; badge.innerText = 'TRIGGERED'; }
if (valEl) valEl.innerText = 'Motion Active';
} else {
if (b) { b.setAttribute('stroke','#f59e0b'); b.setAttribute('fill','#451a03'); }
if (badge) { badge.className = 'mini-badge waiting'; badge.innerText = 'WAITING'; }
if (valEl) valEl.innerText = 'No Motion';
}
}

/* ── Radar card (same as index1) ── */
function setRadarOnline(online) {
var badge = document.getElementById('radar-link-badge');
var offBanner = document.getElementById('radar-offline-banner');
if (online) {
badge.innerText = 'ONLINE'; badge.className = 'status-badge active';
offBanner.classList.remove('visible');
} else {
badge.innerText = 'OFFLINE'; badge.className = 'status-badge waiting';
offBanner.classList.add('visible');
}
}

function updateRadarPosture(standing, presence, online) {
var box   = document.getElementById('radar-posture');
var icon  = document.getElementById('radar-posture-icon');
var label = document.getElementById('radar-posture-label');
var sub   = document.getElementById('radar-posture-sub');
box.className = 'radar-posture';
if (!online) { icon.innerText='📡'; label.innerText='Radar Offline'; sub.innerText='Check 5V power and wiring'; return; }
if (!presence) { icon.innerText='⚪'; label.innerText='No Target Detected'; sub.innerText='Bathroom empty or below energy threshold'; return; }
if (standing) {
box.classList.add('standing');
icon.innerText='🧍'; label.innerText='Target at Standing Height'; sub.innerText='Torso reflection in Gate 0-1 (28 cm from radar)';
} else {
box.classList.add('floor');
icon.innerText='🔴'; label.innerText='Target at Floor Level'; sub.innerText='Reflection shifted > 28 cm — possible fall';
}
}

/* ── Main fetch loop ── */
async function fetchStatus() {
try {
var res  = await fetch('/api/status');
var data = await res.json();

/* Connection restored */
consecutiveFailures = 0;
lastSuccessTime = Date.now();
document.getElementById('conn-banner').classList.remove('visible');

/* Shared scale constants — keep the floor plan/radar gate in sync with
   whatever the firmware is actually configured with */
if (data.clearN !== undefined) scaleClearN = data.clearN;
if (data.clearW !== undefined) scaleClearW = data.clearW;
if (data.radarGateCm !== undefined) radarGateWidthCm = data.radarGateCm;

/* 1. State badge */
var badge = document.getElementById('sys-state');
badge.innerText = data.state; badge.className = 'status-badge';
if (data.state === 'IDLE' || data.state === 'STANDING') badge.classList.add('active');
else if (data.state === 'CHECKING') badge.classList.add('checking');
else if (data.state === '!! FALL !!') badge.classList.add('danger');
else badge.classList.add('waiting');

/* Recent-events log: watch for a fall / recovery state transition */
if (prevSysState !== null && data.state !== prevSysState) {
if (data.state === '!! FALL !!') logEvent('⚠ Fall detected');
else if (prevSysState === '!! FALL !!') logEvent('✓ Fall recovered — person standing again');
}
prevSysState = data.state;

/* 2. Fall countdown */
var cW = document.getElementById('fall-countdown-wrap');
var cT = document.getElementById('fall-countdown-text');
var cF = document.getElementById('fall-bar-fill');
if (data.state === 'CHECKING' && data.fallCountdown !== undefined) {
if (!sawCheckingThisCycle) {
sawCheckingThisCycle = true;
/* First observed countdown this cycle approximates the true total,
   since clearStartTime was just set server-side — only ever raises
   fallTimerTotal, never lowers it below a settings-confirmed value, so
   a successful fetchSettings() still wins if it arrives separately. */
if (data.fallCountdown > fallTimerTotal) fallTimerTotal = Math.ceil(data.fallCountdown);
}
cW.classList.add('visible'); cT.innerText = data.fallCountdown.toFixed(1) + 's';
cF.style.width = Math.min(100, ((fallTimerTotal - data.fallCountdown) / fallTimerTotal) * 100) + '%';
} else {
sawCheckingThisCycle = false;
cW.classList.remove('visible'); cF.style.width = '0%';
}

/* 3. Environmental */
if (data.temp !== undefined)     document.getElementById('temp-val').innerText = data.temp.toFixed(1);
if (data.humidity !== undefined) document.getElementById('hum-val').innerText  = data.humidity.toFixed(1);

/* 4. Steam banner */
var sc = document.getElementById('steam-card');
var sb = document.getElementById('steam-badge');
var st = document.getElementById('steam-text');
if (data.showering) {
sc.classList.add('active'); sb.innerText = 'Shower / Mist Active';
st.innerText = 'High humidity detected. Radar now primary fall validator.';
} else {
sc.classList.remove('active'); sb.innerText = 'Air Clear / Dry';
st.innerText = 'No steam detected. Optical ToF beams reliable.';
}
updateOverrideUI('ov-', data.showerMode || 'auto', 'on', 'off', 'override-badge', 'Shower Forced ON', 'Shower Forced OFF');

/* 5. Floor plan beams & external sensor readings */
setToF('beam-n1','hit-n1','badge-n1','val-n1','dot-n1', data.b1, data.n1, 'south');
setToF('beam-n2','hit-n2','badge-n2','val-n2','dot-n2', data.b2, data.n2, 'south');
setToF('beam-w1','hit-w1','badge-w1','val-w1','dot-w1', data.b3, data.w1, 'east');
setToF('beam-w2','hit-w2','badge-w2','val-w2','dot-w2', data.b4, data.w2, 'east');

/* 6. PIR */
var pirActive = (data.state !== 'WAITING PIR' && data.state !== 'WARMUP');
updatePIR(pirActive);

/* 8. Radar card */
var on = data.radarOnline;
setRadarOnline(on);
updateRadarPosture(data.radarStanding, data.radarState !== 'None' && data.radarState !== 'Offline', on);
updateOverrideUI('rov-', data.radarOverrideMode || 'auto', 'standing', 'floor', 'radar-demo-badge', 'Radar Forced STANDING', 'Radar Forced FLOOR');

var distEl = document.getElementById('radar-dist-val');
distEl.innerText = on ? data.radarDist : '--';
if (data.radarStandMaxCm !== undefined) document.getElementById('radar-stand-max').innerText = data.radarStandMaxCm;
if (on && data.radarDist > 0 && data.radarStandMaxCm !== undefined) {
distEl.style.color = (data.radarDist <= data.radarStandMaxCm) ? 'var(--accent)' : 'var(--danger)';
} else {
distEl.style.color = '';
}

document.getElementById('radar-stat-dist').innerText  = on ? data.radarStatDist : '--';
document.getElementById('radar-mov-dist').innerText   = on ? data.radarMovDist  : '--';
document.getElementById('radar-energy-val').innerText = on ? data.radarEnergy   : '--';

var gate = on && data.radarDist > 0 ? Math.floor(data.radarDist / radarGateWidthCm) : '--';
document.getElementById('radar-gate-val').innerText = gate;

var statE = on ? Math.min(data.radarStatEnergy, 100) : 0;
var movE  = on ? Math.min(data.radarMovEnergy,  100) : 0;
document.getElementById('radar-stat-bar').style.width = statE + '%';
document.getElementById('radar-mov-bar').style.width  = movE  + '%';
document.getElementById('radar-stat-pct').innerText   = statE + '%';
document.getElementById('radar-mov-pct').innerText    = movE  + '%';

document.getElementById('radar-override-pill').classList.toggle('visible',
on && data.showering && data.state === 'CHECKING' && data.radarStanding);
document.getElementById('radar-confirmed-pill').classList.toggle('visible',
data.radarConfirmed && data.state === '!! FALL !!');

} catch(e) {
console.error('Fetch error:', e);
consecutiveFailures++;
if (consecutiveFailures >= CONN_LOST_THRESHOLD) {
var secsAgo = Math.round((Date.now() - lastSuccessTime) / 1000);
document.getElementById('conn-banner-time').innerText = secsAgo + 's ago';
document.getElementById('conn-banner').classList.add('visible');
}
}
}

async function fetchSettings(attempt) {
attempt = attempt || 0;
try {
var res = await fetch('/api/settings'); var d = await res.json();
if (d.warmup      !== undefined) { document.getElementById('pir-warmup').value   = d.warmup;        updateLbl('lbl-warmup',   d.warmup,       's'); }
if (d.debounce    !== undefined) { document.getElementById('pir-debounce').value  = d.debounce;      updateLbl('lbl-debounce', d.debounce,     's'); }
if (d.fall        !== undefined) { document.getElementById('fall-timer').value    = d.fall;          updateLbl('lbl-fall',     d.fall,         's'); fallTimerTotal = d.fall; }
if (d.showerThresh!== undefined) { document.getElementById('shower-thresh').value = d.showerThresh;  updateLbl('lbl-shower',   d.showerThresh, '%'); }
} catch(e) {
console.error('Settings error:', e);
/* Retry with backoff — a failed initial fetch would otherwise leave
   fallTimerTotal at its hardcoded default, desyncing the countdown bar
   from the device's real FALL_CONFIRM_MS for the rest of the session. */
if (attempt < 5) setTimeout(function(){ fetchSettings(attempt + 1); }, 1000 * (attempt + 1));
}
}

async function saveSettings() {
var p = {
warmup:       parseFloat(document.getElementById('pir-warmup').value),
debounce:     parseFloat(document.getElementById('pir-debounce').value),
fall:         parseFloat(document.getElementById('fall-timer').value),
showerThresh: parseFloat(document.getElementById('shower-thresh').value)
};
fallTimerTotal = p.fall;
try {
var res = await fetch('/api/settings', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(p) });
if (res.ok) {
showToast('save-msg', '✓ Parameters Saved & Applied!', false);
} else {
var msg = res.status === 500 ? '✗ Failed to save (flash write error)' : '✗ Failed to save settings';
showToast('save-msg', msg, true);
}
} catch(e) { showToast('save-msg', '✗ Network error', true); }
}

async function setShowerOverride(mode) {
try {
await fetch('/api/shower_override', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({mode: mode}) });
} catch(e) { /* next poll resyncs button state regardless */ }
}

async function setRadarOverride(mode) {
try {
await fetch('/api/radar_override', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({mode: mode}) });
} catch(e) { /* next poll resyncs button state regardless */ }
}

/* Shared by both demo override controls. prefix: button id prefix
   ('ov-' or 'rov-'). modeA/modeB: the two non-auto mode value strings.
   labelA/labelB: their text for the warning badge. */
function updateOverrideUI(prefix, mode, modeA, modeB, badgeId, labelA, labelB) {
['auto', modeA, modeB].forEach(function(m) {
var el = document.getElementById(prefix + m);
if (el) el.classList.toggle('active', m === mode);
});
var badge = document.getElementById(badgeId);
if (mode === modeA) { badge.style.display = 'inline-flex'; badge.innerText = '⚠ DEMO OVERRIDE: ' + labelA; }
else if (mode === modeB) { badge.style.display = 'inline-flex'; badge.innerText = '⚠ DEMO OVERRIDE: ' + labelB; }
else { badge.style.display = 'none'; }
}

async function testAlert() {
try {
var res = await fetch('/api/test_alert', { method:'POST' });
if (res.ok) {
showToast('test-alert-msg', '✓ Test alert sent! Check Serial monitor.', false);
logEvent('Test alert sent');
} else {
showToast('test-alert-msg', '✗ Device error', true);
}
} catch(e) { showToast('test-alert-msg', '✗ Could not reach device', true); }
}

/* ── Polling: self-rescheduling chain instead of setInterval ──
   Each cycle waits for the previous fetchStatus() to fully settle (success
   or failure) before scheduling the next one 800ms later — this prevents
   overlapping/piled-up requests if the device responds slowly, and recovers
   faster after a slow response than a fixed setInterval would. */
async function pollLoop() {
await fetchStatus();
setTimeout(pollLoop, 800);
}

fetchSettings();
pollLoop();
</script>
</body>
</html>
)rawliteral";