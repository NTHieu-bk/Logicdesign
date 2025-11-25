#include "led_blinky.h"
#include "global.h"
// Task 1: Status LED blinky
// - Supports a manual override (from web / other tasks) via
//   glob_blinky_is_overridden + glob_blinky_override_state.
// - In automatic mode, LED blink patterns depend on temperature
//   and humidity ranges.
void led_blinky(void *pvParameters){
    // Configure the GPIO pin as output for the status LED
    pinMode(LED_GPIO, OUTPUT);
  
  while(1) {                        
    bool run = true; // flag: true = run automatic logic, false = manual override

        // ----- 1. Check manual override flags (protected by mutex) -----
        // Take the control mutex before reading / writing override variables
        if (xSemaphoreTake(xBlinkyControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_blinky_is_overridden) {
                 // Manual mode: force LED state according to override flag
                digitalWrite(LED_GPIO, glob_blinky_override_state ? HIGH : LOW);
                run = false;  // skip automatic behavior this loop
            } else {
                run = true; // No override: allow automatic pattern
            }
            xSemaphoreGive(xBlinkyControlMutex);  // Release the mutex so other tasks can use it
        }
        // ----- 2. Automatic LED behavior based on sensor data -----
        if (run) {
            
            float local_temp = 0;
            float local_humid = 0;
            
              // Safely copy the latest temperature and humidity
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                local_temp = glob_temperature;
                local_humid = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("Task Monitor: Không thể lấy Mutex!"); // Could not get the mutex -> just skip this cycle
            }

            // ------------ BLINK PATTERN LOGIC ------------
            // Higher risk -> faster blinking
            // Ideal zone -> very slow blinking

            // Region 1: Extremely hot and humid
            if (local_temp > 35 && local_humid > 80) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            else if (local_temp > 35 || local_humid > 80) {
            // Region 1b: Very hot OR very humid
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            else if (local_temp > 30 && local_humid > 60) {
            // Region 1c: Hot and humid (moderately severe)
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            else if (local_temp > 30 || local_humid > 60) {
            // Region 1d: Warm OR humid
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            // Region 2: Cold and dry
            else if (local_temp < 10 && local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(200));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            // Region 2b: Very cold OR very dry
            else if (local_temp < 10 || local_humid < 20) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(500));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            // Region 2c: Cool and dry
            else if (local_temp < 20 && local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(300));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(300));
            }
            // Region 2d: Cool OR dry
            else if (local_temp < 20 || local_humid < 30) {
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(800));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(800));
            }
            // Region 3 & 4: Mixed hot–dry or cold–humid conditions
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
            // Region 5: Ideal comfort zone
            else {
                // Very slow blink to indicate normal / safe conditions
                digitalWrite(LED_GPIO, HIGH);
                vTaskDelay(pdMS_TO_TICKS(2000));
                digitalWrite(LED_GPIO, LOW);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        } 
            // ----- 3. Manual override path -----
        else {
            // When overridden, we already set the LED once above.
            // Use a small delay to avoid busy-waiting.
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
