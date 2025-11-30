#include "global.h"           
#include "task_webserver.h"
#include <WiFi.h>
#include "main_server_task.h"   
#include "esp32-hal-gpio.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "task_check_info.h"
#include "ArduinoJson.h"
#include "tinyml.h"

void main_server_task(void *pvParameters) {

    // Đợi cho WiFi/AP sẵn sàng khi khởi động
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        float t = 0.0f;
        float h = 0.0f;

        // Biến lưu trạng thái thực tế của đèn (đọc trực tiếp GPIO)
        int blinky_state = 0;
        int neo_state    = 0;

        // Có kết nối WiFi (STA) HOẶC có thiết bị đang kết nối AP
        bool has_connection =
            (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0);

        if (has_connection) {

            // 1. --- LẤY DỮ LIỆU CẢM BIẾN (TASK 3) ---
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                t = glob_temperature;
                h = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("[ServerTask] Không thể lấy xDataMutex để đọc DHT20!");
            }

            // 2. --- LẤY TRẠNG THÁI THIẾT BỊ (LED / NEO) ---
            // Đọc trạng thái điện áp thực tế trên chân GPIO
            blinky_state = digitalRead(48); // LED Blinky
            neo_state    = digitalRead(45); // NeoPixel
            StaticJsonDocument<512> doc;  // đủ nhỏ cho ESP32

            doc["page"] = "telemetry";
            JsonObject value = doc.createNestedObject("value");

            // --- Cảm biến ---
            value["temp"]   = t;
            value["hum"]    = h;
            value["blinky"] = blinky_state;
            value["neo"]    = neo_state;

            // --- TinyML: trạng thái tổng ---
            value["ml_st"]    = (int)current_ml_state;
            value["ml_score"] = current_anomaly_score;
            value["ml_ratio"] = current_anomaly_ratio;

            // --- TinyML: phân loại môi trường (Task 5) ---
            // Chú ý: đảm bảo tinyml.cpp luôn cập nhật các biến này
            value["env_class"] = (int)current_env_class;   // 0 / 1 / 2
            value["env_label"] = current_env_label;        // "LẠNH"/"DỄ CHỊU"/"NÓNG"

            // --- TinyML: lời khuyên hệ thống ---
            value["advice"] = current_advice_msg;

            // Chuyển thành chuỗi JSON để gửi qua WebSocket
            String data;
            serializeJson(doc, data);

            // 4. --- GỬI LÊN WEB ---
            Webserver_sendata(data);
            // Serial.println(data); // bật lên nếu muốn debug JSON
        }

        // 5. --- CHỜ 2 GIÂY MỚI GỬI LẦN SAU ---
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
