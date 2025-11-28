// ==================== WEBSOCKET ====================
var gateway = `ws://${window.location.hostname}/ws`;
var websocket;
// Biến trạng thái
let isCelsius = true;
let chart;         // Biểu đồ
let gTemp, gHumi;  // Đồng hồ JustGage
let tempHistory = []; 
let humHistory = [];
let relayList = []; // Danh sách thiết bị
let deleteTarget = null;
let reconnectDelay = 2000;     // ms, bắt đầu từ 2s
let reconnectTimer = null;     // id của setTimeout để tránh chồng lấn

// ==================== 2. KHỞI TẠO (INIT) ====================
window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
    initGauges();  // Khởi tạo đồng hồ
    initChart();   // Khởi tạo biểu đồ
    
    const savedTheme = localStorage.getItem('theme') || 'light';
    applyTheme(savedTheme);
    
    // --- KHÔI PHỤC RELAY HOẶC KHỞI TẠO CỐ ĐỊNH (TASK 4) ---
    let savedRelays = localStorage.getItem('myRelays');
    
    if (savedRelays) {
        try {
            relayList = JSON.parse(savedRelays);
        } catch (e) {
            console.error("Lỗi dữ liệu relay, đã reset:", e);
            localStorage.removeItem('myRelays'); 
            relayList = []; 
        }
    }

    // Nếu relayList rỗng (chưa có thiết bị nào), thêm 2 thiết bị cố định
    if (relayList.length === 0) {
        relayList = [
            { id: 1000, name: "LED Blinky", gpio: 48, state: true }, 
            { id: 1001, name: "NeoPixel", gpio: 45, state: true }  
        ];
        localStorage.setItem('myRelays', JSON.stringify(relayList));
    }
    
    renderRelays(); 
    // ------------------------------------------

    const forgetBtn = document.getElementById('btnForgetWifi');
    if (forgetBtn) {
        forgetBtn.addEventListener('click', function () {
            if (!confirm("Bạn có chắc chắn muốn xóa cấu hình Wi-Fi và quay lại AP mode không?")) return;
            Send_Data(JSON.stringify({ page: "forget_wifi" }));
            alert("Đã gửi yêu cầu quên Wi-Fi. ESP32 sẽ khởi động lại trong giây lát.");
        });
    }
}

// ==================== 3. WEBSOCKET LOGIC ====================
function initWebSocket() {
    console.log('Đang kết nối WebSocket...', gateway);
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
    websocket.onerror = function (e) {
        console.error('Lỗi WebSocket:', e);
    };
}

function onOpen(event) {
    console.log('Kết nối WebSocket thành công!');
    document.getElementById("statusText").innerText = "Đã kết nối";
    document.getElementById("connStatus").style.backgroundColor = "#00ff9d"; // Xanh

    const icon = document.getElementById("wifiIcon");
    if (icon) {
        icon.classList.remove('disconnected');
        icon.classList.add('connected');
    }

    // Reset backoff khi kết nối lại được
    reconnectDelay = 2000;
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }

    // Mỗi lần kết nối lại, xin thông tin hệ thống
    requestSysInfo();
}

function onClose(event) {
    console.log('Mất kết nối WebSocket!');
    document.getElementById("statusText").innerText = "Mất kết nối...";
    document.getElementById("connStatus").style.backgroundColor = "#ff4757"; // Đỏ

    const icon = document.getElementById("wifiIcon");
    if (icon) {
        icon.classList.remove('connected');
        icon.classList.add('disconnected');
    }

    // Backoff: 2s → 4s → 8s → tối đa 10s
    if (reconnectTimer) clearTimeout(reconnectTimer);
    reconnectTimer = setTimeout(() => {
        console.log(`Thử kết nối WebSocket lại sau ${reconnectDelay / 1000}s...`);
        initWebSocket();
        reconnectDelay = Math.min(reconnectDelay * 2, 10000);
    }, reconnectDelay);
}


function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Đã gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng!");
    }
}

function requestSysInfo() {
    // Gửi yêu cầu thông tin hệ thống lên ESP
    Send_Data(JSON.stringify({ page: "sysinfo" }));
}


