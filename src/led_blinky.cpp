#include "led_blinky.h"
#include "global.h"
void led_blinky(void *pvParameters){
    pinMode(LED_GPIO, OUTPUT);
  
  while(1) {                        
    float local_temp;
        float local_humid;
        if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                        local_temp = glob_temperature;
                        local_humid = glob_humidity;
                        xSemaphoreGive(xDataMutex);} 
                        else {
                        Serial.println("Task Monitor: Không thể lấy Mutex!");
                    }
    // Critical
if (local_temp > 35 && local_humid > 80) {
    // Cấp 1 - Nghiêm trọng nhất (Nóng & Ẩm)
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
    // Cấp 1 - Nghiêm trọng nhất
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(200));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(200));
}
else if (local_temp < 10 || local_humid < 20) {
    // Cấp 2 - Cảnh báo Cực thấp
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(500));
}
else if (local_temp < 20 && local_humid < 30) {
    // Cấp 3 - Nghiêm trọng Thấp
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(300));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(300));
}
else if (local_temp < 20 || local_humid < 30) {
    // Cấp 4 - Cảnh báo Thấp
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(800));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(800));
}

// --- VÙNG 3 & 4 (Nóng/Khô hoặc Lạnh/Ẩm) ---
else if (local_temp > 30 && local_humid < 30) {
    // Nóng & Khô
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(400));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(400));
}
else if (local_temp < 20 && local_humid > 60) {
    // Lạnh & Ẩm
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(400));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(400));
}

// --- VÙNG 5: LÝ TƯỞNG / BÌNH THƯỜNG ---
else {
    // Nhấp nháy chậm vì điều kiện tốt
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(2000));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(2000));
}
  }
}