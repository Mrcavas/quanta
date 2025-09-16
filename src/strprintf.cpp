#include "strprintf.h"

String strprintf(const char *fmt, ...) {
  String result;
  va_list args;

  // First, figure out how long the formatted string needs to be
  va_start(args, fmt);
  int len = vsnprintf(nullptr, 0, fmt, args);
  va_end(args);

  if (len < 0) {
    return String(); // formatting error -> return empty
  }

  // Allocate a temporary buffer
  char *buf = new char[len + 1];
  if (!buf) {
    return String(); // allocation failed
  }

  // Write into buffer
  va_start(args, fmt);
  vsnprintf(buf, len + 1, fmt, args);
  va_end(args);

  // Convert to Arduino String
  result = String(buf);

  // Free buffer
  delete[] buf;

  return result;
}