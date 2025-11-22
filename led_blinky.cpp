#include "led_blinky.h"
#include "global.h"
void led_blinky(void *pvParameters){
    pinMode(LED_GPIO, OUTPUT);
  
  while(1) {                        
    bool run = true; // flag

        // Xin "chìa khóa" để đọc biến 
        if (xSemaphoreTake(xBlinkyControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_blinky_is_overridden) {
                digitalWrite(LED_GPIO, glob_blinky_override_state ? HIGH : LOW);
                run = false; 
            } else {
                run = true;
            }
            xSemaphoreGive(xBlinkyControlMutex); // Trả "chìa khóa"
        }

        if (run) {
            
            float local_temp = 0;
            float local_humid = 0;
            
            // Lấy dữ liệu cảm biến
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                local_temp = glob_temperature;
                local_humid = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("Task Monitor: Không thể lấy Mutex!");
            }

            // --- BẮT ĐẦU LOGIC NHÁY ĐÈN ---
            
            // Cấp 1 - Nghiêm trọng nhất (Nóng & Ẩm)
            if (local_temp > 35 && local_humid > 80) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            else if (local_temp > 35 || local_humid > 80) {
                // Cấp 2 - Cảnh báo Cực cao
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else if (local_temp > 30 && local_humid > 60) {
                // Cấp 3 - Nghiêm trọng Cao
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            else if (local_temp > 30 || local_humid > 60) {
                // Cấp 4 - Cảnh báo Cao
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            // --- VÙNG 2: LẠNH & KHÔ ---
            else if (local_temp < 10 && local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            else if (local_temp < 10 || local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else if (local_temp < 20 && local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            else if (local_temp < 20 || local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            // --- VÙNG 3 & 4 ---
            else if (local_temp > 30 && local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(400));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(400));
            }
            else if (local_temp < 20 && local_humid > 60) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(400));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(400));
            }
            // --- VÙNG 5: LÝ TƯỞNG ---
            else {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(2000));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        } 
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}