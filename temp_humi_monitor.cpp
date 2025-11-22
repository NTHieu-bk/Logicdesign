#include "temp_humi_monitor.h"
#include "global.h"
#include "DHT20.h"
#include "LiquidCrystal_I2C.h"
#include "Wire.h"
#include "led_blinky.h"
#include "neo_blinky.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33, 16, 2);

// --- CÁC HÀM PHỤ TRỢ (LOGIC CŨ) ---
bool isCritical(float temperature, float humidity) {
    return (temperature > 35.0 || humidity > 85.0 || temperature < 15.0 || humidity < 20.0);
}

bool isWarning(float temperature, float humidity) {
    return (temperature > 30.0 || humidity > 70.0 || temperature < 20.0 || humidity < 30.0);
}

String getStatus_Temp(float temp) {
    if (temp > 35.0 || temp < 15.0) return "CRIT";
    else if (temp > 30.0 || temp < 20.0) return "WARN";
    else return "NORM";
}

String getStatus_Humi(float humi) {
    if (humi > 85.0 || humi < 20.0) return "CRIT";
    else if (humi > 70.0 || humi < 30.0) return "WARN";
    else return "NORM";
}

void temp_humi_monitor(void *pvParameters) {
    Wire.begin(11, 12);
    Serial.begin(115200);
    dht20.begin();
    lcd.begin();
    lcd.backlight();
    static unsigned long last_sensor_read = 0;
    while (1) {
        // ĐỌC CẢM BIẾN
        if (millis() - last_sensor_read > 2000) {
            if (dht20.read() == DHT20_OK) {
                if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    glob_temperature = dht20.getTemperature();
                    glob_humidity = dht20.getHumidity();
                    xSemaphoreGive(xDataMutex);
                }
            }
            last_sensor_read = millis();
        }

        // HIỂN THỊ
        unsigned long current_time = millis();
        if (current_time - glob_last_interaction_time < 600) { 
            
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(glob_lcd_msg_line1); // Ví dụ: "LED Blinky:"
            
            lcd.setCursor(0, 1);
            lcd.print(glob_lcd_msg_line2); // Ví dụ: "ON (WEB)"
            
            // QUAN TRỌNG: Delay cứng 500ms để giữ dòng chữ này trên màn hình
            // Sau 500ms này, vòng lặp quay lại thì (time - last) đã > 600 -> Tự nhảy xuống else
            vTaskDelay(pdMS_TO_TICKS(500)); 

        } 
        // --- ƯU TIÊN 2: HIỂN THỊ NHIỆT ĐỘ / ĐỘ ẨM (Mặc định) ---
        else {
            
            float t = glob_temperature;
            float h = glob_humidity;
            String stT = getStatus_Temp(t);
            String stH = getStatus_Humi(h);

            lcd.clear(); 

            // Dòng 0: Nhiệt độ
            lcd.setCursor(0, 0);
            lcd.print("T:"); lcd.print(t, 1); lcd.print("C ");
            lcd.print(stT); 

            // Dòng 1: Độ ẩm
            lcd.setCursor(0, 1);
            lcd.print("H:"); lcd.print(h, 1); lcd.print("% ");
            lcd.print(stH);

            // Logic nháy đèn nền khi CRIT
            if (stT == "CRIT" || stH == "CRIT") {
                // Nháy cảnh báo (Chu kỳ ngắn hơn để phản hồi nhanh)
                for(int i=0; i<4; i++) { // Nháy 4 lần nhanh (tổng ~1 giây)
                    lcd.noBacklight(); vTaskDelay(pdMS_TO_TICKS(150));
                    lcd.backlight();   vTaskDelay(pdMS_TO_TICKS(150));
                    
                    // Kiểm tra ngắt ngay lập tức nếu có lệnh Web chen ngang
                    if (millis() - glob_last_interaction_time < 600) break;
                }
            } else {
                // Trạng thái bình thường: Delay 2 giây
                lcd.backlight(); 
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
    }
}