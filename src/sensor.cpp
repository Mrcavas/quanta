#include "sensor.h"
#include "calibration.h"
#include "packets.h"
#include "strprintf.h"
#include "ws.h"
#include <Adafruit_AHRS.h>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_MPU6050.h>
#include <Wire.h>
#include <driver/i2c.h>

const int IMU_TASK_PERIOD_MS = 1000 / SAMPLE_RATE;

Adafruit_MPU6050 mpu;
Adafruit_HMC5883_Unified hmc;

Adafruit_NXPSensorFusion fusion;
static TaskHandle_t imuTaskHandle = NULL;
static float yaw = 0.0f;
static SemaphoreHandle_t yawMutex;
static IMUCallback onYawUpdateCallback = NULL;

uint8_t noDelayCount = 0;

uint8_t magReadReg = 0x03;
uint8_t mpuReadReg = 0x3B;

#define YAW_WINDOW 37
static float yawBuffer[YAW_WINDOW];
static int yawIndex = 0;
static bool bufferFilled = false;

float filterYaw(float newYawDeg) {
  // Store new sample
  yawBuffer[yawIndex] = newYawDeg * PI / 180.0f; // store in radians
  yawIndex = (yawIndex + 1) % YAW_WINDOW;
  if (yawIndex == 0)
    bufferFilled = true;

  int count = bufferFilled ? YAW_WINDOW : yawIndex;

  // Average on the unit circle
  float sumSin = 0.0f, sumCos = 0.0f;
  for (int i = 0; i < count; i++) {
    sumSin += sin(yawBuffer[i]);
    sumCos += cos(yawBuffer[i]);
  }

  float avg = atan2(sumSin / count, sumCos / count); // radians
  if (avg < 0)
    avg += 2 * PI;

  return avg * 180.0f / PI; // back to degrees
}

void imuTask(void *pvParameters) {
  // Serial.println("Task start");
  fusion.begin(SAMPLE_RATE);
  // fusion.setBeta(0.2);

  TickType_t xLastWakeTime = xTaskGetTickCount();
  float newYaw = 0;

  for (;;) {
    uint8_t buf[14];
    RawICUData raw;

    // mpu.getEvent();

    i2c_master_write_read_device(I2C_NUM_0, MPU6050_I2CADDR_DEFAULT,
                                 &mpuReadReg, 1, buf, 14,
                                 1000 / portTICK_PERIOD_MS);
    raw.ax = ((int16_t)(buf[0] << 8 | buf[1])) / 4096.0f;
    raw.ay = ((int16_t)(buf[2] << 8 | buf[3])) / 4096.0f;
    raw.az = ((int16_t)(buf[4] << 8 | buf[5])) / 4096.0f; // g
    raw.gx = ((int16_t)(buf[8] << 8 | buf[9])) / 32.8f;
    raw.gy = ((int16_t)(buf[10] << 8 | buf[11])) / 32.8f;
    raw.gz = ((int16_t)(buf[12] << 8 | buf[13])) / 32.8f; // dps

    i2c_master_write_read_device(I2C_NUM_0, HMC5883_ADDRESS_MAG, &magReadReg, 1,
                                 buf, 6, 1000 / portTICK_PERIOD_MS);

    raw.mx = ((int16_t)(buf[1] | ((int16_t)buf[0] << 8))) * 0.0909090909f;
    raw.mz = ((int16_t)(buf[3] | ((int16_t)buf[2] << 8))) * 0.0909090909f;
    raw.my = ((int16_t)(buf[5] | ((int16_t)buf[4] << 8))) * 0.1020408163f; // uT

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

bool setupIMU(IMUCallback pidCallback) {
  onYawUpdateCallback = pidCallback;

  Wire.begin();
  Wire.setClock(1000000);

  if (!mpu.begin()) {
    // Serial.println("MPU not initialized");
    return false;
  }

  mpu.setI2CBypass(true);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_1000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_5_HZ);

  if (!hmc.begin()) {
    // Serial.println("Magnetometer not initialized");
    return false;
  }

  uint8_t value = 0;
  Wire.beginTransmission(0x1E);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)0x1E, (uint8_t)1);
  while (!Wire.available())
    yield();
  value = Wire.read();

  value &= 0b11100011;
  value |= (0b011 << 2); // 7.5hz
  // value |= (0b100 << 2); // 15hz
  // value |= (0b101 << 2); // 30hz
  // value |= (0b110 << 2); // 75hz
  // value |= (0b111 << 2); // 220hz

  Wire.beginTransmission(0x1E);
  Wire.write(0x00);
  Wire.write(value);
  Wire.endTransmission();

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