/* The SDL3 joypad driver, held to what the snapshot bridge assumes of
 * a driver it serves (see bridge_view.h). As with SDL2 there is no pad
 * table to fill - a read is a call into SDL - so SDL is given three
 * virtual joysticks: one it takes for a gamepad, one it does not
 * and a DualShock 3 with its pressure axes. The driver opens them the
 * way it opens any, and their buttons, axes and hats are set through
 * SDL. */
#include <stdlib.h>

#include "../../../input/drivers_joypad/sdl3_joypad.c"

#include "bridge_view.h"

/* No video thread here: the driver pumps the queue itself. */
bool sdl3_pump_input_events(void) { SDL_PumpEvents(); return true; }

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

/* SDL's HIDAPI driver represents a PS3 controller as a gamepad with
 * ten additional axes for the pressure sensitive buttons. */
static bool attach_ds3(void)
{
   SDL_VirtualJoystickDesc desc;
   SDL_INIT_INTERFACE(&desc);
   desc.type       = SDL_JOYSTICK_TYPE_GAMEPAD;
   desc.vendor_id  = 0x054c;
   desc.product_id = 0x0268;
   desc.naxes      = 16;
   desc.nbuttons   = 15;
   desc.nhats      = 0;
   return SDL_AttachVirtualJoystick(&desc) != 0;
}

/* Pressure sensitive buttons read 0 at rest and 32767 when pressed all
 * the way down, like a trigger. */
static unsigned check_pressure(unsigned port)
{
   static const Sint16  raw[3]    = { -32768, 0, 32767 };
   static const int16_t scaled[3] = { 0, 16384, 32767 };
   sdl3_joypad_t *pad = &sdl3_joypads[port];
   unsigned bad = 0, a, v;

   for (a = SDL_GAMEPAD_AXIS_COUNT; a < 16; a++)
      for (v = 0; v < 3; v++)
      {
         SDL_SetJoystickVirtualAxis(pad->joypad, (int)a, raw[v]);
         SDL_UpdateJoysticks();
         if (sdl3_joypad.axis(port, AXIS_POS(a)) != scaled[v])
            bad++;
         if (sdl3_joypad.axis(port, AXIS_NEG(a)) != 0)
            bad++;
      }
   return bad;
}

/* A pad with a touchpad of two fingers: what get_touchpad() hands over
 * is where SDL has the fingers, the pad's width -0x7fff to 0x7fff, and
 * the press. */
