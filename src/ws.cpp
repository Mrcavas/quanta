#include "ws.h"
#include "Arduino.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_wifi.h>

static AsyncWebServer server(80);
static AsyncWebSocketMessageHandler wsHandler;
AsyncWebSocket ws("/ws", wsHandler.eventHandler());

bool setupWS(
    std::function<void(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       const uint8_t *data, size_t len)>
        onMessage) {

  if (!LittleFS.begin()) {
    // Serial.printfln("LittleFS mount failed!");
    return false;
  }
  // Serial.printfln("LittleFS mounted, connecting to WiFi");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_17dBm);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    // Serial.printf(WiFi.status());
  }

  // Serial.printfln("\nWiFi connected");
  // Serial.printf("IP address: ");
  // Serial.printfln(WiFi.localIP());

  // serves root html page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  ElegantOTA.begin(&server);

  wsHandler.onMessage(onMessage);

  server.addHandler(&ws);
  server.begin();

  return true;
}

void tickWS() {
  ws.cleanupClients();
  ElegantOTA.loop();
}