#include "tinyml.h"
#include "global.h"
#include "dht_anomaly_model2.h"  
// ============================================================
// PHẦN 1: KHAI BÁO BIẾN TOÀN CỤC & TENSORFLOW OBJECTS
// ============================================================

// Biến trạng thái để Web Server đọc (được định nghĩa extern trong global.h)
MLSystemState current_ml_state = ML_STATE_NORMAL;
float current_anomaly_score = 0.0f;
float current_anomaly_ratio = 0.0f;
String current_advice_msg = "Đang khởi tạo...";
namespace {
    // --- Cấu hình TensorFlow Lite Micro ---
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    
    // Vùng nhớ cho TFLite (Tinh chỉnh kích thước nếu bị lỗi bộ nhớ)
    constexpr int kTensorArenaSize = 8 * 1024; 
    uint8_t tensor_arena[kTensorArenaSize];

    // --- Cấu hình Logic & Lọc nhiễu ---
    constexpr int   WINDOW_SIZE       = 10;    // Kích thước cửa sổ lọc trung bình
    constexpr int   HISTORY_SIZE      = 30;    // Lịch sử phán đoán (30 lần đo)
    constexpr float ANOMALY_THRESHOLD = 0.7f; // Ngưỡng điểm số (>0.7 là bất thường)
    
    // Giới hạn chuẩn hóa (Min-Max Normalization)
    // Cần khớp với lúc bạn train model Python
    constexpr float TEMP_MIN = 0.0f;
    constexpr float TEMP_MAX = 80.0f;
    constexpr float HUM_MIN  = 0.0f;
    constexpr float HUM_MAX  = 100.0f;

    // Biến cho bộ lọc trung bình trượt (Moving Average)
    static float temp_window[WINDOW_SIZE] = {0.0f};
    static float hum_window[WINDOW_SIZE]  = {0.0f};
    static int   window_idx    = 0;
    static float sum_temp      = 0.0f;
    static float sum_hum       = 0.0f;

    // Biến cho Logic Máy trạng thái (State Machine)
    bool prediction_history[HISTORY_SIZE] = {false}; 
    int  history_idx = 0;
    int  consec_anomalies = 0; // Đếm số lỗi liên tiếp
    
    // Biến thống kê
    long total_samples = 0;
    long total_anomalies_detected = 0;
}

// PHẦN 2: HÀM HỖ TRỢ TFLITE VÀ XỬ LÝ DỮ LIỆU

void setupTinyML() {
    Serial.println("Initializing TensorFlow Lite Micro...");

    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    // 1. Load Model TFLite
    model = tflite::GetModel(dht_anomaly_model2_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        error_reporter->Report("Model version mismatch!");
        return;
    }

    // 2. Khởi tạo Interpreter
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    // 3. Cấp phát bộ nhớ Tensor
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    // 4. Lấy con trỏ Input/Output
    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("✅ TFLite Init Success!");
}

