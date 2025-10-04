#include "calibration.h"
#include <Arduino.h>
#include <Preferences.h>

Preferences calPrefs;
CalibrationStore calibration;

void saveBiasStore(CalibrationStore *store) {
  calPrefs.putFloat("gyroX", store->gyroX);
  calPrefs.putFloat("gyroY", store->gyroY);
  calPrefs.putFloat("gyroZ", store->gyroZ);
  calPrefs.putFloat("accelX", store->accelX);
  calPrefs.putFloat("accelY", store->accelY);
  calPrefs.putFloat("accelZ", store->accelZ);
  calPrefs.putFloat("magX", store->magX);
  calPrefs.putFloat("magY", store->magY);
  calPrefs.putFloat("magZ", store->magZ);
  calPrefs.putFloat("magScale0", store->magScale[0][0]);
  calPrefs.putFloat("magScale1", store->magScale[0][1]);
  calPrefs.putFloat("magScale2", store->magScale[0][2]);
  calPrefs.putFloat("magScale3", store->magScale[1][0]);
  calPrefs.putFloat("magScale4", store->magScale[1][1]);
  calPrefs.putFloat("magScale5", store->magScale[1][2]);
  calPrefs.putFloat("magScale6", store->magScale[2][0]);
  calPrefs.putFloat("magScale7", store->magScale[2][1]);
  calPrefs.putFloat("magScale8", store->magScale[2][2]);
  calPrefs.putFloat("north", store->north);
  calPrefs.putFloat("servoMiddle", store->servoMiddle);
  calPrefs.putFloat("maxAPSpeed", store->maxAPSpeed);
}

bool setupBiasesStorage() {
  if (!calPrefs.begin("calibration", false))
    return false;

  calibration.gyroX = calPrefs.getFloat("gyroX", 0);
  calibration.gyroY = calPrefs.getFloat("gyroY", 0);
  calibration.gyroZ = calPrefs.getFloat("gyroZ", 0);
  calibration.accelX = calPrefs.getFloat("accelX", 0);
  calibration.accelY = calPrefs.getFloat("accelY", 0);
  calibration.accelZ = calPrefs.getFloat("accelZ", 0);
  calibration.magX = calPrefs.getFloat("magX", 0);
  calibration.magY = calPrefs.getFloat("magY", 0);
  calibration.magZ = calPrefs.getFloat("magZ", 0);
  calibration.magScale[0][0] = calPrefs.getFloat("magScale0", 1);
  calibration.magScale[0][1] = calPrefs.getFloat("magScale1", 0);
  calibration.magScale[0][2] = calPrefs.getFloat("magScale2", 0);
  calibration.magScale[1][0] = calPrefs.getFloat("magScale3", 0);
  calibration.magScale[1][1] = calPrefs.getFloat("magScale4", 1);
  calibration.magScale[1][2] = calPrefs.getFloat("magScale5", 0);
  calibration.magScale[2][0] = calPrefs.getFloat("magScale6", 0);
  calibration.magScale[2][1] = calPrefs.getFloat("magScale7", 0);
  calibration.magScale[2][2] = calPrefs.getFloat("magScale8", 1);
  calibration.north = calPrefs.getFloat("north", 0);
  calibration.servoMiddle = calPrefs.getFloat("servoMiddle", 91.5);
  calibration.maxAPSpeed = calPrefs.getFloat("maxAPSpeed", 25);

  return true;
}
