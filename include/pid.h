#include "Arduino.h"
#include "main.h"
#include "uPID.h"

extern uPIDfast<I_SATURATE | PID_REVERSE> pid;

void setupPID(uint16_t period_, float *yaw_,
              std::function<void(float)> onOutput_);

void saveCoefficients();
bool loadCoefficients();

void startPidTask();
void stopPidTask();