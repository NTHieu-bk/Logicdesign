#include "led_blinky.h"
#include "global.h"
void led_blinky(void *pvParameters){
    pinMode(LED_GPIO, OUTPUT);
  
  while(1) {                         
    bool run = true; // flag

        // Take the "key" to read the variable 
        if (xSemaphoreTake(xBlinkyControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_blinky_is_overridden) {
                digitalWrite(LED_GPIO, glob_blinky_override_state ? HIGH : LOW);
                run = false; 
            } else {
                run = true;
            }
            xSemaphoreGive(xBlinkyControlMutex); // Release the "key"
        }

        if (run) {
            
            float local_temp = 0;
            float local_humid = 0;
            
            // Get sensor data
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                local_temp = glob_temperature;
                local_humid = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("Task Monitor: Unable to acquire Mutex!");
            }

            
            // Level 1 - Most critical (Hot & Humid)
            if (local_temp > 35 && local_humid > 80) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            else if (local_temp > 35 || local_humid > 80) {
                // Level 2 - Extreme high warning
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else if (local_temp > 30 && local_humid > 60) {
                // Level 3 - High critical
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            else if (local_temp > 30 || local_humid > 60) {
                // Level 4 - High warning
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            //    REGION 2: COLD & DRY   
            else if (local_temp < 10 && local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            else if (local_temp < 10 || local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else if (local_temp < 20 && local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            else if (local_temp < 20 || local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            //    REGION 3 & 4   
            else if (local_temp > 30 && local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(400));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(400));
            }
            else if (local_temp < 20 && local_humid > 60) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(400));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(400));
            }
            //    REGION 5: IDEAL   
            else {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(2000));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        } 
        else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
