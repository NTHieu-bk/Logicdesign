#include "global.h"           
#include "task_webserver.h"
#include <WiFi.h>
#include "main_server_task.h"   
#include "esp32-hal-gpio.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "task_check_info.h"
#include "ArduinoJson.h"
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

            // 2. --- LẤY TRẠNG THÁI THIẾT BỊ (TASK 4 bổ sung) ---
            // Đọc trực tiếp trạng thái điện áp trên chân GPIO để biết đèn đang Sáng hay Tắt
            // (Dù là do Task 1 tự động hay do Web ghi đè, hàm này đều đọc được kết quả cuối cùng)
            blinky_state = digitalRead(48); 
            neo_state = digitalRead(45);


            // 3. --- ĐÓNG GÓI JSON ---
            // Format: {"page":"telemetry", "value":{"temp":25.5, "hum":60.2, "blinky":1, "neo":0}}
            String data = "{\"page\":\"telemetry\", \"value\":{";
            data += "\"temp\":" + String(t, 1) + ",";
            data += "\"hum\":" + String(h, 1) + ",";
            data += "\"blinky\":" + String(blinky_state) + ","; // Thêm trạng thái Blinky
            data += "\"neo\":" + String(neo_state);             // Thêm trạng thái NeoPixel
            data += "}}";

            // 4. --- GỬI LÊN WEB ---
            Webserver_sendata(data);
            // Serial.println(data); // Bật dòng này nếu muốn debug xem JSON gửi đi
        }
        
        // Đợi 2 giây (giảm xuống để web cập nhật trạng thái đèn nhanh hơn)
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}