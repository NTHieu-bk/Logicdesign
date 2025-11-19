#include "neo_blinky.h"
#include "global.h"

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

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

uint32_t color; 

// --- VÙNG 1: NÓNG & ẨM (Ưu tiên từ cao xuống thấp) ---
// (Cấp 1 - Nghiêm trọng nhất &&)
if (local_temp > 35 && local_humid > 80) {
    color = strip.Color(255, 0, 0);   // Đỏ (Nghiêm trọng nhất)
}
// (Cấp 2 - Cảnh báo Cực cao ||)
else if (local_temp > 35 || local_humid > 80) {
    color = strip.Color(255, 165, 0); // Cam (Cảnh báo Cực cao)
}
// (Cấp 3 - Nghiêm trọng Cao &&)
else if (local_temp > 30 && local_humid > 60) {
    color = strip.Color(255, 69, 0);   // Cam-Đỏ (Nghiêm trọng Cao)
}
// (Cấp 4 - Cảnh báo Cao ||)
else if (local_temp > 30 || local_humid > 60) {
    color = strip.Color(255, 255, 0); // Vàng (Cảnh báo Cao)
}

// --- VÙNG 2: LẠNH & KHÔ (Ưu tiên từ thấp xuống) ---
// (Cấp 1 - Nghiêm trọng nhất &&)
else if (local_temp < 10 && local_humid < 20) {
    color = strip.Color(0, 0, 255);   // Xanh dương (Nghiêm trọng nhất)
}
// (Cấp 2 - Cảnh báo Cực thấp ||)
else if (local_temp < 10 || local_humid < 20) {
    color = strip.Color(0, 255, 255); // Cyan (Cảnh báo Cực thấp)
}
// (Cấp 3 - Nghiêm trọng Thấp &&)
else if (local_temp < 20 && local_humid < 30) {
    color = strip.Color(75, 0, 130);   // Indigo / Tím đậm (Nghiêm trọng Thấp)
}
// (Cấp 4 - Cảnh báo Thấp ||)
else if (local_temp < 20 || local_humid < 30) {
    color = strip.Color(173, 216, 230); // Xanh nhạt (Light Blue) (Cảnh báo Thấp)
}

// --- VÙNG 3: NÓNG & KHÔ (Gộp các điều kiện '&&') ---
// Bao gồm (T>35, H<20), (T>35, H<30), (T>30, H<20), (T>30, H<30)
else if (local_temp > 30 && local_humid < 30) {
    color = strip.Color(255, 0, 255); // Magenta (Hồng đậm)
}

// --- VÙNG 4: LẠNH & ẨM (Gộp các điều kiện '&&') ---
// Bao gồm (T<20, H>60), (T<20, H>80), (T<10, H>60), (T<10, H>80)
else if (local_temp < 20 && local_humid > 60) {
    color = strip.Color(128, 0, 128); // Tím (Purple)
}

// --- VÙNG 5: LÝ TƯỞNG 
else {
    color = strip.Color(0, 255, 0);   // Xanh lá (Lý tưởng)
}

// --- Cập nhật đèn LED (CHỈ GỌI 1 LẦN) ---
strip.setPixelColor(0, color);
strip.show(); // Update the strip
vTaskDelay(pdMS_TO_TICKS(500)); // Update every half second
    }
}   