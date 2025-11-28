#include "tinyml.h"
#include "global.h"
#include "dht_anomaly_model2.h"

 
// GLOBAL PUBLIC VARIABLES FOR WEB SERVER
 
volatile int   current_ml_state      = 0;                // 0/1/2
volatile float current_anomaly_score = 0.0f;             // điểm anomaly hiện tại
volatile float current_anomaly_ratio = 0.0f;             // % anomaly trong cửa sổ
String         current_advice_msg    = "Initializing TinyML...";

volatile int   current_env_class  = 1;                   // 0=COLD,1=COMFORT,2=HOT
String         current_env_label = "DỄ CHỊU";

namespace
{
    // ================== 1. CONSTANT / CẤU HÌNH ==================

    constexpr int ML_STATE_NORMAL       = 0;
    constexpr int ML_STATE_SENSOR_CHECK = 1;
    constexpr int ML_STATE_WARNING      = 2;

    // TFLite Micro
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model            = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input                   = nullptr;
    TfLiteTensor *output                  = nullptr;

    constexpr int kTensorArenaSize = 8 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    // Tham số filter & history
    constexpr int   WINDOW_SIZE        = 10;
    constexpr int   ANOM_HISTORY_SIZE  = 30;
    constexpr int   ENV_HISTORY_SIZE   = 5;
    constexpr float ANOMALY_THRESHOLD  = 0.7f;

    constexpr float TEMP_MIN = 0.0f, TEMP_MAX = 100.0f;
    constexpr float HUM_MIN  = 0.0f, HUM_MAX  = 100.0f;

    float temp_window[WINDOW_SIZE] = {0};
    float hum_window [WINDOW_SIZE] = {0};
    int   window_idx   = 0;
    int   window_count = 0;
    float sum_temp     = 0.0f;
    float sum_hum      = 0.0f;

    bool anomaly_history[ANOM_HISTORY_SIZE] = {false};
    int  anom_history_idx   = 0;
    int  consec_anomalies   = 0;
    long total_samples      = 0;
    long total_anomalies_detected = 0;

    // Phân loại môi trường nội bộ (dùng enum cho dễ đọc)
    enum EnvClass { ENV_COLD, ENV_COMFORT, ENV_HOT };

    EnvClass env_history[ENV_HISTORY_SIZE];       // lịch sử các class gần đây
    int      env_history_idx   = 0;
    EnvClass env_class_internal = ENV_COMFORT;    // class nội bộ, tránh trùng tên với biến global


    // ================== 2. HÀM PHỤ TRỢ ==================

