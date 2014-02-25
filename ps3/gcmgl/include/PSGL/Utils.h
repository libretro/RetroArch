#ifndef _RGLUTILS_H_
#define _RGLUTILS_H_

#include "Types.h"
#include "../export/PSGL/export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RGL_LIKELY(COND) __builtin_expect((COND),1)
#define RGL_UNLIKELY(COND) __builtin_expect((COND),0)

#define MAX(A,B) ((A)>(B)?(A):(B))
#define MIN(A,B) ((A)<(B)?(A):(B))

#define _RGL_FLOAT_AS_UINT(x) ({union {float f; unsigned int i;} u; u.f=(x); u.i;})

#define rglClampf(value)   (MAX( MIN((value), 1.f ), 0.f ))
#define endianSwapHalf(v) (((v) >> 8 & 0x00ff) | ((v) << 8 & 0xff00))
#define endianSwapWord(v) (((v) & 0xff ) << 24 | ((v) & 0xff00 ) << 8 | ((v) & 0xff0000 ) >> 8 | ((v) & 0xff000000 ) >> 24)
#define endianSwapWordByHalf(v) (((v) & 0xffff ) << 16 | (v) >> 16)

static inline int rglLog2( unsigned int i )
{
   int l = 0;
   while ( i )
   {
      ++l;
      i >>= 1;
   }
   return l -1;
}

#define rglIsPow2(i) (((i) & ((i) - 1 )) == 0)
// Pad argument x to the next multiple of argument pad.
#define rglPad(x, pad) (((x) + (pad) - 1 ) / (pad) * (pad))
// Pad pointer x to the next multiple of argument pad.
#define rglPadPtr(p, pad) ((char*)(((intptr_t)(p) + (pad) - 1 ) / (pad) * (pad)))

// names API

RGL_EXPORT unsigned int rglCreateName (void *data, void* object);
RGL_EXPORT unsigned int rglIsName( void *data, unsigned int name);
RGL_EXPORT void rglEraseName (void *data, unsigned int name);

#define rglGetNamedValue(x, name) (((struct rglNameSpace*)(x))->data[(name) - 1])

void rglTexNameSpaceResetNames(void *data);
GLboolean rglTexNameSpaceCreateNameLazy(void *data, GLuint name );
GLboolean rglTexNameSpaceIsName(void *data, GLuint name );
void rglTexNameSpaceDeleteNames(void *data, GLsizei n, const GLuint *names );
void rglTexNameSpaceReinit(void *saved, void *active);

#ifdef __cplusplus
}
#endif

#endif // _RGL_UTILS_H_
