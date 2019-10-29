/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Michael Lelli
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
#include <stddef.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <emscripten/html5.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define CLAMPDOUBLE(x) MIN(1.0, MAX(-1.0, (x)))

static bool g_rwebpad_initialized;

static EM_BOOL rwebpad_gamepad_cb(int event_type,
   const EmscriptenGamepadEvent *gamepad_event, void *user_data)
{
   unsigned vid = 0;
   unsigned pid = 0;

   (void)event_type;
   (void)gamepad_event;
   (void)user_data;

   if (strncmp(gamepad_event->mapping, "standard",
       sizeof(gamepad_event->mapping)) == 0)
   {
      /* give a dummy vid/pid for automapping */
      vid = 1;
      pid = 1;
   }

   if (event_type == EMSCRIPTEN_EVENT_GAMEPADCONNECTED)
      input_autoconfigure_connect(
               gamepad_event->id,    /* name */
               NULL,                 /* display name */
               rwebpad_joypad.ident, /* driver */
               gamepad_event->index, /* idx */
               vid,                  /* vid */
               pid);                 /* pid */
   else if (event_type == EMSCRIPTEN_EVENT_GAMEPADDISCONNECTED)
      input_autoconfigure_disconnect(gamepad_event->index,
         rwebpad_joypad.ident);

   return EM_TRUE;
}

static bool rwebpad_joypad_init(void *data)
{
   EMSCRIPTEN_RESULT supported;
   (void)data;

   supported = emscripten_sample_gamepad_data();
   if (supported == EMSCRIPTEN_RESULT_NOT_SUPPORTED)
      return false;

   if (!g_rwebpad_initialized)
   {
      EMSCRIPTEN_RESULT r;
      g_rwebpad_initialized = true;

      /* callbacks needs to be registered for gamepads to connect */
      r = emscripten_set_gamepadconnected_callback(NULL, false,
         rwebpad_gamepad_cb);
      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR(
            "[EMSCRIPTEN/PAD] failed to create connect callback: %d\n", r);
      }

      r = emscripten_set_gamepaddisconnected_callback(NULL, false,
         rwebpad_gamepad_cb);
      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR(
            "[EMSCRIPTEN/PAD] failed to create disconnect callback: %d\n", r);
      }
   }

   return true;
}

static const char *rwebpad_joypad_name(unsigned pad)
{
   static EmscriptenGamepadEvent gamepad_state;
   EMSCRIPTEN_RESULT r = emscripten_get_gamepad_status(pad, &gamepad_state);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
      return gamepad_state.id;
   return "";
}

static bool rwebpad_joypad_button(unsigned port_num, uint16_t joykey)
{
   EmscriptenGamepadEvent gamepad_state;
   EMSCRIPTEN_RESULT r = emscripten_get_gamepad_status(
         port_num, &gamepad_state);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
      if (joykey < gamepad_state.numButtons)
         return gamepad_state.digitalButton[joykey];

   return false;
}

static void rwebpad_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   EmscriptenGamepadEvent gamepad_state;
   EMSCRIPTEN_RESULT r = emscripten_get_gamepad_status(
         port_num, &gamepad_state);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
   {
      unsigned i;

      for (i = 0; i < gamepad_state.numButtons; i++)
      {
         if (gamepad_state.digitalButton[i])
            BIT256_SET_PTR(state, i);
      }
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t rwebpad_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   EmscriptenGamepadEvent gamepad_state;
   EMSCRIPTEN_RESULT r;
   int16_t val = 0;

   if (joyaxis == 0xFFFFFFFF)
      return 0;

   r = emscripten_get_gamepad_status(port_num, &gamepad_state);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
   {
      if (AXIS_NEG_GET(joyaxis) < gamepad_state.numAxes)
      {
         val = CLAMPDOUBLE(gamepad_state.axis[AXIS_NEG_GET(joyaxis)]) * 0x7FFF;
         if (val > 0)
            val = 0;
      }
      else if (AXIS_POS_GET(joyaxis) < gamepad_state.numAxes)
      {
         val = CLAMPDOUBLE(gamepad_state.axis[AXIS_POS_GET(joyaxis)]) * 0x7FFF;
         if (val < 0)
            val = 0;
      }
   }

   return val;
}

static void rwebpad_joypad_poll(void)
{
   (void)emscripten_sample_gamepad_data();
}

static bool rwebpad_joypad_query_pad(unsigned pad)
{
   EmscriptenGamepadEvent gamepad_state;
   EMSCRIPTEN_RESULT r = emscripten_get_gamepad_status(pad, &gamepad_state);

   if (r == EMSCRIPTEN_RESULT_SUCCESS)
      return gamepad_state.connected == EM_TRUE;

   return false;
}

static void rwebpad_joypad_destroy(void)
{
}

input_device_driver_t rwebpad_joypad = {
   rwebpad_joypad_init,
   rwebpad_joypad_query_pad,
   rwebpad_joypad_destroy,
   rwebpad_joypad_button,
   rwebpad_joypad_get_buttons,
   rwebpad_joypad_axis,
   rwebpad_joypad_poll,
   NULL,
   rwebpad_joypad_name,
   "rwebpad",
};
