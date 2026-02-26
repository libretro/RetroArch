
#ifndef _ASPRINTF_H_
#define _ASPRINTF_H_

#if !defined(__AROS__) && !defined(__ps2sdk_iop__)
#include <malloc.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef _XBOX
#define inline __inline
#endif

#if !defined(_XBOX) && !defined(__MINGW32__)
#ifndef _vscprintf
/* For some reason, MSVC fails to honour this #ifndef. */
/* Hence function renamed to _vscprintf_so(). */
static inline int _vscprintf_so(const char * format, va_list pargs) {
  int retval;
  va_list argcopy;
  va_copy(argcopy, pargs);
  retval = vsnprintf(NULL, 0, format, argcopy);
  va_end(argcopy);
  return retval;
}
#endif /* _vscprintf */
#endif

#ifndef vasprintf
static inline int vasprintf(char **strp, const char *fmt, va_list ap) {
#ifdef _XBOX
  int len = _vscprintf(fmt, ap);
#else
  int len = _vscprintf_so(fmt, ap);
#endif
  char *str;
  int r;
  if (len == -1) return -1;
  str = malloc((size_t)len + 1);
  if (!str) return -1;
#ifdef _XBOX
  r = _vsnprintf(str, len + 1, fmt, ap); /* "secure" version of vsprintf */
#else
  r = vsnprintf(str, len + 1, fmt, ap); /* "secure" version of vsprintf */
#endif
  if (r == -1) return free(str), -1;
  *strp = str;
  return r;
}
#endif /* vasprintf */

#ifndef asprintf
static inline int asprintf(char *strp[], const char *fmt, ...) {
  int r;
  va_list ap;
  va_start(ap, fmt);
  r = vasprintf(strp, fmt, ap);
  va_end(ap);
  return r;
}
#endif /* asprintf */

#endif /* ! _ASPRINTF_H_ */
