#include "sensor.h"
#include "calibration.h"
#include "packets.h"
#include "strprintf.h"
#include "ws.h"
#include <ADXL345.h>
#include <Adafruit_AHRS.h>
#include <ITG3200.h>
#include <MechaQMC5883.h>
#include <Wire.h>
#include <driver/i2c.h>

const int IMU_TASK_PERIOD_MS = 1000 / SAMPLE_RATE;

MechaQMC5883 qmc;
ITG3200 gyro;
ADXL345 adxl;

Adafruit_NXPSensorFusion fusion;
static TaskHandle_t imuTaskHandle = NULL;
static float yaw = 0.0f;
static SemaphoreHandle_t yawMutex;
static IMUCallback onYawUpdateCallback = NULL;

uint8_t noDelayCount = 0;

void imuTask(void *pvParameters) {
  // Serial.println("Task start");
  fusion.begin(SAMPLE_RATE);
  // fusion.setBeta(0.2);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  float newYaw = 0;

  for (;;) {
    int mx_raw, my_raw, mz_raw;
    int16_t gx_raw, gy_raw, gz_raw;
    int16_t ax_raw, ay_raw, az_raw;

    adxl.getAcceleration(&ax_raw, &ay_raw, &az_raw);
    gyro.getRotation(&gx_raw, &gy_raw, &gz_raw);
    qmc.read(&mx_raw, &my_raw, &mz_raw);

    RawICUData raw = {.ax = ax_raw * 0.004f,
                      .ay = ay_raw * 0.004f,
                      .az = az_raw * 0.004f,
                      .gx = gx_raw * 0.0695652174f,
                      .gy = gy_raw * 0.0695652174f,
                      .gz = gz_raw * 0.0695652174f,
                      .mx = mx_raw * 0.0083333333f,
                      .my = my_raw * 0.0083333333f,
                      .mz = mz_raw * 0.0083333333f};

    float ax = raw.ax - calibration.accelX;
    float ay = raw.ay - calibration.accelY;
    float az = raw.az - calibration.accelZ;
    float gx = raw.gx - calibration.gyroX;
    float gy = raw.gy - calibration.gyroY;
    float gz = raw.gz - calibration.gyroZ;
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

    newYaw = fusion.getYaw();
    newYaw = fmodf(newYaw - calibration.north + 360.0f, 360.0f);
    // newYaw = filterYaw(newYaw);

    if (xSemaphoreTake(yawMutex, (TickType_t)10) == pdTRUE) {
      yaw = newYaw;
      xSemaphoreGive(yawMutex);
    }

    if (onYawUpdateCallback != NULL)
      onYawUpdateCallback(newYaw, raw);

    if (noDelayCount == SAMPLE_RATE) {
      sendMessagePacket(strf("SAMPLE_RATE is too big"));
      // sendMessagePacket(strf("%d micros", time2 - time1));
      noDelayCount = 0;
    }

    if (xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(IMU_TASK_PERIOD_MS)) ==
        pdFALSE)
      noDelayCount++;
  }
}

int8_t setupIMU(IMUCallback pidCallback) {
  onYawUpdateCallback = pidCallback;

  Wire.begin();
  Wire.setClock(1000000);

  qmc.init();
  qmc.setMode(Mode_Continuous, ODR_10Hz, RNG_2G, OSR_128);

  gyro.initialize();
  if (!gyro.testConnection())
    return -2;

  gyro.setDLPFBandwidth(ITG3200_DLPF_BW_10);
  gyro.setRate(99);

  adxl.initialize();
  if (!adxl.testConnection())
    return -3;

  adxl.setRange(0x0);
  adxl.setRate(ADXL345_RATE_12P5);
  adxl.setLowPowerEnabled(false);
  adxl.setAutoSleepEnabled(false);
  adxl.setMeasureEnabled(true);

  yawMutex = xSemaphoreCreateMutex();
  if (yawMutex == NULL)
    return -4;

  xTaskCreatePinnedToCore(imuTask, "IMU Task", 8192, NULL, 1, &imuTaskHandle,
                          0);
  return 0;
}

float getYaw() {
  float currentYaw = 0.0f;

  if (yawMutex != NULL && xSemaphoreTake(yawMutex, (TickType_t)10) == pdTRUE) {
    currentYaw = yaw;
    xSemaphoreGive(yawMutex);
  }
  return currentYaw;
}