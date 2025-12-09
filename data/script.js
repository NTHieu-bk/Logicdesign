//   1. GLOBAL / UI STATE   
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

let isCelsius = true;
let chart;
let gTemp, gHumi;
let relayList = [];
let deleteTarget = null;

let reconnectDelay = 2000;   // backoff start (ms)
let reconnectTimer = null;

//    2. INIT   
window.addEventListener('load', onLoad);

function onLoad() {
    initWebSocket();
    initGauges();
    initChart();

    const savedTheme = localStorage.getItem('theme') || 'light';
    applyTheme(savedTheme);

    // Restore saved relays or create default ones
    let savedRelays = localStorage.getItem('myRelays');
    if (savedRelays) {
        try {
            relayList = JSON.parse(savedRelays);
        } catch (e) {
            console.error("Relay data error, resetting:", e);
            localStorage.removeItem('myRelays');
            relayList = [];
        }
    }
    if (relayList.length === 0) {
        relayList = [
            { id: 1000, name: "LED Blinky", gpio: 48, state: true },
            { id: 1001, name: "NeoPixel",   gpio: 45, state: true }
        ];
        localStorage.setItem('myRelays', JSON.stringify(relayList));
    }
    renderRelays();

    // Forget Wi-Fi button (only makes sense in STA mode, handled in onMessage)
    const forgetBtn = document.getElementById('btnForgetWifi');
    if (forgetBtn) {
        forgetBtn.addEventListener('click', function () {
            if (forgetBtn.disabled) return;
            if (!confirm("Are you sure you want to erase STA Wi-Fi config and reboot to AP mode?")) return;
            Send_Data(JSON.stringify({ page: "forget_wifi" }));
            alert("Request sent. ESP32 will erase Wi-Fi config and restart soon.");
        });
    }
}

//    3. WEBSOCKET   
function initWebSocket() {
    console.log('Connecting WebSocket...', gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
    websocket.onerror = function (e) {
        console.error('WebSocket error:', e);
    };
}

function onOpen() {
    console.log('WebSocket connected!');
    document.getElementById("statusText").innerText = "Connected";
    document.getElementById("connStatus").style.backgroundColor = "#00ff9d";

    const icon = document.getElementById("wifiIcon");
    if (icon) {
        icon.classList.remove('disconnected');
        icon.classList.add('connected');
    }

    // Reset backoff timer
    reconnectDelay = 2000;
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }

    // Ask for system info on every reconnect
    requestSysInfo();
}

function onClose() {
    console.log('WebSocket disconnected!');
    document.getElementById("statusText").innerText = "Disconnected...";
    document.getElementById("connStatus").style.backgroundColor = "#ff4757";

    const icon = document.getElementById("wifiIcon");
    if (icon) {
        icon.classList.remove('connected');
        icon.classList.add('disconnected');
    }

    // Exponential backoff reconnect: 2 → 4 → 8 → max 10s
    if (reconnectTimer) clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(() => {
        console.log(`Reconnecting WebSocket in ${reconnectDelay / 1000}s...`);
        initWebSocket();
        reconnectDelay = Math.min(reconnectDelay * 2, 10000);
    }, reconnectDelay);
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Sent:", data);
    } else {
        console.warn("⚠️ WebSocket is not ready!");
    }
}

function requestSysInfo() {
    Send_Data(JSON.stringify({ page: "sysinfo" }));
}

