#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "DeviceManager.h"
#include "LogBuffer.h"
#include "WebAssets.h"
#include "CmdUtils.h"

class WebServer {
public:
    static void begin();

private:
    static AsyncWebServer server;
    static void setupRoutes();

    // API Handlers
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleControl(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleDisconnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleForget(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    // Extended Data
    static void handleHistory(AsyncWebServerRequest *request);

    // Log Handlers
    static void handleLogs(AsyncWebServerRequest *request);
    static void handleLogConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleRawCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};

#endif
