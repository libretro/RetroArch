/* MinGW runtime math entry points used by the frontend. The SDK hides
 * their declarations in strict C89, even though the linked runtime supplies
 * them. Implicit int declarations corrupt their floating-point return ABI.
 * Keep the compiler in strict C89 and expose only the required prototypes. */
#ifndef LIBRETRO_COMPAT_MINGW_MATH_H
#define LIBRETRO_COMPAT_MINGW_MATH_H
#if defined(__MINGW32__) && defined(__STRICT_ANSI__) && !defined(__cplusplus) \
      && !defined(__ASSEMBLER__) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)
#include <math.h>
extern float __cdecl ceilf(float);
extern float __cdecl cosf(float);
extern float __cdecl expf(float);
extern float __cdecl fabsf(float);
extern float __cdecl floorf(float);
extern long __cdecl lroundf(float);
extern float __cdecl powf(float, float);
extern float __cdecl roundf(float);
extern float __cdecl sinf(float);
extern float __cdecl sqrtf(float);
#endif
#endif
