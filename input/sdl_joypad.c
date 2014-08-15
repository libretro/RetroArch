/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "input_common.h"
#include "SDL.h"
#include "../general.h"

typedef struct _sdl_joypad
{
   SDL_Joystick *joypad;
#ifdef HAVE_SDL2
   SDL_Haptic *haptic;
   int rumble_effect; // -1 = not initialized, -2 = error/unsupported
#endif
   unsigned num_axes;
   unsigned num_buttons;
   unsigned num_hats;
#ifdef HAVE_SDL2
   unsigned num_balls;
#endif
} sdl_joypad_t;

static sdl_joypad_t g_pads[MAX_PLAYERS];
#ifdef HAVE_SDL2
static bool g_has_haptic;
#endif

static void sdl_joypad_connect(int id)
{
   sdl_joypad_t *pad = &g_pads[id];
   pad->joypad = SDL_JoystickOpen(id);
   if (!pad->joypad)
   {
      RARCH_ERR("[SDL]: Couldn't open SDL joystick #%u.\n", id);
      return;
   }

   RARCH_LOG("[SDL]: Joypad #%u connected: %s.\n",
#ifdef HAVE_SDL2
             id, SDL_JoystickName(pad->joypad));
#else
             id, SDL_JoystickName(id));
#endif


#ifdef HAVE_SDL2
   pad->haptic = g_has_haptic ? SDL_HapticOpenFromJoystick(pad->joypad) : NULL;

   if (g_has_haptic && !pad->haptic)
      RARCH_WARN("[SDL]: Couldn't open haptic device of the joypad #%u: %s\n",
                 id, SDL_GetError());

   pad->rumble_effect = -1;

   if (pad->haptic)
   {
      SDL_HapticEffect efx;
      efx.type = SDL_HAPTIC_LEFTRIGHT;
      efx.leftright.type = SDL_HAPTIC_LEFTRIGHT;
      efx.leftright.large_magnitude = efx.leftright.small_magnitude = 0x4000;
      efx.leftright.length = 5000;

      if (SDL_HapticEffectSupported(pad->haptic, &efx) == SDL_FALSE)
      {
         pad->rumble_effect = -2;
         RARCH_WARN("[SDL]: Joypad #%u does not support rumble.\n", id);
      }
   }
#endif

   pad->num_axes    = SDL_JoystickNumAxes(pad->joypad);
   pad->num_buttons = SDL_JoystickNumButtons(pad->joypad);
   pad->num_hats    = SDL_JoystickNumHats(pad->joypad);

#ifdef HAVE_SDL2
   pad->num_balls   = SDL_JoystickNumBalls(pad->joypad);

   RARCH_LOG("[SDL]: Joypad #%u has: %u axes, %u buttons, %u hats and %u trackballs.\n",
             id, pad->num_axes, pad->num_buttons, pad->num_hats, pad->num_balls);
#else
   RARCH_LOG("[SDL]: Joypad #%u has: %u axes, %u buttons, %u hats.\n",
             id, pad->num_axes, pad->num_buttons, pad->num_hats);
#endif
}

static void sdl_joypad_disconnect(int id)
{
#ifdef HAVE_SDL2
   if (g_pads[id].haptic)
      SDL_HapticClose(g_pads[id].haptic);
#endif

   if (g_pads[id].joypad)
   {
      SDL_JoystickClose(g_pads[id].joypad);
      RARCH_LOG("[SDL]: Joypad #%u disconnected.\n", id);
   }

   memset(&g_pads[id], 0, sizeof(g_pads[id]));
}

static void sdl_joypad_destroy(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
      sdl_joypad_disconnect(i);

   SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
   memset(g_pads, 0, sizeof(g_pads));
}

static bool sdl_joypad_init(void)
{
   unsigned i;
   if (SDL_WasInit(0) == 0 && SDL_Init(SDL_INIT_JOYSTICK) < 0)
      return false;
   else if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
      return false;

#if HAVE_SDL2
   // TODO: Add SDL_GameController support.
   //if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
   //   RARCH_LOG("[SDL]: Failed to initialize game controller interface: %s\n",
   //             SDL_GetError());

   g_has_haptic = false;
   if (SDL_InitSubSystem(SDL_INIT_HAPTIC) < 0)
      RARCH_WARN("[SDL]: Failed to initialize haptic device support: %s\n",
            SDL_GetError());
   else
      g_has_haptic = true;
#endif

   unsigned num_sticks = SDL_NumJoysticks();
   if (num_sticks > MAX_PLAYERS)
      num_sticks = MAX_PLAYERS;

   for (i = 0; i < num_sticks; i++)
      sdl_joypad_connect(i);

#ifndef HAVE_SDL2
   /* quit if no joypad is detected. */
   num_sticks = 0;
   for (i = 0; i < MAX_PLAYERS; i++)
      if (g_pads[i].joypad)
         num_sticks++;

   if (num_sticks == 0)
      goto error;
#endif

   return true;

#ifndef HAVE_SDL2
error:
   sdl_joypad_destroy();
   return false;
#endif
}

