#ifndef WEB_ASSETS_H
#define WEB_ASSETS_H

#include <Arduino.h>

const char WEB_APP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Ecoflow CTRL</title>
    <style>
        :root {
            --bg-dark: #050505;
            --glass-bg: rgba(20, 20, 25, 0.6);
            --glass-border: rgba(255, 255, 255, 0.08);
            --neon-cyan: #00f3ff;
            --neon-pink: #ff00ff;
            --neon-green: #00ff9d;
            --text-main: #e0e0e0;
            --text-sub: #909090;
        }
        body {
            background: var(--bg-dark); color: var(--text-main);
            font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            margin: 0; padding: 20px; padding-bottom: 100px; overflow-x: hidden;
        }
        #canvas-bg { position: fixed; top: 0; left: 0; width: 100%; height: 100%; z-index: -1; opacity: 0.8; pointer-events: none; }
        h1, h2, h3 { margin: 0; font-weight: 300; letter-spacing: 1px; }

        .container { max-width: 850px; margin: 0 auto; display: flex; flex-direction: column; gap: 25px; position: relative; z-index: 1; }

        /* Glassmorphism Card */
        .card {
            background: var(--glass-bg);
            backdrop-filter: blur(16px) saturate(180%);
            -webkit-backdrop-filter: blur(16px) saturate(180%);
            border: 1px solid var(--glass-border);
            border-radius: 16px;
            padding: 20px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.3);
            transition: transform 0.3s cubic-bezier(0.25, 0.8, 0.25, 1), box-shadow 0.3s;
            position: relative;
            overflow: hidden;
            animation: slideUp 0.6s ease-out forwards;
            opacity: 0;
            transform: translateY(20px);
        }
        .card::before {
            content: ''; position: absolute; top: 0; left: -100%; width: 50%; height: 100%;
            background: linear-gradient(to right, transparent, rgba(255,255,255,0.05), transparent);
            transform: skewX(-25deg); transition: 0.5s; pointer-events: none;
        }
        .card:hover::before { left: 150%; transition: 0.7s; }
        .card:hover { transform: translateY(-5px) scale(1.01); box-shadow: 0 12px 40px 0 rgba(0, 0, 0, 0.5), 0 0 15px rgba(0, 243, 255, 0.1); }

        /* Offline State */
        .card.offline { filter: grayscale(1) opacity(0.6); }
        .card.offline:hover { filter: grayscale(0.8) opacity(0.8); }

        @keyframes slideUp { to { opacity: 1; transform: translateY(0); } }

        .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; cursor: pointer; user-select: none; }
        .status-dot { width: 8px; height: 8px; border-radius: 50%; background: #444; margin-right: 10px; display: inline-block; box-shadow: 0 0 5px rgba(0,0,0,0.5); transition: all 0.3s; }
        .status-dot.on { background: var(--neon-green); box-shadow: 0 0 12px var(--neon-green); }

        .grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 15px; margin-bottom: 15px; }
        .grid-3 { grid-template-columns: repeat(3, 1fr); }

        .stat {
            background: rgba(255,255,255,0.03); border-radius: 10px; padding: 10px; text-align: center;
            border: 1px solid rgba(255,255,255,0.02); transition: background 0.2s;
        }
        .stat:hover { background: rgba(255,255,255,0.06); }
        .stat-label { font-size: 0.75em; color: var(--text-sub); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 4px; }
        .stat-val { font-size: 1.2em; font-weight: 600; color: var(--neon-cyan); text-shadow: 0 0 5px rgba(0, 243, 255, 0.3); }

        /* Controls */
        .controls { border-top: 1px solid var(--glass-border); padding-top: 15px; display: none; }
        .controls.open { display: block; animation: expand 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275); }
        @keyframes expand { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }

        .ctrl-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px; font-size: 0.9em; }

        /* Buttons */
        .btn {
            border: none; padding: 8px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; color: #fff;
            transition: all 0.2s; background: rgba(255,255,255,0.1); border: 1px solid rgba(255,255,255,0.1);
        }
        .btn:hover { background: rgba(255,255,255,0.2); box-shadow: 0 0 10px rgba(255,255,255,0.2); }
        .btn:active { transform: scale(0.95); }
        .btn-primary { background: linear-gradient(45deg, rgba(0, 243, 255, 0.2), rgba(0, 255, 157, 0.2)); border-color: var(--neon-cyan); color: var(--neon-cyan); }
        .btn-primary:hover { background: linear-gradient(45deg, rgba(0, 243, 255, 0.4), rgba(0, 255, 157, 0.4)); box-shadow: 0 0 15px var(--neon-cyan); color: #fff; }

        /* Add Device Button */
        #add-device-btn {
            background: rgba(0, 255, 157, 0.15); border: 1px solid var(--neon-green); color: var(--neon-green);
            border-radius: 20px; padding: 6px 14px; display: flex; align-items: center; gap: 6px; font-size: 0.85em;
            backdrop-filter: blur(4px);
        }
        #add-device-btn:hover { background: rgba(0, 255, 157, 0.3); box-shadow: 0 0 15px var(--neon-green); color: #fff; }

        /* Add Menu */
        #add-device-menu {
            position: absolute; top: 50px; right: 0;
            background: rgba(15, 15, 20, 0.9); backdrop-filter: blur(20px);
            border: 1px solid var(--glass-border); border-radius: 12px; padding: 8px;
            display: flex; flex-direction: column; gap: 5px; min-width: 160px;
            transform-origin: top right; transition: all 0.3s cubic-bezier(0.68, -0.55, 0.27, 1.55);
            opacity: 0; transform: scale(0.8) translateY(-20px); pointer-events: none; z-index: 100;
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
        }
        #add-device-menu.show { opacity: 1; transform: scale(1) translateY(0); pointer-events: auto; }
        .menu-item {
            padding: 10px; border-radius: 8px; cursor: pointer; transition: 0.2s; font-size: 0.9em; color: #ccc;
            border: 1px solid transparent;
        }
        .menu-item:hover { background: rgba(0, 243, 255, 0.1); color: #fff; border-color: rgba(0, 243, 255, 0.3); }

        /* 3-Dot Menu */
        .card-menu-btn { cursor: pointer; padding: 0 8px; font-weight: bold; font-size: 1.4em; color: var(--text-sub); transition: color 0.2s; }
        .card-menu-btn:hover { color: #fff; }
        .card-menu {
            position: absolute; top: 45px; right: 15px;
            background: rgba(10, 10, 15, 0.95); border: 1px solid #333; border-radius: 8px;
            padding: 5px; display: none; z-index: 20; box-shadow: 0 5px 15px rgba(0,0,0,0.5);
        }
        .card-menu.show { display: block; animation: fadeIn 0.2s; }
        .menu-opt { padding: 8px 15px; cursor: pointer; font-size: 0.9em; color: #aaa; transition: 0.2s; border-radius: 4px; }
        .menu-opt:hover { background: rgba(255, 50, 50, 0.2); color: #ff5252; }

        /* Switches & Inputs */
        .switch { position: relative; display: inline-block; width: 44px; height: 24px; }
        .switch input { opacity: 0; width: 0; height: 0; }
        .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #333; transition: .3s; border-radius: 34px; border: 1px solid #444; }
        .slider:before { position: absolute; content: ""; height: 16px; width: 16px; left: 3px; bottom: 3px; background-color: #888; transition: .3s; border-radius: 50%; }
        input:checked + .slider { background-color: rgba(0, 255, 157, 0.2); border-color: var(--neon-green); }
        input:checked + .slider:before { transform: translateX(20px); background-color: var(--neon-green); box-shadow: 0 0 10px var(--neon-green); }

        input[type=range] { width: 100%; background: transparent; -webkit-appearance: none; margin: 10px 0; }
        input[type=range]::-webkit-slider-runnable-track { width: 100%; height: 4px; background: #333; border-radius: 2px; }
        input[type=range]::-webkit-slider-thumb {
            height: 16px; width: 16px; border-radius: 50%; background: var(--neon-cyan);
            -webkit-appearance: none; margin-top: -6px; box-shadow: 0 0 10px var(--neon-cyan);
            cursor: pointer; transition: transform 0.1s;
        }
        input[type=range]::-webkit-slider-thumb:hover { transform: scale(1.2); }

        /* Segmented Control */
        .seg-ctrl { display: flex; background: rgba(0,0,0,0.3); border-radius: 10px; padding: 3px; gap: 3px; border: 1px solid rgba(255,255,255,0.05); }
        .seg-opt {
            flex: 1; padding: 8px; text-align: center; cursor: pointer; border-radius: 8px;
            font-size: 0.85em; transition: all 0.3s ease; color: #777; position: relative; overflow: hidden;
            display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 2px;
        }
        .seg-opt:hover { color: #aaa; background: rgba(255,255,255,0.05); }
        .seg-opt.active {
            background: rgba(255,255,255,0.1); color: #fff; font-weight: 600;
            text-shadow: 0 0 8px rgba(255,255,255,0.5); box-shadow: 0 2px 10px rgba(0,0,0,0.2);
        }
        .seg-opt.active div { transform: scale(1.1); }

        /* Power Button */
        .pwr-btn-large {
            width: 70px; height: 70px; border-radius: 50%;
            background: radial-gradient(circle at 30% 30%, #2a2a2a, #1a1a1a);
            border: 2px solid #333;
            display: flex; align-items: center; justify-content: center;
            font-size: 1.8em; color: #444; cursor: pointer;
            transition: all 0.3s; margin: 0 auto;
            box-shadow: 0 5px 15px rgba(0,0,0,0.4), inset 0 2px 5px rgba(255,255,255,0.05);
        }
        .pwr-btn-large:hover { border-color: #555; color: #666; }
        .pwr-btn-large.on {
            border-color: var(--neon-cyan); color: #fff;
            background: radial-gradient(circle at 30% 30%, rgba(0, 243, 255, 0.2), rgba(0,0,0,0.8));
            box-shadow: 0 0 20px rgba(0, 243, 255, 0.4), inset 0 0 10px rgba(0, 243, 255, 0.2);
            text-shadow: 0 0 10px var(--neon-cyan);
        }
        .pwr-btn-large:active { transform: scale(0.95); }

        /* Misc */
        select {
            padding: 6px 10px; background: #222; color: #fff; border: 1px solid #444;
            border-radius: 6px; outline: none; font-family: inherit;
        }
        #console { font-family: 'Fira Code', monospace; background: rgba(0,0,0,0.5); border: 1px solid #333; padding: 10px; border-radius: 8px; height: 200px; overflow-y: auto; font-size: 0.8em; margin-top: 15px; }
        .log-line { border-bottom: 1px solid rgba(255,255,255,0.05); padding: 3px 0; }
        .log-I { color: #fff; } .log-W { color: #ffd700; } .log-E { color: #ff5252; } .log-D { color: #aaa; }

        .hidden { display: none !important; }
        .intro { text-align: center; padding: 60px 20px; animation: fadeIn 1s; }
        @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
    </style>
</head>
<body>
    <canvas id="canvas-bg"></canvas>
    <div class="container">
        <!-- Main Header -->
        <div style="display: flex; justify-content: space-between; align-items: center; padding: 0 10px;">
            <div style="display:flex; align-items:center; gap:10px">
                <h2 style="background: linear-gradient(to right, #fff, #aaa); -webkit-background-clip: text; -webkit-text-fill-color: transparent;">⚡ Ecoflow CTRL</h2>
                <div id="global-status" class="status-dot"></div>
            </div>
            <div style="display:flex; align-items:center; gap:15px; position: relative;">
                <button id="add-device-btn" onclick="toggleAddMenu()">
                    <span>+ DEVICE</span>
                </button>
                <div id="add-device-menu">
                    <div class="menu-item" onclick="connectDevice('d3')">Delta 3</div>
                    <div class="menu-item" onclick="connectDevice('w2')">Wave 2</div>
                    <div class="menu-item" onclick="connectDevice('d3p')">Delta Pro 3</div>
                    <div class="menu-item" onclick="connectDevice('ac')">Alternator Charger</div>
                </div>
                <span id="esp-temp" style="font-size:0.85em; color:var(--text-sub)">--°C</span>
            </div>
        </div>

        <div id="intro-screen" class="intro hidden">
            <h3 style="color:#fff; margin-bottom: 10px;">Ready to Connect</h3>
            <p style="color:var(--text-sub);">Select a device type from the menu to begin scanning.</p>
        </div>

        <div id="device-list"></div>

        <div class="card">
            <div class="header" onclick="toggleLogs()">
                <h3>📋 System Logs</h3>
                <button class="btn btn-sub" id="log-toggle-btn" style="font-size:0.8em;">Show</button>
            </div>
            <div id="log-section" class="hidden">
                <div class="ctrl-row">
                    <label class="switch"><input type="checkbox" id="log-enable" onchange="setLogConfig()"><span class="slider"></span></label>
                    <span>Enable Logs</span>
                    <select id="log-level" onchange="setLogConfig()" style="margin-left: auto;">
                        <option value="5">Verbose</option>
                        <option value="4">Debug</option>
                        <option value="3">Info</option>
                        <option value="2">Warn</option>
                        <option value="1">Error</option>
                    </select>
                </div>
                <div class="ctrl-row">
                    <select id="log-tag" onchange="setLogConfig()" style="width: 100%; padding: 8px; background: #333; border:1px solid #444; color:#fff; border-radius:4px;">
                        <option value="Global">Global (All Tags)</option>
                        <option value="NimBLE">NimBLE</option>
                        <option value="NimBLEScan">NimBLEScan</option>
                        <option value="CmdUtils">CmdUtils</option>
                        <option value="EcoflowCrypto">EcoflowCrypto</option>
                        <option value="EcoflowDataParser">EcoflowDataParser</option>
                        <option value="EcoflowESP32">EcoflowESP32</option>
                        <option value="EcoflowProtocol">EcoflowProtocol</option>
                        <option value="WebServer">WebServer</option>
                        <option value="DeviceManager">DeviceManager</option>
                        <option value="System">System</option>
                        <option value="WiFi">WiFi</option>
                    </select>
                </div>
                <div class="ctrl-row" style="justify-content: flex-end; font-size: 0.8em;">
                     <label><input type="checkbox" id="auto-scroll" checked> Auto-scroll</label>
                </div>
                <div id="console"></div>
                <div class="ctrl-row">
                    <button class="btn" style="background: #333;" onclick="clearLogs()">Clear</button>
                </div>
                <hr style="border-color:rgba(255,255,255,0.1); margin: 15px 0;">
                <div class="ctrl-row">
                     <input type="text" id="cli-input" placeholder="Raw command..." style="flex:1; padding: 8px; background: rgba(0,0,0,0.3); border:1px solid #444; color:#fff; border-radius:4px; margin-right: 10px;" onkeydown="if(event.key==='Enter') sendCli()">
                     <button class="btn btn-primary" onclick="sendCli()">Send</button>
                </div>
            </div>
        </div>
    </div>

<script>
    const API = '/api';
    let knownDevices = new Set();
    let logPollInterval = null;
    let lastLogCount = 0;

    function el(id) { return document.getElementById(id); }

    // --- Templates ---
    const renderCardMenu = (type, connected, paired) => `
        <div class="card-menu-btn" onclick="event.stopPropagation(); toggleCardMenu('${type}')">⋮</div>
        <div id="menu-${type}" class="card-menu">
            ${connected ? `<div class="menu-opt" onclick="disconnect('${type}')">Disconnect</div>` : ''}
            ${(paired || connected) ? `<div class="menu-opt" onclick="forget('${type}')">Forget</div>` : ''}
        </div>
    `;

    const renderD3 = (d) => `
        <div class="card ${d.connected?'':'offline'}" id="card-${d.type}">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.connected?'on':''}" id="dot-${d.type}"></span>Delta 3 <span style="font-size:0.7em; color:#666">(${d.sn})</span></h3>
                <div style="display:flex; align-items:center; gap:15px">
                    <span class="stat-val" id="batt-${d.type}" style="color:${d.connected?'var(--neon-green)':'#666'}">${d.batt}%</span>
                    ${renderCardMenu(d.type, d.connected, d.paired)}
                </div>
            </div>
            <div class="grid grid-3">
                <div class="stat"><div class="stat-label">In</div><div class="stat-val" id="in-${d.type}">${d.in}W</div></div>
                <div class="stat"><div class="stat-label">Out</div><div class="stat-val" id="out-${d.type}">${d.out}W</div></div>
                <div class="stat"><div class="stat-label">Solar</div><div class="stat-val" id="solar-${d.type}">${d.solar}W</div></div>
            </div>
            <div style="text-align:center; margin-bottom:15px; color:#888; font-size:0.8em;">Cell Temp: <span id="temp-${d.type}" style="color:#ccc">${d.cell_temp}</span>°C</div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>AC Output (<span id="ac-pow-${d.type}" style="color:var(--neon-cyan)">0</span>W)</span>
                    <label class="switch"><input type="checkbox" id="ac-${d.type}" onchange="cmd('${d.type}', 'set_ac', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>DC 12V (<span id="dc-pow-${d.type}" style="color:var(--neon-cyan)">0</span>W)</span>
                    <label class="switch"><input type="checkbox" id="dc-${d.type}" onchange="cmd('${d.type}', 'set_dc', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>USB (<span id="usb-pow-${d.type}" style="color:var(--neon-cyan)">0</span>W)</span>
                    <label class="switch"><input type="checkbox" id="usb-${d.type}" onchange="cmd('${d.type}', 'set_usb', this.checked)"><span class="slider"></span></label>
                </div>
                <hr style="border-color:var(--glass-border); margin: 15px 0;">

                <div class="ctrl-row"><span>AC Charge Limit: <b id="val-ac-${d.type}" style="color:var(--neon-cyan)">${d.cfg_ac_lim}</b>W</span></div>
                <input type="range" id="rg-ac-${d.type}" min="400" max="1500" step="100" value="${d.cfg_ac_lim}" onchange="cmd('${d.type}', 'set_ac_lim', parseInt(this.value))" oninput="el('val-ac-${d.type}').innerText=this.value">

                <div class="ctrl-row"><span>Max Charge: <b id="val-max-${d.type}" style="color:var(--neon-cyan)">${d.cfg_max}</b>%</span></div>
                <input type="range" id="rg-max-${d.type}" min="50" max="100" step="1" value="${d.cfg_max}" onchange="cmd('${d.type}', 'set_max_soc', parseInt(this.value))" oninput="el('val-max-${d.type}').innerText=this.value">

                <div class="ctrl-row"><span>Min Discharge: <b id="val-min-${d.type}" style="color:var(--neon-cyan)">${d.cfg_min}</b>%</span></div>
                <input type="range" id="rg-min-${d.type}" min="0" max="30" step="1" value="${d.cfg_min}" onchange="cmd('${d.type}', 'set_min_soc', parseInt(this.value))" oninput="el('val-min-${d.type}').innerText=this.value">

                <div style="margin-top:20px">
                    <span style="font-size:0.8em; color:var(--text-sub); text-transform: uppercase; letter-spacing:1px;">Solar Input (1h)</span>
                    <canvas id="graph-d3" class="graph" width="300" height="100"></canvas>
                </div>
            </div>
        </div>`;

    const renderW2 = (d) => `
        <div class="card ${d.connected?'':'offline'}" id="card-${d.type}">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.connected?'on':''}" id="dot-${d.type}"></span>Wave 2 <span style="font-size:0.7em; color:#666">(${d.sn})</span></h3>
                <div style="display:flex; align-items:center; gap:15px">
                    <span class="stat-val" id="batt-${d.type}" style="color:${d.connected?'var(--neon-green)':'#666'}">${d.batt}%</span>
                     ${renderCardMenu(d.type, d.connected, d.paired)}
                </div>
            </div>
            <div class="grid">
                <div class="stat"><div class="stat-label">Amb Temp</div><div class="stat-val"><span id="amb-${d.type}">${d.amb_temp}</span>°C</div></div>
                <div class="stat"><div class="stat-label">Out Temp</div><div class="stat-val"><span id="out-${d.type}">${d.out_temp}</span>°C</div></div>
            </div>
            <div class="controls" id="ctrl-${d.type}">
                <div style="display: flex; align-items: center; justify-content: center; gap: 30px; margin: 20px 0;">
                     <!-- Power Button (Left) -->
                     <div style="text-align:center;">
                         <div class="pwr-btn-large" id="pwr-btn-${d.type}" onclick="toggleW2Power()">⏻</div>
                         <div style="margin-top:8px; font-size:0.8em; color:var(--text-sub);">
                            <span id="w2-draw-${d.type}" style="color:#fff; font-weight:bold;">0</span>W
                         </div>
                     </div>

                     <!-- Temp Control (Right) -->
                     <div class="dial-container" style="margin:0; flex-direction: column; gap: 5px;">
                        <button class="btn" style="font-size:1.2em; padding: 5px 20px;" onclick="optimisticUpdate('dial-${d.type}', 1); cmd('${d.type}', 'set_temp', parseInt(el('dial-${d.type}').innerText) + 1)">+</button>
                        <div class="dial-val" style="color:var(--neon-cyan); font-size:2.2em; min-width:50px;"><span id="dial-${d.type}">${d.set_temp}</span>°C</div>
                        <button class="btn" style="font-size:1.2em; padding: 5px 20px;" onclick="optimisticUpdate('dial-${d.type}', -1); cmd('${d.type}', 'set_temp', parseInt(el('dial-${d.type}').innerText) - 1)">-</button>
                     </div>
                </div>

                <div id="w2-ui-body-${d.type}" style="transition: opacity 0.3s;">
                    <div class="ctrl-row">
                        <span>Mode</span>
                        <div class="seg-ctrl">
                            <div class="seg-opt" id="mode-0-${d.type}" onclick="cmd('${d.type}', 'set_mode', 0)"><div>❄️</div><small>Cool</small></div>
                            <div class="seg-opt" id="mode-1-${d.type}" onclick="cmd('${d.type}', 'set_mode', 1)"><div>☀️</div><small>Heat</small></div>
                            <div class="seg-opt" id="mode-2-${d.type}" onclick="cmd('${d.type}', 'set_mode', 2)"><div>🌀</div><small>Fan</small></div>
                        </div>
                    </div>

                    <div class="ctrl-row hidden" id="sub-mode-row-${d.type}">
                        <span>Sub Mode</span>
                        <div class="seg-ctrl">
                            <div class="seg-opt" id="sub-0-${d.type}" onclick="cmd('${d.type}', 'set_sub_mode', 0)"><div>🚀</div><small>Max</small></div>
                            <div class="seg-opt" id="sub-3-${d.type}" onclick="cmd('${d.type}', 'set_sub_mode', 3)"><div>⚪</div><small>Norm</small></div>
                            <div class="seg-opt" id="sub-2-${d.type}" onclick="cmd('${d.type}', 'set_sub_mode', 2)"><div>🌱</div><small>Eco</small></div>
                            <div class="seg-opt" id="sub-1-${d.type}" onclick="cmd('${d.type}', 'set_sub_mode', 1)"><div>🌙</div><small>Night</small></div>
                        </div>
                    </div>
                    <div class="ctrl-row">
                        <span>Fan Speed</span>
                        <div class="seg-ctrl">
                            <div class="seg-opt" id="fan-0-${d.type}" onclick="cmd('${d.type}', 'set_fan', 0)">Low</div>
                            <div class="seg-opt" id="fan-1-${d.type}" onclick="cmd('${d.type}', 'set_fan', 1)">Med</div>
                            <div class="seg-opt" id="fan-2-${d.type}" onclick="cmd('${d.type}', 'set_fan', 2)">High</div>
                        </div>
                    </div>

                    <div class="ctrl-row">
                        <span>Ambient Light</span>
                        <label class="switch"><input type="checkbox" id="light-${d.type}" onchange="cmd('${d.type}', 'set_light', this.checked)"><span class="slider"></span></label>
                    </div>
                    <div class="ctrl-row">
                        <span>Beep Sound</span>
                        <label class="switch"><input type="checkbox" id="beep-${d.type}" onchange="cmd('${d.type}', 'set_beep', this.checked)"><span class="slider"></span></label>
                    </div>
                    <div class="ctrl-row">
                        <span>Auto Drain</span>
                        <label class="switch"><input type="checkbox" id="drain-${d.type}" onchange="cmd('${d.type}', 'set_drain', this.checked)"><span class="slider"></span></label>
                    </div>

                    <div style="margin-top:20px">
                        <span style="font-size:0.8em; color:var(--text-sub); text-transform: uppercase;">Temp History (1h)</span>
                        <canvas id="graph-w2" class="graph" width="300" height="100"></canvas>
                    </div>
                </div>
            </div>
        </div>`;

    const renderD3P = (d) => `
        <div class="card ${d.connected?'':'offline'}" id="card-${d.type}">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.connected?'on':''}" id="dot-${d.type}"></span>Delta Pro 3 (${d.sn})</h3>
                <div style="display:flex; align-items:center; gap:15px">
                    <span class="stat-val" id="batt-${d.type}">${d.batt}%</span>
                    ${renderCardMenu(d.type, d.connected, d.paired)}
                </div>
            </div>
            <div class="grid grid-3">
                <div class="stat"><div class="stat-label">In</div><div class="stat-val" id="in-${d.type}">${d.in}W</div></div>
                <div class="stat"><div class="stat-label">Out</div><div class="stat-val" id="out-${d.type}">${d.out}W</div></div>
                <div class="stat"><div class="stat-label">Solar</div><div class="stat-val" id="solar-${d.type}">${d.solar}W</div></div>
            </div>
            <div style="text-align:center; margin-bottom:15px; color:#888;">Cell Temp: <span id="temp-${d.type}">${d.cell_temp}</span>°C</div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>AC Low Volt</span>
                    <label class="switch"><input type="checkbox" id="ac-lv-${d.type}" onchange="cmd('${d.type}', 'set_ac_lv', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>AC High Volt</span>
                    <label class="switch"><input type="checkbox" id="ac-hv-${d.type}" onchange="cmd('${d.type}', 'set_ac_hv', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>DC (12V)</span>
                    <label class="switch"><input type="checkbox" id="dc-${d.type}" onchange="cmd('${d.type}', 'set_dc', this.checked)"><span class="slider"></span></label>
                </div>

                <hr style="border-color:var(--glass-border); margin:15px 0">
                <div class="ctrl-row"><span>Max Charge: <b id="val-max-${d.type}" style="color:var(--neon-cyan)">${d.cfg_max}</b>%</span></div>
                <input type="range" id="rg-max-${d.type}" min="50" max="100" step="1" value="${d.cfg_max}" onchange="cmd('${d.type}', 'set_max_soc', parseInt(this.value))" oninput="el('val-max-${d.type}').innerText=this.value">

                <div class="ctrl-row"><span>Min Discharge: <b id="val-min-${d.type}" style="color:var(--neon-cyan)">${d.cfg_min}</b>%</span></div>
                <input type="range" id="rg-min-${d.type}" min="0" max="30" step="1" value="${d.cfg_min}" onchange="cmd('${d.type}', 'set_min_soc', parseInt(this.value))" oninput="el('val-min-${d.type}').innerText=this.value">

                <div class="ctrl-row">
                    <span>Energy Backup</span>
                    <label class="switch"><input type="checkbox" id="bkp-en-${d.type}" onchange="cmd('${d.type}', 'set_backup_en', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row"><span>Backup Level: <b id="val-bkp-${d.type}" style="color:var(--neon-cyan)">${d.backup_lvl}</b>%</span></div>
                <input type="range" id="rg-bkp-${d.type}" min="0" max="100" value="${d.backup_lvl}" onchange="cmd('${d.type}', 'set_backup_level', parseInt(this.value))" oninput="el('val-bkp-${d.type}').innerText=this.value">

                <div style="margin-top:20px">
                    <span style="font-size:0.8em; color:var(--text-sub); text-transform: uppercase;">Solar Input (1h)</span>
                    <canvas id="graph-d3p" class="graph" width="300" height="100"></canvas>
                </div>
            </div>
        </div>`;

    const renderAC = (d) => `
        <div class="card ${d.connected?'':'offline'}" id="card-${d.type}">
            <div class="header" onclick="toggleCtrl('${d.type}')">
                <h3><span class="status-dot ${d.connected?'on':''}" id="dot-${d.type}"></span>Alt Charger (${d.sn})</h3>
                <div style="display:flex; align-items:center; gap:15px">
                    <span class="stat-val" id="batt-${d.type}">${d.batt}%</span>
                    ${renderCardMenu(d.type, d.connected, d.paired)}
                </div>
            </div>
            <div class="grid">
                <div class="stat"><div class="stat-label">Car Batt</div><div class="stat-val"><span id="car-${d.type}">${d.car_volt.toFixed(1)}</span>V</div></div>
                <div class="stat"><div class="stat-label">Limit</div><div class="stat-val"><span id="lim-${d.type}">${d.pow_lim}</span>W</div></div>
            </div>
            <div class="controls" id="ctrl-${d.type}">
                <div class="ctrl-row">
                    <span>Charger Enabled</span>
                    <label class="switch"><input type="checkbox" id="chg-${d.type}" onchange="cmd('${d.type}', 'set_open', this.checked)"><span class="slider"></span></label>
                </div>
                <div class="ctrl-row">
                    <span>Mode</span>
                    <select id="mode-${d.type}" onchange="cmd('${d.type}', 'set_mode', parseInt(this.value))" style="width:120px">
                        <option value="0">Idle</option>
                        <option value="1">Charge</option>
                        <option value="2">Maintenance</option>
                        <option value="3">Reverse</option>
                    </select>
                </div>
                <div class="ctrl-row">
                    <span>Power Limit: <b id="val-ac-lim">${d.pow_lim}</b>W</span>
                </div>
                <input type="range" id="rg-lim-${d.type}" min="100" max="800" step="50" value="${d.pow_lim}" onchange="cmd('${d.type}', 'set_limit', parseInt(this.value))" oninput="el('val-ac-lim').innerText=this.value">
            </div>
        </div>`;

    // --- Core Logic ---

    function update() {
        fetch(API + '/status').then(r => r.json()).then(data => {
            if(data.esp_temp) el('esp-temp').innerText = data.esp_temp.toFixed(1) + '°C';

            let anyKnown = false;
            const types = ['d3', 'w2', 'd3p', 'ac'];

            types.forEach(type => {
                const d = data[type];
                if (d && (d.connected || d.paired)) {
                    d.type = type;
                    anyKnown = true;

                    // If new device, render card
                    if (!knownDevices.has(type)) {
                        const list = el('device-list');
                        const div = document.createElement('div');
                        if (type === 'd3') div.innerHTML = renderD3(d);
                        else if (type === 'w2') div.innerHTML = renderW2(d);
                        else if (type === 'd3p') div.innerHTML = renderD3P(d);
                        else if (type === 'ac') div.innerHTML = renderAC(d);
                        list.appendChild(div);
                        knownDevices.add(type);

                        if ((type === 'w2' || type === 'd3' || type === 'd3p') && d.connected) setTimeout(() => loadGraph(type), 500);

                        // Open ctrl panel if connected
                        if (d.connected) setTimeout(() => el('ctrl-'+type).classList.add('open'), 100);
                    }

                    // Update values
                    updateDeviceUI(type, d);
                } else {
                     // If device disconnected AND forgotten, remove card
                     if (knownDevices.has(type)) {
                         const card = el('card-' + type);
                         if (card) card.parentElement.removeChild(card);
                         knownDevices.delete(type);
                     }
                }
            });

            if (anyKnown) el('intro-screen').classList.add('hidden');
            else el('intro-screen').classList.remove('hidden');

            // Global Status: Green if any connected
            const anyConnected = types.some(t => data[t]?.connected);
            el('global-status').classList.toggle('on', anyConnected);
        });
    }

    function updateDeviceUI(type, d) {
        // Common styling for offline state
        const card = el('card-'+type);
        if(card) {
            if(d.connected) card.classList.remove('offline');
            else card.classList.add('offline');
        }

        // Debug Power State
        if (type === 'w2' && d.connected) {
            console.log('W2 UI Update: pwr=', d.pwr, ' mode=', d.mode);
        }

        // Render menu again to update state (Disconnect vs Forget)
        const menuContainer = card?.querySelector('.header > div');
        // We need to preserve the batt value span
        if (menuContainer) {
             // This is a bit hacky but non-destructive to the rest of the card
             // We find the 3-dot btn and menu and replace them
             const btn = menuContainer.querySelector('.card-menu-btn');
             const menu = menuContainer.querySelector('.card-menu');
             if(btn) btn.remove();
             if(menu) menu.remove();
             menuContainer.insertAdjacentHTML('beforeend', renderCardMenu(type, d.connected, d.paired));
        }

        setTxt('batt-'+type, d.batt + '%');
        setCls('dot-'+type, 'on', d.connected);

        if (!d.connected) return; // Skip updating dynamic values if offline

        if (type === 'd3') {
            setTxt('in-'+type, d.in + 'W');
            setTxt('out-'+type, d.out + 'W');
            setTxt('solar-'+type, d.solar + 'W');
            setTxt('temp-'+type, d.cell_temp);
            setTxt('ac-pow-'+type, d.ac_out_pow);
            setTxt('dc-pow-'+type, d.dc_out_pow);
            setTxt('usb-pow-'+type, d.usb_out_pow);

            setCheck('ac-'+type, d.ac_on);
            setCheck('dc-'+type, d.dc_on);
            setCheck('usb-'+type, d.usb_on);

            setVal('rg-ac-'+type, d.cfg_ac_lim); setTxt('val-ac-'+type, d.cfg_ac_lim);
            setVal('rg-max-'+type, d.cfg_max); setTxt('val-max-'+type, d.cfg_max);
            setVal('rg-min-'+type, d.cfg_min); setTxt('val-min-'+type, d.cfg_min);
        }
        else if (type === 'w2') {
            setTxt('amb-'+type, d.amb_temp);
            setTxt('out-'+type, d.out_temp);
            setTxt('dial-'+type, d.set_temp);

            setCheck('light-'+type, d.light);
            setCheck('beep-'+type, d.beep);
            setCheck('drain-'+type, d.drain);

            const pwrBtn = el('pwr-btn-'+type);
            const uiBody = el('w2-ui-body-'+type);
            if (d.pwr) {
                pwrBtn.classList.add('on');
                uiBody.style.opacity = '1';
                uiBody.style.pointerEvents = 'auto';
            } else {
                pwrBtn.classList.remove('on');
                uiBody.style.opacity = '0.3';
                uiBody.style.pointerEvents = 'none';
            }

            let draw = 0;
            if (d.pwr_mppt > 0) draw = "Solar " + d.pwr_mppt;
            else if (d.pwr_psdr > 0) draw = "DC " + d.pwr_psdr;
            else draw = Math.abs(d.pwr_bat);
            setTxt('w2-draw-'+type, draw);

            setSeg('mode', type, d.mode, 3);
            setSeg('fan', type, d.fan, 3);

            const subRow = el('sub-mode-row-'+type);
            if (d.mode === 0 || d.mode === 1) {
                subRow.classList.remove('hidden');
                setSeg('sub', type, d.sub_mode, 4);
            } else {
                subRow.classList.add('hidden');
            }
        }
        else if (type === 'd3p') {
            setTxt('in-'+type, d.in + 'W');
            setTxt('out-'+type, d.out + 'W');
            setTxt('solar-'+type, d.solar + 'W');
            setTxt('temp-'+type, d.cell_temp);
            setCheck('ac-lv-'+type, d.ac_lv_on);
            setCheck('ac-hv-'+type, d.ac_hv_on);
            setCheck('dc-'+type, d.dc_on);
            setCheck('bkp-en-'+type, d.backup_en);
            setVal('rg-max-'+type, d.cfg_max); setTxt('val-max-'+type, d.cfg_max);
            setVal('rg-min-'+type, d.cfg_min); setTxt('val-min-'+type, d.cfg_min);
            setVal('rg-bkp-'+type, d.backup_lvl); setTxt('val-bkp-'+type, d.backup_lvl);
        }
        else if (type === 'ac') {
             setTxt('car-'+type, d.car_volt.toFixed(1));
             setTxt('lim-'+type, d.pow_lim);
             setCheck('chg-'+type, d.chg_open);
             setVal('mode-'+type, d.mode);
             setVal('rg-lim-'+type, d.pow_lim); setTxt('val-ac-lim', d.pow_lim);
        }
    }

    // --- UI Helpers ---
    function setTxt(id, val) { const e = el(id); if(e && e.innerText != val) e.innerText = val; }
    function setVal(id, val) { const e = el(id); if(e && e.value != val && document.activeElement !== e) e.value = val; }
    function setCheck(id, val) { const e = el(id); if(e && e.checked != val) e.checked = val; }
    function setCls(id, cls, on) { const e = el(id); if(e) { if(on) e.classList.add(cls); else e.classList.remove(cls); } }

    function setSeg(prefix, type, val, count) {
        const all = document.querySelectorAll(`[id^="${prefix}-"][id$="-${type}"]`);
        all.forEach(e => e.classList.remove('active'));
        const active = el(`${prefix}-${val}-${type}`);
        if(active) active.classList.add('active');
    }

    function optimisticUpdate(id, change) {
        const elVal = el(id);
        if(elVal) {
            let val = parseInt(elVal.innerText);
            if(!isNaN(val)) elVal.innerText = val + change;
        }
    }

    function toggleAddMenu() { el('add-device-menu').classList.toggle('show'); }

    document.addEventListener('click', function(e) {
        const menu = el('add-device-menu');
        const btn = el('add-device-btn');
        if (!menu.contains(e.target) && !btn.contains(e.target)) menu.classList.remove('show');

        document.querySelectorAll('.card-menu').forEach(m => {
            if (!m.contains(e.target) && !e.target.classList.contains('card-menu-btn')) m.classList.remove('show');
        });
    });

    // Mouse move glow effect
    document.addEventListener('mousemove', e => {
        document.querySelectorAll('.card').forEach(card => {
            const rect = card.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const y = e.clientY - rect.top;
            card.style.background = `radial-gradient(circle at ${x}px ${y}px, rgba(255,255,255,0.08), rgba(20,20,25,0.6) 60%)`;
        });
    });

    function toggleCardMenu(type) {
        const menu = el('menu-'+type);
        document.querySelectorAll('.card-menu').forEach(m => { if(m!==menu) m.classList.remove('show'); });
        menu.classList.toggle('show');
    }

    function toggleW2Power() {
        // Visual toggle immediately for responsiveness
        const btn = el('pwr-btn-w2');
        const isOn = btn.classList.contains('on');
        // If ON (1), we want to turn OFF (0/false -> backend maps to 2)
        // If OFF (2), we want to turn ON (1/true -> backend maps to 1)
        // Backend expects boolean in JSON: true -> 1 (ON), false -> 2 (OFF)
        cmd('w2', 'set_power', !isOn);
    }

    function toggleCtrl(id) {
        const e = el('ctrl-'+id);
        e.classList.toggle('open');
        if((id === 'w2' || id === 'd3' || id === 'd3p') && e.classList.contains('open')) loadGraph(id);
    }

    function loadGraph(type) {
        const canvas = el('graph-' + type);
        if(!canvas) return;
        fetch(API + '/history?type=' + type).then(r => r.json()).then(data => {
            const ctx = canvas.getContext('2d');
            const w = canvas.width;
            const h = canvas.height;
            ctx.clearRect(0,0,w,h);

            if(!data || data.length === 0) {
                ctx.fillStyle = '#555'; ctx.fillText('No Data', w/2-20, h/2); return;
            }

            // Gradient Line
            const gradient = ctx.createLinearGradient(0, 0, w, 0);
            gradient.addColorStop(0, '#00f3ff');
            gradient.addColorStop(1, '#00ff9d');
            ctx.strokeStyle = gradient;
            ctx.lineWidth = 2;
            ctx.beginPath();

            const min = Math.min(...data) - 2;
            const max = Math.max(...data) + 2;
            const range = max - min || 1;

            data.forEach((val, i) => {
                const x = (i / (data.length - 1)) * w;
                const y = h - ((val - min) / range) * h;
                if(i===0) ctx.moveTo(x,y); else ctx.lineTo(x,y);
            });
            ctx.stroke();

            // Fill
            ctx.lineTo(w, h); ctx.lineTo(0, h); ctx.fillStyle = 'rgba(0, 243, 255, 0.1)'; ctx.fill();
        });
    }

    function connectDevice(type) {
        el('add-device-menu').classList.remove('show');
        if (!type) type = el('dev-select').value;
        fetch(API + '/connect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({type}) });
    }

    function disconnect(type) {
        if(!confirm('Disconnect device?')) return;
        fetch(API + '/disconnect', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({type}) });
    }

    function forget(type) {
        if(!confirm('Forget device? This will remove credentials.')) return;
        fetch(API + '/forget', { method: 'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({type}) })
        .then(() => {
             knownDevices.delete(type);
             const card = el('card-' + type);
             if (card) card.parentElement.removeChild(card);
        });
    }

    function cmd(type, cmdName, val) {
        fetch(API + '/control', {
            method: 'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({type, cmd: cmdName, val: val})
        });
    }

    // --- Logs ---
    function toggleLogs() {
        el('log-section').classList.toggle('hidden');
        if (!el('log-section').classList.contains('hidden')) startLogPoll(); else stopLogPoll();
    }

    function startLogPoll() {
        if (logPollInterval) return;
        lastLogCount = 0;
        el('console').innerHTML = '';
        logPollInterval = setInterval(() => {
            fetch(API + '/logs?index=' + lastLogCount).then(r => r.json()).then(logs => {
                const c = el('console');
                logs.forEach(l => {
                    const d = new Date(l.ts);
                    const time = d.getHours()+':'+d.getMinutes()+':'+d.getSeconds();
                    const line = document.createElement('div');
                    line.className = 'log-line log-' + (['?','E','W','I','D','V'][l.lvl] || 'I');
                    line.innerText = `[${time}] ${l.tag}: ${l.msg}`;
                    c.appendChild(line);
                });
                lastLogCount += logs.length;
                if(logs.length > 0 && el('auto-scroll').checked) c.scrollTop = c.scrollHeight;
            });
        }, 1000);
    }

    function stopLogPoll() { clearInterval(logPollInterval); logPollInterval = null; }

    function setLogConfig() {
        const enabled = el('log-enable').checked;
        const level = parseInt(el('log-level').value);
        const tag = el('log-tag').value;
        fetch(API + '/log_config', {
             method: 'POST',
             headers:{'Content-Type':'application/json'},
             body:JSON.stringify({enable: enabled, level, tag})
        });
    }

    function clearLogs() { el('console').innerHTML = ''; }

    function sendCli() {
        const cmd = el('cli-input').value;
        if(!cmd) return;
        fetch(API + '/raw_command', {
            method: 'POST',
            headers:{'Content-Type':'application/json'},
            body:JSON.stringify({cmd})
        }).then(r => {
            if(r.ok) {
                el('cli-input').value = '';
                const c = el('console');
                const line = document.createElement('div');
                line.className = 'log-line log-I';
                line.innerText = `> ${cmd}`;
                c.appendChild(line);
                if (el('auto-scroll').checked) c.scrollTop = c.scrollHeight;
            }
        });
    }

    setInterval(update, 1000);
    update();

    // --- Cyberpunk Particles ---
    const canvas = document.getElementById('canvas-bg');
    const ctx = canvas.getContext('2d');
    let particles = [];
    let w, h;

    function resize() {
        w = canvas.width = window.innerWidth;
        h = canvas.height = window.innerHeight;
        initParticles();
    }

    function initParticles() {
        particles = [];
        const cnt = Math.floor(w * h / 20000);
        for(let i=0; i<cnt; i++) {
            particles.push({
                x: Math.random()*w, y: Math.random()*h,
                vx: (Math.random()-0.5)*0.8, vy: (Math.random()-0.5)*0.8,
                size: Math.random() * 2 + 1,
                color: Math.random() > 0.5 ? 'rgba(0, 243, 255, ' : 'rgba(0, 255, 157, '
            });
        }
    }

    function draw() {
        ctx.clearRect(0,0,w,h);

        for(let i=0; i<particles.length; i++) {
            let p = particles[i];
            p.x += p.vx; p.y += p.vy;
            if(p.x < 0 || p.x > w) p.vx *= -1;
            if(p.y < 0 || p.y > h) p.vy *= -1;

            ctx.beginPath();
            ctx.arc(p.x, p.y, p.size, 0, Math.PI*2);
            ctx.fillStyle = p.color + '0.5)';
            ctx.fill();

            for(let j=i+1; j<particles.length; j++) {
                let p2 = particles[j];
                let dx = p.x - p2.x, dy = p.y - p2.y;
                let dist = dx*dx + dy*dy;
                if(dist < 10000) {
                    ctx.beginPath();
                    ctx.moveTo(p.x, p.y);
                    ctx.lineTo(p2.x, p2.y);
                    ctx.strokeStyle = p.color + (1 - dist/10000)*0.2 + ')';
                    ctx.stroke();
                }
            }
        }
        requestAnimationFrame(draw);
    }

    window.addEventListener('resize', resize);
    resize();
    draw();
</script>
</body>
</html>
)rawliteral";

#endif // WEB_ASSETS_H
