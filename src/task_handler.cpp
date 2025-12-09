#include "task_handler.h"
#include "global.h"          
#include <ArduinoJson.h>     
#include "task_check_info.h"  
#include <String.h>
#include "led_blinky.h"
#include "neo_blinky.h"
#include <WiFi.h>

void handleWebSocketMessage(String message, AsyncWebSocket &ws) {
    // Create memory for JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print(F("❌ JSON parse error: "));
        Serial.println(error.c_str());
        return;
    }

    // Get the page name (device or setting)
    String page = doc["page"].as<String>();

    // CASE 1: DEVICE CONTROL (TASK 4)
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
                    // CASE TURN ON: Return to AUTO mode
                    glob_blinky_is_overridden = false;
                    
                    glob_lcd_msg_line1 = "LED Blinky:";
                    glob_lcd_msg_line2 = "Mode: AUTO"; // Display LCD as Auto
                    Serial.println("   -> LED Blinky: Switching to AUTO Mode");
                } else {
                    // CASE TURN OFF: Force OFF (MANUAL OFF)
                    glob_blinky_is_overridden = true;  // Enable override mode
                    glob_blinky_override_state = false; // Set OFF state
                    
                    glob_lcd_msg_line1 = "LED Blinky:";
                    glob_lcd_msg_line2 = "Mode: OFF (WEB)";
                    Serial.println("   -> LED Blinky: Forced OFF");
                }

                xSemaphoreGive(xBlinkyControlMutex);
            }
        }
    
        // --- NEOPIXEL (GPIO 45) ---
        else if (gpio == NEO_PIN) {
            if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            
                if (is_on) {
                    glob_neo_is_overridden = false; 
                    
                    glob_lcd_msg_line1 = "NeoPixel:";
                    glob_lcd_msg_line2 = "Mode: AUTO";
                    Serial.println("   -> NeoPixel: Switching to AUTO Mode");
                } else {
                    // CASE TURN OFF: Force OFF (MANUAL OFF)
                    glob_neo_is_overridden = true;   // Enable override mode
                    glob_neo_override_state = false; // Set OFF state (Black)
                    
                    glob_lcd_msg_line1 = "NeoPixel:";
                    glob_lcd_msg_line2 = "Mode: OFF (WEB)";
                    Serial.println("   -> NeoPixel: Forced OFF");
                }

                xSemaphoreGive(xNeoControlMutex);
            }
        }
        
        // OTHER DEVICES
        else {
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, is_on ? HIGH : LOW);
            glob_lcd_msg_line1 = "GPIO " + String(gpio) + ":";
            glob_lcd_msg_line2 = statusStr + " (MANUAL)";
            Serial.println("   -> Direct GPIO control (Non-RTOS)");
        }
    }
    
    // SAVE WI-FI SETTINGS
    else if (page == "setting") {
        String ssid = doc["value"]["ssid"].as<String>();
        String pass = doc["value"]["password"].as<String>();
        String token = doc["value"]["token"].as<String>();
        String server = doc["value"]["server"].as<String>();
        String port = doc["value"]["port"].as<String>();

        Serial.println("📥 Received new configuration from Web. Saving...");

        // Call the function to save the file (This function is in task_check_info.h)
        Save_info_File(ssid, pass, token, server, port);

        // Send a response back to the Web (if it's before restart)
        ws.textAll("{\"status\":\"ok\",\"page\":\"setting_saved\"}");
    }
    // REQUEST SYSTEM INFO
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
            v["ssid"] = String(SSID_AP);       // Default AP SSID 
        } else if (mode & WIFI_STA) {
            modeStr = "STA";
            ip = WiFi.localIP();
            v["ssid"] = WIFI_SSID;             // Configured SSID
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

    // FORGET WI-FI (DELETE FILE + RESET TO AP)
    else if (page == "forget_wifi") {
        Serial.println("🧹 Received Wi-Fi forget request from Web. Deleting info.dat & resetting to AP...");
        Clear_info_File();

        // Send a response back to the Web (if it's before reset)
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
