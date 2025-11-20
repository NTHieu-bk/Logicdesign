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

// ==================== 2. KHỞI TẠO (INIT) ====================
window.addEventListener('load', onLoad);

function onLoad(event) {
    initWebSocket();
    //initGauges();  // Khởi tạo đồng hồ
    //initChart();   // Khởi tạo biểu đồ
    
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
            { id: 1000, name: "LED Blinky (Task 1)", gpio: 48, state: true }, 
            { id: 1001, name: "NeoPixel (Task 2)", gpio: 45, state: true }  
        ];
        localStorage.setItem('myRelays', JSON.stringify(relayList));
    }
    
    // Luôn gọi renderRelays để vẽ giao diện (dù là khôi phục hay khởi tạo mới)
    renderRelays(); 
}

// ==================== 3. WEBSOCKET LOGIC ====================
function initWebSocket() {
    console.log('Đang kết nối WebSocket...');
    websocket = new WebSocket(gateway);
    websocket.onopen = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Kết nối thành công!');
    document.getElementById("statusText").innerText = "Đã kết nối";
    document.getElementById("connStatus").style.backgroundColor = "#00ff9d"; // Xanh
}

function onClose(event) {
    console.log('Mất kết nối!');
    document.getElementById("statusText").innerText = "Mất kết nối...";
    document.getElementById("connStatus").style.backgroundColor = "#ff4757"; // Đỏ
    setTimeout(initWebSocket, 2000); // Thử lại sau 2s
}

function Send_Data(data) {
    if (websocket && websocket.readyState === WebSocket.OPEN) {
        websocket.send(data);
        console.log("📤 Đã gửi:", data);
    } else {
        console.warn("⚠️ WebSocket chưa sẵn sàng!");
    }
}

