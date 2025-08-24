////////////////////////////////////////////////////////////////////////////
//
//  This file is part of RTIMULib
//
//  Copyright (c) 2014-2015, richards-tech, LLC
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to
//  deal in the Software without restriction, including without limitation the
//  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
//  sell copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
//  IN THE SOFTWARE.

#include "RTIMUGY85.h"
#include "Arduino.h"
#include "RTIMUSettings.h"

#if defined(GY85)

//  this sets the learning rate for compass running average calculation

#define COMPASS_ALPHA 0.2f

RTIMUGY85::RTIMUGY85(RTIMUSettings *settings) : RTIMU(settings) {
  m_sampleRate = 100;
}

RTIMUGY85::~RTIMUGY85() {}

int RTIMUGY85::IMUInit() {
  unsigned char result;

  //  configure IMU

  m_accelSlaveAddr = m_settings->m_I2CSlaveAddress;

  // work out gyro address
  if (I2Cdev::readBytes(ITG3205_ADDRESS0, ITG3205_WHO_AM_I, 1, &result)) {
    if (result == ITG3205_ID) {
      m_gyroSlaveAddr = ITG3205_ADDRESS0;
    }
  } else {
    m_gyroSlaveAddr = ITG3205_ADDRESS1;
  }

  // work out compass address

  // Serial.println("1");
  if (I2Cdev::readBytes(HMC5883L_ADDRESS0, HMC5883L_WHO_AM_I, 1, &result)) {
    if (result == HMC5883L_ID) {
      m_compassSlaveAddr = HMC5883L_ADDRESS0;
      Serial.println("заебок");
    }
  } else {
    m_compassSlaveAddr = HMC5883L_ADDRESS0;
    Serial.println("какая-то наебка");
  }

  accelInit();
  gyroInit();
  compassInit();

  gyroBiasInit();

  return 0;
}

void RTIMUGY85::accelInit() {
  I2Cdev::writeByte(m_accelSlaveAddr, ADXL345_PWR_CTL, 0x0);
  I2Cdev::writeByte(m_accelSlaveAddr, ADXL345_PWR_CTL,
                    ADXL345_OPER_MODE_MEASURE);
  setAccelRange();
  setAccelSampleRate();
}

void RTIMUGY85::setAccelSampleRate() {
  I2Cdev::writeByte(m_accelSlaveAddr, ADXL345_BW_RATE,
                    m_settings->m_GY85AccelSampleRate);
}

void RTIMUGY85::setAccelRange() {
  bool valid_config = true;
  switch (m_settings->m_GY85AccelFsr) {
  case ADXL345_FSR_2:
    m_accelScale = (RTFLOAT)0.0039;
    break;
  case ADXL345_FSR_4:
    m_accelScale = (RTFLOAT)0.0078;
    break;

  case ADXL345_FSR_8:
    m_accelScale = (RTFLOAT)0.0156;
    break;

  case ADXL345_FSR_16:
    m_accelScale = (RTFLOAT)0.0312;
    break;

  case ADXL345_FSR_FULL:
    m_accelScale = (RTFLOAT)0.0039;
    break;

  default:
    valid_config = false;
  }
  if (valid_config) {
    switch (m_settings->m_GY85AccelFsr) {
    case ADXL345_FSR_FULL:
      I2Cdev::writeByte(m_accelSlaveAddr, ADXL345_DATA_FORMAT, 1 << 3);
      break;

    default:
      I2Cdev::writeByte(m_accelSlaveAddr, ADXL345_DATA_FORMAT,
                        m_settings->m_GY85AccelFsr);
    }
  }
}

void RTIMUGY85::gyroInit() {
  I2Cdev::writeByte(m_gyroSlaveAddr, ITG3205_DLPF_FS, (0x03 << 3));
  setGyroBW();
  setGyroSampleRate();
  m_gyroScale = (RTFLOAT)69.56521739 * 1e-3 * RTMATH_DEGREE_TO_RAD;
}

void RTIMUGY85::setGyroBW() {
  uint8_t reg;
  I2Cdev::readBytes(m_gyroSlaveAddr, ITG3205_DLPF_FS, 1, &reg);
  I2Cdev::writeByte(m_gyroSlaveAddr, ITG3205_DLPF_FS,
                    (reg | m_settings->m_GY85GyroBW));
}

