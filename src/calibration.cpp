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
}

bool setupBiasesStorage() {
  if (!calPrefs.begin("calibration", false))
    return false;

  if (calPrefs.isKey("gyroX")) {
    calibration.gyroX = calPrefs.getFloat("gyroX");
    calibration.gyroY = calPrefs.getFloat("gyroY");
    calibration.gyroZ = calPrefs.getFloat("gyroZ");
    calibration.accelX = calPrefs.getFloat("accelX");
    calibration.accelY = calPrefs.getFloat("accelY");
    calibration.accelZ = calPrefs.getFloat("accelZ");
    calibration.magX = calPrefs.getFloat("magX");
    calibration.magY = calPrefs.getFloat("magY");
    calibration.magZ = calPrefs.getFloat("magZ");
    calibration.magScale[0][0] = calPrefs.getFloat("magScale0");
    calibration.magScale[0][1] = calPrefs.getFloat("magScale1");
    calibration.magScale[0][2] = calPrefs.getFloat("magScale2");
    calibration.magScale[1][0] = calPrefs.getFloat("magScale3");
    calibration.magScale[1][1] = calPrefs.getFloat("magScale4");
    calibration.magScale[1][2] = calPrefs.getFloat("magScale5");
    calibration.magScale[2][0] = calPrefs.getFloat("magScale6");
    calibration.magScale[2][1] = calPrefs.getFloat("magScale7");
    calibration.magScale[2][2] = calPrefs.getFloat("magScale8");
    calibration.north = calPrefs.getFloat("north");
  }

  return true;
}
