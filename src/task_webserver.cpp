#include "task_webserver.h"
#include "global.h"           
#include <ArduinoJson.h>      
#include "task_handler.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data)
{
    // specific flag to track if we have already printed the warning
    static bool noClientWarningPrinted = false; 

    if (ws.count() > 0)
    {
        ws.textAll(data); // Send to all connected clients
        Serial.println("📤 Data sent via WebSocket: " + data);
        
        // Reset the warning flag because we have a connection now.
        // If the connection drops later, the warning will print again.
        noClientWarningPrinted = false; 
    }
    else
    {
        // Only print if we haven't printed it yet
        if (!noClientWarningPrinted) {
            Serial.println("⚠️ No WebSocket clients currently connected!");
            noClientWarningPrinted = true; 
        }
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        // Fixed printf missing argument for %u
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        
        // Only handle Text messages
        if (info->opcode == WS_TEXT)
        {
            String message = "";
            if (len > 0) {
                message = String((char *)data).substring(0, len);
            }

            // Pass 'ws' so the handler can send a response if needed
            handleWebSocketMessage(message, ws);
        }
    }
}

void connnectWSV()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", "text/html"); });
              
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/script.js", "application/javascript"); });
              
    server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/styles.css", "text/css"); });
              
    server.begin();
    ElegantOTA.begin(&server);
    webserver_isrunning = true;
    Serial.println("✅ Webserver Started!");
}

void Webserver_stop()
{
    ws.closeAll();
    server.end();
    webserver_isrunning = false;
    Serial.println("🛑 Webserver Stopped!");
}

void Webserver_reconnect()
{
    if (!webserver_isrunning)
    {
        connnectWSV();
    }
    ElegantOTA.loop();
}