void RTIMUGY85::setGyroSampleRate() {
  I2Cdev::writeByte(m_gyroSlaveAddr, ITG3205_SMPLRT_DIV,
                    m_settings->m_GY85GyroSampleRate);
}

void RTIMUGY85::compassInit() {
  I2Cdev::writeByte(m_compassSlaveAddr, HMC5883L_MODE, 0x0);
  setCompassRange();
  setCompassSampleRate();
}

void RTIMUGY85::setCompassSampleRate() {
  uint8_t reg;
  I2Cdev::readBytes(m_compassSlaveAddr, HMC5883L_CONFIG_A, 1, &reg);
  reg = reg & 0xE3;
  I2Cdev::writeByte(m_compassSlaveAddr, HMC5883L_CONFIG_A,
                    (reg | (m_settings->m_GY85CompassSampleRate << 2)));
}

void RTIMUGY85::setCompassRange() {
  bool valid_config = true;
  float scale_factor = (1e-1);
  switch (m_settings->m_GY85CompassFsr) {
  case HMC5883L_FSR_0P88:
    m_compassScale = (RTFLOAT)0.73 * scale_factor;
    break;
  case HMC5883L_FSR_1P3:
    m_compassScale = (RTFLOAT)0.92 * scale_factor;
    break;
  case HMC5883L_FSR_1P9:
    m_compassScale = (RTFLOAT)1.22 * scale_factor;
    break;
  case HMC5883L_FSR_2P5:
    m_compassScale = (RTFLOAT)1.52 * scale_factor;
    break;
  case HMC5883L_FSR_4:
    m_compassScale = (RTFLOAT)2.27 * scale_factor;
    break;
  case HMC5883L_FSR_4P7:
    m_compassScale = (RTFLOAT)2.56 * scale_factor;
    break;
  case HMC5883L_FSR_5P6:
    m_compassScale = (RTFLOAT)3.03 * scale_factor;
    break;
  case HMC5883L_FSR_8P1:
    m_compassScale = (RTFLOAT)4.35 * scale_factor;
  default:
    valid_config = false;
  }
  if (valid_config) {
    I2Cdev::writeByte(m_compassSlaveAddr, HMC5883L_CONFIG_B,
                      m_settings->m_GY85CompassFsr << 7);
  }
}

int RTIMUGY85::IMUGetPollInterval() { return (1000 / m_sampleRate); }

bool RTIMUGY85::IMURead() {
  unsigned char accelData[6];
  unsigned char gyroData[6];
  unsigned char compassData[6];

  I2Cdev::readBytes(m_accelSlaveAddr, ADXL345_DATA_X_LSB, 6, accelData);
  I2Cdev::readBytes(m_gyroSlaveAddr, ITG3205_GYRO_XOUT_H, 6, gyroData);
  // I2Cdev::readBytes(m_compassSlaveAddr, HMC5883L_DATAX_H, 6);
  I2Cdev::readBytes(m_compassSlaveAddr, HMC5883L_DATAX_H, 2, compassData + 0);
  I2Cdev::readBytes(m_compassSlaveAddr, HMC5883L_DATAZ_H, 2, compassData + 4);
  I2Cdev::readBytes(m_compassSlaveAddr, HMC5883L_DATAY_H, 2, compassData + 2);

  m_timestamp = millis();

  RTMath::convertToVector(accelData, m_accel, m_accelScale, false);
  RTMath::convertToVector(gyroData, m_gyro, m_gyroScale, true);
  RTMath::convertToVector(compassData, m_compass, m_compassScale, true);

  //  sort out accel data;

  m_accel.setX(-m_accel.x());

  //  sort out gyro axes and correct for bias

  m_gyro.setX(m_gyro.x());
  m_gyro.setY(-m_gyro.y());
  m_gyro.setZ(-m_gyro.z());

  //  sort out compass axes

  m_compass.setY(-m_compass.y());

  //  now do standard processing

  handleGyroBias();
  calibrateAverageCompass();
  //
  //  now update the filter
  //
  //
  return true;
}

#endif