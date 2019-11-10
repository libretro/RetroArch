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
      if (!strcmp(format_str, "ECS_AS_MINS")) {
        return RC_FORMAT_SECONDS_AS_MINUTES;
      }

      break;
    
    case 'M':
      if (!strcmp(format_str, "ILLISECS")) {
        return RC_FORMAT_CENTISECS;
      }
      if (!strcmp(format_str, "INUTES")) {
        return RC_FORMAT_MINUTES;
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
        return RC_FORMAT_SCORE;
      }

      break;
  }

  return RC_FORMAT_VALUE;
}

static int rc_format_value_minutes(char* buffer, int size, unsigned minutes) {
    unsigned hours;

    hours = minutes / 60;
    minutes -= hours * 60;
    return snprintf(buffer, size, "%uh%02u", hours, minutes);
}

static int rc_format_value_seconds(char* buffer, int size, unsigned seconds) {
  unsigned hours, minutes;

  /* apply modulus math to split the seconds into hours/minutes/seconds */
  minutes = seconds / 60;
  seconds -= minutes * 60;
  if (minutes < 60) {
    return snprintf(buffer, size, "%u:%02u", minutes, seconds);
  }

  hours = minutes / 60;
  minutes -= hours * 60;
  return snprintf(buffer, size, "%uh%02u:%02u", hours, minutes, seconds);
}

static int rc_format_value_centiseconds(char* buffer, int size, unsigned centiseconds) {
  unsigned seconds;
  int chars, chars2;

  /* modulus off the centiseconds */
  seconds = centiseconds / 100;
  centiseconds -= seconds * 100;

  chars = rc_format_value_seconds(buffer, size, seconds);
  if (chars > 0) {
    chars2 = snprintf(buffer + chars, size - chars, ".%02u", centiseconds);
    if (chars2 > 0) {
      chars += chars2;
    } else {
      chars = chars2;
    }
  }

  return chars;
}

int rc_format_value(char* buffer, int size, int value, int format) {
  int chars;

  switch (format) {
    case RC_FORMAT_FRAMES:
      /* 60 frames per second = 100 centiseconds / 60 frames; multiply frames by 100 / 60 */
      chars = rc_format_value_centiseconds(buffer, size, value * 10 / 6);
      break;

    case RC_FORMAT_SECONDS:
      chars = rc_format_value_seconds(buffer, size, value);
      break;

    case RC_FORMAT_CENTISECS:
      chars = rc_format_value_centiseconds(buffer, size, value);
      break;

    case RC_FORMAT_SECONDS_AS_MINUTES:
      chars = rc_format_value_minutes(buffer, size, value / 60);
      break;

    case RC_FORMAT_MINUTES:
      chars = rc_format_value_minutes(buffer, size, value);
      break;

    case RC_FORMAT_SCORE:
      chars = snprintf(buffer, size, "%06d", value);
      break;

    default:
    case RC_FORMAT_VALUE:
      chars = snprintf(buffer, size, "%d", value);
      break;
  }

  return chars;
}
