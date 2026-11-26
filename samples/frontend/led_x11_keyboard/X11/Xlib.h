/* Stand-in for <X11/Xlib.h>: the part led/drivers/led_x11_keyboard.c
 * uses, so led_x11_keyboard_test builds and runs without an X server
 * or libX11, on any host. The test defines the functions. */

#ifndef LED_X11_TEST_XLIB_H
#define LED_X11_TEST_XLIB_H

typedef struct _XDisplay Display;

typedef struct
{
   unsigned long led_mask;
} XKeyboardState;

typedef struct
{
   int led;
   int led_mode;
} XKeyboardControl;

#define LedModeOff 0
#define LedModeOn  1

#define KBLed      (1L << 4)
#define KBLedMode  (1L << 5)

Display *XOpenDisplay(const char *name);
int XCloseDisplay(Display *dpy);
int XGetKeyboardControl(Display *dpy, XKeyboardState *state);
int XChangeKeyboardControl(Display *dpy, unsigned long mask,
      XKeyboardControl *values);
int XFlush(Display *dpy);

#endif
