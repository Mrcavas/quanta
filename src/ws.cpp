#include "ws.h"
#include "Arduino.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_wifi.h>

static AsyncWebServer server(80);
static AsyncWebSocketMessageHandler wsHandler;
AsyncWebSocket ws("/ws", wsHandler.eventHandler());

int setupWS(
    std::function<void(AsyncWebSocket *server, AsyncWebSocketClient *client,
                       const uint8_t *data, size_t len)>
        onMessage) {

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed!");
    return -1;
  }
  Serial.println("LittleFS mounted, connecting to WiFi");

  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(WiFi.status());
  }

  // if (!WiFi.softAP("QUANTA", "12277903543", 11)) {
  //   Serial.println("Soft AP creation failed.");
  //   return -1;
  // }

  // IPAddress myIP = WiFi.softAPIP();
  // Serial.print("AP IP address: ");
  // Serial.println(myIP);
  // server.begin();

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // serves root html page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  wsHandler.onMessage(onMessage);

  server.addHandler(&ws);
  server.begin();

  return 0;
}

void tickWS() { ws.cleanupClients(); }