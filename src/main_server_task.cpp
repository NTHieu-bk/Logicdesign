#include "global.h"
#include "task_webserver.h"
#include <WiFi.h>
#include "main_server_task.h"
#include "esp32-hal-gpio.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "task_check_info.h"
#include "ArduinoJson.h"
#include "tinyml.h"

void main_server_task(void *pvParameters) {

    vTaskDelay(pdMS_TO_TICKS(5000));   // Allow WiFi/AP to stabilize

    while (1) {

        // Wait until the sensor task signals new data ready
        if (xSemaphoreTake(xSensorUpdateSemaphore, portMAX_DELAY) == pdTRUE) {
            
            float t = 0.0f;
            float h = 0.0f;
            int blinky_state = 0;
            int neo_state    = 0;

            // Check if any device is connected (STA or AP client)
            bool has_connection =
                (WiFi.status() == WL_CONNECTED || WiFi.softAPgetStationNum() > 0);

            if (has_connection) {

                // Read shared temperature/humidity safely
                if (xSemaphoreTake(xDataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    t = glob_temperature;
                    h = glob_humidity;
                    xSemaphoreGive(xDataMutex);
                }

                // Read current GPIO states of LEDs
                blinky_state = digitalRead(48);
                neo_state    = digitalRead(45);

                // Build telemetry JSON packet
                StaticJsonDocument<512> doc;
                doc["page"] = "telemetry";
                JsonObject value = doc.createNestedObject("value");

                value["temp"]   = t;
                value["hum"]    = h;
                value["blinky"] = blinky_state;
                value["neo"]    = neo_state;

                // TinyML anomaly detection outputs
                value["ml_st"]    = (int)current_ml_state;
                value["ml_score"] = current_anomaly_score;
                value["ml_ratio"] = current_anomaly_ratio;

                // TinyML environment classification outputs
                value["env_class"] = (int)current_env_class;
                value["env_label"] = current_env_label;

                // TinyML recommendation string
                value["advice"] = current_advice_msg;

                // Convert JSON to string
                String data;
                serializeJson(doc, data);

                // Push telemetry to the web dashboard
                Webserver_sendata(data);
            }
        }
    }
}
