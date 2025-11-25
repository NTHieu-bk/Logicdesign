#include "neo_blinky.h"
#include "global.h"
// Task 2: NeoPixel color indicator
// - Uses a single RGB NeoPixel as an environment status light.
// - Supports manual override (from the web/UI) via
//   glob_neo_is_overridden + glob_neo_override_state.
// - In automatic mode, color depends on temperature & humidity zones.
void neo_blinky(void *pvParameters){
    // Create and initialize the NeoPixel strip object
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
     // Start with LED off
    strip.clear();
    strip.show();

    while(1) {                       
        bool run = true; // flag: true = auto color logic, false = manual override

        // ----- 1. Check manual override (protected by xNeoControlMutex) -----

        if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_neo_is_overridden) {
                // Manual command active: do NOT run automatic logic this cycle
                run = false; 

                if (glob_neo_override_state) {
                    // Manual ON: show solid white as default manual color
                    strip.setPixelColor(0, strip.Color(255, 255, 255)); 
                    strip.show();
                } else {
                    // Manual OFF: turn LED completely off
                    strip.clear();
                    strip.show();
                }
            } else {
                 // No override: allow automatic color mapping
                run = true;
            }
            
            xSemaphoreGive(xNeoControlMutex); // Release control mutex
        } else {
             Serial.println("Task Neo: Không thể lấy Mutex Control!");
        }

         // ----- 2. Automatic color mode based on sensor data -----
        if (run) {
            
            float local_temp = 0;
            float local_humid = 0;
            
            // Safely copy the latest temperature & humidity values
            if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                local_temp = glob_temperature;
                local_humid = glob_humidity;
                xSemaphoreGive(xDataMutex);
            } else {
                Serial.println("Task Neo: Không thể lấy Mutex Data!");
            }

            uint32_t color = 0; // RGB color to display

            // -------- Zone 1: HOT & HUMID --------
            if (local_temp > 35 && local_humid > 80) {
                color = strip.Color(255, 0, 0);  // Red: extremely hot & humid
            }
            else if (local_temp > 35 || local_humid > 80) {
                color = strip.Color(255, 165, 0); // Orange: very hot OR very humid
            }
            else if (local_temp > 30 && local_humid > 60) {
                color = strip.Color(255, 255, 0);  // Yellow: hot & humid (high)
            }
            else if (local_temp > 30 || local_humid > 60) {
                color = strip.Color(173, 255, 47); // Chartreuse: warm OR humid
            }

            // -------- Zone 2: COLD & DRY --------
            else if (local_temp < 10 && local_humid < 20) {
                color = strip.Color(0, 0, 255);   // Blue: extremely cold & dry
            }
            else if (local_temp < 10 || local_humid < 20) {
                color = strip.Color(0, 255, 255); // Cyan: very cold OR very dry
            }
            else if (local_temp < 20 && local_humid < 30) {
                color = strip.Color(75, 0, 130); // Indigo: cool & dry
            }
            else if (local_temp < 20 || local_humid < 30) {
                color = strip.Color(173, 216, 230); // Light blue: cool OR dry
            }

            // -------- Zone 3: HOT & DRY --------
            else if (local_temp > 30 && local_humid < 30) {
                color = strip.Color(255, 0, 255); // Magenta: hot & dry
            }
            else if (local_temp > 35 && local_humid < 20) {
                color = strip.Color(255, 40, 0); // Orange-red: extremely hot & dry
            }

            // -------- Zone 4: COLD & HUMID --------
            else if (local_temp < 20 && local_humid > 60) {
                color = strip.Color(128, 0, 128);// Purple: cold & humid
            }
            else if (local_temp < 10 && local_humid > 80) {
                color = strip.Color(0, 120, 255); // Sky blue: very cold & very humid
            }


            // -------- Zone 5: IDEAL / COMFORTABLE --------
            else {
                color = strip.Color(0, 255, 0);    // Green: comfortable range
            }

           // Apply the selected color to the NeoPixel
            strip.setPixelColor(0, color);
            strip.show();
            
           // Automatic mode update rate ~500 ms
            vTaskDelay(pdMS_TO_TICKS(500)); 

        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
