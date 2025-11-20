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

    while (1) {
        // 1. LUÔN ĐỌC CẢM BIẾN
        if (dht20.read() == DHT20_OK) {
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                glob_temperature = dht20.getTemperature();
                glob_humidity = dht20.getHumidity();
                xSemaphoreGive(xDataMutex);
            }
        }

        // 2. LOGIC HIỂN THỊ
        // Ý 4: Thời gian hiện thông báo điều khiển ngắn thôi (500ms)
        if (millis() - glob_last_interaction_time < 500) {
            
            // ---> HIỂN THỊ CỤ THỂ THIẾT BỊ VỪA BẤM <---
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(glob_lcd_msg_line1); // Ví dụ: "LED Blinky:"
            
            lcd.setCursor(0, 1);
            lcd.print(glob_lcd_msg_line2); // Ví dụ: "OFF (WEB)"
            
            // Chỉ delay ngắn đúng bằng thời gian còn lại
            vTaskDelay(pdMS_TO_TICKS(100)); 

        } else {
            
            // ---> HIỂN THỊ MÔI TRƯỜNG (Mặc định) <---
            float t = glob_temperature;
            float h = glob_humidity;
            String stT = getStatus_Temp(t);
            String stH = getStatus_Humi(h);

            lcd.clear(); // Xóa sạch để tránh lỗi hiển thị

            // Dòng 0: Nhiệt độ + Trạng thái (Ý 3: Đã thêm lại chữ NORM)
            lcd.setCursor(0, 0);
            lcd.print("T:"); lcd.print(t, 1); lcd.print("C ");
            lcd.print(stT); // Hiện: NORM, WARN, CRIT

            // Dòng 1: Độ ẩm + Trạng thái
            lcd.setCursor(0, 1);
            lcd.print("H:"); lcd.print(h, 1); lcd.print("% ");
            lcd.print(stH);

            // Ý 1: Logic nháy đèn nền khi CRIT
            if (stT == "CRIT" || stH == "CRIT") {
                // Nháy nhanh 2 lần rồi cập nhật lại
                for(int i=0; i<2; i++) {
                    lcd.noBacklight(); vTaskDelay(pdMS_TO_TICKS(250));
                    lcd.backlight();   vTaskDelay(pdMS_TO_TICKS(250));
                    
                    // Kiểm tra ngắt: Nếu có người bấm nút lúc đang nháy thì thoát ngay
                    if (millis() - glob_last_interaction_time < 500) break;
                }
            } else {
                // Nếu bình thường, chỉ cần delay 1 giây chờ lần cập nhật sau
                lcd.backlight(); // Đảm bảo đèn luôn sáng
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }
    }
}