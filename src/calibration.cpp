#include "calibration.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <ICM_20948.h>

struct biasStore {
  int32_t header = 0x42;
  int32_t biasGyroX = 0;
  int32_t biasGyroY = 0;
  int32_t biasGyroZ = 0;
  int32_t biasAccelX = 0;
  int32_t biasAccelY = 0;
  int32_t biasAccelZ = 0;
  int32_t biasCPassX = 0;
  int32_t biasCPassY = 0;
  int32_t biasCPassZ = 0;
  int32_t sum = 0;
};

void updateBiasStoreSum(biasStore *store) {
  int32_t sum = store->header;
  sum += store->biasGyroX;
  sum += store->biasGyroY;
  sum += store->biasGyroZ;
  sum += store->biasAccelX;
  sum += store->biasAccelY;
  sum += store->biasAccelZ;
  sum += store->biasCPassX;
  sum += store->biasCPassY;
  sum += store->biasCPassZ;
  store->sum = sum;
}

bool isBiasStoreValid(biasStore *store) {
  int32_t sum = store->header;

  if (sum != 0x42)
    return false;

  sum += store->biasGyroX;
  sum += store->biasGyroY;
  sum += store->biasGyroZ;
  sum += store->biasAccelX;
  sum += store->biasAccelY;
  sum += store->biasAccelZ;
  sum += store->biasCPassX;
  sum += store->biasCPassY;
  sum += store->biasCPassZ;

  return (store->sum == sum);
}

bool readBiases(ICM_20948_I2C *icm, biasStore *store) {
  bool success = (icm->getBiasGyroX(&store->biasGyroX) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasGyroY(&store->biasGyroY) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasGyroZ(&store->biasGyroZ) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasAccelX(&store->biasAccelX) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasAccelY(&store->biasAccelY) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasAccelZ(&store->biasAccelZ) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasCPassX(&store->biasCPassX) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasCPassY(&store->biasCPassY) == ICM_20948_Stat_Ok);
  success &= (icm->getBiasCPassZ(&store->biasCPassZ) == ICM_20948_Stat_Ok);

  updateBiasStoreSum(store);

  return success;
}

bool writeBiases(ICM_20948_I2C *icm, biasStore *store) {
  bool success = (icm->setBiasGyroX(store->biasGyroX) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasGyroY(store->biasGyroY) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasGyroZ(store->biasGyroZ) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasAccelX(store->biasAccelX) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasAccelY(store->biasAccelY) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasAccelZ(store->biasAccelZ) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasCPassX(store->biasCPassX) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasCPassY(store->biasCPassY) == ICM_20948_Stat_Ok);
  success &= (icm->setBiasCPassZ(store->biasCPassZ) == ICM_20948_Stat_Ok);

  return success;
}

bool saveBiasStore(biasStore *store) {
  EEPROM.put(0, store);
  EEPROM.commit();

  biasStore readStore;
  EEPROM.get(0, readStore);

  return isBiasStoreValid(&readStore);
}

bool setupBiasesStorage(ICM_20948_I2C *icm) {
  if (!EEPROM.begin(128))
    return false;

  biasStore store;
  EEPROM.get(0, store);

  if (isBiasStoreValid(&store))
    writeBiases(icm, &store);

  return true;
}
