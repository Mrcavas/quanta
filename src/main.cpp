#include "Arduino.h"
#include "dmp.h"
#include "hexdump.h"
#include "pid.h"
#include "ws.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <EncButton.h>
#include <LittleFS.h>
#include <Servo.h>
#include <WiFi.h>

#define EB_NO_FOR
#define EB_NO_CALLBACK
#define EB_NO_COUNTER
#define EB_NO_BUFFER
#define EB_DEB_TIME 20

#define SERVO_PIN 21
#define MOTOR_PIN 20
#define BUTTON_PIN 10

#define SERVO_MIDDLE 92
#define SERVO_MAX_DIFF 50

Servo servo = Servo();
Button button(BUTTON_PIN);
bool anchoring = false;

void handleAnchoring();

void sendInitPacket() {
  auto buf = ws.makeBuffer(1 + 3 * sizeof(float));
  uint8_t *p = buf->get();

  p[0] = 0x01;
  float kp = pid.getKp(), ki = pid.getKi(), kd = pid.getKd();
  memcpy(p + 1, &kp, sizeof(float));
  memcpy(p + 1 + sizeof(float), &ki, sizeof(float));
  memcpy(p + 1 + 2 * sizeof(float), &kd, sizeof(float));

  ws.binaryAll(buf);
}

void sendAnchoringPacket() {
  auto buf = ws.makeBuffer(1 + sizeof(bool));
  uint8_t *p = buf->get();

  p[0] = 0x0a;
  memcpy(p + 1, &anchoring, sizeof(bool));

  ws.binaryAll(buf);
}

void handlePacket(uint8_t id, const uint8_t *data, size_t len) {
  if (id == 0x0c && len == 8) {
    float angle, speed;
    memcpy(&angle, data, sizeof(float));
    memcpy(&speed, data + sizeof(float), sizeof(float));

    if (!anchoring)
      servo.write(SERVO_PIN, SERVO_MIDDLE - angle);
    servo.write(MOTOR_PIN, speed);

    Serial.printf("Got angle: %f, speed: %f\n", angle, speed);
    return;
  }

  if (id == 0x0a && len == 1) {
    memcpy(&anchoring, data, sizeof(bool));
    handleAnchoring();
    return;
  }

  if ((id == 'p' || id == 'i' || id == 'd') && len == 4) {
    float val;
    memcpy(&val, data, sizeof(float));

    if (id == 'p')
      pid.setKp(val);
    if (id == 'i')
      pid.setKi(val);
    if (id == 'd')
      pid.setKd(val);

    saveCoefficients();
    return;
  }

  if (id == 0x01 && len == 0) {
    sendInitPacket();
    Serial.println("New client, sending init");
    return;
  }

  Serial.printf("Unrecognized packet 0x%x!\n", id);
  hexdump(data, len);
}

void onPID(double output) { servo.write(SERVO_PIN, SERVO_MIDDLE - output); }

void handleAnchoring() {
  if (anchoring)
    startPidTask(yaw);
  else {
    stopPidTask();
    servo.write(SERVO_PIN, SERVO_MIDDLE);
  }

  Serial.printf("Anchoring: %s\n", anchoring ? "on" : "off");
}

void setup() {
  Serial.begin(115200);
  delay(1800);

  setupDMP();
  setupWS([](AsyncWebSocket *server, AsyncWebSocketClient *client,
             const uint8_t *data, size_t len) {
    auto id = data[0];
    handlePacket(id, data + 1, len - 1);
  });

  if (!loadCoefficients()) {
    pid.setKp(1);
    saveCoefficients();
  }
  pid.outMax = SERVO_MAX_DIFF;
  pid.outMin = -SERVO_MAX_DIFF;
  pid.setpoint = 0;

  setupPID(10, [] { return yaw; }, onPID);

  Serial.println("\nDone initialization\n");
}

void loop() {
  tickDMP();
  button.tick();

  if (button.click()) {
    anchoring = !anchoring;

    handleAnchoring();
    sendAnchoringPacket();
  }

  tickWS();
}