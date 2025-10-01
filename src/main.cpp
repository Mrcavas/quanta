#include "main.h"
#include "calibration.h"
#include "hexdump.h"
#include "packets.h"
#include "pid.h"
#include "sensor.h"
#include "strprintf.h"
#include "ws.h"
#include <Arduino.h>
#include <AsyncTCP.h>
#include <AsyncWebSocket.h>
#include <ESPAsyncWebServer.h>
#include <EncButton.h>
#include <LittleFS.h>
#include <Servo.h>
#include <WiFi.h>
#include <esp_wifi.h>

#define EB_NO_FOR
#define EB_NO_CALLBACK
#define EB_NO_COUNTER
#define EB_NO_BUFFER
#define EB_DEB_TIME 20

#define SERVO_PIN 21
#define MOTOR_PIN 20
#define BUTTON_PIN 10

Servo servo = Servo();
Button button(BUTTON_PIN);
float yawAnchor;
bool anchoring = false, magCalibrating = false, gyroCalibrating = false;
uint8_t accelCalibrating = 255;
float mx, my, mz;
double accelSum[3] = {0, 0, 0};
double gyroSum[3] = {0, 0, 0};
uint16_t sampleIndex = 0;
uint32_t lastPingTime;
uint32_t lastUpdateSentTime;
bool imuInitialized;

#define MAX_AP_SPEED_PERCENTAGE 23

enum AutoPilotState {
  AP_DISABLED,
  AP_READY,
  AP_GOING,
};

enum AutoPilotState apState = AP_DISABLED;
float apSpeed = 1500;
uint32_t apStartTime = -1;

void handleAnchoring();

void writeServo(float output) {
  servo.write(SERVO_PIN, SERVO_MIDDLE - fmaxf(-SERVO_MAX_DIFF,
                                              fminf(SERVO_MAX_DIFF, output)));
}

void handlePacket(uint8_t id, const uint8_t *data, size_t len) {
  if (id == 0x0c && len == 8) {
    float angle, speed;
    memcpy(&angle, data, sizeof(float));
    memcpy(&speed, data + sizeof(float), sizeof(float));

    if (!anchoring)
      writeServo(angle);
    servo.write(MOTOR_PIN, speed);
  }

  if (id == 0x0a && len == 1) {
    memcpy(&anchoring, data, sizeof(bool));
    handleAnchoring();
  }

  if (id == 0x0a && len == 4) {
    memcpy(&yawAnchor, data, sizeof(float));
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
  }

  if (id == 0x01 && len == 0) {
    sendInitPacket(pid.getKp(), pid.getKi(), pid.getKd());
    sendAnchoringPacket(&anchoring);
    sendYawAnchorPacket(yawAnchor);
    sendMessagePacket(
        strprintf("IMU is %sinitialized", imuInitialized ? "" : "not "));
  }

  if (id == 0xff && len == 0) {
    lastPingTime = millis();
  }

  if (id == 0xc2 && len == 0) {
    gyroCalibrating = true;
  }

  if (id == 0xc3 && len == 0) {
    magCalibrating = true;
  }

  if (id == 0xc4 && len == 1) {
    memcpy(&accelCalibrating, data, 1);
  }

  if (id == 0xc1 && len == 12 * 4) {
    CalibrationStore newCal = calibration;

    memcpy(&newCal.magX, data, 4);
    memcpy(&newCal.magY, data + 4, 4);
    memcpy(&newCal.magZ, data + 8, 4);
    memcpy(&newCal.magScale, data + 12, 4 * 9);

    calibration = newCal;
    saveBiasStore(&calibration);
  }

  if (id == 0xc1) {
    magCalibrating = false;
  }

  if (id == 0xc5 && len == 3 * 4) {
    CalibrationStore newCal = calibration;

    memcpy(&newCal.accelX, data, 4);
    memcpy(&newCal.accelY, data + 4, 4);
    memcpy(&newCal.accelZ, data + 8, 4);

    calibration = newCal;
    saveBiasStore(&calibration);
  }

  if (id == 0xa0 && len == 0) {
    sendCalibrationDataPacket(&calibration);
  }

  if (id == 0xa1 && len == sizeof(CalibrationStore)) {
    memcpy(&calibration, data, sizeof(CalibrationStore));
    saveBiasStore(&calibration);
  }
}

