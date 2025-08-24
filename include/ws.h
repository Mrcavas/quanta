#include "Arduino.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>

#define SSID "Mrcavas"
#define PASSWORD "12277903543"

// #define SSID "iPhoneGleba"
// #define PASSWORD "herobib2"

// #define SSID "RT-GPON-4988"
// #define PASSWORD "fK46Mkqt"

extern AsyncWebSocket ws;

int setupWS(
    std::function<void(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       const uint8_t *data, size_t len)>
        onMessage);
void tickWS();