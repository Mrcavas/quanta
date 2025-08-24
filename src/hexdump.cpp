#include "Arduino.h"

void hexdump(const uint8_t *data, size_t len) {
  for (size_t i = 0; i < len; i += 16) {
    Serial.printf("%08zx  ", i); // offset
    // hex part
    for (size_t j = 0; j < 16; j++) {
      if (i + j < len)
        Serial.printf("%02x ", data[i + j]);
      else
        Serial.printf("   ");
      if (j == 7)
        Serial.printf(" "); // extra space in middle
    }
    Serial.printf(" ");
    // ASCII part
    for (size_t j = 0; j < 16 && i + j < len; j++) {
      unsigned char c = data[i + j];
      Serial.printf("%c", isprint(c) ? c : '.');
    }
    Serial.printf("\n");
  }
}