#include "global.h"           
#include "task_webserver.h"
#include <WiFi.h>
#include "main_server_task.h"   
void main_server_task(void *pvParameters) {
    
    // Đợi một chút cho WiFi/AP sẵn sàng
    vTaskDelay(pdMS_TO_TICKS(5000));

    while(1) {
        float t;
        float h;
        bool has_connection = (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0);

        // Chỉ gửi data nếu có WiFi (STA) hoặc có client kết nối (AP)
        if (has_connection) {

            // --- YÊU CẦU SEMAPHORE (TASK 4) ---
            // "Lấy chìa khóa" để ĐỌC dữ liệu an toàn
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                
                // Đọc từ biến toàn cục
                t = glob_temperature;
                h = glob_humidity;
                
                // "Trả chìa khóa"
                xSemaphoreGive(xDataMutex);

            } else {
                Serial.println("Task Web Telemetry: Không thể lấy Mutex!");
            }


            String data = "{\"page\":\"telemetry\", \"value\":{\"temp\":" + String(t, 1) + ", \"hum\":" + String(h, 1) + "}}";

            // --- Gửi (Publish) lên Web ---
            // Gọi hàm từ file task_webserver.cpp
            Webserver_sendata(data);
        }
        
        // Đợi 5 giây cho lần gửi tiếp theo
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}