    // Moving-average + normalize vào [0,1]
    void preprocessData(float raw_temp, float raw_hum,
                        float &norm_temp, float &norm_hum,
                        float &filt_temp, float &filt_hum)
    {
        // Cập nhật cửa sổ trượt
        sum_temp -= temp_window[window_idx];
        sum_hum  -= hum_window [window_idx];

        temp_window[window_idx] = raw_temp;
        hum_window [window_idx] = raw_hum;

        sum_temp += raw_temp;
        sum_hum  += raw_hum;

        if (window_count < WINDOW_SIZE) window_count++;
        window_idx = (window_idx + 1) % WINDOW_SIZE;

        // Giá trị trung bình lọc
        filt_temp = sum_temp / window_count;
        filt_hum  = sum_hum  / window_count;

        // Giới hạn
        filt_temp = constrain(filt_temp, TEMP_MIN, TEMP_MAX);
        filt_hum  = constrain(filt_hum , HUM_MIN , HUM_MAX );

        // Chuẩn hoá 0–1 để feed cho model
        norm_temp = (filt_temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
        norm_hum  = (filt_hum  - HUM_MIN ) / (HUM_MAX  - HUM_MIN );
    }

    // Chạy suy luận TinyML
    bool runInference(float norm_temp, float norm_hum, float &out_score)
    {
        if (!interpreter || !input || !output) return false;

        input->data.f[0] = norm_temp;
        input->data.f[1] = norm_hum;

        if (interpreter->Invoke() != kTfLiteOk) {
            return false;
        }

        out_score = output->data.f[0];   // model 1 output: anomaly score
        return true;
    }

    // Phân loại môi trường dựa trên ngưỡng temp/hum
    EnvClass classifyEnvironment(float temp, float hum)
    {
        // temp thấp hoặc hum quá thấp -> COLD/DRY
        if (temp < 24.0f) return ENV_COLD;
        if (temp > 28.0f) return ENV_HOT;

        if (hum < 35.0f) return ENV_COLD;
        if (hum > 75.0f) return ENV_HOT;

        return ENV_COMFORT;
    }

    // Cập nhật class môi trường bằng đa số từ lịch sử
    void updateEnvironmentClass(EnvClass new_class)
    {
        env_history[env_history_idx] = new_class;
        env_history_idx = (env_history_idx + 1) % ENV_HISTORY_SIZE;

        int cnt_cold = 0, cnt_comf = 0, cnt_hot = 0;
        for (int i = 0; i < ENV_HISTORY_SIZE; ++i) {
            switch (env_history[i]) {
            case ENV_COLD:    cnt_cold++; break;
            case ENV_COMFORT: cnt_comf++; break;
            case ENV_HOT:     cnt_hot++;  break;
            }
        }

        int majority_count = max(max(cnt_cold, cnt_comf), cnt_hot);
        EnvClass majority =
            (majority_count == cnt_cold) ? ENV_COLD :
            (majority_count == cnt_comf) ? ENV_COMFORT : ENV_HOT;

        // Chỉ đổi nếu đa số >= 3 mẫu để tránh jitter
        if (majority != env_class_internal && majority_count >= 3) {
            env_class_internal = majority;
        }
    }

    // Cập nhật trạng thái anomaly (ml_state + ratio)
    void updateAnomalyState(bool is_anomaly_now)
    {
        anomaly_history[anom_history_idx] = is_anomaly_now;
        anom_history_idx = (anom_history_idx + 1) % ANOM_HISTORY_SIZE;

        int total_in_window = 0;
        for (int i = 0; i < ANOM_HISTORY_SIZE; ++i) {
            if (anomaly_history[i]) total_in_window++;
        }

        consec_anomalies = is_anomaly_now ? (consec_anomalies + 1) : 0;

        total_samples++;
        if (is_anomaly_now) total_anomalies_detected++;

        // Rule cho 3 mức ml_state
        if (consec_anomalies >= 5 || total_in_window > 20) {
            current_ml_state = ML_STATE_WARNING;
        } else if (total_in_window > 0 && total_in_window <= 5) {
            current_ml_state = ML_STATE_SENSOR_CHECK;
        } else {
            current_ml_state = ML_STATE_NORMAL;
        }

        current_anomaly_ratio = (total_in_window * 100.0f) / ANOM_HISTORY_SIZE;
    }

    // Sinh lời khuyên tuỳ theo trạng thái
    String getSmartAdvice(float temp, float hum, float ai_score)
    {
        // Ưu tiên cảnh báo nặng
        if (current_ml_state == ML_STATE_WARNING && ai_score > ANOMALY_THRESHOLD) {
            return "🚨 CẢNH BÁO: Môi trường hoặc cảm biến bất thường, hãy kiểm tra hệ thống!";
        }

        if (current_ml_state == ML_STATE_SENSOR_CHECK) {
            return "⚠️ Tín hiệu cảm biến chưa ổn định, nên kiểm tra dây nối hoặc nguồn.";
        }

        // Nếu ML state bình thường → dựa trên env_class_internal + humidity
        String advice;

        switch (env_class_internal) {
        case ENV_COLD:
            advice = "❄️ Nhiệt độ hơi thấp, có thể cần sưởi nếu kéo dài.";
            break;
        case ENV_HOT:
            advice = "🔥 Nhiệt độ cao, nên mở quạt hoặc tăng thông gió.";
            break;
        case ENV_COMFORT:
        default:
            advice = "✅ Nhiệt độ dễ chịu, không cần hành động.";
            break;
        }

        if (hum > 80.0f) {
            advice += " 💧 Độ ẩm cao, cân nhắc dùng hút ẩm hoặc mở cửa sổ.";
        } else if (hum < 40.0f) {
            advice += " 🌵 Không khí khô, có thể dùng máy phun sương.";
        }

        return advice;
    }

} // namespace  (kết thúc phần helper nội bộ)


// ================== HÀM KHỞI TẠO TFLITE MICRO ==================

static void setupTinyML()
{
    Serial.println("[TinyML] Initializing TensorFlow Lite Micro...");

    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model2_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        error_reporter->Report("Model schema version mismatch!");
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        error_reporter->Report("AllocateTensors() failed");
        interpreter = nullptr;
        return;
    }