void handleAnchoring() {
  if (anchoring) {
    yawAnchor = getYaw();
    sendYawAnchorPacket(yawAnchor);
  } else {
    yawAnchor = 0.0f;
    writeServo(0);
  }
}

void setup() {
  pinMode(SERVO_PIN, OUTPUT);
  // Serial.begin(460800);
  // delay(1800);
  writeServo(0);

  setupBiasesStorage();
  setupPID();
  imuInitialized = setupIMU([](float yaw, RawICUData raw) {
    if (anchoring) {
      writeServo(tickPID(yawAnchor, yaw));
      return;
    }

    if (magCalibrating) {
      mx = raw.mx;
      my = raw.my;
      mz = raw.mz;
      return;
    }

    if (gyroCalibrating) {
      sampleIndex++;

      if (sampleIndex % 50 == 0) {
        sendGyroCalibrationProgressPacket((float)sampleIndex / 1000.0 * 100.0);
      }

      if (sampleIndex == 1000) {
        calibration.gyroX = gyroSum[0] / 1000.0;
        calibration.gyroY = gyroSum[1] / 1000.0;
        calibration.gyroZ = gyroSum[2] / 1000.0;

        saveBiasStore(&calibration);

        gyroSum[0] = 0;
        gyroSum[1] = 0;
        gyroSum[2] = 0;
        sampleIndex = 0;
        gyroCalibrating = false;
      }

      gyroSum[0] += raw.gx;
      gyroSum[1] += raw.gy;
      gyroSum[2] += raw.gz;
      return;
    }

    if (accelCalibrating != 255) {
      sampleIndex++;

      if (sampleIndex % 25 == 0) {
        sendAccelCalibrationProgressPacket(accelCalibrating,
                                           (float)sampleIndex / 500.0 * 100.0);
      }

      if (sampleIndex == 500) {
        sendAccelCalibrationDataPacket(accelCalibrating, accelSum[0] / 500.0f,
                                       accelSum[1] / 500.0f,
                                       accelSum[2] / 500.0f);

        accelSum[0] = 0;
        accelSum[1] = 0;
        accelSum[2] = 0;
        sampleIndex = 0;
        accelCalibrating = 255;
      }

      accelSum[0] += raw.ax;
      accelSum[1] += raw.ay;
      accelSum[2] += raw.az;
      return;
    }
  });

  loadCoefficients();

  button.tick();
  if (button.read()) {
    apState = AP_READY;
    return;
  }

  setupWS([](AsyncWebSocket *server, AsyncWebSocketClient *client,
             const uint8_t *data, size_t len) {
    auto id = data[0];
    handlePacket(id, data + 1, len - 1);
  });

  lastPingTime = millis();
  lastUpdateSentTime = millis();
}

void loop() {
  if (apState == AP_DISABLED) {
    if (millis() - lastUpdateSentTime > 180) {
      lastUpdateSentTime = millis();

      if (magCalibrating)
        sendMagPointPacket(mx, my, mz);
      else
        sendRotationPacket(getYaw());
    }

    tickWS();
    return;
  }

  button.tick();

  if (apState == AP_READY && !button.read()) {
    apState = AP_GOING;
    apStartTime = millis();
    anchoring = true;
    yawAnchor = getYaw();
  }

  if (apState == AP_GOING) {
    auto duration = millis() - apStartTime;

    apSpeed = 1500.0f + min(MAX_AP_SPEED_PERCENTAGE * 5.0f, 0.05f * duration);
    servo.write(MOTOR_PIN, apSpeed);

    if (duration > 30000) {
      servo.write(MOTOR_PIN, 0);
      anchoring = false;

      for (;;)
        yield();
    }
  }
}