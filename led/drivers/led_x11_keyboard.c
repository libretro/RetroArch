#include <stdio.h>
#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"
#include "../../retroarch.h"

#include <X11/Xlib.h>
#include <X11/XKBlib.h>

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

typedef struct
{
   int setup[MAX_LEDS];
   int state[MAX_LEDS];
   int map[MAX_LEDS];
   bool init;
} keyboard_led_t;

/* TODO/FIXME - static globals */
static keyboard_led_t x11kb_curins;
static keyboard_led_t *x11kb_cur = &x11kb_curins;

static int get_led(int led)
{
   Display *dpy = XOpenDisplay(0);
   XKeyboardState state;
   XGetKeyboardControl(dpy, &state);
   XCloseDisplay(dpy);

   switch (led)
   {
      case XK_NUMLOCK:
         return (state.led_mask & XM_NUMLOCK) ? 1 : 0;
         break;
      case XK_CAPSLOCK:
         return (state.led_mask & XM_CAPSLOCK) ? 1 : 0;
         break;
      case XK_SCROLLLOCK:
         return (state.led_mask & XM_SCROLLLOCK) ? 1 : 0;
         break;
      default:
         break;
   }
   return 0;
}

static void set_led(int led, int state)
{
   Display *dpy = XOpenDisplay(0);
   XKeyboardControl values;
   values.led = led;
   values.led_mode = state ? LedModeOn : LedModeOff;
   XChangeKeyboardControl(dpy, KBLed | KBLedMode, &values);
   XCloseDisplay(dpy);
}

static int keyboard_led(int led, int state)
{
   int status = 0;
   int key    = led;

   if ((led < 0) || (led >= MAX_LEDS))
      return -1;

   if (!(key = key_translate(key)))
      return -1;

   status = get_led(key);

   if (state == -1)
      return status;

   if (   ( state  && !status)
       || (!state  &&  status))
   {
      set_led(key, state);
      x11kb_cur->state[led] = state;
   }
   return -1;
}

static void keyboard_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings || x11kb_cur->init)
      return;

   for (i = 0; i < MAX_LEDS; i++)
   {
      x11kb_cur->setup[i] = keyboard_led(i, -1);
      x11kb_cur->state[i] = -1;
      x11kb_cur->map[i]   = settings->uints.led_map[i];
      if (x11kb_cur->map[i] < 0)
         x11kb_cur->map[i] = i;
   }
   x11kb_cur->init = true;
}

static void keyboard_free(void)
{
   int i;

   for (i = 0; i < MAX_LEDS; i++)
   {
      if (x11kb_cur->state[i] != -1 &&
          x11kb_cur->state[i] != x11kb_cur->setup[i])
         keyboard_led(i, x11kb_cur->setup[i]);
   }
}

static void keyboard_set(int led, int state)
{
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   keyboard_led(x11kb_cur->map[led], state);
}

const led_driver_t keyboard_led_driver = {
   keyboard_init,
   keyboard_free,
   keyboard_set,
   "Keyboard"
};
