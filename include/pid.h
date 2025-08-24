#include "Arduino.h"
#include "uPID.h"

extern uPIDfast<I_SATURATE | PID_REVERSE> pid;

void setupPID(uint16_t period_, std::function<float()> getYaw_,
              std::function<void(float)> onOutput_);

void saveCoefficients();
bool loadCoefficients();

void startPidTask(float yawAnchor);
void stopPidTask();