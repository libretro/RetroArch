#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"

/* Keys when setting in XKeyboardControl.led */
#define XK_NUMLOCK    2
#define XK_CAPSLOCK   1
#define XK_SCROLLLOCK 3

/* Keys when reading from XKeyboardState.led_mask */
#define XM_NUMLOCK    2
#define XM_CAPSLOCK   1
#define XM_SCROLLLOCK 4

static int key_translate(int key)
{
   switch (key)
   {
      case 0:
         return XK_NUMLOCK;
      case 1:
         return XK_CAPSLOCK;
      case 2:
         return XK_SCROLLLOCK;
      default:
         break;
   }

   return 0;
}

/* One display connection for the driver's life: a set is a request
 * on it, not a connection of its own. */
typedef struct
{
   Display *dpy;
   int setup[MAX_LEDS];  /* lock LED state found at init, -1 none */
   int state[MAX_LEDS];  /* last state written, -1 none */
   int map[MAX_LEDS];
} keyboard_led_t;

/* TODO/FIXME - static globals */
static keyboard_led_t x11kb_curins;
static keyboard_led_t *x11kb_cur = &x11kb_curins;

static int get_led(unsigned long led_mask, int led)
{
   switch (led)
   {
      case XK_NUMLOCK:
         return (led_mask & XM_NUMLOCK)    ? 1 : 0;
      case XK_CAPSLOCK:
         return (led_mask & XM_CAPSLOCK)   ? 1 : 0;
      case XK_SCROLLLOCK:
         return (led_mask & XM_SCROLLLOCK) ? 1 : 0;
      default:
         break;
   }

   return 0;
}

/* Writes @state to the keyboard LED numbered @led (0 Num Lock, 1 Caps
 * Lock, 2 Scroll Lock) when it differs from the last one written. */
static void keyboard_led(int led, int state)
{
   XKeyboardControl values;
   int key;

   if (     !x11kb_cur->dpy
         || (led < 0) || (led >= MAX_LEDS)
         || !(key = key_translate(led))
         || x11kb_cur->state[led] == state)
      return;

   values.led      = key;
   values.led_mode = state ? LedModeOn : LedModeOff;
   XChangeKeyboardControl(x11kb_cur->dpy, KBLed | KBLedMode, &values);
   XFlush(x11kb_cur->dpy);
   x11kb_cur->state[led] = state;
}

static void keyboard_init(void)
{
   int i;
   XKeyboardState kb;
   settings_t *settings = config_get_ptr();

   if (!settings || x11kb_cur->dpy)
      return;

   if (!(x11kb_cur->dpy = XOpenDisplay(NULL)))
      return;
   XGetKeyboardControl(x11kb_cur->dpy, &kb);

   for (i = 0; i < MAX_LEDS; i++)
   {
      int key              = key_translate(i);
      x11kb_cur->setup[i]  = key ? get_led(kb.led_mask, key) : -1;
      x11kb_cur->state[i]  = x11kb_cur->setup[i];
      x11kb_cur->map[i]    = (int)settings->uints.led_map[i];
      if (x11kb_cur->map[i] < 0)
         x11kb_cur->map[i] = i;
   }
}

static void keyboard_free(void)
{
   int i;

   if (!x11kb_cur->dpy)
      return;

   for (i = 0; i < MAX_LEDS; i++)
      if (x11kb_cur->setup[i] != -1)
         keyboard_led(i, x11kb_cur->setup[i]);

   XCloseDisplay(x11kb_cur->dpy);
   x11kb_cur->dpy = NULL;
}

static void keyboard_set(int led, int state)
{
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   keyboard_led(x11kb_cur->map[led], state ? 1 : 0);
}

const led_driver_t keyboard_led_driver = {
   keyboard_init,
   keyboard_free,
   keyboard_set,
   "keyboard"
};
