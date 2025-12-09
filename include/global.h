#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

extern float glob_temperature;
extern float glob_humidity;
extern SemaphoreHandle_t xDataMutex;

extern SemaphoreHandle_t xBlinkyControlMutex; // Safe lock
extern bool glob_blinky_is_overridden;        // Flag: Is overridden by Web?
extern bool glob_blinky_override_state;       // (ON/OFF)

extern SemaphoreHandle_t xNeoControlMutex;    // Safe lock
extern bool glob_neo_is_overridden;           // Flag: Is overridden by Web?
extern bool glob_neo_override_state;          // (ON/OFF)
extern volatile unsigned long glob_last_interaction_time;
extern SemaphoreHandle_t xSensorUpdateSemaphore;
extern String glob_lcd_msg_line1;
extern String glob_lcd_msg_line2;
extern String current_advice_msg;
extern bool glob_should_restart;
extern unsigned long glob_restart_timer;

extern String current_advice_msg;

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

extern boolean isWifiConnected;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
#endif