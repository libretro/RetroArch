#include "internal.h"

#include <string.h>
#include <stdio.h>

int rc_parse_format(const char* format_str) {
  switch (*format_str++) {
    case 'F':
      if (!strcmp(format_str, "RAMES")) {
        return RC_FORMAT_FRAMES;
      }

      break;
    
    case 'T':
      if (!strcmp(format_str, "IME")) {
        return RC_FORMAT_FRAMES;
      }
      else if (!strcmp(format_str, "IMESECS")) {
        return RC_FORMAT_SECONDS;
      }

      break;
    
    case 'S':
      if (!strcmp(format_str, "ECS")) {
        return RC_FORMAT_SECONDS;
      }
      if (!strcmp(format_str, "CORE")) {
        return RC_FORMAT_SCORE;
      }

      break;
    
    case 'M':
      if (!strcmp(format_str, "ILLISECS")) {
        return RC_FORMAT_CENTISECS;
      }

      break;

    case 'P':
      if (!strcmp(format_str, "OINTS")) {
        return RC_FORMAT_SCORE;
      }

      break;

    case 'V':
      if (!strcmp(format_str, "ALUE")) {
        return RC_FORMAT_VALUE;
      }

      break;

    case 'O':
      if (!strcmp(format_str, "THER")) {
        return RC_FORMAT_OTHER;
      }

      break;
  }

  return RC_FORMAT_VALUE;
}

int rc_format_value(char* buffer, int size, unsigned value, int format) {
  unsigned a, b, c;
  int chars;

  switch (format) {
    case RC_FORMAT_FRAMES:
      a = value * 10 / 6; /* centisecs */
      b = a / 100; /* seconds */
      a -= b * 100;
      c = b / 60; /* minutes */
      b -= c * 60;
      chars = snprintf(buffer, size, "%02u:%02u.%02u", c, b, a);
      break;

    case RC_FORMAT_SECONDS:
      a = value / 60; /* minutes */
      value -= a * 60;
      chars = snprintf(buffer, size, "%02u:%02u", a, value);
      break;

    case RC_FORMAT_CENTISECS:
      a = value / 100; /* seconds */
      value -= a * 100;
      b = a / 60; /* minutes */
      a -= b * 60;
      chars = snprintf(buffer, size, "%02u:%02u.%02u", b, a, value);
      break;

    case RC_FORMAT_SCORE:
      chars = snprintf(buffer, size, "%06u Points", value);
      break;

    case RC_FORMAT_VALUE:
      chars = snprintf(buffer, size, "%01u", value);
      break;

    case RC_FORMAT_OTHER:
    default:
      chars = snprintf(buffer, size, "%06u", value);
      break;
  }

  return chars;
}
