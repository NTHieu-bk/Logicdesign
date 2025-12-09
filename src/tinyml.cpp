#include "tinyml.h"
#include "global.h"
#include "dht_anomaly_model2.h"

volatile int   current_ml_state      = 0;  // Current state of the ML model (Normal, Sensor Check, Warning)
volatile float current_anomaly_score = 0.0f;  // Current anomaly score from the model
volatile float current_anomaly_ratio = 0.0f;  // Ratio of anomaly detections
String         current_advice_msg    = "Initializing TinyML...";  // Advice message for the user

volatile int   current_env_class  = 1;  // Environment class (0: Cold, 1: Comfortable, 2: Hot)
String         current_env_label = "COMFORTABLE";  // Human-readable label for the environment state

namespace
{
    // ML States Definitions
    constexpr int ML_STATE_NORMAL       = 0;
    constexpr int ML_STATE_SENSOR_CHECK = 1;
    constexpr int ML_STATE_WARNING      = 2;

    // TensorFlow Lite Model and Interpreter setup
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model            = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input                   = nullptr;
    TfLiteTensor *output                  = nullptr;

    constexpr int kTensorArenaSize = 8 * 1024;  // Memory space for model inference
    uint8_t tensor_arena[kTensorArenaSize];

    // Window size and history configuration for data processing
    constexpr int   WINDOW_SIZE        = 10;
    constexpr int   ANOM_HISTORY_SIZE  = 30;
    constexpr int   ENV_HISTORY_SIZE   = 5;
    constexpr float ANOMALY_THRESHOLD  = 0.7f;  // Anomaly detection threshold

    // Min and Max for sensor values
    constexpr float TEMP_MIN = 0.0f, TEMP_MAX = 100.0f;
    constexpr float HUM_MIN  = 0.0f, HUM_MAX  = 100.0f;

    // Sliding windows for temperature and humidity
    float temp_window[WINDOW_SIZE] = {0};
    float hum_window [WINDOW_SIZE] = {0};
    int   window_idx   = 0;
    int   window_count = 0;
    float sum_temp     = 0.0f;
    float sum_hum      = 0.0f;

    // History of anomalies detected
    bool anomaly_history[ANOM_HISTORY_SIZE] = {false};
    int  anom_history_idx   = 0;
    int  consec_anomalies   = 0;
    long total_samples      = 0;
    long total_anomalies_detected = 0;

    // Classifications for environmental conditions (Cold, Comfortable, Hot)
    enum EnvClass { ENV_COLD, ENV_COMFORT, ENV_HOT };

    // Environment history
    EnvClass env_history[ENV_HISTORY_SIZE];
    int      env_history_idx   = 0;
    EnvClass env_class_internal = ENV_COMFORT;  // Default environment class

    // Preprocess raw data (temperature and humidity) to normalize and filter
    void preprocessData(float raw_temp, float raw_hum,
                        float &norm_temp, float &norm_hum,
                        float &filt_temp, float &filt_hum)
    {
        // Remove old data from the sliding window
        sum_temp -= temp_window[window_idx];
        sum_hum  -= hum_window [window_idx];

        // Add new data to the sliding window
        temp_window[window_idx] = raw_temp;
        hum_window [window_idx] = raw_hum;

        // Update sum of temperatures and humidities
        sum_temp += raw_temp;
        sum_hum  += raw_hum;

        // Increase window count until full window size
        if (window_count < WINDOW_SIZE) window_count++;
        window_idx = (window_idx + 1) % WINDOW_SIZE;

        // Calculate filtered (smoothed) temperature and humidity
        filt_temp = sum_temp / window_count;
        filt_hum  = sum_hum  / window_count;

        // Constrain the filtered values to a valid range
        filt_temp = constrain(filt_temp, TEMP_MIN, TEMP_MAX);
        filt_hum  = constrain(filt_hum , HUM_MIN , HUM_MAX );

        // Normalize the data to [0,1] range for ML model input
        norm_temp = (filt_temp - TEMP_MIN) / (TEMP_MAX - TEMP_MIN);
        norm_hum  = (filt_hum  - HUM_MIN ) / (HUM_MAX  - HUM_MIN );
    }

    // Run inference with the TinyML model
    bool runInference(float norm_temp, float norm_hum, float &out_score)
    {
        if (!interpreter || !input || !output) return false;

        // Set input tensor values
        input->data.f[0] = norm_temp;
        input->data.f[1] = norm_hum;

        // Run the model and check for errors
        if (interpreter->Invoke() != kTfLiteOk) {
            return false;
        }

        // Get the output score from the model
        out_score = output->data.f[0];
        return true;
    }

    // Classify environment based on temperature and humidity
    EnvClass classifyEnvironment(float temp, float hum)
    {
        if (temp < 24.0f) return ENV_COLD;
        if (temp > 28.0f) return ENV_HOT;

        if (hum < 35.0f) return ENV_COLD;
        if (hum > 75.0f) return ENV_HOT;

        return ENV_COMFORT;
    }

    // Update environment classification based on recent history
    void updateEnvironmentClass(EnvClass new_class)
    {
        env_history[env_history_idx] = new_class;
        env_history_idx = (env_history_idx + 1) % ENV_HISTORY_SIZE;

        // Count how many times each environment class has appeared
        int cnt_cold = 0, cnt_comf = 0, cnt_hot = 0;
        for (int i = 0; i < ENV_HISTORY_SIZE; ++i) {
            switch (env_history[i]) {
            case ENV_COLD:    cnt_cold++; break;
            case ENV_COMFORT: cnt_comf++; break;
            case ENV_HOT:     cnt_hot++;  break;
            }
        }

        // Determine the majority class
        int majority_count = max(max(cnt_cold, cnt_comf), cnt_hot);
        EnvClass majority =
            (majority_count == cnt_cold) ? ENV_COLD :
            (majority_count == cnt_comf) ? ENV_COMFORT : ENV_HOT;

        // Update the internal environment class if the majority is strong enough
        if (majority != env_class_internal && majority_count >= 3) {
            env_class_internal = majority;
        }
    }