// Tiền xử lý: Lọc nhiễu + Chuẩn hóa dữ liệu về [0, 1]
static void preprocessData(float raw_temp, float raw_hum, float &norm_temp, float &norm_hum) {
    // A. Moving Average (Lọc nhiễu gai)
    sum_temp -= temp_window[window_idx];
    sum_hum  -= hum_window[window_idx];
    
    temp_window[window_idx] = raw_temp;
    hum_window[window_idx]  = raw_hum;
    
    sum_temp += raw_temp;
    sum_hum  += raw_hum;
    
    window_idx = (window_idx + 1) % WINDOW_SIZE;

    float filt_temp = sum_temp / WINDOW_SIZE;
    float filt_hum  = sum_hum  / WINDOW_SIZE;

    // B. Min-Max Normalization
    norm_temp = (filt_temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
    norm_hum  = (filt_hum  - HUM_MIN)  / (HUM_MAX  - HUM_MIN);

    // Kẹp giá trị [0, 1]
    if (norm_temp < 0.0f) norm_temp = 0.0f; if (norm_temp > 1.0f) norm_temp = 1.0f;
    if (norm_hum  < 0.0f) norm_hum  = 0.0f; if (norm_hum  > 1.0f) norm_hum  = 1.0f;
}

//  Suy luận AI
static bool runInference(float norm_temp, float norm_hum, float &out_score) {
    input->data.f[0] = norm_temp;
    input->data.f[1] = norm_hum;

    if (interpreter->Invoke() != kTfLiteOk) {
        return false;
    }

    out_score = output->data.f[0];
    return true;
}


// PHẦN 3: STATE MACHINE - CẬP NHẬT TRẠNG THÁI HỆ THỐNG DỰA TRÊN KẾT QUẢ AI

// Logic 1: Đưa ra lời khuyên hành động
// Trong file tinyml.cpp

String getSmartAdvice(float temp, float hum, float ai_score) {
    // Báo cháy (AI Score cao) 
    if (ai_score > ANOMALY_THRESHOLD) {
        return "🚨 CẢNH BÁO: NGHI CÓ CHÁY!<br>KIỂM TRA NGAY!";
    }

    String advice = "";
    bool has_msg = false; // Cờ đánh dấu xem đã có dòng nào chưa

    // --- Kiểm tra Nhiệt độ ---
    if (temp > 32.0) {
        advice += "🔥 Nóng quá! Bật quạt/AC.";
        has_msg = true;
    } 
    else if (temp < 18.0) {
        advice += "❄️ Lạnh! Bật sưởi/Mặc ấm.";
        has_msg = true;
    }

    // --- Kiểm tra Độ ẩm ---
    if (hum > 80.0) {
        if (has_msg) advice += "<br>"; // Nếu ở trên có chữ rồi thì xuống dòng
        advice += "💧 Ẩm ướt! Bật hút ẩm/Dry.";
        has_msg = true;
    } 
    else if (hum < 40.0) {
        if (has_msg) advice += "<br>"; // Xuống dòng
        advice += "🌵 Khô hanh! Bật phun sương.";
        has_msg = true;
    }

    // --- Kết luận ---
    if (advice == "") {
        return "✅ Môi trường lý tưởng.";
    }
    
    return advice;
}

// Logic 2: Máy trạng thái (State Machine) để chốt đèn báo
static void updateStateLogic(bool is_anomaly_now) {
    // Cập nhật lịch sử
    prediction_history[history_idx] = is_anomaly_now;
    history_idx = (history_idx + 1) % HISTORY_SIZE;

    // Đếm lỗi trong cửa sổ 30 mẫu
    int total_in_window = 0;
    for(int i=0; i<HISTORY_SIZE; i++) {
        if(prediction_history[i]) total_in_window++;
    }

    // Đếm lỗi liên tiếp
    if (is_anomaly_now) consec_anomalies++;
    else consec_anomalies = 0;

    // --- RA QUYẾT ĐỊNH ---
    // TH1: Cháy thật (Liên tiếp >= 5 lần HOẶC Tổng > 20/30 lần)
    if (consec_anomalies >= 5 || total_in_window > 20) {
        current_ml_state = ML_STATE_WARNING; 
    }
    // TH2: Nghi vấn lỗi Sensor (Lác đác < 5 lần)
    else if (total_in_window > 0 && total_in_window <= 5) {
        current_ml_state = ML_STATE_SENSOR_CHECK;
    }
    // TH3: Bình thường
    else {
        current_ml_state = ML_STATE_NORMAL;
    }
    
    // Cập nhật tỷ lệ thống kê
    current_anomaly_ratio = (float)total_in_window / HISTORY_SIZE * 100.0f;
}

// PHẦN 4: TINYML

void tiny_ml_task(void *pvParameters) {
    setupTinyML();
    vTaskDelay(pdMS_TO_TICKS(2000)); // Chờ ổn định

    while (1) {
        // 1. Lấy dữ liệu thô
        float raw_temp = glob_temperature;
        float raw_hum  = glob_humidity;

        // 2. Tiền xử lý
        float norm_temp, norm_hum;
        preprocessData(raw_temp, raw_hum, norm_temp, norm_hum);

        // 3. Chạy AI
        float score = 0.0f;
        if (runInference(norm_temp, norm_hum, score)) {
            current_anomaly_score = score;
        } else {
            score = 0.0f; // AI lỗi thì coi như 0
        }

        // 4. Logic Trợ lý (Cập nhật lời khuyên)
        current_advice_msg = getSmartAdvice(raw_temp, raw_hum, score);

        // 5. Logic Trạng thái (Cập nhật đèn báo)
        bool is_anomaly = (score > ANOMALY_THRESHOLD);
        if (is_anomaly) total_anomalies_detected++;
        total_samples++;

        updateStateLogic(is_anomaly);

        // 6. Nghỉ 2 giây (Đồng bộ hệ thống)
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}