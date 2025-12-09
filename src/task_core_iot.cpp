#include "task_core_iot.h"
#include "global.h"

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>

// --- CẤU HÌNH THINGSBOARD / COREIOT ---
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// Hàm connect tới CoreIOT (ThingsBoard)
void CORE_IOT_connect() {
    if (tb.connected()) return;

    Serial.print("[CoreIOT] Connecting to: ");
    Serial.print(CORE_IOT_SERVER);
    Serial.print(":");
    Serial.println(CORE_IOT_PORT);

    if (!tb.connect(CORE_IOT_SERVER.c_str(),
                    CORE_IOT_TOKEN.c_str(),
                    CORE_IOT_PORT.toInt())) {
        Serial.println("[CoreIOT] Connect Failed!");
        return;
    }

    // Gửi vài attribute cơ bản
    tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());

    Serial.println("[CoreIOT] Connected!");
}

// --- TASK CHÍNH (RTOS) ---
void coreiot_task(void *pvParameters) {

    Serial.println("[CoreIOT] Task Started. Waiting for Internet...");

    // 1. Chờ WiFi STA kết nối
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    Serial.println("[CoreIOT] WiFi connected, starting MQTT...");

    unsigned long lastTelemetrySend = 0;

    while (true) {
        // A. Duy trì kết nối MQTT
        if (!tb.connected()) {
            CORE_IOT_connect();
        }

        // B. Xử lý message đến (nếu sau này có dùng RPC/attribute)
        tb.loop();

        // C. Gửi dữ liệu mỗi 10s
        if (millis() - lastTelemetrySend >= 10000UL) {

            float temp = 0.0f;
            float hum  = 0.0f;

            // Lấy dữ liệu từ task monitor một cách an toàn
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                temp = glob_temperature;
                hum  = glob_humidity;
                xSemaphoreGive(xDataMutex);
            }

            // Chỉ gửi khi đã có dữ liệu hợp lý (tuỳ bạn check thêm)
            tb.sendTelemetryData("temperature", temp);
            tb.sendTelemetryData("humidity", hum);

            Serial.printf("[CoreIOT] Sent Telemetry: T=%.1f, H=%.1f\n", temp, hum);

            lastTelemetrySend = millis();
        }

        // Nhường CPU cho task khác
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
