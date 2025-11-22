#include "task_handler.h"
#include "global.h"          
#include <ArduinoJson.h>     
#include "task_check_info.h"  
#include <String.h>
#include "led_blinky.h"
#include "neo_blinky.h"
#include <WiFi.h>
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
        if (xSemaphoreTake(xBlinkyControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            if (is_on) {
                // TRƯỜNG HỢP BẬT: Trả về chế độ TỰ ĐỘNG (AUTO)
                glob_blinky_is_overridden = false; // <--- QUAN TRỌNG: Bỏ ghi đè
                
                glob_lcd_msg_line1 = "LED Blinky:";
                glob_lcd_msg_line2 = "Mode: AUTO"; // Hiển thị LCD là Auto
                Serial.println("   -> LED Blinky: Chuyển sang AUTO Mode");
            } else {
                // TRƯỜNG HỢP TẮT: Cưỡng chế TẮT (MANUAL OFF)
                glob_blinky_is_overridden = true;  // Bật chế độ ghi đè
                glob_blinky_override_state = false; // Gán trạng thái TẮT
                
                glob_lcd_msg_line1 = "LED Blinky:";
                glob_lcd_msg_line2 = "Mode: OFF (WEB)";
                Serial.println("   -> LED Blinky: Cưỡng chế TẮT");
            }

            xSemaphoreGive(xBlinkyControlMutex);
        }
    }
    
    // --- NEOPIXEL (GPIO 45) ---
    else if (gpio == NEO_PIN) {
        if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
            if (is_on) {
                // TRƯỜNG HỢP BẬT: Trả về chế độ TỰ ĐỘNG (AUTO)
                glob_neo_is_overridden = false; // <--- QUAN TRỌNG: Bỏ ghi đè
                
                glob_lcd_msg_line1 = "NeoPixel:";
                glob_lcd_msg_line2 = "Mode: AUTO";
                Serial.println("   -> NeoPixel: Chuyển sang AUTO Mode");
            } else {
                // TRƯỜNG HỢP TẮT: Cưỡng chế TẮT (MANUAL OFF)
                glob_neo_is_overridden = true;   // Bật chế độ ghi đè
                glob_neo_override_state = false; // Gán trạng thái TẮT (Đen)
                
                glob_lcd_msg_line1 = "NeoPixel:";
                glob_lcd_msg_line2 = "Mode: OFF (WEB)";
                Serial.println("   -> NeoPixel: Cưỡng chế TẮT");
            }

            xSemaphoreGive(xNeoControlMutex);
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
    // TRƯỜNG HỢP 3: YÊU CẦU THÔNG TIN HỆ THỐNG
else if (page == "sysinfo") {
    DynamicJsonDocument resp(256);
    resp["page"] = "sysinfo";
    JsonObject v = resp.createNestedObject("value");

    wifi_mode_t mode = WiFi.getMode();
    String modeStr;
    IPAddress ip;

    if ((mode & WIFI_AP) && !(mode & WIFI_STA)) {
        modeStr = "AP";
        ip = WiFi.softAPIP();
        v["ssid"] = String(SSID_AP);       // SSID AP mặc định (trong global.h / task_wifi.h)
    } else if (mode & WIFI_STA) {
        modeStr = "STA";
        ip = WiFi.localIP();
        v["ssid"] = WIFI_SSID;             // SSID đã cấu hình
    } else {
        modeStr = "OFF";
        ip = IPAddress(0, 0, 0, 0);
        v["ssid"] = "";
    }

    v["mode"] = modeStr;
    v["ip"]   = ip.toString();
    v["status"] = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";

    String out;
    serializeJson(resp, out);
    ws.textAll(out);
}

// TRƯỜNG HỢP 4: QUÊN WI-FI (XÓA FILE + RESTART VỀ AP)
else if (page == "forget_wifi") {
    Serial.println("🧹 Nhận yêu cầu quên Wi-Fi từ Web. Đang xóa info.dat & reset về AP...");
    Clear_info_File();

    // Thông báo lại cho Web (nếu kịp)
    DynamicJsonDocument resp(128);
    resp["page"] = "forget_wifi";
    resp["status"] = "ok";
    String out;
    serializeJson(resp, out);
    ws.textAll(out);

    delay(200);
    ESP.restart();
    }
}