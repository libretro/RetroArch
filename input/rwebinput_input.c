/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Michael Lelli
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

#include "../driver.h"

#include "../boolean.h"
#include "../general.h"

#include "../emscripten/RWebInput.h"

static bool uninited = false;

typedef struct rwebinput_input
{
   rwebinput_state_t state;
   int context;
} rwebinput_input_t;

static void *rwebinput_input_init(void)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)calloc(1, sizeof(*rwebinput));
   if (!rwebinput)
      return NULL;

   rwebinput->context = RWebInputInit();
   if (!rwebinput->context)
   {
      free(rwebinput);
      return NULL;
   }

   input_init_keyboard_lut(rarch_key_map_rwebinput);

   return rwebinput;
}

static bool rwebinput_key_pressed(rwebinput_input_t *rwebinput, int key)
{
   if (key >= RETROK_LAST)
      return false;

   unsigned sym = input_translate_rk_to_keysym((enum retro_key)key);
   bool ret = rwebinput->state.keys[sym >> 3] & (1 << (sym & 7));
   return ret;
}

static bool rwebinput_is_pressed(rwebinput_input_t *rwebinput, const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && rwebinput_key_pressed(rwebinput, binds[id].key);
   }
   else
      return false;
}

static bool rwebinput_bind_button_pressed(void *data, int key)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;
   return rwebinput_is_pressed(rwebinput, g_settings.input.binds[0], key);
}

static int16_t rwebinput_mouse_state(rwebinput_input_t *rwebinput, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return (int16_t) rwebinput->state.mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (int16_t) rwebinput->state.mouse_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return rwebinput->state.mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return rwebinput->state.mouse_r;
      default:
         return 0;
   }
}

static int16_t rwebinput_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return rwebinput_is_pressed(rwebinput, binds[port], id);

      case RETRO_DEVICE_KEYBOARD:
         return rwebinput_key_pressed(rwebinput, id);

      case RETRO_DEVICE_MOUSE:
         return rwebinput_mouse_state(rwebinput, id);

      default:
         return 0;
   }
}

static void rwebinput_input_free(void *data)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;
   uninited = true;

   RWebInputDestroy(rwebinput->context);

   free(data);
}

static void rwebinput_input_poll(void *data)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;

   rwebinput_state_t *state = RWebInputPoll(rwebinput->context);
   memcpy(&rwebinput->state, state, sizeof(rwebinput->state));
}

static void rwebinput_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

const input_driver_t input_rwebinput = {
   rwebinput_input_init,
   rwebinput_input_poll,
   rwebinput_input_state,
   rwebinput_bind_button_pressed,
   rwebinput_input_free,
   NULL,
   "rwebinput",
   rwebinput_grab_mouse,
};

