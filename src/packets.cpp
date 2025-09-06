#include "packets.h"
#include "calibration.h"

WS_BUFFER *buildInitPacket(AsyncWebSocket *ws, float kp, float ki, float kd) {
  auto buf = ws->makeBuffer(1 + 3 * sizeof(float));
  uint8_t *p = buf->get();

  p[0] = 0x01;
  memcpy(p + 1, &kp, sizeof(float));
  memcpy(p + 1 + sizeof(float), &ki, sizeof(float));
  memcpy(p + 1 + 2 * sizeof(float), &kd, sizeof(float));

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

WS_BUFFER *buildBiasesPacket(AsyncWebSocket *ws, biasStore *store) {
  auto buf = ws->makeBuffer(1 + 4 * 9);
  uint8_t *p = buf->get();

  p[0] = 0x1c;
  memcpy(p + 1, &store->biasAccelX, 4);
  memcpy(p + 5, &store->biasAccelY, 4);
  memcpy(p + 9, &store->biasAccelZ, 4);
  memcpy(p + 13, &store->biasGyroX, 4);
  memcpy(p + 17, &store->biasGyroY, 4);
  memcpy(p + 21, &store->biasGyroZ, 4);
  memcpy(p + 25, &store->biasCPassX, 4);
  memcpy(p + 29, &store->biasCPassY, 4);
  memcpy(p + 33, &store->biasCPassZ, 4);

  return buf;
}