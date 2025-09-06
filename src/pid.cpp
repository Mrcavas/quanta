#include "pid.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <uPID.h>

uPIDfast<I_SATURATE | PID_REVERSE> pid;
TaskHandle_t pidTaskHandle = NULL;
float *pYaw;
std::function<void(float)> onOutput;
uint16_t period;

void setupPID(uint16_t period_, float *pYaw_,
              std::function<void(float)> onOutput_) {

  pid.outMax = SERVO_MAX_DIFF;
  pid.outMin = -SERVO_MAX_DIFF;
  pid.setpoint = 0;

  period = period_;
  pYaw = pYaw_;
  onOutput = onOutput_;
}

void pidTask(void *parameter) {
  const float yawAnchor = *pYaw;
  const auto freq = pdMS_TO_TICKS(period);
  pid.setDt(period);

  TickType_t lastWakeTime = xTaskGetTickCount();
  for (;;) {
    float a = *pYaw + 180 - yawAnchor;
    if (a < 0)
      a += 360;
    if (a > 360)
      a -= 360;

    onOutput(pid.compute(a - 180));

    vTaskDelayUntil(&lastWakeTime, freq);
  }
}

void saveCoefficients() {
  File f = LittleFS.open("/pid_conf", "w");
  float buf[3] = {pid.getKp(), pid.getKi(), pid.getKd()};

  f.write((uint8_t *)buf, sizeof(buf));
}

bool loadCoefficients() {
  if (!LittleFS.exists("/pid_conf"))
    return false;

  File f = LittleFS.open("/pid_conf", "r");
  float buf[3] = {1, 0, 0};

  f.read((uint8_t *)buf, sizeof(buf));

  pid.setKp(buf[0]);
  pid.setKi(buf[1]);
  pid.setKd(buf[2]);

  return true;
}

void startPidTask() {
  xTaskCreatePinnedToCore(pidTask, "PID Task", 4096, NULL, 1, &pidTaskHandle,
                          1);
}

void stopPidTask() {
  if (pidTaskHandle != NULL) {
    vTaskDelete(pidTaskHandle);
    pidTaskHandle = NULL;
    // Serial.printfln("PID task deleted");
  }
}