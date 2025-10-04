#include <Arduino.h>
#include <Preferences.h>

#ifndef calibration_h
#define calibration_h

struct CalibrationStore {
  float gyroX;
  float gyroY;
  float gyroZ;
  float accelX;
  float accelY;
  float accelZ;
  float magX;
  float magY;
  float magZ;
  float magScale[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  float north;
  float servoMiddle;
  float maxAPSpeed;
};

extern Preferences calPrefs;
extern CalibrationStore calibration;

void saveBiasStore(CalibrationStore *store);
bool setupBiasesStorage();

#endif