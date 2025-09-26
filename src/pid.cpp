#include "pid.h"
#include "sensor.h"
#include <Arduino.h>
#include <Preferences.h>
#include <uPID.h>

Preferences pidPrefs;
uPIDfast<I_SATURATE | PID_REVERSE> pid;

void setupPID() {
  pid = uPIDfast<I_SATURATE | PID_REVERSE>(1000.0f / SAMPLE_RATE);
  pid.outMax = SERVO_MAX_DIFF;
  pid.outMin = -SERVO_MAX_DIFF;
  pid.setpoint = 0;
}

float tickPID(float yawAnchor, float yaw) {
  float a = fmodf(yaw - yawAnchor + 540.0f, 360.0f) - 180.0f;

  if (a < -90)
    a += 180;
  else if (a > 90)
    a -= 180;

  return pid.compute(a);
}

void saveCoefficients() {
  pidPrefs.putFloat("kp", pid.getKp());
  pidPrefs.putFloat("ki", pid.getKi());
  pidPrefs.putFloat("kd", pid.getKd());
}

bool loadCoefficients() {
  if (!pidPrefs.begin("pid"))
    return false;

  if (!pidPrefs.isKey("kp") || !pidPrefs.isKey("ki") || !pidPrefs.isKey("kd")) {
    pid.setKp(0);
    pid.setKi(0);
    pid.setKd(0);
    saveCoefficients();
  } else {
    pid.setKp(pidPrefs.getFloat("kp"));
    pid.setKi(pidPrefs.getFloat("ki"));
    pid.setKd(pidPrefs.getFloat("kd"));
  }

  return true;
}