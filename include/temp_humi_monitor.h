#ifndef __TEMP_HUMI_MONITOR__
#define __TEMP_HUMI_MONITOR__
#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "DHT20.h"
#include "global.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Sensor data sample structure
struct SensorSample {
    float      temperature;
    float      humidity;
    TickType_t timestamp;
};

// LCD message structure
struct LcdMessage {
    char     line1[17];      // 16 chars + null terminator
    char     line2[17];
    uint16_t durationMs;     // Time to display the message
};

// Task configuration structure
struct TempHumiTaskConfig {
    QueueHandle_t sensorQueue; // queue< SensorSample >
    QueueHandle_t lcdMsgQueue; // queue< LcdMessage >
};


void temp_humi_monitor(void *pvParameters);

#endif