static unsigned check_touchpad(void)
{
   SDL_VirtualJoystickDesc desc;
   SDL_VirtualJoystickTouchpadDesc touch;
   input_pad_view_t view;
   unsigned bad = 0, i, port = MAX_USERS;

   /* the random states above filled SDL's event queue: a poll empties
    * it, or the new pad's arrival would not fit */
   sdl3_joypad_poll();
   SDL_INIT_INTERFACE(&desc);
   memset(&touch, 0, sizeof(touch));
   touch.nfingers  = 2;
   desc.type       = SDL_JOYSTICK_TYPE_GAMEPAD;
   /* its own IDs: SDL keeps the mapping it made for the first virtual
    * pad, which has no touchpad button, for any other with the same */
   desc.vendor_id  = 0x1209;
   desc.product_id = 0x7a01;
   desc.naxes      = 6;
   desc.nbuttons   = SDL_GAMEPAD_BUTTON_TOUCHPAD + 1;
   /* every button up to the touchpad's, so it is mapped where it is */
   desc.button_mask = (1u << (SDL_GAMEPAD_BUTTON_TOUCHPAD + 1)) - 1;
   desc.ntouchpads = 1;
   desc.touchpads  = &touch;
   if (!SDL_AttachVirtualJoystick(&desc))
   {
      printf("FAIL sdl3_checked_test: SDL could not attach a pad with a touchpad (%s)\n", SDL_GetError());
      return 1;
   }
   sdl3_joypad_poll();
   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].touch_fingers)
         port = i;
   if (port == MAX_USERS)
   {
      printf("FAIL sdl3_checked_test: the driver did not see the touchpad\n");
      return 1;
   }
   if (sdl3_joypad.get_touchpad(0 == port ? 1 : 0, &view))
      bad++;   /* a pad without one */

   SDL_SetJoystickVirtualTouchpad(sdl3_joypads[port].joypad, 0, 0, false, 0, 0, 0);
   SDL_SetJoystickVirtualTouchpad(sdl3_joypads[port].joypad, 0, 1, true, 1.0f, 0.5f, 1.0f);
   SDL_UpdateJoysticks();
   memset(&view, 0, sizeof(view));
   if (!sdl3_joypad.get_touchpad(port, &view) || view.touch_down != 2)
      bad++;
   if ((int16_t)(view.touch[1] & 0xffff) != 0x7fff || (int16_t)(view.touch[1] >> 16) != 0)
      bad++;

   SDL_SetJoystickVirtualTouchpad(sdl3_joypads[port].joypad, 0, 0, true, 0.0f, 0.0f, 1.0f);
   SDL_SetJoystickVirtualButton(sdl3_joypads[port].joypad, SDL_GAMEPAD_BUTTON_TOUCHPAD, true);
   SDL_UpdateJoysticks();
   if (!sdl3_joypad.get_touchpad(port, &view)
         || view.touch_down != (3 | INPUT_PAD_TOUCH_PRESS)
         || (int16_t)(view.touch[0] & 0xffff) != -0x7fff
         || (int16_t)(view.touch[0] >> 16) != -0x7fff)
      bad++;
   if (bad)
      printf("FAIL sdl3_checked_test: the touchpad's fingers were not handed over as SDL has them\n");
   return bad;
}

/* set_player() shows the port's player on the pad, none for 0 */
static unsigned check_players(void)
{
   unsigned bad = 0, i, slot = MAX_USERS;

   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].joypad)
         slot = i;
   sdl3_joypad.set_player(slot, 3);
   if (SDL_GetJoystickPlayerIndex(sdl3_joypads[slot].joypad) != 2)
      bad++;
   sdl3_joypad.set_player(slot, 0);
   if (SDL_GetJoystickPlayerIndex(sdl3_joypads[slot].joypad) != -1)
      bad++;
   if (bad)
      printf("FAIL sdl3_checked_test: set_player() did not reach the pad\n");
   return bad;
}

int main(void)
{
   unsigned n, bad = 0, pads = 0, gamepads = 0, i;
   unsigned ds3 = MAX_USERS;

   SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
   SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS3, "1");

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
   if (!attach_ds3())
   {
      printf("FAIL sdl3_checked_test: SDL could not attach a virtual DualShock 3 (%s)\n", SDL_GetError());
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
   if (pads != 3)
   {
      printf("FAIL sdl3_checked_test: the driver opened %u of the 3 virtual joysticks\n", pads);
      return 1;
   }
   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].gamepad && sdl3_joypads[i].num_axes == 16)
         ds3 = i;
   if (ds3 == MAX_USERS)
   {
      printf("FAIL sdl3_checked_test: the virtual DualShock 3 was not given its pressure axes\n");
      return 1;
   }
   bad += check_pressure(ds3);

   for (n = 0; n < 1500; n++)
      for (i = 0; i < MAX_USERS; i++)
      {
         if (!sdl3_joypads[i].joypad)
            continue;
         randomise(&sdl3_joypads[i]);
         SDL_UpdateJoysticks();
         bad += bv_compare(&sdl3_joypad, i);
      }

   bad += check_touchpad();
   bad += check_players();

   sdl3_joypad_destroy();
   if (bad)
   {
      printf("FAIL sdl3_checked_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS sdl3_checked_test: 1500 random states of %u virtual joysticks"
         " (%u opened as a gamepad); what the bridge would read is what the"
         " driver says; a touchpad's fingers and press as SDL has them;"
         " set_player() reaches the pad\n",
         pads, gamepads);
   return 0;
}
