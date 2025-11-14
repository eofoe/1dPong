#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

// External references from main.cpp
extern AsyncWebServer server;
extern GameConfig gameConfig;
extern VisualConfig visualConfig;
extern Statistics stats;
extern ConfigManager configManager;
extern uint8_t leftScore, rightScore;
extern uint16_t currentRound;
extern bool gameActive;
extern DeviceInfo lamps[];
extern DeviceInfo buzzerLeft, buzzerRight;
extern uint8_t numActiveLamps;
extern void sendIdleCommand();
extern void broadcastConfig();
extern void sendScoreUpdate();
extern String getIdleEffectName(uint8_t effect);

// =============================================================================
// HTML INTERFACE (stored in PROGMEM to save RAM)
// =============================================================================

const char HTML_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>1D Pong - Configuration</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: #333;
            padding: 20px;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: white;
            border-radius: 15px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            overflow: hidden;
        }
        header {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            padding: 30px;
            text-align: center;
        }
        h1 { font-size: 2.5em; margin-bottom: 10px; }
        .subtitle { opacity: 0.9; }
        .tabs {
            display: flex;
            background: #f5f5f5;
            border-bottom: 2px solid #ddd;
        }
        .tab {
            flex: 1;
            padding: 15px;
            text-align: center;
            cursor: pointer;
            background: #f5f5f5;
            border: none;
            font-size: 16px;
            transition: all 0.3s;
        }
        .tab:hover { background: #e0e0e0; }
        .tab.active {
            background: white;
            border-bottom: 3px solid #667eea;
            font-weight: bold;
        }
        .tab-content {
            display: none;
            padding: 30px;
        }
        .tab-content.active { display: block; }
        .section {
            margin-bottom: 30px;
            padding: 20px;
            background: #f9f9f9;
            border-radius: 10px;
        }
        .section h2 {
            color: #667eea;
            margin-bottom: 15px;
            border-bottom: 2px solid #667eea;
            padding-bottom: 10px;
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: 600;
            color: #555;
        }
        input[type="number"], input[type="text"], select {
            width: 100%;
            padding: 10px;
            border: 2px solid #ddd;
            border-radius: 5px;
            font-size: 14px;
            transition: border 0.3s;
        }
        input:focus, select:focus {
            outline: none;
            border-color: #667eea;
        }
        .slider-container {
            display: flex;
            align-items: center;
            gap: 15px;
        }
        input[type="range"] {
            flex: 1;
            height: 8px;
            border-radius: 5px;
            background: #ddd;
            outline: none;
            -webkit-appearance: none;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #667eea;
            cursor: pointer;
        }
        .value-display {
            min-width: 60px;
            text-align: center;
            font-weight: bold;
            color: #667eea;
        }
        .btn {
            padding: 12px 30px;
            border: none;
            border-radius: 5px;
            font-size: 16px;
            cursor: pointer;
            transition: all 0.3s;
            font-weight: 600;
        }
        .btn-primary {
            background: #667eea;
            color: white;
        }
        .btn-primary:hover {
            background: #5568d3;
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(102, 126, 234, 0.4);
        }
        .btn-secondary {
            background: #6c757d;
            color: white;
        }
        .btn-secondary:hover { background: #5a6268; }
        .btn-danger {
            background: #dc3545;
            color: white;
        }
        .btn-danger:hover { background: #c82333; }
        .stats-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .stat-card {
            background: white;
            padding: 20px;
            border-radius: 10px;
            text-align: center;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .stat-value {
            font-size: 2em;
            font-weight: bold;
            color: #667eea;
        }
        .stat-label {
            color: #666;
            margin-top: 5px;
        }
        .device-list {
            list-style: none;
        }
        .device-item {
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 10px;
            background: white;
            margin-bottom: 10px;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
        }
        .device-status {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            margin-right: 10px;
        }
        .status-online { background: #28a745; }
        .status-offline { background: #dc3545; }
        .color-preview {
            display: inline-block;
            width: 30px;
            height: 30px;
            border-radius: 5px;
            border: 2px solid #ddd;
            vertical-align: middle;
            margin-left: 10px;
        }
        .alert {
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 5px;
            display: none;
        }
        .alert-success {
            background: #d4edda;
            border: 1px solid #c3e6cb;
            color: #155724;
        }
        .alert-error {
            background: #f8d7da;
            border: 1px solid #f5c6cb;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>⚡ 1D PONG ⚡</h1>
            <p class="subtitle">Sequencer Configuration Panel</p>
        </header>

        <div class="tabs">
            <button class="tab active" onclick="switchTab('game')">Game Settings</button>
            <button class="tab" onclick="switchTab('visual')">Visual Settings</button>
            <button class="tab" onclick="switchTab('stats')">Statistics</button>
            <button class="tab" onclick="switchTab('network')">Network Status</button>
        </div>

        <div id="game-tab" class="tab-content active">
            <div id="alert-game" class="alert"></div>

            <div class="section">
                <h2>⏱️ Timing Settings</h2>
                <div class="form-group">
                    <label>GodShot Window (ms)</label>
                    <div class="slider-container">
                        <input type="range" id="godShotWindow" min="50" max="200" step="10" value="100" oninput="updateValue(this)">
                        <span class="value-display" id="godShotWindow-value">100</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Perfect Window (ms)</label>
                    <div class="slider-container">
                        <input type="range" id="perfectWindow" min="200" max="1000" step="50" value="600" oninput="updateValue(this)">
                        <span class="value-display" id="perfectWindow-value">600</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Timing Window Start</label>
                    <select id="timingMode">
                        <option value="0">When fade begins</option>
                        <option value="1" selected>When fully bright</option>
                        <option value="2">When animation complete</option>
                    </select>
                </div>
            </div>

            <div class="section">
                <h2>🚀 Speed Settings</h2>
                <div class="form-group">
                    <label>Start Speed (ms per lamp)</label>
                    <div class="slider-container">
                        <input type="range" id="startSpeed" min="50" max="500" step="10" value="200" oninput="updateValue(this)">
                        <span class="value-display" id="startSpeed-value">200</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Speed Profile</label>
                    <select id="speedProfile" onchange="toggleSpeedSettings()">
                        <option value="0" selected>Linear</option>
                        <option value="1">Exponential</option>
                    </select>
                </div>
                <div class="form-group" id="linearSettings">
                    <label>Speed Increment (Linear)</label>
                    <div class="slider-container">
                        <input type="range" id="speedIncrement" min="5" max="50" step="5" value="20" oninput="updateValue(this)">
                        <span class="value-display" id="speedIncrement-value">20</span>
                    </div>
                </div>
                <div class="form-group" id="expSettings" style="display:none;">
                    <label>Speed Multiplier (Exponential)</label>
                    <div class="slider-container">
                        <input type="range" id="speedMultiplier" min="1.05" max="1.50" step="0.05" value="1.15" oninput="updateValue(this)">
                        <span class="value-display" id="speedMultiplier-value">1.15</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Max Speed (speed cap)</label>
                    <div class="slider-container">
                        <input type="range" id="maxSpeed" min="20" max="100" step="5" value="50" oninput="updateValue(this)">
                        <span class="value-display" id="maxSpeed-value">50</span>
                    </div>
                </div>
            </div>

            <div class="section">
                <h2>🎮 Game Rules</h2>
                <div class="form-group">
                    <label>Winning Score</label>
                    <div class="slider-container">
                        <input type="range" id="winningScore" min="1" max="10" step="1" value="3" oninput="updateValue(this)">
                        <span class="value-display" id="winningScore-value">3</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Animation Mode</label>
                    <select id="animationMode">
                        <option value="0">Discrete Jump</option>
                        <option value="1">Smooth Fade</option>
                        <option value="2" selected>Smooth with Strip</option>
                    </select>
                </div>
            </div>

            <button class="btn btn-primary" onclick="saveGameConfig()">💾 Save Game Settings</button>
        </div>

        <div id="visual-tab" class="tab-content">
            <div id="alert-visual" class="alert"></div>

            <div class="section">
                <h2>🎨 Display Settings</h2>
                <div class="form-group">
                    <label>Idle Effect</label>
                    <select id="idleEffect">
                        <option value="0">Rainbow</option>
                        <option value="1">Breathing</option>
                        <option value="2">Knight Rider</option>
                        <option value="3">Twinkle</option>
                        <option value="4">Fire</option>
                        <option value="5">Pulse</option>
                    </select>
                </div>
                <div class="form-group">
                    <label>Idle Animation Speed (ms)</label>
                    <div class="slider-container">
                        <input type="range" id="idleSpeed" min="10" max="200" step="10" value="50" oninput="updateValue(this)">
                        <span class="value-display" id="idleSpeed-value">50</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Global Brightness (%)</label>
                    <div class="slider-container">
                        <input type="range" id="globalBrightness" min="10" max="100" step="5" value="80" oninput="updateValue(this)">
                        <span class="value-display" id="globalBrightness-value">80</span>
                    </div>
                </div>
                <div class="form-group">
                    <label>Score Display Mode</label>
                    <select id="scoreDisplayMode">
                        <option value="0" selected>Edges (3 lamps per player)</option>
                        <option value="1">Minimal (brief flash)</option>
                    </select>
                </div>
            </div>

            <div class="section">
                <h2>🎨 Colors</h2>
                <div class="form-group">
                    <label>Player 1 Color (Left)
                        <span class="color-preview" id="p1-preview" style="background:#00ff00;"></span>
                    </label>
                    <input type="text" id="player1Color" value="00FF00" maxlength="6" oninput="updateColorPreview('p1')">
                </div>
                <div class="form-group">
                    <label>Player 2 Color (Right)
                        <span class="color-preview" id="p2-preview" style="background:#ff0000;"></span>
                    </label>
                    <input type="text" id="player2Color" value="FF0000" maxlength="6" oninput="updateColorPreview('p2')">
                </div>
                <div class="form-group">
                    <label>Neutral Color
                        <span class="color-preview" id="neutral-preview" style="background:#0000ff;"></span>
                    </label>
                    <input type="text" id="neutralColor" value="0000FF" maxlength="6" oninput="updateColorPreview('neutral')">
                </div>
            </div>

            <button class="btn btn-primary" onclick="saveVisualConfig()">💾 Save Visual Settings</button>
            <button class="btn btn-secondary" onclick="testIdleEffect()">🎬 Test Idle Effect</button>
        </div>

        <div id="stats-tab" class="tab-content">
            <div class="section">
                <h2>📊 Game Statistics</h2>
                <div class="stats-grid" id="stats-container">
                    <!-- Populated by JavaScript -->
                </div>
            </div>
            <button class="btn btn-danger" onclick="resetStats()">🗑️ Reset Statistics</button>
        </div>

        <div id="network-tab" class="tab-content">
            <div class="section">
                <h2>📡 Connected Devices</h2>
                <h3>Buzzers</h3>
                <ul class="device-list" id="buzzer-list">
                    <!-- Populated by JavaScript -->
                </ul>
                <h3>Lamps</h3>
                <ul class="device-list" id="lamp-list">
                    <!-- Populated by JavaScript -->
                </ul>
            </div>
            <button class="btn btn-secondary" onclick="refreshNetwork()">🔄 Refresh</button>
        </div>
    </div>

    <script>
        // Tab switching
        function switchTab(tab) {
            document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
            document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
            document.querySelector(`.tab[onclick*="${tab}"]`).classList.add('active');
            document.getElementById(`${tab}-tab`).classList.add('active');

            if (tab === 'stats') loadStatistics();
            if (tab === 'network') loadNetworkStatus();
        }

        // Update slider value display
        function updateValue(slider) {
            document.getElementById(slider.id + '-value').textContent = slider.value;
        }

        // Toggle speed settings based on profile
        function toggleSpeedSettings() {
            const profile = document.getElementById('speedProfile').value;
            document.getElementById('linearSettings').style.display = profile == '0' ? 'block' : 'none';
            document.getElementById('expSettings').style.display = profile == '1' ? 'block' : 'none';
        }

        // Update color preview
        function updateColorPreview(player) {
            const input = document.getElementById(player === 'p1' ? 'player1Color' :
                                                  player === 'p2' ? 'player2Color' : 'neutralColor');
            const preview = document.getElementById(player + '-preview');
            let color = input.value.replace(/[^0-9A-Fa-f]/g, '');
            if (color.length === 6) {
                preview.style.background = '#' + color;
            }
        }

        // Show alert
        function showAlert(tab, message, type) {
            const alert = document.getElementById('alert-' + tab);
            alert.className = 'alert alert-' + type;
            alert.textContent = message;
            alert.style.display = 'block';
            setTimeout(() => alert.style.display = 'none', 3000);
        }

        // Load current configuration
        async function loadConfig() {
            try {
                const response = await fetch('/api/config');
                const data = await response.json();

                // Game config
                document.getElementById('godShotWindow').value = data.game.godShotWindow;
                document.getElementById('perfectWindow').value = data.game.perfectWindow;
                document.getElementById('startSpeed').value = data.game.startSpeed;
                document.getElementById('speedIncrement').value = data.game.speedIncrement;
                document.getElementById('speedMultiplier').value = data.game.speedMultiplier;
                document.getElementById('maxSpeed').value = data.game.maxSpeed;
                document.getElementById('speedProfile').value = data.game.speedProfile;
                document.getElementById('winningScore').value = data.game.winningScore;
                document.getElementById('timingMode').value = data.game.timingMode;
                document.getElementById('animationMode').value = data.game.animationMode;

                // Visual config
                document.getElementById('idleEffect').value = data.visual.idleEffect;
                document.getElementById('idleSpeed').value = data.visual.idleSpeed;
                document.getElementById('globalBrightness').value = data.visual.globalBrightness;
                document.getElementById('scoreDisplayMode').value = data.visual.scoreDisplayMode;
                document.getElementById('player1Color').value = data.visual.player1Color;
                document.getElementById('player2Color').value = data.visual.player2Color;
                document.getElementById('neutralColor').value = data.visual.neutralColor;

                // Update all value displays
                document.querySelectorAll('input[type="range"]').forEach(updateValue);
                updateColorPreview('p1');
                updateColorPreview('p2');
                updateColorPreview('neutral');
                toggleSpeedSettings();

            } catch (error) {
                console.error('Failed to load config:', error);
            }
        }

        // Save game configuration
        async function saveGameConfig() {
            const config = {
                godShotWindow: parseInt(document.getElementById('godShotWindow').value),
                perfectWindow: parseInt(document.getElementById('perfectWindow').value),
                startSpeed: parseInt(document.getElementById('startSpeed').value),
                speedIncrement: parseInt(document.getElementById('speedIncrement').value),
                speedMultiplier: parseFloat(document.getElementById('speedMultiplier').value),
                maxSpeed: parseInt(document.getElementById('maxSpeed').value),
                speedProfile: parseInt(document.getElementById('speedProfile').value),
                winningScore: parseInt(document.getElementById('winningScore').value),
                timingMode: parseInt(document.getElementById('timingMode').value),
                animationMode: parseInt(document.getElementById('animationMode').value)
            };

            try {
                const response = await fetch('/api/config/game', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(config)
                });

                if (response.ok) {
                    showAlert('game', '✓ Game settings saved successfully!', 'success');
                } else {
                    showAlert('game', '✗ Failed to save settings', 'error');
                }
            } catch (error) {
                showAlert('game', '✗ Network error', 'error');
            }
        }

        // Save visual configuration
        async function saveVisualConfig() {
            const config = {
                idleEffect: parseInt(document.getElementById('idleEffect').value),
                idleSpeed: parseInt(document.getElementById('idleSpeed').value),
                globalBrightness: parseInt(document.getElementById('globalBrightness').value),
                scoreDisplayMode: parseInt(document.getElementById('scoreDisplayMode').value),
                player1Color: document.getElementById('player1Color').value,
                player2Color: document.getElementById('player2Color').value,
                neutralColor: document.getElementById('neutralColor').value
            };

            try {
                const response = await fetch('/api/config/visual', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify(config)
                });

                if (response.ok) {
                    showAlert('visual', '✓ Visual settings saved successfully!', 'success');
                } else {
                    showAlert('visual', '✗ Failed to save settings', 'error');
                }
            } catch (error) {
                showAlert('visual', '✗ Network error', 'error');
            }
        }

        // Test idle effect
        async function testIdleEffect() {
            try {
                await fetch('/api/test/idle', {method: 'POST'});
            } catch (error) {
                console.error('Test failed:', error);
            }
        }

        // Load statistics
        async function loadStatistics() {
            try {
                const response = await fetch('/api/stats');
                const stats = await response.json();

                const container = document.getElementById('stats-container');
                container.innerHTML = `
                    <div class="stat-card">
                        <div class="stat-value">${stats.totalGames}</div>
                        <div class="stat-label">Total Games</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.player1Wins}</div>
                        <div class="stat-label">P1 Wins</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.player2Wins}</div>
                        <div class="stat-label">P2 Wins</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.godShotCount}</div>
                        <div class="stat-label">GodShots</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.perfectCount}</div>
                        <div class="stat-label">Perfect Hits</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.missCount}</div>
                        <div class="stat-label">Misses</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.avgReaction.toFixed(1)} ms</div>
                        <div class="stat-label">Avg Reaction</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.bestReaction} ms</div>
                        <div class="stat-label">Best Time</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.player1GodShots}</div>
                        <div class="stat-label">P1 GodShots</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.player2GodShots}</div>
                        <div class="stat-label">P2 GodShots</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.p1AvgReaction.toFixed(1)} ms</div>
                        <div class="stat-label">P1 Avg Reaction</div>
                    </div>
                    <div class="stat-card">
                        <div class="stat-value">${stats.p2AvgReaction.toFixed(1)} ms</div>
                        <div class="stat-label">P2 Avg Reaction</div>
                    </div>
                `;
            } catch (error) {
                console.error('Failed to load statistics:', error);
            }
        }

        // Reset statistics
        async function resetStats() {
            if (!confirm('Are you sure you want to reset all statistics?')) return;

            try {
                const response = await fetch('/api/stats/reset', {method: 'POST'});
                if (response.ok) {
                    loadStatistics();
                }
            } catch (error) {
                console.error('Failed to reset stats:', error);
            }
        }

        // Load network status
        async function loadNetworkStatus() {
            try {
                const response = await fetch('/api/network');
                const data = await response.json();

                // Buzzers
                const buzzerList = document.getElementById('buzzer-list');
                buzzerList.innerHTML = `
                    <li class="device-item">
                        <span><span class="device-status ${data.buzzerLeft.active ? 'status-online' : 'status-offline'}"></span>
                        Buzzer Left (Player 1)</span>
                        <span>RSSI: ${data.buzzerLeft.rssi} dBm</span>
                    </li>
                    <li class="device-item">
                        <span><span class="device-status ${data.buzzerRight.active ? 'status-online' : 'status-offline'}"></span>
                        Buzzer Right (Player 2)</span>
                        <span>RSSI: ${data.buzzerRight.rssi} dBm</span>
                    </li>
                `;

                // Lamps
                const lampList = document.getElementById('lamp-list');
                lampList.innerHTML = data.lamps.map((lamp, i) => `
                    <li class="device-item">
                        <span><span class="device-status ${lamp.active ? 'status-online' : 'status-offline'}"></span>
                        Lamp ${i}</span>
                        <span>RSSI: ${lamp.rssi} dBm</span>
                    </li>
                `).join('');

            } catch (error) {
                console.error('Failed to load network status:', error);
            }
        }

        function refreshNetwork() {
            loadNetworkStatus();
        }

        // Load config on page load
        window.addEventListener('load', loadConfig);
    </script>
</body>
</html>
)rawliteral";

// =============================================================================
// API HANDLERS
// =============================================================================

void setupWebServer() {
    // Serve main page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html", HTML_PAGE);
    });

    // Get current configuration
    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);

        // Game config
        JsonObject game = doc.createNestedObject("game");
        game["godShotWindow"] = gameConfig.godShotWindow;
        game["perfectWindow"] = gameConfig.perfectWindow;
        game["startSpeed"] = gameConfig.startSpeed;
        game["speedIncrement"] = gameConfig.speedIncrement;
        game["speedMultiplier"] = gameConfig.speedMultiplier;
        game["maxSpeed"] = gameConfig.maxSpeed;
        game["speedProfile"] = gameConfig.speedProfile;
        game["winningScore"] = gameConfig.winningScore;
        game["timingMode"] = gameConfig.timingMode;
        game["animationMode"] = gameConfig.animationMode;

        // Visual config
        JsonObject visual = doc.createNestedObject("visual");
        visual["idleEffect"] = visualConfig.idleEffect;
        visual["idleSpeed"] = visualConfig.idleSpeed;
        visual["globalBrightness"] = visualConfig.globalBrightness;
        visual["scoreDisplayMode"] = visualConfig.scoreDisplayMode;

        char colorBuf[7];
        sprintf(colorBuf, "%06X", visualConfig.player1Color);
        visual["player1Color"] = colorBuf;
        sprintf(colorBuf, "%06X", visualConfig.player2Color);
        visual["player2Color"] = colorBuf;
        sprintf(colorBuf, "%06X", visualConfig.neutralColor);
        visual["neutralColor"] = colorBuf;

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Save game configuration
    server.on("/api/config/game", HTTP_POST, [](AsyncWebServerRequest *request) {},
              NULL,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, data);

        if (error) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }

        gameConfig.godShotWindow = doc["godShotWindow"] | DEFAULT_GODSHOT_WINDOW;
        gameConfig.perfectWindow = doc["perfectWindow"] | DEFAULT_PERFECT_WINDOW;
        gameConfig.startSpeed = doc["startSpeed"] | DEFAULT_START_SPEED;
        gameConfig.speedIncrement = doc["speedIncrement"] | DEFAULT_SPEED_INCREMENT;
        gameConfig.speedMultiplier = doc["speedMultiplier"] | DEFAULT_SPEED_MULTIPLIER;
        gameConfig.maxSpeed = doc["maxSpeed"] | DEFAULT_MAX_SPEED;
        gameConfig.speedProfile = doc["speedProfile"] | DEFAULT_SPEED_PROFILE;
        gameConfig.winningScore = doc["winningScore"] | DEFAULT_WINNING_SCORE;
        gameConfig.timingMode = doc["timingMode"] | DEFAULT_TIMING_MODE;
        gameConfig.animationMode = doc["animationMode"] | DEFAULT_ANIMATION_MODE;

        configManager.saveGameConfig(gameConfig);
        broadcastConfig();

        request->send(200, "text/plain", "OK");
        Serial.println("[WebUI] Game config updated");
    });

    // Save visual configuration
    server.on("/api/config/visual", HTTP_POST, [](AsyncWebServerRequest *request) {},
              NULL,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, data);

        if (error) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }

        visualConfig.idleEffect = doc["idleEffect"] | DEFAULT_IDLE_EFFECT;
        visualConfig.idleSpeed = doc["idleSpeed"] | DEFAULT_IDLE_SPEED;
        visualConfig.globalBrightness = doc["globalBrightness"] | DEFAULT_GLOBAL_BRIGHTNESS;
        visualConfig.scoreDisplayMode = doc["scoreDisplayMode"] | DEFAULT_SCORE_DISPLAY_MODE;

        // Parse hex colors
        String p1Color = doc["player1Color"] | "00FF00";
        String p2Color = doc["player2Color"] | "FF0000";
        String neutralColor = doc["neutralColor"] | "0000FF";

        visualConfig.player1Color = strtoul(p1Color.c_str(), NULL, 16);
        visualConfig.player2Color = strtoul(p2Color.c_str(), NULL, 16);
        visualConfig.neutralColor = strtoul(neutralColor.c_str(), NULL, 16);

        configManager.saveVisualConfig(visualConfig);
        broadcastConfig();

        request->send(200, "text/plain", "OK");
        Serial.println("[WebUI] Visual config updated");
    });

    // Test idle effect
    server.on("/api/test/idle", HTTP_POST, [](AsyncWebServerRequest *request) {
        sendIdleCommand();
        request->send(200, "text/plain", "OK");
    });

    // Get statistics
    server.on("/api/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(1024);

        doc["totalGames"] = stats.totalGames;
        doc["player1Wins"] = stats.player1Wins;
        doc["player2Wins"] = stats.player2Wins;
        doc["godShotCount"] = stats.godShotCount;
        doc["perfectCount"] = stats.perfectCount;
        doc["missCount"] = stats.missCount;
        doc["avgReaction"] = getAverageReactionTime(stats);
        doc["bestReaction"] = stats.bestReactionTime == 0xFFFF ? 0 : stats.bestReactionTime;
        doc["player1GodShots"] = stats.player1GodShots;
        doc["player2GodShots"] = stats.player2GodShots;
        doc["p1AvgReaction"] = getPlayerAverageReaction(stats, PLAYER_LEFT);
        doc["p2AvgReaction"] = getPlayerAverageReaction(stats, PLAYER_RIGHT);

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Reset statistics
    server.on("/api/stats/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        initStatistics(stats);
        configManager.saveStatistics(stats);
        request->send(200, "text/plain", "OK");
        Serial.println("[WebUI] Statistics reset");
    });

    // Get network status
    server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);

        // Buzzers
        JsonObject buzzL = doc.createNestedObject("buzzerLeft");
        buzzL["active"] = buzzerLeft.active;
        buzzL["rssi"] = buzzerLeft.rssi;

        JsonObject buzzR = doc.createNestedObject("buzzerRight");
        buzzR["active"] = buzzerRight.active;
        buzzR["rssi"] = buzzerRight.rssi;

        // Lamps
        JsonArray lampsArray = doc.createNestedArray("lamps");
        for (int i = 0; i < NUM_LAMPS; i++) {
            JsonObject lamp = lampsArray.createNestedObject();
            lamp["index"] = i;
            lamp["active"] = lamps[i].active;
            lamp["rssi"] = lamps[i].rssi;

            // Add MAC address if available
            if (lamps[i].active) {
                char macStr[18];
                sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
                       lamps[i].macAddr[0], lamps[i].macAddr[1], lamps[i].macAddr[2],
                       lamps[i].macAddr[3], lamps[i].macAddr[4], lamps[i].macAddr[5]);
                lamp["mac"] = macStr;
            }
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Get MAC mapping table
    server.on("/api/mac-mapping", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(2048);

        doc["count"] = macMapping.count;
        JsonArray entries = doc.createNestedArray("entries");

        for (uint8_t i = 0; i < macMapping.count; i++) {
            JsonObject entry = entries.createNestedObject();
            char macStr[9];
            sprintf(macStr, "%02X:%02X:%02X",
                   macMapping.entries[i].mac[0],
                   macMapping.entries[i].mac[1],
                   macMapping.entries[i].mac[2]);
            entry["mac"] = macStr;
            entry["lampIndex"] = macMapping.entries[i].lampIndex;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // Add or update MAC mapping
    server.on("/api/mac-mapping", HTTP_POST, [](AsyncWebServerRequest *request) {},
              NULL,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, data);

        if (error) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }

        const char* macStr = doc["mac"];
        uint8_t lampIndex = doc["lampIndex"];

        if (!macStr || lampIndex > LAMP_INDEX_MAX) {
            request->send(400, "text/plain", "Invalid parameters");
            return;
        }

        // Parse MAC address (last 3 bytes)
        uint8_t mac[3];
        if (!MACHelper::parseMAC(macStr, mac, 3)) {
            request->send(400, "text/plain", "Invalid MAC address format");
            return;
        }

        // Add or update mapping
        if (configManager.addMacMapping(macMapping, mac, lampIndex)) {
            request->send(200, "text/plain", "OK");
            Serial.printf("[WebUI] MAC mapping added: %s -> Lamp %d\n", macStr, lampIndex);
        } else {
            request->send(500, "text/plain", "Failed to save mapping");
        }
    });

    // Delete MAC mapping
    server.on("/api/mac-mapping", HTTP_DELETE, [](AsyncWebServerRequest *request) {},
              NULL,
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, data);

        if (error) {
            request->send(400, "text/plain", "Invalid JSON");
            return;
        }

        const char* macStr = doc["mac"];
        if (!macStr) {
            request->send(400, "text/plain", "Missing MAC address");
            return;
        }

        // Parse MAC address (last 3 bytes)
        uint8_t mac[3];
        if (!MACHelper::parseMAC(macStr, mac, 3)) {
            request->send(400, "text/plain", "Invalid MAC address format");
            return;
        }

        // Remove mapping
        if (configManager.removeMacMapping(macMapping, mac)) {
            request->send(200, "text/plain", "OK");
            Serial.printf("[WebUI] MAC mapping removed: %s\n", macStr);
        } else {
            request->send(404, "text/plain", "Mapping not found");
        }
    });

    server.begin();
    Serial.println("[WebServer] Started on port 80");
}

// Call this from main.cpp initWebServer()
void initWebServer() {
    setupWebServer();
}

#endif // WEBSERVER_H