//    4. HANDLE INCOMING MESSAGES   
function onMessage(event) {
    console.log("📩 Received:", event.data);
    try {
        var msg = JSON.parse(event.data);

        // Telemetry: sensor + TinyML status
        if (msg.page === "telemetry") {
            const t = parseFloat(msg.value.temp);
            const h = parseFloat(msg.value.hum);
            updateDashboard(t, h);

            const ml_st    = msg.value.ml_st;
            const ml_ratio = msg.value.ml_ratio;
            const statusText = document.getElementById("ai_status_text");
            const ratioText  = document.getElementById("ai_ratio_val");
            const envLabelEl = document.getElementById("ai_env_label");

            if (statusText && ratioText && ml_st !== undefined) {
                ratioText.innerText = parseFloat(ml_ratio).toFixed(1);

                // reset shake animation
                statusText.parentElement.style.animation = "none";
                statusText.parentElement.offsetHeight;

                switch (parseInt(ml_st)) {
                    case 0: // normal
                        statusText.innerText = "✅ ENVIRONMENT NORMAL";
                        statusText.style.color = "#2ecc71";
                        statusText.parentElement.style.borderColor = "#2ecc71";
                        break;
                    case 1: // light adjustment / sensor check
                        statusText.innerText = "⚠️ CHECK / SMALL ADJUSTMENT";
                        statusText.style.color = "#f1c40f";
                        statusText.parentElement.style.borderColor = "#f1c40f";
                        break;
                    case 2: // warning
                        statusText.innerText = "🚨 DANGEROUS ENVIRONMENT!";
                        statusText.style.color = "#e74c3c";
                        statusText.parentElement.style.borderColor = "#e74c3c";
                        statusText.parentElement.style.animation = "shake 0.5s infinite";
                        break;
                }
            }

            if (envLabelEl && msg.value.env_label) {
                envLabelEl.innerText = msg.value.env_label;
            }

            const adviceEl = document.getElementById("sys-advice");
            if (adviceEl && msg.value.advice) {
                adviceEl.innerHTML = msg.value.advice;

                // If advice contains "WARNING", highlight red
                if (msg.value.advice.toUpperCase().includes("WARNING")) {
                    adviceEl.style.color = "#e74c3c";
                    adviceEl.style.fontWeight = "900";
                } else {
                    adviceEl.style.color = "#007bff";
                    adviceEl.style.fontWeight = "bold";
                }
            }
        }

        // System info card
        else if (msg.page === "sysinfo") {
            const v = msg.value || {};
            document.getElementById('sys-mode').innerText = v.mode   || '-';
            document.getElementById('sys-ssid').innerText = v.ssid   || '-';
            document.getElementById('sys-ip').innerText   = v.ip     || '-';

            let statusStr = '-';
            if (v.status === 'connected') statusStr = 'Connected';
            else if (v.status === 'disconnected') statusStr = 'Disconnected';
            else if (v.status) statusStr = v.status;
            document.getElementById('sys-status').innerText = statusStr;

            // Handle Forget Wi-Fi button UX:
            const forgetBtn = document.getElementById('btnForgetWifi');
            const infoNote  = document.getElementById('info-note');
            
            if (forgetBtn) {
                if (v.mode === 'STA') {
                    // SHOW button if in STA mode
                    forgetBtn.style.display = 'block'; 
                    forgetBtn.disabled = false;
                    
                    if (infoNote) {
                        infoNote.innerHTML =
                          'Note: When you click <b>Forget Wi-Fi</b>, ESP32 will erase the saved STA Wi-Fi, ' +
                          'restart and fall back to the default AP. You must connect to the AP again to reconfigure.';
                    }
                } else {
                    // ❌ HIDE button if in AP mode (Change from disabled to display: none)
                    forgetBtn.style.display = 'none';
                    
                    if (infoNote) {
                        infoNote.innerHTML =
                          'Note: ESP32 is currently running in AP-only mode. ' +
                          'There is no saved STA Wi-Fi configuration to forget.';
                    }
                }
            }
        }

        // Forget Wi-Fi response
        else if (msg.page === "forget_wifi") {
            if (msg.status === "ok") {
                alert("ESP32 erased STA Wi-Fi config and will restart in AP mode.");
            }
        }

    } catch (e) {
        console.warn("JSON parse error:", e);
    }
}

