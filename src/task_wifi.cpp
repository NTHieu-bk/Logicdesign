#include "task_wifi.h"
#include <esp_wifi.h>

// Function prototype for WiFi event handler
void WiFiEvent(WiFiEvent_t event);

// Setup WiFi events 
void setupWiFiEvents() {
    WiFi.onEvent(WiFiEvent);  // Register the event handler
}

// WiFi event handler to detect when a device connects or disconnects from the AP
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
            Serial.println("\n--------------------------------------------------");
            Serial.println("⚠️ New Client connected to ESP32 AP!");
            Serial.println("   (Please open your browser)");
            
            // Print the URL again here so you don't miss it
            Serial.print("   👉 AP URL  : http://"); 
            Serial.print(WiFi.softAPIP()); 
            Serial.println("/");
            Serial.println("--------------------------------------------------\n");
            break;

        case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
            Serial.println("👋 Client disconnected from ESP32 AP");
            break;
            
        default:
            break;
    }
}

// Start the ESP32 as an Access Point (AP)
void startAP()
{
    WiFi.mode(WIFI_AP);  // Set WiFi mode to AP
    WiFi.softAP(String(SSID_AP), String(PASS_AP));  // Start AP with SSID and Password
    Serial.println("\n[WiFi] SoftAP is UP (Access Point mode)");
    Serial.print("  AP SSID : "); Serial.println(SSID_AP);
    Serial.print("  AP PASS : "); Serial.println(PASS_AP);
    Serial.print("  AP URL  : http://"); Serial.print(WiFi.softAPIP()); Serial.println("/");
    setupWiFiEvents();  // Register event handler if not done elsewhere
}

// Start the ESP32 as a Station (STA) and connect to WiFi
void startSTA()
{
    if (WIFI_SSID.isEmpty()) {  // Check if SSID is provided
        Serial.println("❌ No SSID to connect to → Enabling AP Mode");
        startAP();  // If no SSID, switch to AP mode
        return;
    }

    WiFi.mode(WIFI_STA);  // Set WiFi mode to STA (Station)
    Serial.println("🔄 Connecting to WiFi...");

    // Try to connect using SSID and Password if available
    if (WIFI_PASS.isEmpty()) {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    unsigned long startAttempt = millis();
    const unsigned long timeout = 10000;  // 10 seconds timeout for connection

    // Attempt to connect to WiFi
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(200 / portTICK_PERIOD_MS);  // Wait a bit before retrying

        if (millis() - startAttempt > timeout) {  // Timeout check
            Serial.println("⛔ WiFi connection failed! Switching back to AP mode.");
            startAP();  // If WiFi connection fails, switch to AP mode
            return;
        }
    }

    Serial.println("✅ Connected to WiFi!");
    Serial.print("📡 IP STA: ");
    Serial.println(WiFi.localIP());  // Print the assigned IP address

    xSemaphoreGive(xBinarySemaphoreInternet);  // Release semaphore for further tasks
}

// Reconnect to WiFi if disconnected
bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED) {  // If already connected, do nothing
        return true;
    }

    Serial.println("⚠️ WiFi disconnected, trying to reconnect...");
    startSTA();  // Try to reconnect to WiFi

    if (WiFi.status() == WL_CONNECTED) {  // Check if connection is successful
        return true;
    } else {
        return false;  // If connection fails, we're back in AP mode
    }
}
