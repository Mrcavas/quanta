#include "packets.h"

void sendMessagePacket(String str) {
  auto buf = ws.makeBuffer(1 + str.length());
  uint8_t *p = buf->get();

  p[0] = 0xbb;
  memcpy(p + 1, str.c_str(), str.length());

  ws.binaryAll(buf);
}

void sendInitPacket(float kp, float ki, float kd) {
  auto buf = ws.makeBuffer(1 + 3 * 4);
  uint8_t *p = buf->get();

  p[0] = 0x01;
  memcpy(p + 1, &kp, 4);
  memcpy(p + 1 + 4, &ki, 4);
  memcpy(p + 1 + 2 * 4, &kd, 4);

  ws.binaryAll(buf);
}

void sendAnchoringPacket(bool *anchoring) {
  auto buf = ws.makeBuffer(1 + sizeof(bool));
  uint8_t *p = buf->get();

  p[0] = 0x0a;
  memcpy(p + 1, anchoring, sizeof(bool));

  ws.binaryAll(buf);
}

void sendRotationPacket(float yaw) {
  auto buf = ws.makeBuffer(1 + sizeof(yaw));
  uint8_t *p = buf->get();

  p[0] = 0x10;
  memcpy(p + 1, &yaw, sizeof(yaw));

  ws.binaryAll(buf);
}

void sendYawAnchorPacket(float yaw) {
  auto buf = ws.makeBuffer(1 + sizeof(yaw));
  uint8_t *p = buf->get();

  p[0] = 0x11;
  memcpy(p + 1, &yaw, sizeof(yaw));

  ws.binaryAll(buf);
}

void sendMagPointPacket(float mx, float my, float mz) {
  auto buf = ws.makeBuffer(1 + 4 * 3);
  uint8_t *p = buf->get();

  p[0] = 0xc0;
  memcpy(p + 1, &mx, 4);
  memcpy(p + 5, &my, 4);
  memcpy(p + 9, &mz, 4);

  ws.binaryAll(buf);
}

void sendGyroCalibrationProgressPacket(float percentage) {
  auto buf = ws.makeBuffer(1 + 4);
  uint8_t *p = buf->get();

  p[0] = 0xc2;
  memcpy(p + 1, &percentage, 4);

  ws.binaryAll(buf);
}

void sendAccelCalibrationProgressPacket(uint8_t axis, float percentage) {
  auto buf = ws.makeBuffer(1 + 4 + 1);
  uint8_t *p = buf->get();

  p[0] = 0xc6;
  memcpy(p + 1, &percentage, 4);
  memcpy(p + 5, &axis, 1);

  ws.binaryAll(buf);
}

void sendAccelCalibrationDataPacket(uint8_t axis, float ax, float ay,
                                    float az) {
  auto buf = ws.makeBuffer(1 + 4 * 3 + 1);
  uint8_t *p = buf->get();

  p[0] = 0xc7;
  memcpy(p + 1, &axis, 1);
  memcpy(p + 2, &ax, 4);
  memcpy(p + 6, &ay, 4);
  memcpy(p + 10, &az, 4);

  ws.binaryAll(buf);
}

void sendCalibrationDataPacket(CalibrationStore *cal) {
  auto buf = ws.makeBuffer(1 + sizeof(CalibrationStore));
  uint8_t *p = buf->get();

  p[0] = 0xa0;
  memcpy(p + 1, cal, sizeof(CalibrationStore));

  ws.binaryAll(buf);
}