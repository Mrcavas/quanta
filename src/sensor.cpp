#include "sensor.h"
#include "calibration.h"
#include "packets.h"
#include "strprintf.h"
#include "ws.h"
#include <Adafruit_AHRS.h>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>

const int IMU_TASK_PERIOD_MS = 1000 / SAMPLE_RATE;

Adafruit_MPU6050 mpu;
Adafruit_HMC5883_Unified hmc;

Adafruit_NXPSensorFusion fusion;
static TaskHandle_t imuTaskHandle = NULL;
static float yaw = 0.0f;
static SemaphoreHandle_t yawMutex;
static IMUCallback onYawUpdateCallback = NULL;

uint8_t noDelayCount = 0;

uint8_t readRegister8(uint8_t reg) {
  uint8_t value = 0;
  Wire.beginTransmission(0x1E);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x1E, (uint8_t)1);
  while (!Wire.available()) {
  };
  value = Wire.read();
  return value;
}

void writeRegister8(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(0x1E);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

void imuTask(void *pvParameters) {
  // Serial.println("Task start");
  fusion.begin(SAMPLE_RATE);

  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    sensors_event_t acc, gyro, temp, mag;

    hmc.getEvent(&mag);
    mpu.getEvent(&acc, &gyro, &temp);

    RawICUData raw = {.ax = acc.acceleration.x / SENSORS_GRAVITY_EARTH,
                      .ay = acc.acceleration.y / SENSORS_GRAVITY_EARTH,
                      .az = acc.acceleration.z / SENSORS_GRAVITY_EARTH,
                      .gx = gyro.gyro.x * SENSORS_RADS_TO_DPS,
                      .gy = gyro.gyro.y * SENSORS_RADS_TO_DPS,
                      .gz = gyro.gyro.z * SENSORS_RADS_TO_DPS,
                      .mx = mag.magnetic.x,
                      .my = mag.magnetic.y,
                      .mz = mag.magnetic.z};

    float gx = raw.gx - calibration.gyroX;
    float gy = raw.gy - calibration.gyroY;
    float gz = raw.gz - calibration.gyroZ;
    float ax = raw.ax - calibration.accelX;
    float ay = raw.ay - calibration.accelY;
    float az = raw.az - calibration.accelZ;
    float mx = raw.mx - calibration.magX;
    float my = raw.my - calibration.magY;
    float mz = raw.mz - calibration.magZ;

    float mx_final = calibration.magScale[0][0] * mx +
                     calibration.magScale[0][1] * my +
                     calibration.magScale[0][2] * mz;
    float my_final = calibration.magScale[1][0] * mx +
                     calibration.magScale[1][1] * my +
                     calibration.magScale[1][2] * mz;
    float mz_final = calibration.magScale[2][0] * mx +
                     calibration.magScale[2][1] * my +
                     calibration.magScale[2][2] * mz;

    fusion.update(gx, gy, gz, ax, ay, az, mx_final, my_final, mz_final);

    float newYaw = fmodf(fusion.getYaw() - calibration.north, 360.0f);

    // 5. Update the global yaw variable (for the thread-safe getter)
    if (xSemaphoreTake(yawMutex, (TickType_t)10) == pdTRUE) {
      yaw = newYaw;
      xSemaphoreGive(yawMutex);
    }

    // 6. NEW: Call the PID callback function if it has been registered.
    if (onYawUpdateCallback != NULL) {
      onYawUpdateCallback(newYaw, raw);
    }

    if (xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(IMU_TASK_PERIOD_MS)) ==
        pdFALSE) {
      noDelayCount++;

      if (noDelayCount == SAMPLE_RATE * 2) {
        sendMessagePacket(strprintf("SAMPLE_RATE is too big"));
        noDelayCount = 0;
      }
    }
  }
}

bool setupIMU(IMUCallback pidCallback) {
  onYawUpdateCallback = pidCallback;

  if (!mpu.begin()) {
    // Serial.println("MPU not initialized");
    return false;
  }

  mpu.setI2CBypass(true);

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_260_HZ);

  if (!hmc.begin()) {
    // Serial.println("Magnetometer not initialized");
    return false;
  }

  uint8_t value = readRegister8(0x00);
  value &= 0b11100011;
  value |= (0b110 << 2);
  writeRegister8(0x00, value);

  yawMutex = xSemaphoreCreateMutex();
  if (yawMutex == NULL)
    return false;

  xTaskCreatePinnedToCore(imuTask, "IMU Task", 8192, NULL, 1, &imuTaskHandle,
                          0);
  return true;
}

float getYaw() {
  float currentYaw = 0.0f;

  if (yawMutex != NULL && xSemaphoreTake(yawMutex, (TickType_t)10) == pdTRUE) {
    currentYaw = yaw;
    xSemaphoreGive(yawMutex);
  }
  return currentYaw;
}