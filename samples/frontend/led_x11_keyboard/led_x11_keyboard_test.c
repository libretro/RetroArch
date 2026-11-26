/* led_x11_keyboard_test.c -- led/drivers/led_x11_keyboard.c against a
 * stand-in Xlib (X11/ here): a keyboard with Caps, Num and Scroll Lock
 * LEDs, and counts of what the driver asked of it.
 *
 * The claims:
 *
 *   1. Init opens one display connection and reads the keyboard's LED
 *      state once. Sets open no further connections.
 *   2. ledN_map = 0, 1, 2 drives Num, Caps, Scroll Lock; a negative
 *      map means LED N itself; any other map drives nothing.
 *   3. A state already on the LED is not sent again, and every state
 *      sent is flushed.
 *   4. Free puts back the LEDs init found, sends nothing for the ones
 *      already back, and closes the connection.
 *   5. Init after free works again: a content reload gets a working
 *      driver.
 *   6. With no display, init, set and free do nothing. */

#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>

#include "configuration.h"

static settings_t test_settings;

settings_t *config_get_ptr(void) { return &test_settings; }

/* The stand-in server. */
static int           fake_display;
static int           display_available = 1;
static unsigned long server_leds;     /* bit (led - 1) lit */
static unsigned      n_opens, n_closes, n_gets, n_changes, n_flushes;
static int           flushed;         /* nothing sent since the flush */

Display *XOpenDisplay(const char *name)
{
   (void)name;
   if (!display_available)
      return NULL;
   n_opens++;
   return (Display*)&fake_display;
}

int XCloseDisplay(Display *dpy)
{
   (void)dpy;
   n_closes++;
   return 0;
}

int XGetKeyboardControl(Display *dpy, XKeyboardState *state)
{
   (void)dpy;
   n_gets++;
   state->led_mask = server_leds;
   return 1;
}

int XChangeKeyboardControl(Display *dpy, unsigned long mask,
      XKeyboardControl *values)
{
   (void)dpy;
   if ((mask & (KBLed | KBLedMode)) == (KBLed | KBLedMode))
   {
      if (values->led_mode == LedModeOn)
         server_leds |=  (1ul << (values->led - 1));
      else
         server_leds &= ~(1ul << (values->led - 1));
   }
   n_changes++;
   flushed = 0;
   return 1;
}

int XFlush(Display *dpy)
{
   (void)dpy;
   n_flushes++;
   flushed = 1;
   return 1;
}

#include "led/drivers/led_x11_keyboard.c"

#define CAPS   1ul
#define NUM    2ul
#define SCROLL 4ul

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

static void map_all(int value)
{
   int i;
   for (i = 0; i < MAX_LEDS; i++)
      test_settings.uints.led_map[i] = (unsigned)value;
}

int main(void)
{
   unsigned changes;

   map_all(-1);
   test_settings.uints.led_map[3] = 1;   /* Caps Lock */
   test_settings.uints.led_map[4] = 7;   /* no such keyboard LED */
   server_leds = NUM;                    /* Num Lock on at start */

   keyboard_led_driver.init();
   CHECK(n_opens == 1, "init did not open one connection");
   CHECK(n_gets == 1, "init did not read the LEDs once");

   keyboard_led_driver.set_led(3, 1);
   CHECK(server_leds == (NUM | CAPS), "map 1 did not light Caps Lock");
   CHECK(flushed, "state sent and not flushed");

   changes = n_changes;
   keyboard_led_driver.set_led(3, 1);
   keyboard_led_driver.set_led(0, 1);    /* map -1: LED 0, Num, already on */
   CHECK(n_changes == changes, "state already on the LED sent again");

   keyboard_led_driver.set_led(2, 1);    /* map -1: LED 2, Scroll Lock */
   CHECK(server_leds == (NUM | CAPS | SCROLL), "Scroll Lock not lit");
   keyboard_led_driver.set_led(2, 0);
   CHECK(server_leds == (NUM | CAPS), "Scroll Lock not off");

   keyboard_led_driver.set_led(0, 0);
   CHECK(server_leds == CAPS, "Num Lock not off");

   changes = n_changes;
   keyboard_led_driver.set_led(4, 1);
   keyboard_led_driver.set_led(9, 1);    /* map -1: LED 9, none */
   CHECK(n_changes == changes, "a map with no keyboard LED sent something");

   CHECK(n_opens == 1, "a set opened a connection");
   CHECK(n_gets == 1, "a set read the LEDs");

   changes = n_changes;
   keyboard_led_driver.free();
   CHECK(server_leds == NUM, "free did not put the LEDs back");
   CHECK(n_changes - changes == 2,
         "free sent other than the two LEDs that differ");
   CHECK(flushed, "restore not flushed");
   CHECK(n_closes == 1, "free did not close the connection");

   /* A reload. */
   keyboard_led_driver.init();
   CHECK(n_opens == 2, "init after free did not open a connection");
   keyboard_led_driver.set_led(3, 1);
   CHECK(server_leds == (NUM | CAPS), "set after reinit did nothing");
   keyboard_led_driver.free();
   CHECK(server_leds == NUM && n_closes == 2,
         "second free did not restore and close");

   /* No display. */
   display_available = 0;
   changes           = n_changes;
   keyboard_led_driver.init();
   keyboard_led_driver.set_led(3, 1);
   keyboard_led_driver.free();
   CHECK(n_changes == changes && n_closes == 2,
         "something sent or closed without a display");

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("led_x11_keyboard_test: all passed\n");
   return 0;
}
