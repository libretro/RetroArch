#include <stdio.h>
#include "../led_driver.h"
#include "../led_defines.h"

#include "../../configuration.h"
#include "../../retroarch.h"

#undef MAX_LEDS
#define MAX_LEDS 3

#ifdef _WIN32
#include <windows.h>
#endif

static void key_translate(int *key)
{
#ifdef _WIN32
   switch (*key)
   {
      case 0:
         *key = VK_NUMLOCK;
         break;
      case 1:
         *key = VK_CAPITAL;
         break;
      case 2:
         *key = VK_SCROLL;
         break;
   }
#endif
}

typedef struct
{
   int setup[MAX_LEDS];
   int state[MAX_LEDS];
   int map[MAX_LEDS];
   bool init;
} keyboard_led_t;

/* TODO/FIXME - static globals */
static keyboard_led_t win32kb_curins;
static keyboard_led_t *win32kb_cur = &win32kb_curins;

static int keyboard_led(int led, int state)
{
   int status;
   int key = led;

   if ((led < 0) || (led >= MAX_LEDS))
      return -1;

   key_translate(&key);

#ifdef _WIN32
   status = GetKeyState(key);
#endif

   if (state == -1)
      return status;

   if ((state && !status) ||
       (!state && status))
   {
#ifdef _WIN32
      keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
      keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
      win32kb_cur->state[led] = state;
#endif
   }
   return -1;
}

static void keyboard_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings || win32kb_cur->init)
      return;

   for (i = 0; i < MAX_LEDS; i++)
   {
      win32kb_cur->setup[i] = keyboard_led(i, -1);
      win32kb_cur->state[i] = -1;
      win32kb_cur->map[i]   = settings->uints.led_map[i];
      if (win32kb_cur->map[i] < 0)
         win32kb_cur->map[i] = i;
   }
   win32kb_cur->init = true;
}

static void keyboard_free(void)
{
   int i;

   for (i = 0; i < MAX_LEDS; i++)
   {
      if (win32kb_cur->state[i] != -1 &&
          win32kb_cur->state[i] != win32kb_cur->setup[i])
         keyboard_led(i, win32kb_cur->setup[i]);
   }
}

static void keyboard_set(int led, int state)
{
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   keyboard_led(win32kb_cur->map[led], state);
}

const led_driver_t keyboard_led_driver = {
   keyboard_init,
   keyboard_free,
   keyboard_set,
   "Keyboard"
};