function onMessage(event) {
    console.log("📩 Nhận:", event.data);
    try {
        var msg = JSON.parse(event.data);

        // --- DỮ LIỆU CẢM BIẾN ---
        if (msg.page === "telemetry") {
            // --- A. Cập nhật Đồng hồ & Biểu đồ ---
            const t = parseFloat(msg.value.temp);
            const h = parseFloat(msg.value.hum);
            updateDashboard(t, h);

            // --- B. Cập nhật Trạng thái AI (Màu sắc & Icon) ---
            const ml_st = msg.value.ml_st;       // 0, 1, 2
            const ml_ratio = msg.value.ml_ratio; // %

            const statusText = document.getElementById("ai_status_text");
            const ratioText = document.getElementById("ai_ratio_val");
            const envLabelEl = document.getElementById("ai_env_label");

            if (statusText && ratioText && ml_st !== undefined) {
                ratioText.innerText = parseFloat(ml_ratio).toFixed(1);

                // Xóa hiệu ứng rung cũ (nếu có)
                statusText.parentElement.style.animation = "none";
                statusText.parentElement.offsetHeight; /* trigger reflow */

                switch (parseInt(ml_st)) {
                    case 0: // NORMAL
                        statusText.innerText = "✅ MÔI TRƯỜNG ỔN ĐỊNH";
                        statusText.style.color = "#2ecc71"; // Xanh lá
                        statusText.parentElement.style.borderColor = "#2ecc71";
                        break;
                    
                    case 1: // SENSOR CHECK / MÔI TRƯỜNG CHƯA LÝ TƯỞNG
                        statusText.innerText = "⚠️ KIỂM TRA / ĐIỀU CHỈNH NHẸ";
                        statusText.style.color = "#f1c40f"; // Vàng
                        statusText.parentElement.style.borderColor = "#f1c40f";
                        break;
                    
                    case 2: // WARNING
                        statusText.innerText = "🚨 CẢNH BÁO NGUY HIỂM!";
                        statusText.style.color = "#e74c3c"; // Đỏ
                        statusText.parentElement.style.borderColor = "#e74c3c";
                        // Hiệu ứng rung lắc
                        statusText.parentElement.style.animation = "shake 0.5s infinite"; 
                        break;
                }
            }

            // Nếu backend gửi thêm env_label (VD: "LẠNH", "DỄ CHỊU", "NÓNG")
            if (envLabelEl && msg.value.env_label) {
                envLabelEl.innerText = msg.value.env_label;
            }

            // --- C. Cập nhật Lời khuyên Trợ lý ảo ---
            const adviceEl = document.getElementById("sys-advice");
            if (adviceEl && msg.value.advice) {
                adviceEl.innerHTML = msg.value.advice;
                
                // Đổi màu chữ nếu nội dung có từ "CẢNH BÁO"
                if (msg.value.advice.includes("CẢNH BÁO")) {
                    adviceEl.style.color = "#e74c3c"; // Đỏ
                    adviceEl.style.fontWeight = "900";
                } else {
                    adviceEl.style.color = "#007bff"; // Xanh dương
                    adviceEl.style.fontWeight = "bold";
                }
            }
        }
        // --- THÔNG TIN HỆ THỐNG ---
        else if (msg.page === "sysinfo") {
            const v = msg.value || {};
            document.getElementById('sys-mode').innerText   = v.mode   || '-';
            document.getElementById('sys-ssid').innerText   = v.ssid   || '-';
            document.getElementById('sys-ip').innerText     = v.ip     || '-';
            document.getElementById('sys-status').innerText = 
                v.status === 'connected' ? 'Đã kết nối' : (v.status || 'Không rõ');
        }
        // --- PHẢN HỒI QUÊN WI-FI ---
        else if (msg.page === "forget_wifi") {
            if (msg.status === "ok") {
                alert("ESP32 đã xóa cấu hình Wi-Fi, sẽ khởi động lại vào AP mode.");
            }
        }
        // --- CÁI KHÁC (có thể thêm sau) ---
    } catch (e) {
        console.warn("Lỗi JSON:", e);
    }
}


// ==================== 4. XỬ LÝ HIỂN THỊ (Gauges + Chart) ====================

// Khởi tạo 2 đồng hồ kim (JustGage)
function createTempGauge(min, max, value) {
    // Xóa đồng hồ cũ nếu có
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

// Hàm tạo đồng hồ Độ ẩm
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

// Sửa lại hàm init ban đầu
function initGauges() {
    createTempGauge(0, 100, 0); // Mặc định độ C: 0 - 100
    createHumiGauge();
}

// Khởi tạo biểu đồ đường (Chart.js)
function initChart() {
    const ctx = document.getElementById('sensorChart').getContext('2d');
    
    // Cấu hình màu mặc định (cho Light mode)
    Chart.defaults.color = '#666'; 
    Chart.defaults.borderColor = '#ddd';

    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [], // Thời gian
            datasets: [{
                label: 'Nhiệt độ',
                data: [],
                borderColor: '#e74c3c', // Đỏ
                backgroundColor: 'rgba(231, 76, 60, 0.2)',
                tension: 0.4,
                fill: true
            }, {
                label: 'Độ ẩm',
                data: [],
                borderColor: '#3498db', // Xanh
                backgroundColor: 'rgba(52, 152, 219, 0.2)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false, // Tắt animation để mượt realtime
            interaction: { mode: 'index', intersect: false },
            scales: {
                x: { ticks: { maxTicksLimit: 10 } }, // Giới hạn nhãn trục X
                y: { beginAtZero: true }
            }
        }
    });
}

