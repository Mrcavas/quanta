#include "pid.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <uPID.h>

uPIDfast<I_SATURATE | PID_REVERSE> pid;
TaskHandle_t pidTaskHandle = NULL;
std::function<float()> getYaw;
std::function<void(float)> onOutput;
uint16_t period;

void setupPID(uint16_t period_, std::function<float()> getYaw_,
              std::function<void(float)> onOutput_) {
  period = period_;
  getYaw = getYaw_;
  onOutput = onOutput_;
}

void pidTask(void *parameter) {
  const auto yawAnchor = *((float *)parameter);
  TickType_t lastWakeTime = xTaskGetTickCount();

  const auto freq = pdMS_TO_TICKS(period);
  pid.setDt(period);

  for (;;) {
    auto input = yawAnchor - getYaw();
    float output = pid.compute(input);
    Serial.printf("====== anchor: %f; yaw: %f\n====== pid %f -> %f\n",
                  yawAnchor, getYaw(), input, output);
    onOutput(output);

    vTaskDelayUntil(&lastWakeTime, freq);
  }
}

void saveCoefficients() {
  File f = LittleFS.open("/pid_conf", "w");
  float buf[3] = {pid.getKp(), pid.getKi(), pid.getKd()};

  f.write((uint8_t *)buf, sizeof(buf));
}

bool loadCoefficients() {
  if (!LittleFS.exists("/pid_conf")) {
    saveCoefficients();
    return false;
  }

  File f = LittleFS.open("/pid_conf", "r");
  float buf[3] = {0, 0, 0};

  f.read((uint8_t *)buf, sizeof(buf));

  pid.setKp(buf[0]);
  pid.setKi(buf[1]);
  pid.setKd(buf[2]);

  return true;
}

void startPidTask(float yawAnchor) {
  xTaskCreatePinnedToCore(pidTask, "PID Task", 4096, &yawAnchor, 1,
                          &pidTaskHandle, 1);
}

void stopPidTask() {
  if (pidTaskHandle != NULL) {
    vTaskDelete(pidTaskHandle);
    pidTaskHandle = NULL;
    Serial.println("PID task deleted");
  }
}