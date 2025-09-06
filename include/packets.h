#include "calibration.h"
#include <ESPAsyncWebServer.h>

#define WS_BUFFER AsyncWebSocketMessageBuffer

WS_BUFFER *buildInitPacket(AsyncWebSocket *ws, float kp, float ki, float kd);

WS_BUFFER *buildAnchoringPacket(AsyncWebSocket *ws, bool *anchoring);

WS_BUFFER *buildRotationPacket(AsyncWebSocket *ws, float yaw);

WS_BUFFER *buildYawAnchorPacket(AsyncWebSocket *ws, float yaw);

WS_BUFFER *buildBiasesPacket(AsyncWebSocket *ws, biasStore *store);