static bool sdl_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   const sdl_joypad_t *pad = &g_pads[port];
   if (!pad->joypad)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      uint16_t hat = GET_HAT(joykey);
      if (hat >= pad->num_hats)
         return false;

      Uint8 dir = SDL_JoystickGetHat(pad->joypad, hat);
      switch (GET_HAT_DIR(joykey))
      {
         case HAT_UP_MASK:
            return dir & SDL_HAT_UP;
         case HAT_DOWN_MASK:
            return dir & SDL_HAT_DOWN;
         case HAT_LEFT_MASK:
            return dir & SDL_HAT_LEFT;
         case HAT_RIGHT_MASK:
            return dir & SDL_HAT_RIGHT;
         default:
            return false;
      }
   }
   else // Check the button
   {
      if (joykey < pad->num_buttons && SDL_JoystickGetButton(pad->joypad, joykey))
         return true;

      return false;
   }
}

static int16_t sdl_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const sdl_joypad_t *pad = &g_pads[port];
   if (!pad->joypad)
      return false;

   Sint16 val = 0;
   if (AXIS_NEG_GET(joyaxis) < pad->num_axes)
   {
      val = SDL_JoystickGetAxis(pad->joypad, AXIS_NEG_GET(joyaxis));

      if (val > 0)
         val = 0;
      else if (val < -0x7fff) // -0x8000 can cause trouble if we later abs() it.
         val = -0x7fff;
   }
   else if (AXIS_POS_GET(joyaxis) < pad->num_axes)
   {
      val = SDL_JoystickGetAxis(pad->joypad, AXIS_POS_GET(joyaxis));

      if (val < 0)
         val = 0;
   }

   return val;
}

static void sdl_joypad_poll(void)
{
#ifdef HAVE_SDL2
   SDL_Event event;

   SDL_PumpEvents();
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED) > 0)
   {
      if (event.type == SDL_JOYDEVICEADDED)
      {
         sdl_joypad_connect(event.jdevice.which);
      }
      else if (event.type == SDL_JOYDEVICEREMOVED)
      {
         sdl_joypad_disconnect(event.jdevice.which);
      }
   }
#else
   SDL_JoystickUpdate();
#endif
}

#ifdef HAVE_SDL2
static bool sdl_joypad_set_rumble(unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   SDL_HapticEffect efx;
   memset(&efx, 0, sizeof(efx));

   sdl_joypad_t *joypad = &g_pads[pad];

   if (!joypad->joypad || !joypad->haptic)
      return false;

   efx.type = SDL_HAPTIC_LEFTRIGHT;
   efx.leftright.type  = SDL_HAPTIC_LEFTRIGHT;
   efx.leftright.length = 5000;

   switch (effect)
   {
      case RETRO_RUMBLE_STRONG: efx.leftright.large_magnitude = strength; break;
      case RETRO_RUMBLE_WEAK: efx.leftright.small_magnitude = strength; break;
      default: return false;
   }

   if (joypad->rumble_effect == -1)
   {
      joypad->rumble_effect = SDL_HapticNewEffect(g_pads[pad].haptic, &efx);
      if (joypad->rumble_effect < 0)
      {
         RARCH_WARN("[SDL]: Failed to create rumble effect for joypad %u: %s\n",
                    pad, SDL_GetError());
         joypad->rumble_effect = -2;
         return false;
      }
   }
   else if (joypad->rumble_effect >= 0)
      SDL_HapticUpdateEffect(joypad->haptic, joypad->rumble_effect, &efx);

   if (joypad->rumble_effect < 0)
      return false;

   if (SDL_HapticRunEffect(joypad->haptic, joypad->rumble_effect, 1) < 0)
   {
      RARCH_WARN("[SDL]: Failed to set rumble effect on joypad %u: %s\n",
                          pad, SDL_GetError());
      return false;
   }

   return true;
}
#endif

static bool sdl_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].joypad;
}

static const char *sdl_joypad_name(unsigned pad)
{
   if (pad >= MAX_PLAYERS)
      return NULL;

#ifdef HAVE_SDL2
   return SDL_JoystickName(g_pads[pad].joypad);
#else
   return SDL_JoystickName(pad);
#endif
}

const rarch_joypad_driver_t sdl_joypad = {
   sdl_joypad_init,
   sdl_joypad_query_pad,
   sdl_joypad_destroy,
   sdl_joypad_button,
   sdl_joypad_axis,
   sdl_joypad_poll,
#ifdef HAVE_SDL2
   sdl_joypad_set_rumble,
#else
   NULL,
#endif
   sdl_joypad_name,
#ifdef HAVE_SDL2
   "sdl2",
#else
   "sdl"
#endif
};

