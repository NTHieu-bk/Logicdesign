#include "neo_blinky.h"
#include "global.h"

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while(1) {                       
        bool run = true; // flag

        if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_neo_is_overridden) {
                // NẾU CÓ LỆNH GHI ĐÈ:
                run = false; 

                if (glob_neo_override_state) {
                    // Lệnh WEB: BẬT (ON) -> Sáng màu Trắng (Mặc định cho chế độ bật tay)
                    strip.setPixelColor(0, strip.Color(255, 255, 255)); 
                    strip.show();
                } else {
                    // Lệnh WEB: TẮT
                    strip.clear();
                    strip.show();
                }
            } else {
                // Không có lệnh ghi đè -> Chạy tự động
                run = true;
            }
            
            xSemaphoreGive(xNeoControlMutex);
        } else {
             Serial.println("Task Neo: Không thể lấy Mutex Control!");
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
                Serial.println("Task Neo: Không thể lấy Mutex Data!");
            }

            uint32_t color = 0; // Biến lưu màu sắc

            // --- VÙNG 1: NÓNG & ẨM ---
            if (local_temp > 35 && local_humid > 80) {
                color = strip.Color(255, 0, 0);   // Đỏ
            }
            else if (local_temp > 35 || local_humid > 80) {
                color = strip.Color(255, 165, 0); // Cam
            }
            else if (local_temp > 30 && local_humid > 60) {
                color = strip.Color(255, 69, 0);  // Cam-Đỏ
            }
            else if (local_temp > 30 || local_humid > 60) {
                color = strip.Color(255, 255, 0); // Vàng
            }

            // --- VÙNG 2: LẠNH & KHÔ ---
            else if (local_temp < 10 && local_humid < 20) {
                color = strip.Color(0, 0, 255);   // Xanh dương
            }
            else if (local_temp < 10 || local_humid < 20) {
                color = strip.Color(0, 255, 255); // Cyan
            }
            else if (local_temp < 20 && local_humid < 30) {
                color = strip.Color(75, 0, 130);  // Indigo
            }
            else if (local_temp < 20 || local_humid < 30) {
                color = strip.Color(173, 216, 230); // Xanh nhạt
            }

            // --- VÙNG 3: NÓNG & KHÔ ---
            else if (local_temp > 30 && local_humid < 30) {
                color = strip.Color(255, 0, 255); // Magenta
            }

            // --- VÙNG 4: LẠNH & ẨM ---
            else if (local_temp < 20 && local_humid > 60) {
                color = strip.Color(128, 0, 128); // Tím
            }

            // --- VÙNG 5: LÝ TƯỞNG ---
            else {
                color = strip.Color(0, 255, 0);   // Xanh lá
            }

            // Cập nhật màu lên đèn
            strip.setPixelColor(0, color);
            strip.show();
            
            // Delay cho chế độ tự động (500ms)
            vTaskDelay(pdMS_TO_TICKS(500)); 

        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}