//    5. GAUGES + CHART   
function createTempGauge(min, max, value) {
    document.getElementById("gauge_temp").innerHTML = "";
    gTemp = new JustGage({
        id: "gauge_temp",
        value: value,
        min: min,
        max: max,
        title: " ",
        label: " ",
        gaugeWidthScale: 0.6,
        counter: true,
        relativeGaugeSize: true,
        decimals: 1,
        valueFontColor: "#e74c3c",
        levelColors: ["#3498db", "#f1c40f", "#e74c3c"]
    });
}

function createHumiGauge() {
    gHumi = new JustGage({
        id: "gauge_humi",
        value: 0,
        min: 0,
        max: 100,
        title: " ",
        label: " ",
        gaugeWidthScale: 0.6,
        counter: true,
        relativeGaugeSize: true,
        decimals: 1,
        valueFontColor: "#3498db",
        levelColors: ["#2ecc71"]
    });
}

function initGauges() {
    createTempGauge(0, 100, 0);
    createHumiGauge();
}

function initChart() {
    const ctx = document.getElementById('sensorChart').getContext('2d');

    Chart.defaults.color = '#666';
    Chart.defaults.borderColor = '#ddd';

    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperature',
                data: [],
                borderColor: '#e74c3c',
                backgroundColor: 'rgba(231, 76, 60, 0.2)',
                tension: 0.4,
                fill: true
            }, {
                label: 'Humidity',
                data: [],
                borderColor: '#3498db',
                backgroundColor: 'rgba(52, 152, 219, 0.2)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            interaction: { mode: 'index', intersect: false },
            scales: {
                x: { ticks: { maxTicksLimit: 10 } },
                y: { beginAtZero: true }
            }
        }
    });
}

function updateDashboard(tempC, hum) {
    let rawTemp = isCelsius ? tempC : (tempC * 1.8) + 32;
    let displayTemp = parseFloat(rawTemp.toFixed(1));
    let displayHum  = parseFloat(hum.toFixed(1));

    if (gTemp && gHumi) {
        try {
            gTemp.refresh(displayTemp);
            gHumi.refresh(displayHum);
        } catch (e) { console.warn("Gauge update error:", e); }
    }

    if (chart) {
        try {
            const now = new Date().toLocaleTimeString();
            chart.data.labels.push(now);
            chart.data.datasets[0].data.push(displayTemp);
            chart.data.datasets[1].data.push(displayHum);

            if (chart.data.labels.length > 20) {
                chart.data.labels.shift();
                chart.data.datasets.forEach(ds => ds.data.shift());
            }
            chart.update('none');
        } catch (e) { console.warn("Chart update error:", e); }
    }
}

//    6. CONTROLS   
function toggleUnit() {
    isCelsius = !isCelsius;

    const btn   = document.getElementById('unitBtn');
    const label = document.getElementById('label-temp');
    let currentVal = parseFloat(gTemp.config.value);

    if (isCelsius) {
        btn.innerText = "Switch to °F";
        label.innerText = "🌡️ Temperature (°C)";
        let valC = (currentVal - 32) * 5 / 9;
        createTempGauge(0, 100, valC.toFixed(1));
        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v - 32) * 5 / 9);
            chart.update('none');
        }
    } else {
        btn.innerText = "Switch to °C";
        label.innerText = "🌡️ Temperature (°F)";
        let valF = (currentVal * 9 / 5) + 32;
        createTempGauge(32, 212, valF.toFixed(1));
        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v * 9 / 5) + 32);
            chart.update('none');
        }
    }
}

function toggleTheme() {
    const html = document.documentElement;
    const current = html.getAttribute('data-theme');
    const newTheme = current === 'dark' ? 'light' : 'dark';
    applyTheme(newTheme);
}

function applyTheme(theme) {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);

    const btn = document.getElementById('themeBtn');
    btn.innerText = theme === 'dark' ? '☀️ Light' : '🌙 Dark';

    if (chart) {
        const isDark = theme === 'dark';
        const textColor = isDark ? '#e0e0e0' : '#666';
        const gridColor = isDark ? '#444' : '#ddd';

        chart.options.scales.x.ticks.color = textColor;
        chart.options.scales.y.ticks.color = textColor;
        chart.options.scales.x.grid.color = gridColor;
        chart.options.scales.y.grid.color = gridColor;
        chart.options.plugins.legend.labels.color = textColor;
        chart.update('none');
    }
}