    // Update anomaly detection state
    void updateAnomalyState(bool is_anomaly_now)
    {
        anomaly_history[anom_history_idx] = is_anomaly_now;
        anom_history_idx = (anom_history_idx + 1) % ANOM_HISTORY_SIZE;

        // Count anomalies in the window
        int total_in_window = 0;
        for (int i = 0; i < ANOM_HISTORY_SIZE; ++i) {
            if (anomaly_history[i]) total_in_window++;
        }

        // Update consecutive anomalies counter
        consec_anomalies = is_anomaly_now ? (consec_anomalies + 1) : 0;

        // Count total samples and anomalies detected
        total_samples++;
        if (is_anomaly_now) total_anomalies_detected++;

        // Update ML state based on anomalies
        if (consec_anomalies >= 5 || total_in_window > 20) {
            current_ml_state = ML_STATE_WARNING;
        } else if (total_in_window > 0 && total_in_window <= 5) {
            current_ml_state = ML_STATE_SENSOR_CHECK;
        } else {
            current_ml_state = ML_STATE_NORMAL;
        }

        // Calculate anomaly ratio
        current_anomaly_ratio = (total_in_window * 100.0f) / ANOM_HISTORY_SIZE;
    }

    // Get advice based on environment and anomaly score
    String getSmartAdvice(float temp, float hum, float ai_score)
    {
        // Provide warnings if anomalies are detected
        if (current_ml_state == ML_STATE_WARNING && ai_score > ANOMALY_THRESHOLD) {
            return "🚨 WARNING: Abnormal environment or sensor, please check the system!";
        }

        if (current_ml_state == ML_STATE_SENSOR_CHECK) {
            return "⚠️ Sensor signal unstable, check wiring or power source.";
        }

        // Provide environment-specific advice
        String advice;

        switch (env_class_internal) {
        case ENV_COLD:
            advice = "❄️ Temperature is low, heating might be needed.";
            break;
        case ENV_HOT:
            advice = "🔥 High temperature, consider using a fan or increasing ventilation.";
            break;
        case ENV_COMFORT:
        default:
            advice = "✅ Comfortable temperature, no action required.";
            break;
        }

        // Add humidity-related advice
        if (hum > 80.0f) {
            advice += " 💧 High humidity, consider dehumidifying or opening windows.";
        } else if (hum < 40.0f) {
            advice += " 🌵 Dry air, consider using a humidifier.";
        }

        return advice;
    }

}

// Initialize TinyML and set up TensorFlow Lite Micro
static void setupTinyML()
{
    Serial.println("[TinyML] Initializing TensorFlow Lite Micro...");

    // Initialize error reporting and model loading
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model2_tflite);  // Load the pre-trained model
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        error_reporter->Report("Model schema version mismatch!");
        return;
    }

    // Initialize interpreter with model and memory arena
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);

    interpreter = &static_interpreter;

    // Allocate tensors for inference
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        error_reporter->Report("AllocateTensors() failed");
        interpreter = nullptr;
        return;
    }

    // Get input and output tensors
    input  = interpreter->input(0);
    output = interpreter->output(0);

    // Initialize state variables and history buffers
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
    current_env_label = "COMFORTABLE";

    Serial.println("[TinyML] Init done.");
}

// Task for running TinyML inference and managing environment state
void tiny_ml_task(void *pvParameters)
{
    setupTinyML();  // Initialize TinyML
    vTaskDelay(pdMS_TO_TICKS(2000));

    while (1) {
        float raw_temp = 0.0f;
        float raw_hum  = 0.0f;

        // Get the current temperature and humidity from global variables
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            raw_temp = glob_temperature;
            raw_hum  = glob_humidity;
            xSemaphoreGive(xDataMutex);
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Preprocess the raw data (normalize and filter)
        float norm_temp = 0.0f, norm_hum = 0.0f;
        float filt_temp = 0.0f, filt_hum = 0.0f;
        preprocessData(raw_temp, raw_hum, norm_temp, norm_hum, filt_temp, filt_hum);

        // Run inference to detect anomalies
        float score = 0.0f;
        if (!runInference(norm_temp, norm_hum, score)) {
            score = 0.0f;
        }
        current_anomaly_score = score;

        // Determine if there is an anomaly
        bool is_anomaly_now = (score > ANOMALY_THRESHOLD);
        updateAnomalyState(is_anomaly_now);

        // If no anomaly, update environment class
        if (!is_anomaly_now) {
            EnvClass inst_class = classifyEnvironment(filt_temp, filt_hum);
            updateEnvironmentClass(inst_class);
        }

        // Update environment class label based on internal state
        switch (env_class_internal) {
        case ENV_COLD:
            current_env_class = 0;
            current_env_label = "COLD";
            break;
        case ENV_COMFORT:
            current_env_class = 1;
            current_env_label = "COMFORTABLE";
            break;
        case ENV_HOT:
            current_env_class = 2;
            current_env_label = "HOT";
            break;
        }

        // Get smart advice based on current conditions
        current_advice_msg = getSmartAdvice(filt_temp, filt_hum, score);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
