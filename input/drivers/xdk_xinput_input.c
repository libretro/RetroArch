/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdint.h>
#include <stdlib.h>

#ifdef _XBOX
#include <xtl.h>
#endif

#include <boolean.h>
#include <libretro.h>

#include "../../driver.h"
#include "../../general.h"

#define MAX_PADS 4

typedef struct xdk_input
{
   bool blocked;
   const input_device_driver_t *joypad;
} xdk_input_t;

static void xdk_input_poll(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

   if (xdk && xdk->joypad)
      xdk->joypad->poll();
}

static int16_t xdk_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

   if (port >= MAX_PADS)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(xdk->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(xdk->joypad, port, index, id, binds[port]);
   }

   return 0;
}

static void xdk_input_free_input(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

   if (!xdk)
      return;

   if (xdk->joypad)
      xdk->joypad->destroy();

   free(xdk);
}

static void *xdk_input_init(void)
{
   settings_t *settings = config_get_ptr();
   xdk_input_t *xdk     = (xdk_input_t*)calloc(1, sizeof(*xdk));
   if (!xdk)
      return NULL;

   xdk->joypad = input_joypad_init_driver(settings->input.joypad_driver, xdk);

   return xdk;
}

static bool xdk_input_key_pressed(void *data, int key)
{
   xdk_input_t *xdk     = (xdk_input_t*)data;
   settings_t *settings = config_get_ptr();

   if (input_joypad_pressed(xdk->joypad, 0, settings->input.binds[0], key))
      return true;

   return false;
}

static bool xdk_input_meta_key_pressed(void *data, int key)
{
   return false;
}

static uint64_t xdk_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
}

/* FIXME - are we sure about treating low frequency motor as the 
 * "strong" motor? Does it apply for Xbox too? */

static bool xdk_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   (void)xdk;
   bool val = false;

  
#if 0
#if defined(_XBOX360)
   XINPUT_VIBRATION rumble_state;

   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.wRightMotorSpeed = strength;
   val = XInputSetState(port, &rumble_state) == ERROR_SUCCESS;
#elif defined(_XBOX1)
#if 0
   XINPUT_FEEDBACK rumble_state;

   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.Rumble.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.Rumble.wRightMotorSpeed = strength;
   val = XInputSetState(xdk->gamepads[port], &rumble_state) == ERROR_SUCCESS;
#endif
#endif
#endif
   return val;
}

static const input_device_driver_t *xdk_input_get_joypad_driver(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   if (!xdk)
      return NULL;
   return xdk->joypad;
}

static void xdk_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool xdk_keyboard_mapping_is_blocked(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   if (!xdk)
      return false;
   return xdk->blocked;
}

static void xdk_keyboard_mapping_set_block(void *data, bool value)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   if (!xdk)
      return;
   xdk->blocked = value;
}

input_driver_t input_xinput = {
   xdk_input_init,
   xdk_input_poll,
   xdk_input_state,
   xdk_input_key_pressed,
   xdk_input_meta_key_pressed,
   xdk_input_free_input,
   NULL,
   NULL,
   xdk_input_get_capabilities,
   "xinput",
   xdk_input_grab_mouse,
   NULL,
   xdk_input_set_rumble,
   xdk_input_get_joypad_driver,
   NULL,
   xdk_keyboard_mapping_is_blocked,
   xdk_keyboard_mapping_set_block,
};