    input  = interpreter->input(0);
    output = interpreter->output(0);

    // Reset toàn bộ buffer / lịch sử
    memset(temp_window, 0, sizeof(temp_window));
    memset(hum_window , 0, sizeof(hum_window));
    window_idx = window_count = 0;
    sum_temp = sum_hum = 0.0f;

    memset(anomaly_history, 0, sizeof(anomaly_history));
    anom_history_idx = consec_anomalies = 0;
    total_samples = total_anomalies_detected = 0;

    for (int i = 0; i < ENV_HISTORY_SIZE; ++i) {
        env_history[i] = ENV_COMFORT;
    }
    env_history_idx    = 0;
    env_class_internal = ENV_COMFORT;

    current_ml_state      = ML_STATE_NORMAL;
    current_anomaly_score = 0.0f;
    current_anomaly_ratio = 0.0f;
    current_advice_msg    = "TinyML ready.";

    current_env_class  = 1;
    current_env_label = "DỄ CHỊU";

    Serial.println("[TinyML] Init done.");
}


// ================== FREE RTOS TASK CHẠY TINYML ==================

void tiny_ml_task(void *pvParameters)
{
    setupTinyML();
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float raw_temp = 0.0f;
        float raw_hum  = 0.0f;

        // Đọc dữ liệu từ task DHT20 (đã bảo vệ bằng mutex)
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            raw_temp = glob_temperature;
            raw_hum  = glob_humidity;
            xSemaphoreGive(xDataMutex);
        } else {
            // Không lấy được dữ liệu thì đợi thêm cho nhẹ CPU
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // 1. Tiền xử lý
        float norm_temp = 0.0f, norm_hum = 0.0f;
        float filt_temp = 0.0f, filt_hum = 0.0f;
        preprocessData(raw_temp, raw_hum, norm_temp, norm_hum, filt_temp, filt_hum);

        // 2. Chạy TinyML anomaly
        float score = 0.0f;
        if (!runInference(norm_temp, norm_hum, score)) {
            score = 0.0f;
        }
        current_anomaly_score = score;

        bool is_anomaly_now = (score > ANOMALY_THRESHOLD);
        updateAnomalyState(is_anomaly_now);

        // 3. Nếu không phải anomaly nặng thì mới phân loại môi trường
        if (!is_anomaly_now) {
            EnvClass inst_class = classifyEnvironment(filt_temp, filt_hum);
            updateEnvironmentClass(inst_class);
        }

        // 4. Map class nội bộ → biến public cho web
        switch (env_class_internal) {
        case ENV_COLD:
            current_env_class = 0;
            current_env_label = "LẠNH";
            break;
        case ENV_COMFORT:
            current_env_class = 1;
            current_env_label = "DỄ CHỊU";
            break;
        case ENV_HOT:
            current_env_class = 2;
            current_env_label = "NÓNG";
            break;
        }

        // 5. Sinh lời khuyên hiển thị lên web
        current_advice_msg = getSmartAdvice(filt_temp, filt_hum, score);

        // 6. Chu kỳ 2 giây
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}