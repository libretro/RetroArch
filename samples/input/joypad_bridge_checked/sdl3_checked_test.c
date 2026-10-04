/* The SDL3 joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). As with SDL2 there is no pad
 * table to fill - a read is a call into SDL - so SDL is given two
 * virtual joysticks, one it takes for a gamepad and one it does not,
 * the driver opens them the way it opens any, and their buttons, axes
 * and hats are set through SDL. */
#include <stdlib.h>

#include "../../../input/drivers_joypad/sdl3_joypad.c"

#include "bridge_view.h"

/* No video thread here: the driver pumps the queue itself. */
void sdl3_pump_input_events(void) { SDL_PumpEvents(); }

static void randomise(sdl3_joypad_t *pad)
{
   static const Uint8 hats[] = {
      SDL_HAT_CENTERED, SDL_HAT_UP, SDL_HAT_RIGHT, SDL_HAT_DOWN, SDL_HAT_LEFT,
      SDL_HAT_RIGHTUP, SDL_HAT_RIGHTDOWN, SDL_HAT_LEFTUP, SDL_HAT_LEFTDOWN };
   int i, n;

   n = SDL_GetNumJoystickButtons(pad->joypad);
   for (i = 0; i < n; i++)
      SDL_SetJoystickVirtualButton(pad->joypad, i, (bv_rand() & 1) != 0);
   n = SDL_GetNumJoystickAxes(pad->joypad);
   for (i = 0; i < n; i++)
   {
      unsigned r = bv_rand() % 8;
      Sint16 v   = (r == 0) ? 0
                 : (r == 1) ? 32767
                 : (r == 2) ? -32767
                 : (r == 3) ? (Sint16)-32768
                 : (Sint16)bv_rand();
      SDL_SetJoystickVirtualAxis(pad->joypad, i, v);
   }
   n = SDL_GetNumJoystickHats(pad->joypad);
   for (i = 0; i < n; i++)
      SDL_SetJoystickVirtualHat(pad->joypad, i,
            hats[bv_rand() % (sizeof(hats) / sizeof(hats[0]))]);
}

static bool attach(SDL_JoystickType type, int axes, int buttons, int hats)
{
   SDL_VirtualJoystickDesc desc;
   SDL_INIT_INTERFACE(&desc);
   desc.type     = type;
   desc.naxes    = (Uint16)axes;
   desc.nbuttons = (Uint16)buttons;
   desc.nhats    = (Uint16)hats;
   return SDL_AttachVirtualJoystick(&desc) != 0;
}

int main(void)
{
   unsigned n, bad = 0, pads = 0, gamepads = 0, i;

   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

   if (!sdl3_joypad_init(NULL))
   {
      printf("FAIL sdl3_checked_test: the driver did not start\n");
      return 1;
   }
   /* a pad, and a stick with more buttons and hats than a pad has */
   if (     !attach(SDL_JOYSTICK_TYPE_GAMEPAD, 6, 15, 1)
         || !attach(SDL_JOYSTICK_TYPE_FLIGHT_STICK, 8, 40, 3))
   {
      printf("FAIL sdl3_checked_test: SDL has no virtual joysticks (%s)\n", SDL_GetError());
      return 1;
   }
   sdl3_joypad_poll(); /* the driver opens what was attached */

   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].joypad)
      {
         pads++;
         if (sdl3_joypads[i].gamepad)
            gamepads++;
      }
   if (pads != 2)
   {
      printf("FAIL sdl3_checked_test: the driver opened %u of the 2 virtual joysticks\n", pads);
      return 1;
   }

   for (n = 0; n < 1500; n++)
      for (i = 0; i < MAX_USERS; i++)
      {
         if (!sdl3_joypads[i].joypad)
            continue;
         randomise(&sdl3_joypads[i]);
         SDL_UpdateJoysticks();
         bad += bv_compare(&sdl3_joypad, i);
      }

   sdl3_joypad_destroy();
   if (bad)
   {
      printf("FAIL sdl3_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS sdl3_checked_test: 1500 random states of 2 virtual joysticks"
         " (%u opened as a gamepad); what the bridge would read is what the"
         " driver says\n", gamepads);
   return 0;
}
