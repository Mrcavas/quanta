#include "sensor.h"
#include "calibration.h"
#include "packets.h"
#include "strprintf.h"
#include "ws.h"
// #include <Adafruit_ADXL345_U.h>
#include <Adafruit_AHRS.h>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_MPU6050.h>
// #include <Adafruit_QMC5883P.h>
// #include <ITG3200.h>
#include <Wire.h>

const int IMU_TASK_PERIOD_MS = 1000 / SAMPLE_RATE;

Adafruit_MPU6050 mpu;
Adafruit_HMC5883_Unified hmc = Adafruit_HMC5883_Unified(123);

Adafruit_NXPSensorFusion fusion;
static TaskHandle_t imuTaskHandle = NULL;
static float yaw = 0.0f;
static SemaphoreHandle_t yawMutex;
static IMUCallback onYawUpdateCallback = NULL;

int i = 0;

void imuTask(void *pvParameters) {
  // Serial.println("Task start");
  fusion.begin(SAMPLE_RATE);

  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    sensors_event_t acc, gyro, temp, mag;
    mpu.getEvent(&acc, &gyro, &temp);
    hmc.getEvent(&mag);

    float ax = acc.acceleration.x / SENSORS_GRAVITY_EARTH,
          ay = acc.acceleration.y / SENSORS_GRAVITY_EARTH,
          az = acc.acceleration.z / SENSORS_GRAVITY_EARTH;
    float gx = gyro.gyro.x * SENSORS_RADS_TO_DPS,
          gy = gyro.gyro.y * SENSORS_RADS_TO_DPS,
          gz = gyro.gyro.z * SENSORS_RADS_TO_DPS;
    float mx = mag.magnetic.x, my = mag.magnetic.y, mz = mag.magnetic.z;

    gx -= calibration.gyroX;
    gy -= calibration.gyroY;
    gz -= calibration.gyroZ;
    ax -= calibration.accelX;
    ay -= calibration.accelY;
    az -= calibration.accelZ;
    mx -= calibration.magX;
    my -= calibration.magY;
    mz -= calibration.magZ;
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

    // float qw_f, qx_f, qy_f, qz_f;
    // fusion.getQuaternion(&qw_f, &qx_f, &qy_f, &qz_f);
    // double qw = qw_f, qx = qx_f, qy = qy_f, qz = qz_f;
    // double t3 = +2.0 * (qw * qz + qx * qy);
    // double t4 = +1.0 - 2.0 * (qy * qy + qz * qz);
    // float newYaw = (float)(atan2(t3, t4) * 180.0 / PI);

    float newYaw = fusion.getYaw();

    // 5. Update the global yaw variable (for the thread-safe getter)
    if (xSemaphoreTake(yawMutex, (TickType_t)10) == pdTRUE) {
      yaw = newYaw;
      xSemaphoreGive(yawMutex);
    }

    // 6. NEW: Call the PID callback function if it has been registered.
    if (onYawUpdateCallback != NULL) {
      RawICUData raw = {.ax = ax,
                        .ay = ay,
                        .az = az,
                        .gx = gx,
                        .gy = gy,
                        .gz = gz,
                        .mx = mx,
                        .my = my,
                        .mz = mz};

      onYawUpdateCallback(newYaw, raw);
    }

    xTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(IMU_TASK_PERIOD_MS));
  }
}

bool setupIMU(IMUCallback pidCallback) {
  onYawUpdateCallback = pidCallback;

  if (!mpu.begin()) {
    Serial.println("MPU not initialized");
    return false;
  }

  mpu.setI2CBypass(true);

  mpu.setAccelerometerRange(MPU6050_RANGE_2_G);
  mpu.setGyroRange(MPU6050_RANGE_250_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);

  if (!hmc.begin()) {
    Serial.println("Magnetometer not initialized");
    return false;
  }

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