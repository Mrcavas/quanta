#include "sensor.h"
#include "calibration.h"
#include "packets.h"
#include "strprintf.h"
#include "ws.h"
#include <ADXL345.h>
#include <Fusion.h>
#include <ITG3200.h>
#include <MechaQMC5883.h>
#include <RunningAverage.h>
#include <Wire.h>
#include <driver/i2c.h>

const int IMU_TASK_PERIOD_MS = 1000 / SAMPLE_RATE;

MechaQMC5883 qmc;
ITG3200 gyro;
ADXL345 adxl;

static TaskHandle_t imuTaskHandle = NULL;
static float yaw = 0.0f;
static SemaphoreHandle_t yawMutex;
static IMUCallback onYawUpdateCallback = NULL;

uint8_t noDelayCount = 0;

RunningAverage magX(15);
RunningAverage magY(15);
RunningAverage magZ(15);
RunningAverage gyroX(15);
RunningAverage gyroY(15);
RunningAverage gyroZ(15);

#define YAW_WINDOW 15
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

  FusionOffset offset;
  FusionAhrs ahrs;

  FusionOffsetInitialise(&offset, SAMPLE_RATE);
  FusionAhrsInitialise(&ahrs);

  const FusionAhrsSettings settings = {
      .convention = FusionConventionNed,
      .gain = 0.6f,
      .gyroscopeRange = 1000.0f,
      .accelerationRejection = 10.0f,
      .magneticRejection = 10.0f,
      .recoveryTriggerPeriod = 5 * SAMPLE_RATE,
  };
  FusionAhrsSetSettings(&ahrs, &settings);

  TickType_t xLastWakeTime = xTaskGetTickCount();

  for (;;) {
    int mx_raw, my_raw, mz_raw;
    int16_t gx_raw, gy_raw, gz_raw;
    int16_t ax_raw, ay_raw, az_raw;

    adxl.getAcceleration(&ax_raw, &ay_raw, &az_raw);
    gyro.getRotation(&gx_raw, &gy_raw, &gz_raw);
    qmc.read(&mx_raw, &my_raw, &mz_raw);

    gyroX.add(gx_raw * 0.0695652174f);
    gyroY.add(gy_raw * 0.0695652174f);
    gyroZ.add(gz_raw * 0.0695652174f);
    magX.add(mx_raw * 0.0083333333f);
    magY.add(my_raw * 0.0083333333f);
    magZ.add(mz_raw * 0.0083333333f);

    RawICUData raw = {.ax = ax_raw * 0.004f,
                      .ay = ay_raw * 0.004f,
                      .az = az_raw * 0.004f,
                      .gx = gyroX.getAverage(),
                      .gy = gyroY.getAverage(),
                      .gz = gyroZ.getAverage(),
                      .mx = magX.getAverage(),
                      .my = magY.getAverage(),
                      .mz = magZ.getAverage()};

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

    const uint32_t timestamp = micros();

    FusionVector gyroscope = {gx, gy, gz};
    FusionVector accelerometer = {ax, ay, az};
    FusionVector magnetometer = {mx_final, my_final, mz_final};

    gyroscope = FusionOffsetUpdate(&offset, gyroscope);

    static uint32_t previousTimestamp;
    const float deltaTime = (float)(timestamp - previousTimestamp) / 1000000;
    previousTimestamp = timestamp;

    FusionAhrsUpdate(&ahrs, gyroscope, accelerometer, magnetometer, deltaTime);

    const FusionEuler euler =
        FusionQuaternionToEuler(FusionAhrsGetQuaternion(&ahrs));

    float newYaw = fmodf(euler.angle.yaw - calibration.north + 360.0f, 360.0f);
    newYaw = filterYaw(newYaw);

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
  qmc.setMode(Mode_Continuous, ODR_50Hz, RNG_2G, OSR_128);

  gyro.initialize();
  if (!gyro.testConnection())
    return -2;

  gyro.setDLPFBandwidth(ITG3200_DLPF_BW_98);
  gyro.setRate(19);

  adxl.initialize();
  if (!adxl.testConnection())
    return -3;

  adxl.setRange(0x0);
  adxl.setRate(ADXL345_RATE_50);
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