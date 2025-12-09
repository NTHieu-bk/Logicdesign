#include "neo_blinky.h"
#include "global.h"

void neo_blinky(void *pvParameters){

    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    // Set all pixels to off to start
    strip.clear();
    strip.show();

    while(1) {                       
        bool run = true; // flag

        if (xSemaphoreTake(xNeoControlMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (glob_neo_is_overridden) {
                // IF THERE IS AN OVERRIDE COMMAND:
                run = false; 

                if (glob_neo_override_state) {
                    // WEB Command: TURN ON -> Set to White (default for manual mode)
                    strip.setPixelColor(0, strip.Color(255, 255, 255)); 
                    strip.show();
                } else {
                    // WEB Command: TURN OFF
                    strip.clear();
                    strip.show();
                }
            } else {
                // No override command -> Run automatically
                run = true;
            }
            
            xSemaphoreGive(xNeoControlMutex);
        } else {
             Serial.println("Task Neo: Unable to acquire Mutex Control!");
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
                Serial.println("Task Neo: Unable to acquire Mutex Data!");
            }

            uint32_t color = 0; // Variable to store color

            //   REGION 1: HOT & HUMID  
            if (local_temp > 35 && local_humid > 80) {
                color = strip.Color(255, 0, 0);   // Red
            }
            else if (local_temp > 35 || local_humid > 80) {
                color = strip.Color(255, 165, 0); // Orange
            }
            else if (local_temp > 30 && local_humid > 60) {
                color = strip.Color(255, 255, 0);  // Yellow
            }
            else if (local_temp > 30 || local_humid > 60) {
                color = strip.Color(173, 255, 47); // Chartreuse
            }

            //   REGION 2: COLD & DRY  
            else if (local_temp < 10 && local_humid < 20) {
                color = strip.Color(0, 0, 255);   // Blue
            }
            else if (local_temp < 10 || local_humid < 20) {
                color = strip.Color(0, 255, 255); // Cyan
            }
            else if (local_temp < 20 && local_humid < 30) {
                color = strip.Color(75, 0, 130);  // Indigo
            }
            else if (local_temp < 20 || local_humid < 30) {
                color = strip.Color(173, 216, 230); // Light blue
            }

            //   REGION 3: HOT & DRY  
            else if (local_temp > 30 && local_humid < 30) {
                color = strip.Color(255, 0, 255); // Magenta
            }
            else if (local_temp > 35 && local_humid < 20) {
                color = strip.Color(255, 40, 0); // Orange Red
            }

            //   REGION 4: COLD & HUMID  
            else if (local_temp < 20 && local_humid > 60) {
                color = strip.Color(128, 0, 128); // Purple
            }
            else if (local_temp < 10 && local_humid > 80) {
                color = strip.Color(0, 120, 255); // Sky blue
            }

            //   REGION 5: IDEAL  
            else {
                color = strip.Color(0, 255, 0);   // Green
            }

            // Update the color on the LED
            strip.setPixelColor(0, color);
            strip.show();
            
            // Delay for automatic mode (500ms)
            vTaskDelay(pdMS_TO_TICKS(500)); 

        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}
