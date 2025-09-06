#include "main.h"
#include "Arduino.h"
#include "calibration.h"
#include "hexdump.h"
#include "packets.h"
#include "pid.h"
#include "sensor.h"
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

Servo servo = Servo();
// Button button(BUTTON_PIN);
bool anchoring = false, calibrating = false;
uint32_t lastPingTime = millis();

void handleAnchoring();

void handlePacket(uint8_t id, const uint8_t *data, size_t len) {
  if (id == 0x0c && len == 8) {
    float angle, speed;
    memcpy(&angle, data, sizeof(float));
    memcpy(&speed, data + sizeof(float), sizeof(float));

    if (!anchoring)
      servo.write(SERVO_PIN, SERVO_MIDDLE - angle);
    servo.write(MOTOR_PIN, speed);

    // Serial.printff("Got angle: %f, speed: %f\n", angle, speed);
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
    ws.binaryAll(buildInitPacket(&ws, pid.getKp(), pid.getKi(), pid.getKd()));
    // Serial.printfln("New client, sending init");
    return;
  }

  if (id == 0xff && len == 0) {
    lastPingTime = millis();
    return;
  }

  if (id == 0x1c && len == 1) {
    memcpy(&calibrating, data, sizeof(boolean));
    return;
  }

  if (id == 0x16 && len == 0) {
    biasStore emptyStore;
    writeBiases(&icm, &emptyStore);
    saveBiasStore(&emptyStore);

    return;
  }

  if (id == 0x10 && len == 0) {
    biasStore store;
    readBiases(&icm, &store);
    saveBiasStore(&store);

    return;
  }

  // Serial.printff("Unrecognized packet 0x%x!\n", id);
  hexdump(data, len);
}

void writeServo(double output) {
  servo.write(SERVO_PIN, SERVO_MIDDLE - output);
}

void handleAnchoring() {
  if (anchoring) {
    startPidTask();
    ws.binaryAll(buildYawAnchorPacket(&ws, yaw));
  } else {
    stopPidTask();
    servo.write(SERVO_PIN, SERVO_MIDDLE);
  }

  // Serial.printff("Anchoring: %s\n", anchoring ? "on" : "off");
}

void setup() {
  writeServo(0);
  // Serial.begin(115200);
  // delay(1800);

  setupIMU();
  setupWS([](AsyncWebSocket *server, AsyncWebSocketClient *client,
             const uint8_t *data, size_t len) {
    auto id = data[0];
    handlePacket(id, data + 1, len - 1);
  });

  if (!loadCoefficients()) {
    pid.setKp(1);
    pid.setKi(0);
    pid.setKd(0);
    saveCoefficients();
  }

  setupPID(25, &yaw, writeServo);

  // Serial.printfln("\nDone initialization\n");
}

uint32_t lastUpdateSentTime = millis();

void loop() {
  tickIMU();
  // button.tick();

  // if (button.click()) {
  //   anchoring = !anchoring;

  //   handleAnchoring();
  //   ws.binaryAll(buildAnchoringPacket(&ws, &anchoring));
  // }

  if (millis() - lastUpdateSentTime > 100) {
    lastUpdateSentTime = millis();

    if (!calibrating) {
      ws.binaryAll(buildRotationPacket(&ws, yaw));
    } else {
      biasStore store;
      readBiases(&icm, &store);

      ws.binaryAll(buildBiasesPacket(&ws, &store));
    }
  }

  // if (millis() - lastPingTime > 1000) {
  //   anchoring = false;
  //   handleAnchoring();
  //   servo.write(MOTOR_PIN, 0);
  // }

  tickWS();
}