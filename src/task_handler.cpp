#include "task_handler.h"
#include "global.h"          
#include <ArduinoJson.h>     
#include "task_check_info.h"  
#include <String.h>
#include "led_blinky.h"
#include "neo_blinky.h"
void handleWebSocketMessage(String message, AsyncWebSocket &ws) {
    // Tạo vùng nhớ JSON 
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print(F("❌ Lỗi parse JSON: "));
        Serial.println(error.c_str());
        return;
    }

    // Lấy tên trang (device hay setting)
    String page = doc["page"].as<String>();


    // TRƯỜNG HỢP 1: ĐIỀU KHIỂN THIẾT BỊ (TASK 4)
    if (page == "device") {
        int gpio = doc["value"]["gpio"].as<int>();
        bool is_on = doc["value"]["status"].as<String>().equalsIgnoreCase("ON");
        glob_last_interaction_time = millis();
        String statusStr = is_on ? "ON " : "OFF";
        Serial.printf("⚙️ WEB CONTROL: GPIO %d -> %s\n", gpio, is_on ? "ON" : "OFF");

        // --- LED BLINKY (GPIO 48) ---
        if (gpio == LED_GPIO) { 
            // "Xin chìa khóa" Mutex để ghi đè an toàn
            if (xSemaphoreTake(xBlinkyControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                glob_lcd_msg_line1 = "LED Blinky:";
                glob_lcd_msg_line2 = statusStr + " (WEB)";
                xSemaphoreGive(xBlinkyControlMutex);    // Trả chìa khóa
                Serial.println("   -> Đã ghi đè Led Blinky");
            } else {
                Serial.println("   -> Lỗi: Không lấy được Mutex Blinky!");
            }
        }
        
        // --- NEOPIXEL (GPIO 45) ---
        else if (gpio == NEO_PIN) {
            if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                glob_lcd_msg_line1 = "NeoPixel:";
                glob_lcd_msg_line2 = statusStr + " (WEB)";
                xSemaphoreGive(xNeoControlMutex);
                Serial.println("   -> Đã ghi đè Led NeoPixel");
            } else {
                Serial.println("   -> Lỗi: Không lấy được Mutex NeoPixel!");
            }
        }
        
        // --- THIẾT BỊ KHÁC (Relay thường - Điều khiển trực tiếp) ---
        else {
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, is_on ? HIGH : LOW);
            glob_lcd_msg_line1 = "GPIO " + String(gpio) + ":";
            glob_lcd_msg_line2 = statusStr + " (MANUAL)";
            Serial.println("   -> Điều khiển GPIO trực tiếp (Non-RTOS)");
        }
    }
    
    // TRƯỜNG HỢP 2: LƯU CÀI ĐẶT WI-FI
    else if (page == "setting") {
        String ssid = doc["value"]["ssid"].as<String>();
        String pass = doc["value"]["password"].as<String>();
        String token = doc["value"]["token"].as<String>();
        String server = doc["value"]["server"].as<String>();
        String port = doc["value"]["port"].as<String>();

        Serial.println("📥 Nhận cấu hình mới từ Web. Đang lưu...");
        
        // Gọi hàm lưu file (Hàm này nằm bên task_check_info.h)
        // Lưu ý: Hàm này sẽ tự gọi ESP.restart() sau khi lưu xong
        Save_info_File(ssid, pass, token, server, port);

        // Gửi phản hồi lại cho Web (Nếu kịp trước khi reset)
        ws.textAll("{\"status\":\"ok\",\"page\":\"setting_saved\"}");
    }
}