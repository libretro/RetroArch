
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

/*
 * MinGW-w64 declares asprintf() and vasprintf() in <stdio.h>, but only
 * under _GNU_SOURCE. lib/libsmb2.c defines _GNU_SOURCE itself before it
 * includes anything, so on that toolchain the declarations are always in
 * scope by the time we get here and the fallbacks below collide with them:
 *
 *   error: static declaration of 'vasprintf' follows non-static declaration
 *
 * The #ifndef vasprintf / #ifndef asprintf guards do not catch this,
 * because those names are functions rather than macros.
 *
 * Previously only the _vscprintf_so() helper was excluded on MinGW while
 * the fallbacks that call it were still emitted, which produced both the
 * collision above and an implicit declaration of _vscprintf_so(). Gate the
 * helper and its users on one condition instead, so the two halves cannot
 * drift apart again: use the runtime's asprintf()/vasprintf() when they
 * are declared, and define our own otherwise.
 */
#if defined(__MINGW32__) && defined(_GNU_SOURCE)
#define SMB2_ASPRINTF_IN_LIBC 1
#endif

#ifndef SMB2_ASPRINTF_IN_LIBC

#if !defined(_XBOX)
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

#endif /* !SMB2_ASPRINTF_IN_LIBC */

#endif /* ! _ASPRINTF_H_ */
