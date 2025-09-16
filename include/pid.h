#include "Arduino.h"
#include "main.h"
#include "uPID.h"

extern uPIDfast<I_SATURATE | PID_REVERSE> pid;

void setupPID();
float tickPID(float yawAnchor, float yaw);

void saveCoefficients();
bool loadCoefficients();