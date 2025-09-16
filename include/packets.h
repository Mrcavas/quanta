#include "calibration.h"
#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#define WS_BUFFER AsyncWebSocketMessageBuffer

WS_BUFFER *buildMessagePacket(AsyncWebSocket *ws, String str);

WS_BUFFER *buildInitPacket(AsyncWebSocket *ws, float kp, float ki, float kd);

WS_BUFFER *buildAnchoringPacket(AsyncWebSocket *ws, bool *anchoring);

WS_BUFFER *buildRotationPacket(AsyncWebSocket *ws, float yaw);

WS_BUFFER *buildYawAnchorPacket(AsyncWebSocket *ws, float yaw);

WS_BUFFER *buildMagPointPacket(AsyncWebSocket *ws, float mx, float my,
                               float mz);

WS_BUFFER *buildGyroCalibrationProgressPacket(AsyncWebSocket *ws,
                                              float percentage);

WS_BUFFER *buildAccelCalibrationProgressPacket(AsyncWebSocket *ws, uint8_t axis,
                                               float percentage);

WS_BUFFER *buildAccelCalibrationDataPacket(AsyncWebSocket *ws, uint8_t axis,
                                           float ax, float ay, float az);

WS_BUFFER *buildCalibrationDataPacket(AsyncWebSocket *ws,
                                      CalibrationStore *cal);