// Switch section (Home / Device / Info / Settings)
function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');

    const el = document.getElementById(id);
    el.style.display = (id === 'settings') ? 'flex' : 'block';

    if (id === 'info') {
        requestSysInfo();
    }

    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    if (event) event.currentTarget.classList.add('active');
}

//    7. RELAY MANAGEMENT   
function openAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'flex';
}
function closeAddRelayDialog() {
    document.getElementById('addRelayDialog').style.display = 'none';
}

function saveRelay() {
    const name = document.getElementById('relayName').value.trim();
    const gpioVal = document.getElementById('relayGPIO').value.trim();
    const gpio = parseInt(gpioVal);

    if (!name || isNaN(gpio) || gpio < 0 || gpioVal === "") {
        alert("Please enter a valid relay name and GPIO (>= 0).");
        return;
    }

    relayList.push({
        id: Date.now(),
        name: name,
        gpio: gpio,
        state: false
    });

    localStorage.setItem('myRelays', JSON.stringify(relayList));
    renderRelays();
    closeAddRelayDialog();

    document.getElementById('relayName').value = "";
    document.getElementById('relayGPIO').value = "";
}

function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = "";

    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';

        let iconHtml = '<i class="fa-solid fa-bolt"></i>';
        if (r.name.includes("Blinky")) {
            iconHtml = '<i class="fa-solid fa-lightbulb"></i>';
        } else if (r.name.includes("NeoPixel")) {
            iconHtml = '<i class="fa-solid fa-palette"></i>';
        }

        let buttonText = r.state ? 'OFF' : 'ON';
        const buttonClass = `btn-control ${r.state ? 'active' : ''}`;

        card.innerHTML = `
            <div class="device-icon">${iconHtml}</div>
            <h3>${r.name}</h3>
            <p style="color:var(--text-sub); font-size:0.9rem">GPIO: ${r.gpio}</p>

            <p style="font-size:0.8rem; color: var(--primary); margin-top: 10px; margin-bottom: 15px; min-height: 30px;">
            </p>

            <button class="${buttonClass}" onclick="toggleRelay(${r.id})">
                ${buttonText}
            </button>

            <i class="fa-solid fa-trash delete-icon" onclick="showDeleteDialog(${r.id})"></i>
        `;

        container.appendChild(card);
    });
}

function toggleRelay(id) {
    const relay = relayList.find(r => r.id === id);
    if (relay) {
        relay.state = !relay.state;
        renderRelays();

        const msg = {
            page: "device",
            value: {
                gpio: parseInt(relay.gpio),
                status: relay.state ? "ON" : "OFF"
            }
        };
        Send_Data(JSON.stringify(msg));
    }
}

function showDeleteDialog(id) {
    deleteTarget = id;
    document.getElementById('confirmDeleteDialog').style.display = 'flex';
}
function closeConfirmDelete() {
    document.getElementById('confirmDeleteDialog').style.display = 'none';
}
function confirmDelete() {
    if (deleteTarget) {
        relayList = relayList.filter(r => r.id !== deleteTarget);
        localStorage.setItem('myRelays', JSON.stringify(relayList));
        renderRelays();
    }
    closeConfirmDelete();
}

//    8. SETTINGS FORM   
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    const ssid = document.getElementById("ssid").value.trim();
    const password = document.getElementById("password").value.trim();
    const token = document.getElementById("token").value.trim();
    const server = document.getElementById("server").value.trim();
    const port = document.getElementById("port").value.trim();

    const settingsJSON = JSON.stringify({
        page: "setting",
        value: {
            ssid: ssid,
            password: password,
            token: token,
            server: server,
            port: port
        }
    });

    Send_Data(settingsJSON);
    alert("✅ Configuration sent to device!");
});
