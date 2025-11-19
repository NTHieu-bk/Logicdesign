#include "temp_humi_monitor.h"
#include "global.h"
#include "DHT20.h"
#include "LiquidCrystal_I2C.h"
#include "Wire.h"
DHT20 dht20;
LiquidCrystal_I2C lcd(33,16,2);


bool isCritical(float temperature, float humidity) {
    return (temperature > 35.0 || humidity > 85.0 || temperature < 15.0 || humidity < 20.0);
}

bool isWarning(float temperature, float humidity) {
    // (Sửa lại logic 'isWarning' từ code trước của bạn)
    return (temperature > 30.0 || humidity > 70.0 || temperature < 20.0 || humidity < 30.0);
}

bool isNormal(float temperature, float humidity) {
    return !isCritical(temperature, humidity) && !isWarning(temperature, humidity);
}

String getStatus_Temp(float temp) {
    if (temp > 35.0 || temp < 15.0) {
        return "CRIT";
    } else if (temp > 30.0 || temp < 20.0) {
        return "WARN";
    } else {
        return "NORM";
    }
}

// Hàm này CHỈ kiểm tra Độ ẩm
String getStatus_Humi(float humi) {
    if (humi > 85.0 || humi < 20.0) {
        return "CRIT";
    } else if (humi > 70.0 || humi < 30.0) {
        return "WARN";
    } else {
        return "NORM";
    }
}

void temp_humi_monitor(void *pvParameters) {

    // (Phần setup của Task)
    Wire.begin(11, 12); 
    Serial.begin(115200);
    dht20.begin();

    lcd.begin();
    lcd.backlight(); 

    while (1) {
        if (dht20.read() == DHT20_OK) {
            
            // Đọc giá trị vào 2 biến *cục bộ*
            float temperature = dht20.getTemperature();
            float humidity = dht20.getHumidity();

            // (Kiểm tra lỗi cảm biến)
            if (temperature < -40.0 || temperature > 100.0 || humidity < 0.0 || humidity > 100.0) {
                Serial.println("Lỗi: Giá trị cảm biến không hợp lệ.");
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Sensor Error!");
                // Đảm bảo đèn nền vẫn sáng khi lỗi
                lcd.backlight(); 
            } else {

                // --- VÙNG BẢO VỆ BẰNG MUTEX ---
                if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    glob_temperature = temperature;
                    glob_humidity = humidity;
                    xSemaphoreGive(xDataMutex);
                } else {
                    Serial.println("Task Monitor: Không thể lấy Mutex!");
                }
                // --- KẾT THÚC VÙNG BẢO VỆ ---

                
                // (Phần hiển thị Serial)
                Serial.print("Humidity: ");
                Serial.print(humidity, 1);
                Serial.print("%  Temperature: ");
                Serial.print(temperature, 1);
                Serial.println("°C");

                // (Phần hiển thị LCD)
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("T: ");
                lcd.print(temperature, 1);

                lcd.setCursor(0, 1);
                lcd.print("H%: ");
                lcd.print(humidity, 1);

                String temp_status = getStatus_Temp(temperature);
                String humi_status = getStatus_Humi(humidity);

                // Bước 2: (BONUS) Quyết định nhấp nháy đèn nền
                // (Nếu 1 TRONG 2 cái là "CRIT", thì nháy đèn)
                if (temp_status == "CRIT" || humi_status == "CRIT") {
                    lcd.noBacklight();
                    vTaskDelay(pdMS_TO_TICKS(200));
                    lcd.backlight();
                    vTaskDelay(pdMS_TO_TICKS(200));
                    lcd.noBacklight();
                    vTaskDelay(pdMS_TO_TICKS(200));
                    lcd.backlight(); 
                } else {
                    // Nếu không có gì Crit, BẬT đèn
                    lcd.backlight(); 
                }

                // Bước 3: Xóa và in layout mới
                lcd.clear();
                
                // --- DÒNG 0: (Temp và Status của Temp) ---
                // "T: 25.5C (WARN)" (14/16 chars)
                lcd.setCursor(0, 0);
                lcd.print("T: ");
                lcd.print(temperature, 1);
                lcd.print("C ("); // Ký hiệu bạn muốn thêm
                lcd.print(temp_status);
                lcd.print(")");

                // --- DÒNG 1: (Humi và Status của Humi) ---
                // "H: 60.0% (NORM)" (14/16 chars)
                lcd.setCursor(0, 1);
                lcd.print("H: ");
                lcd.print(humidity, 1);
                lcd.print("% ("); // Ký hiệu bạn muốn thêm
                lcd.print(humi_status);
                lcd.print(")");
            }
        } // (Hết phần dht20.read())

        // Delay 2 giây
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}