// Hàm cập nhật toàn bộ giao diện khi có dữ liệu mới
function updateDashboard(tempC, hum) {
    
    // 1. Tính toán logic đơn vị (Độ C / Độ F)
    let rawTemp;
    if (isCelsius) {
        rawTemp = tempC;
    } else {
        rawTemp = (tempC * 1.8) + 32;
    }

    let displayTemp = parseFloat(rawTemp.toFixed(1)); 
    let displayHum = parseFloat(hum.toFixed(1));

    // --- 🛡️ THÊM KIỂM TRA AN TOÀN ---
    if (typeof gTemp !== 'undefined' && typeof gHumi !== 'undefined' && gTemp && gHumi) {
        try {
            gTemp.refresh(displayTemp);
            gHumi.refresh(displayHum);
        } catch (e) { console.warn("Lỗi update Gauge:", e); }
    }

    if (typeof chart !== 'undefined' && chart) {
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
        } catch (e) { console.warn("Lỗi update Chart:", e); }
    }
}

// ==================== 5. CHỨC NĂNG ĐIỀU KHIỂN ====================

// Đổi đơn vị °C / °F
function toggleUnit() {
    // 1. Đảo trạng thái
    isCelsius = !isCelsius;
    
    const btn = document.getElementById('unitBtn');
    const label = document.getElementById('label-temp');
    
    // Lấy giá trị hiện tại của đồng hồ để quy đổi
    let currentVal = parseFloat(gTemp.config.value);

    if (isCelsius) {
        // ================== CHUYỂN VỀ ĐỘ C ==================
        btn.innerText = "Đổi sang °F";
        label.innerText = "🌡️ Nhiệt độ (°C)";
        
        let valC = (currentVal - 32) * 5/9;
        createTempGauge(0, 100, valC.toFixed(1));

        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v - 32) * 5/9);
            chart.update('none');
        }
        
    } else {
        // ================== CHUYỂN SANG ĐỘ F ==================
        btn.innerText = "Đổi sang °C";
        label.innerText = "🌡️ Nhiệt độ (°F)";
        
        let valF = (currentVal * 9/5) + 32;
        createTempGauge(32, 212, valF.toFixed(1));

        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v * 9/5) + 32);
            chart.update('none');
        }
    }
}

// Đổi Theme Sáng / Tối
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
    btn.innerText = theme === 'dark' ? '☀️ Sáng' : '🌙 Tối';

    // Cập nhật màu sắc cho biểu đồ (Chart.js cần set thủ công)
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

// Chuyển Tab (Home/Device/Settings)
function showSection(id, event) {
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');

    const el = document.getElementById(id);
    el.style.display = (id === 'settings' || id === 'home') ? 'block' : 'block';
    if (id === 'settings') el.style.display = 'flex';

    if (id === 'info') {
        requestSysInfo();
    }

    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    if (event) event.currentTarget.classList.add('active');
}


// ==================== 6. QUẢN LÝ THIẾT BỊ (RELAY) ====================
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
        alert("Vui lòng nhập tên và chân GPIO hợp lệ (>= 0)!");
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
        let noteText = '';

        if (r.name.includes("Blinky")) {
            iconHtml = '<i class="fa-solid fa-lightbulb"></i>';
        } 
        else if (r.name.includes("NeoPixel")) {
            iconHtml = '<i class="fa-solid fa-palette"></i>';
        }

        let buttonText = r.state ? 'OFF' : 'ON';
        const buttonClass = `btn-control ${r.state ? 'active' : ''}`;

        card.innerHTML = `
            <div class="device-icon">${iconHtml}</div>
            <h3>${r.name}</h3>
            <p style="color:var(--text-sub); font-size:0.9rem">GPIO: ${r.gpio}</p>

            <p style="font-size:0.8rem; color: var(--primary); margin-top: 10px; margin-bottom: 15px; min-height: 30px;">
                ${noteText}
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

// ==================== 7. XỬ LÝ FORM SETTINGS ====================
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
    alert("✅ Đã gửi cấu hình xuống thiết bị!");
});