function onMessage(event) {
     console.log("📩 Nhận:", event.data);
    try {
        var msg = JSON.parse(event.data);

        // --- XỬ LÝ DỮ LIỆU CẢM BIẾN ---
        if (msg.page === "telemetry") {
            const t = parseFloat(msg.value.temp);
            const h = parseFloat(msg.value.hum);
            updateDashboard(t, h);
        }
        
        // --- XỬ LÝ TRẠNG THÁI THIẾT BỊ (Nếu ESP32 gửi về) ---
        // Ví dụ: Cập nhật trạng thái nút bấm nếu điều khiển từ nơi khác
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

    // --- 🛡️ THÊM KIỂM TRA AN TOÀN (FIX LỖI CRASH) ---
    
    // Chỉ cập nhật Đồng hồ nếu biến gTemp và gHumi ĐÃ TỒN TẠI
    if (typeof gTemp !== 'undefined' && typeof gHumi !== 'undefined' && gTemp && gHumi) {
        try {
            gTemp.refresh(displayTemp);
            gHumi.refresh(displayHum);
        } catch (e) { console.warn("Lỗi update Gauge:", e); }
    }

    // Chỉ cập nhật Biểu đồ nếu biến chart ĐÃ TỒN TẠI
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
        
        // 1. Cập nhật Đồng hồ (F -> C)
        let valC = (currentVal - 32) * 5/9;
        createTempGauge(0, 100, valC.toFixed(1));

        // 2. Cập nhật Biểu đồ (Quy đổi từng điểm dữ liệu F -> C)
        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v - 32) * 5/9);
            chart.update('none'); // Cập nhật ngay lập tức, không hiệu ứng
        }
        
    } else {
        // ================== CHUYỂN SANG ĐỘ F ==================
        btn.innerText = "Đổi sang °C";
        label.innerText = "🌡️ Nhiệt độ (°F)";
        
        // 1. Cập nhật Đồng hồ (C -> F)
        let valF = (currentVal * 9/5) + 32;
        createTempGauge(32, 212, valF.toFixed(1));

        // 2. Cập nhật Biểu đồ (Quy đổi từng điểm dữ liệu C -> F)
        if (chart) {
            chart.data.datasets[0].data = chart.data.datasets[0].data.map(v => (v * 9/5) + 32);
            chart.update('none'); // Cập nhật ngay lập tức
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
    // Ẩn tất cả section
    document.querySelectorAll('.section').forEach(sec => sec.style.display = 'none');
    // Hiện section được chọn
    const el = document.getElementById(id);
    // Settings dùng flex để căn giữa, còn lại block
    el.style.display = (id === 'settings' || id === 'home') ? 'block' : 'block';
    if(id === 'settings') el.style.display = 'flex'; // Căn giữa cho form settings

    // Cập nhật active class cho menu
    document.querySelectorAll('.nav-item').forEach(i => i.classList.remove('active'));
    if(event) event.currentTarget.classList.add('active');
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
    // Chuyển GPIO sang số nguyên để xử lý
    const gpioVal = document.getElementById('relayGPIO').value.trim();
    const gpio = parseInt(gpioVal); 
    
    if (!name || isNaN(gpio) || gpio < 0 || gpioVal === "") {
        alert("Vui lòng nhập tên và chân GPIO hợp lệ (>= 0)!");
        return;
    }

    //Thêm vào danh sách
    relayList.push({ 
        id: Date.now(), 
        name: name, 
        gpio: gpio, // Lưu dưới dạng số
        state: false 
    });

    //Lưu vào bộ nhớ trình duyệt
    localStorage.setItem('myRelays', JSON.stringify(relayList));

    //Cập nhật giao diện
    renderRelays();
    closeAddRelayDialog();
    
    //Reset form
    document.getElementById('relayName').value = "";
    document.getElementById('relayGPIO').value = "";
}

function renderRelays() {
    const container = document.getElementById('relayContainer');
    container.innerHTML = ""; // Xóa cũ

    relayList.forEach(r => {
        const card = document.createElement('div');
        card.className = 'device-card';
        
        // Xác định icon và chú thích dựa trên tên thiết bị
        let iconHtml = '<i class="fa-solid fa-bolt"></i>';
        let noteText = '';
        let buttonText = r.state ? 'TẮT GHI ĐÈ' : 'BẬT GHI ĐÈ';

        if (r.name.includes("Blinky")) {
            iconHtml = '<i class="fa-solid fa-lightbulb"></i>';
            noteText = 'Điều khiển này sẽ **ghi đè** logic nháy theo Nhiệt độ (Task 1).';
        } else if (r.name.includes("NeoPixel")) {
            iconHtml = '<i class="fa-solid fa-palette"></i>';
            noteText = 'Điều khiển này sẽ **ghi đè** logic màu theo Độ ẩm (Task 2).';
        }
        
        // Trong trường hợp thiết bị đã được bật/ON, chúng ta có thể làm cho nút nổi bật hơn
        const buttonClass = `toggle-btn ${r.state ? 'on' : ''}`;
        
        card.innerHTML = `
            <div class="device-icon">${iconHtml}</div>
            <h3>${r.name}</h3>
            <p style="color:var(--text-sub); font-size:0.9rem">GPIO: ${r.gpio}</p>
            
            <p style="font-size:0.8rem; color: var(--primary); margin-top: 10px; margin-bottom: 15px;">
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
        relay.state = !relay.state; // Đảo trạng thái
        renderRelays(); // Vẽ lại giao diện

        // Gửi lệnh xuống ESP32 qua WebSocket
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
        //Lọc bỏ thiết bị cần xóa
        relayList = relayList.filter(r => r.id !== deleteTarget);
        
        //Cập nhật lại bộ nhớ trình duyệt

        localStorage.setItem('myRelays', JSON.stringify(relayList));
        
        //Cập nhật giao diện
        renderRelays();
    }
    closeConfirmDelete();
}

// ==================== 7. XỬ LÝ FORM SETTINGS ====================
document.getElementById("settingsForm").addEventListener("submit", function (e) {
    e.preventDefault();

    // Lấy giá trị từ form
    const ssid = document.getElementById("ssid").value.trim();
    const password = document.getElementById("password").value.trim();
    const token = document.getElementById("token").value.trim();
    const server = document.getElementById("server").value.trim();
    const port = document.getElementById("port").value.trim();

    // Đóng gói JSON
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

    // Gửi đi
    Send_Data(settingsJSON);
    alert("✅ Đã gửi cấu hình xuống thiết bị!");
});
/*
// ==================== CHẾ ĐỘ TEST (SIMULATION) ====================
let simInterval = null;

function toggleSimulation() {
    const btn = document.getElementById('simBtn');
    
    if (simInterval) {
        // --- ĐANG CHẠY -> DỪNG LẠI ---
        clearInterval(simInterval);
        simInterval = null;
        btn.innerText = "▶️ Chạy thử";
        btn.style.background = "#2ecc71"; // Xanh lá
        console.log("⏹️ Đã dừng mô phỏng");
    } else {
        // --- ĐANG DỪNG -> BẮT ĐẦU CHẠY ---
        btn.innerText = "⏹️ Dừng";
        btn.style.background = "#e74c3c"; // Đỏ
        console.log("▶️ Bắt đầu mô phỏng dữ liệu...");

        simInterval = setInterval(() => {
            // 1. Random Nhiệt độ (từ 28 đến 35 độ C)
            let randomTemp = Math.random() * (35 - 28) + 28;
            
            // 2. Random Độ ẩm (từ 60 đến 90 %)
            let randomHum = Math.random() * (90 - 60) + 60;

            // 3. Gọi hàm cập nhật giao diện (Giả vờ như ESP32 gửi lên)
            // Lưu ý: Hàm updateDashboard luôn nhận đầu vào là ĐỘ C
            updateDashboard(randomTemp, randomHum);

        }, 2000); // Cập nhật mỗi 2 giây
    }
}
    */