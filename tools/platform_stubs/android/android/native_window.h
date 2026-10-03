/* Hermetic compile-only stand-in for the NDK header. */
#ifndef STUB_ANDROID_NATIVE_WINDOW_H
#define STUB_ANDROID_NATIVE_WINDOW_H
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
int32_t ANativeWindow_getWidth(ANativeWindow*); int32_t ANativeWindow_getHeight(ANativeWindow*); int32_t ANativeWindow_setBuffersGeometry(ANativeWindow*, int32_t, int32_t, int32_t);
#endif
