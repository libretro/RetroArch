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
      case RETROK_NUMLOCK:
         *key = VK_NUMLOCK;
         break;
      case RETROK_CAPSLOCK:
         *key = VK_CAPITAL;
         break;
      case RETROK_SCROLLOCK:
         *key = VK_SCROLL;
         break;
   }
#endif
}

void led_set(int key, int state)
{
#ifdef _WIN32
   BYTE keyState[256];
#endif

   key_translate(&key);

#ifdef _WIN32
   GetKeyboardState((LPBYTE)&keyState);
   if ((state && !(keyState[key] & 1)) ||
       (!state && (keyState[key] & 1)))
   {
      keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY | 0, 0);
      keybd_event(key, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
   }
#endif
}

int led_get(int key)
{
   short status;
   key_translate(&key);

#ifdef _WIN32
   status = GetKeyState(key);
#endif
   return status;
}

typedef struct
{
   int setup[MAX_LEDS];
   int map[MAX_LEDS];
} keyboard_led_t;

/* TODO/FIXME - static globals */
static keyboard_led_t win32kb_curins;
static keyboard_led_t *win32kb_cur = &win32kb_curins;

static void keyboard_init(void)
{
   int i;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return;

   for (i = 0; i < MAX_LEDS; i++)
   {
      win32kb_cur->setup[i]     = -1;
      win32kb_cur->map[i]       = settings->uints.led_map[i];
      if (win32kb_cur->map[i] < 0)
         win32kb_cur->map[i]    = i;
      
      switch (i)
      {
         case 0:
            win32kb_cur->setup[i] = led_get(RETROK_NUMLOCK);
            break;
         case 1:
            win32kb_cur->setup[i] = led_get(RETROK_CAPSLOCK);
            break;
         case 2:
            win32kb_cur->setup[i] = led_get(RETROK_SCROLLOCK);
            break;
      }
   }
}

static void keyboard_free(void)
{
   int i;

   for (i = 0; i < MAX_LEDS; i++)
   {
      if (win32kb_cur->setup[i] < 0)
         continue;
         
      switch (i)
      {
         case 0:
            led_set(RETROK_NUMLOCK, win32kb_cur->setup[i]);
            break;
         case 1:
            led_set(RETROK_CAPSLOCK, win32kb_cur->setup[i]);
            break;
         case 2:
            led_set(RETROK_SCROLLOCK, win32kb_cur->setup[i]);
            break;
      }

      win32kb_cur->setup[i] = -1;
   }
}

static void keyboard_set(int led, int state)
{
   if ((led < 0) || (led >= MAX_LEDS))
      return;

   switch (win32kb_cur->map[led])
   {
      case 0:
         led_set(RETROK_NUMLOCK, state);
         break;
      case 1:
         led_set(RETROK_CAPSLOCK, state);
         break;
      case 2:
         led_set(RETROK_SCROLLOCK, state);
         break;
   }
}

const led_driver_t keyboard_led_driver = {
   keyboard_init,
   keyboard_free,
   keyboard_set,
   "Keyboard"
};
