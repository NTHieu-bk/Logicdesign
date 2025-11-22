#include "task_wifi.h"

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    if (WIFI_SSID.isEmpty()) {
        Serial.println("❌ Không có SSID để kết nối → dừng task STA");
        vTaskDelete(NULL);
    }

    WiFi.mode(WIFI_STA);
    Serial.println("🔄 Đang kết nối WiFi...");

    if (WIFI_PASS.isEmpty()) {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    unsigned long startAttempt = millis();
    const unsigned long timeout = 10000; // 10 giây

    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(200 / portTICK_PERIOD_MS);

        if (millis() - startAttempt > timeout) {
            Serial.println("⛔ Kết nối WiFi thất bại! Quay lại AP mode.");
            startAP();
            return;
        }
    }

    Serial.println("✅ Đã kết nối WiFi!");
    Serial.print("📡 IP STA: ");
    Serial.println(WiFi.localIP());
    //Give a semaphore here
    xSemaphoreGive(xBinarySemaphoreInternet);
}

bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED)
        return true;

    Serial.println("⚠️ WiFi mất kết nối, đang thử lại...");
    startSTA();
    return WiFi.status() == WL_CONNECTED;
}
