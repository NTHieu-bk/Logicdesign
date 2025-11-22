
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include <task_check_info.h>
#include <ESPAsyncWebServer.h>
extern void handleWebSocketMessage(String message, AsyncWebSocket &ws);
#endif