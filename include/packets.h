#include "calibration.h"
#include "ws.h"
#include <Arduino.h>

void sendMessagePacket(String str);

void sendInitPacket(float kp, float ki, float kd);

void sendAnchoringPacket(bool *anchoring);

void sendRotationPacket(float yaw);

void sendYawAnchorPacket(float yaw);

void sendMagPointPacket(float mx, float my, float mz);

void sendGyroCalibrationProgressPacket(float percentage);

void sendAccelCalibrationProgressPacket(uint8_t axis, float percentage);

void sendAccelCalibrationDataPacket(uint8_t axis, float ax, float ay, float az);

void sendCalibrationDataPacket(CalibrationStore *cal);