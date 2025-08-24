#include "dmp.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <ICM_20948.h>

float yaw;
ICM_20948_I2C icm;

int setupDMP() {
  Wire.begin();
  Wire.setClock(400000);

  icm.begin(Wire, 0);

  bool success = icm.initializeDMP() == ICM_20948_Stat_Ok;
  success &= (icm.enableDMPSensor(INV_ICM20948_SENSOR_ORIENTATION) ==
              ICM_20948_Stat_Ok);
  success &= (icm.setDMPODRrate(DMP_ODR_Reg_Quat9, 0) == ICM_20948_Stat_Ok);
  success &= (icm.enableFIFO() == ICM_20948_Stat_Ok);
  success &= (icm.enableDMP() == ICM_20948_Stat_Ok);
  success &= (icm.resetDMP() == ICM_20948_Stat_Ok);
  success &= (icm.resetFIFO() == ICM_20948_Stat_Ok);

  if (success) {
    Serial.println("DMP enabled!");
  } else {
    Serial.println("Enable DMP failed!");
    return -1;
  }

  success &= (icm.setBiasGyroX(-162816) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasGyroY(79072) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasGyroZ(6528) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasAccelX(-33792) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasAccelY(-697344) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasAccelZ(48128) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasCPassX(-837376) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasCPassY(585920) == ICM_20948_Stat_Ok);
  success &= (icm.setBiasCPassZ(-1725344) == ICM_20948_Stat_Ok);

  Serial.println("Biases restored.");

  return 0;
}

void tickDMP() {
  icm_20948_DMP_data_t data;
  icm.readDMPdataFromFIFO(&data);

  if (icm.status == ICM_20948_Stat_Ok ||
      icm.status == ICM_20948_Stat_FIFOMoreDataAvail) {
    if ((data.header & DMP_header_bitmap_Quat9) > 0) {
      double q1 = ((double)data.Quat9.Data.Q1) / 1073741824.0;
      double q2 = ((double)data.Quat9.Data.Q2) / 1073741824.0;
      double q3 = ((double)data.Quat9.Data.Q3) / 1073741824.0;
      double q0 = sqrt(1.0 - ((q1 * q1) + (q2 * q2) + (q3 * q3)));

      double qw = q0; // See issue #145 - thank you @Gord1
      double qx = q2;
      double qy = q1;
      double qz = -q3;

      // roll (x-axis rotation)
      double t0 = +2.0 * (qw * qx + qy * qz);
      double t1 = +1.0 - 2.0 * (qx * qx + qy * qy);
      double roll = atan2(t0, t1) * 180.0 / PI;

      // pitch (y-axis rotation)
      double t2 = +2.0 * (qw * qy - qx * qz);
      t2 = t2 > 1.0 ? 1.0 : t2;
      t2 = t2 < -1.0 ? -1.0 : t2;
      double pitch = asin(t2) * 180.0 / PI;

      // yaw (z-axis rotation)
      double t3 = +2.0 * (qw * qz + qx * qy);
      double t4 = +1.0 - 2.0 * (qy * qy + qz * qz);
      yaw = atan2(t3, t4) * 180.0 / PI;
    }
  }

  // if (icm.status != ICM_20948_Stat_FIFOMoreDataAvail)
  //   delay(10);
}