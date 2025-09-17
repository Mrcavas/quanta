#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <Arduino.h>

#define SAMPLE_RATE 70

struct RawICUData {
  float ax, ay, az;
  float gx, gy, gz;
  float mx, my, mz;
};

typedef void (*IMUCallback)(float yaw, RawICUData raw);

bool setupIMU(IMUCallback pidCallback);
float getYaw();