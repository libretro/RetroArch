/* The SDL2 joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). This one keeps no pad table
 * of its own to fill - a read is a call into SDL - so SDL is given two
 * virtual joysticks, one that it takes for a game controller and one
 * that it does not, the driver opens them the way it opens any, and
 * their buttons, axes and hats are set through SDL. */
#include <stdlib.h>

#include "../../../input/drivers_joypad/sdl2_joypad.c"

#include "bridge_view.h"

static void randomise(sdl2_joypad_t *pad)
{
   static const Uint8 hats[] = {
      SDL_HAT_CENTERED, SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT,
      SDL_HAT_RIGHTUP, SDL_HAT_RIGHTDOWN, SDL_HAT_LEFTUP, SDL_HAT_LEFTDOWN };
   int i, n;

   n = SDL_JoystickNumButtons(pad->joypad);
   for (i = 0; i < n; i++)
      SDL_JoystickSetVirtualButton(pad->joypad, i, (Uint8)(bv_rand() & 1));
   n = SDL_JoystickNumAxes(pad->joypad);
   for (i = 0; i < n; i++)
   {
      unsigned r = bv_rand() % 8;
      Sint16 v   = (r == 0) ? 0
                 : (r == 1) ? 32767
                 : (r == 2) ? -32767
                 : (r == 3) ? (Sint16)-32768
                 : (Sint16)bv_rand();
      SDL_JoystickSetVirtualAxis(pad->joypad, i, v);
   }
   n = SDL_JoystickNumHats(pad->joypad);
   for (i = 0; i < n; i++)
      SDL_JoystickSetVirtualHat(pad->joypad, i,
            hats[bv_rand() % (sizeof(hats) / sizeof(hats[0]))]);
}

int main(void)
{
   unsigned n, bad = 0, pads = 0, controllers = 0, i;

   setenv("SDL_VIDEODRIVER", "dummy", 1);
   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

   if (!sdl2_joypad_init(NULL))
   {
      printf("FAIL sdl2_checked_test: the driver did not start\n");
      return 1;
   }
   /* a pad, and a stick with more buttons and hats than a pad has */
   if (     SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 6, 15, 1) < 0
         || SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_FLIGHT_STICK, 8, 40, 3) < 0)
   {
      printf("FAIL sdl2_checked_test: SDL has no virtual joysticks (%s)\n", SDL_GetError());
      return 1;
   }
   sdl2_joypad_poll(); /* the driver opens what was attached */

   for (i = 0; i < MAX_USERS; i++)
      if (sdl2_pads[i].joypad)
      {
         pads++;
         if (sdl2_pads[i].controller)
            controllers++;
      }
   if (pads != 2)
   {
      printf("FAIL sdl2_checked_test: the driver opened %u of the 2 virtual joysticks\n", pads);
      return 1;
   }

   for (n = 0; n < 1500; n++)
      for (i = 0; i < MAX_USERS; i++)
      {
         if (!sdl2_pads[i].joypad)
            continue;
         randomise(&sdl2_pads[i]);
         SDL_JoystickUpdate();
         bad += bv_compare(&sdl2_joypad, i);
      }

   sdl2_joypad_destroy();
   if (bad)
   {
      printf("FAIL sdl2_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS sdl2_checked_test: 1500 random states of 2 virtual joysticks"
         " (%u opened as a game controller); what the bridge would read is"
         " what the driver says\n", controllers);
   return 0;
}
