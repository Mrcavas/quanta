#include "packets.h"

WS_BUFFER *buildMessagePacket(AsyncWebSocket *ws, String str) {
  auto buf = ws->makeBuffer(1 + str.length());
  uint8_t *p = buf->get();

  p[0] = 0xbb;
  memcpy(p + 1, str.c_str(), str.length());

  return buf;
}

WS_BUFFER *buildInitPacket(AsyncWebSocket *ws, float kp, float ki, float kd) {
  auto buf = ws->makeBuffer(1 + 3 * 4);
  uint8_t *p = buf->get();

  p[0] = 0x01;
  memcpy(p + 1, &kp, 4);
  memcpy(p + 1 + 4, &ki, 4);
  memcpy(p + 1 + 2 * 4, &kd, 4);

  return buf;
}

WS_BUFFER *buildAnchoringPacket(AsyncWebSocket *ws, bool *anchoring) {
  auto buf = ws->makeBuffer(1 + sizeof(bool));
  uint8_t *p = buf->get();

  p[0] = 0x0a;
  memcpy(p + 1, anchoring, sizeof(bool));

  return buf;
}

WS_BUFFER *buildRotationPacket(AsyncWebSocket *ws, float yaw) {
  auto buf = ws->makeBuffer(1 + sizeof(yaw));
  uint8_t *p = buf->get();

  p[0] = 0x10;
  memcpy(p + 1, &yaw, sizeof(yaw));

  return buf;
}

WS_BUFFER *buildYawAnchorPacket(AsyncWebSocket *ws, float yaw) {
  auto buf = ws->makeBuffer(1 + sizeof(yaw));
  uint8_t *p = buf->get();

  p[0] = 0x11;
  memcpy(p + 1, &yaw, sizeof(yaw));

  return buf;
}

WS_BUFFER *buildMagPointPacket(AsyncWebSocket *ws, float mx, float my,
                               float mz) {
  auto buf = ws->makeBuffer(1 + 4 * 3);
  uint8_t *p = buf->get();

  p[0] = 0xc0;
  memcpy(p + 1, &mx, 4);
  memcpy(p + 5, &my, 4);
  memcpy(p + 9, &mz, 4);

  return buf;
}

WS_BUFFER *buildGyroCalibrationProgressPacket(AsyncWebSocket *ws,
                                              float percentage) {
  auto buf = ws->makeBuffer(1 + 4);
  uint8_t *p = buf->get();

  p[0] = 0xc2;
  memcpy(p + 1, &percentage, 4);

  return buf;
}

WS_BUFFER *buildAccelCalibrationProgressPacket(AsyncWebSocket *ws, uint8_t axis,
                                               float percentage) {
  auto buf = ws->makeBuffer(1 + 4 + 1);
  uint8_t *p = buf->get();

  p[0] = 0xc6;
  memcpy(p + 1, &percentage, 4);
  memcpy(p + 5, &axis, 1);

  return buf;
}

WS_BUFFER *buildAccelCalibrationDataPacket(AsyncWebSocket *ws, uint8_t axis,
                                           float ax, float ay, float az) {
  auto buf = ws->makeBuffer(1 + 4 * 3 + 1);
  uint8_t *p = buf->get();

  p[0] = 0xc7;
  memcpy(p + 1, &axis, 1);
  memcpy(p + 2, &ax, 4);
  memcpy(p + 6, &ay, 4);
  memcpy(p + 10, &az, 4);

  return buf;
}

WS_BUFFER *buildCalibrationDataPacket(AsyncWebSocket *ws,
                                      CalibrationStore *cal) {
  auto buf = ws->makeBuffer(1 + sizeof(CalibrationStore));
  uint8_t *p = buf->get();

  p[0] = 0xa0;
  memcpy(p + 1, cal, sizeof(CalibrationStore));

  return buf;
}