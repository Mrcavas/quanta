#include <Arduino.h>
#include <ICM_20948.h>

extern float yaw;
extern ICM_20948_I2C icm;

bool setupIMU();
void tickIMU();
