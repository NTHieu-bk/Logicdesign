#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;

SemaphoreHandle_t xDataMutex;
SemaphoreHandle_t xBlinkyControlMutex;
bool glob_blinky_is_overridden = false;
bool glob_blinky_override_state = false;

SemaphoreHandle_t xNeoControlMutex;
bool glob_neo_is_overridden = false;
bool glob_neo_override_state = false;
volatile unsigned long glob_last_interaction_time = 0;
String glob_lcd_msg_line1 = "";
String glob_lcd_msg_line2 = "";
bool glob_should_restart = false;
unsigned long glob_restart_timer = 0;

SemaphoreHandle_t xSensorUpdateSemaphore;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

String ssid = "ESP32-YOUR NETWORK HERE!!!";
String password = "12345678";
String wifi_ssid = "abcde";
String wifi_password = "123456789";
boolean isWifiConnected = false;
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();