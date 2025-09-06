#include <Arduino.h>
#include <EEPROM.h>
#include <ICM_20948.h>

struct biasStore {
  int32_t header;
  int32_t biasGyroX;
  int32_t biasGyroY;
  int32_t biasGyroZ;
  int32_t biasAccelX;
  int32_t biasAccelY;
  int32_t biasAccelZ;
  int32_t biasCPassX;
  int32_t biasCPassY;
  int32_t biasCPassZ;
  int32_t sum;
};

void updateBiasStoreSum(biasStore *store);
bool isBiasStoreValid(biasStore *store);

bool readBiases(ICM_20948_I2C *icm, biasStore *store);
bool writeBiases(ICM_20948_I2C *icm, biasStore *store);

bool saveBiasStore(biasStore *store);

bool setupBiasesStorage(ICM_20948_I2C *icm);