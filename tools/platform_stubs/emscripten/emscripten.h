/* Hermetic compile-only stand-in: only the names libretro-common uses. */
#ifndef STUB_emscripten_emscripten_h
#define STUB_emscripten_emscripten_h
double emscripten_get_now(void);

/* EM_ASM_* take a braced block of JavaScript, which the preprocessor
 * sees as several arguments; the variadic swallows it. The value is
 * never the point on this path - the gate is the C around it. */
#define EM_ASM(...)        ((void)0)
#define EM_ASM_INT(...)    0
#define EM_ASM_DOUBLE(...) 0.0
#define EM_ASM_PTR(...)    0

#define MAIN_THREAD_EM_ASM(...)        ((void)0)
#define MAIN_THREAD_EM_ASM_INT(...)    0
#define MAIN_THREAD_EM_ASM_DOUBLE(...) 0.0
#define MAIN_THREAD_EM_ASM_PTR(...)    0
#define MAIN_THREAD_ASYNC_EM_ASM(...)  ((void)0)
#endif
