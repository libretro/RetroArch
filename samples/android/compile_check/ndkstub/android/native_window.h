#ifndef STUB_NATIVE_WINDOW_H
#define STUB_NATIVE_WINDOW_H
typedef struct ANativeWindow ANativeWindow;
#define ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_FIXED_SOURCE 1
int ANativeWindow_setFrameRate(ANativeWindow *w, float f, signed char c);
#endif
