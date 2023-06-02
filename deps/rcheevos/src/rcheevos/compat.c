#include "rc_compat.h"

#include <ctype.h>
#include <stdarg.h>

#ifdef RC_C89_HELPERS

int rc_strncasecmp(const char* left, const char* right, size_t length)
{
  while (length)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
    --length;
  }

  return 0;
}

int rc_strcasecmp(const char* left, const char* right)
{
  while (*left || *right)
  {
    if (*left != *right)
    {
      const int diff = tolower(*left) - tolower(*right);
      if (diff != 0)
        return diff;
    }

    ++left;
    ++right;
  }

  return 0;
}

char* rc_strdup(const char* str)
{
  const size_t length = strlen(str);
  char* buffer = (char*)malloc(length + 1);
  if (buffer)
    memcpy(buffer, str, length + 1);
  return buffer;
}

int rc_snprintf(char* buffer, size_t size, const char* format, ...)
{
  int result;
  va_list args;

  va_start(args, format);

#ifdef __STDC_WANT_SECURE_LIB__
  result = vsprintf_s(buffer, size, format, args);
#else
  /* assume buffer is large enough and ignore size */
  (void)size;
  result = vsprintf(buffer, format, args);
#endif

  va_end(args);

  return result;
}

#endif

#ifndef __STDC_WANT_SECURE_LIB__

struct tm* rc_gmtime_s(struct tm* buf, const time_t* timer)
{
  struct tm* tm = gmtime(timer);
  memcpy(buf, tm, sizeof(*tm));
  return buf;
}

#endif
