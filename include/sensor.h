#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>

#define SAMPLE_RATE 10

struct RawICUData {
  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;
};

typedef void (*IMUCallback)(float yaw, RawICUData raw);

int8_t setupIMU(IMUCallback pidCallback);
float getYaw();