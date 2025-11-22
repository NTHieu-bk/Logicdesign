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

    while(1) {
        float t = 0;
        float h = 0;
        // Biến lưu trạng thái thực tế của đèn
        int blinky_state = 0; 
        int neo_state = 0;

        // Kiểm tra kết nối: Có WiFi (STA) HOẶC Có người đang kết nối vào AP (ESP32 Setup)
        bool has_connection = (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0);

        if (has_connection) {

            // 1. --- LẤY DỮ LIỆU CẢM BIẾN (TASK 3) ---
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                t = glob_temperature;
                h = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("Task Server: Không thể lấy Mutex Data!");
            }

            // 2. --- LẤY TRẠNG THÁI THIẾT BỊ ---
            // Đọc trực tiếp trạng thái điện áp trên chân GPIO để biết đèn đang Sáng hay Tắt
            blinky_state = digitalRead(48); 
            neo_state = digitalRead(45);


            // 3. --- ĐÓNG GÓI JSON ---
            // Format: {"page":"telemetry", "value":{"temp":25.5, "hum":60.2, "blinky":1, "neo":0}}
            String data = "{\"page\":\"telemetry\", \"value\":{";
            data += "\"temp\":" + String(t, 1) + ",";
            data += "\"hum\":" + String(h, 1) + ",";
            data += "\"blinky\":" + String(blinky_state) + ",";
            data += "\"neo\":" + String(neo_state) + ",";
            
            // Phần AI
            data += "\"ml_st\":" + String((int)current_ml_state) + ",";
            data += "\"ml_score\":" + String(current_anomaly_score, 2) + ",";
            data += "\"ml_ratio\":" + String(current_anomaly_ratio, 1) + ","; 
            data += "\"advice\":\"" + current_advice_msg + "\""; 
            data += "}}";

            // 4. --- GỬI LÊN WEB ---
            Webserver_sendata(data);
            // Serial.println(data); // debug xem JSON gửi đi
        }
        
        // 5. --- CHỜ 2 GIÂY MỚI GỬI LẦN SAU ---
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}