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
#include "../../frontend/drivers/platform_emscripten.h"

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

#define CLAMPDOUBLE(x) MIN(1.0, MAX(-1.0, (x)))
#define NUM_BUTTONS 64
#define NUM_AXES 64

struct rwebpad_joypad_rumble_state
{
   uint16_t pending_strong;
   uint16_t pending_weak;
   uint16_t strong;
   uint16_t weak;
};

typedef struct
{
   struct EmscriptenGamepadEvent pads[DEFAULT_MAX_PADS];
   struct rwebpad_joypad_rumble_state rumble[DEFAULT_MAX_PADS];
   bool live_pads[DEFAULT_MAX_PADS];
} rwebpad_joypad_data_t;

/* TODO/FIXME - static globals */
static rwebpad_joypad_data_t *rwebpad_joypad_data = NULL;

static EM_BOOL rwebpad_gamepad_cb(int event_type,
   const EmscriptenGamepadEvent *gamepad_event, void *user_data)
{
   unsigned vid = 1;
   unsigned pid = 1;

   if (gamepad_event->index >= DEFAULT_MAX_PADS)
      return EM_FALSE;

   switch (event_type)
   {
      case EMSCRIPTEN_EVENT_GAMEPADCONNECTED:
         rwebpad_joypad_data->pads[gamepad_event->index] = *gamepad_event;
         rwebpad_joypad_data->live_pads[gamepad_event->index] = true;
         input_autoconfigure_connect(
               gamepad_event->id,    /* name */
               NULL, NULL,           /* display names */
               rwebpad_joypad.ident, /* driver */
               gamepad_event->index, /* idx */
               vid,                  /* vid */
               pid);                 /* pid */
         break;
      case EMSCRIPTEN_EVENT_GAMEPADDISCONNECTED:
         rwebpad_joypad_data->live_pads[gamepad_event->index] = false;
         input_autoconfigure_disconnect(gamepad_event->index,
               rwebpad_joypad.ident);
         break;
      default:
         break;
   }

   return EM_TRUE;
}

static void *rwebpad_joypad_init(void *data)
{
   EMSCRIPTEN_RESULT r = emscripten_sample_gamepad_data();
   if (r == EMSCRIPTEN_RESULT_NOT_SUPPORTED)
      return NULL;

   if (!rwebpad_joypad_data)
   {
      rwebpad_joypad_data = (rwebpad_joypad_data_t*)calloc(1, sizeof(rwebpad_joypad_data_t));
      if (!rwebpad_joypad_data)
         return NULL;
   }

   /* callbacks need to be registered for gamepads to connect */
   r = emscripten_set_gamepadconnected_callback(NULL, false,
      rwebpad_gamepad_cb);

   r = emscripten_set_gamepaddisconnected_callback(NULL, false,
      rwebpad_gamepad_cb);

   return (void*)(-1);
}

static const char *rwebpad_joypad_name(unsigned port)
{
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return "";
   return rwebpad_joypad_data->pads[port].id;
}

static int32_t rwebpad_joypad_button(unsigned port, uint16_t joykey)
{
   EmscriptenGamepadEvent gamepad_state;
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return 0;
   gamepad_state = rwebpad_joypad_data->pads[port];
   if (joykey < gamepad_state.numButtons)
      return gamepad_state.digitalButton[joykey];
   return 0;
}

static void rwebpad_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   EmscriptenGamepadEvent gamepad_state;
   unsigned i;
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
   {
      BIT256_CLEAR_ALL_PTR(state);
      return;
   }
   gamepad_state = rwebpad_joypad_data->pads[port];
   for (i = 0; i < gamepad_state.numButtons; i++)
   {
      if (gamepad_state.digitalButton[i])
         BIT256_SET_PTR(state, i);
   }
}

static int16_t rwebpad_joypad_axis_state(
      EmscriptenGamepadEvent *gamepad_state,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < gamepad_state->numAxes)
   {
      int16_t val = CLAMPDOUBLE(
            gamepad_state->axis[AXIS_NEG_GET(joyaxis)]) * 0x7FFF;
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < gamepad_state->numAxes)
   {
      int16_t val = CLAMPDOUBLE(
            gamepad_state->axis[AXIS_POS_GET(joyaxis)]) * 0x7FFF;
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t rwebpad_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return 0;
   return rwebpad_joypad_axis_state(&rwebpad_joypad_data->pads[port], port, joyaxis);
}

static int16_t rwebpad_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   EmscriptenGamepadEvent gamepad_state;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   if (port_idx >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return 0;
   gamepad_state = rwebpad_joypad_data->pads[port];
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (joykey < gamepad_state.numButtons)
            && gamepad_state.digitalButton[(uint16_t)joykey]
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(rwebpad_joypad_axis_state(
                  &gamepad_state, port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void rwebpad_joypad_update_rumble(unsigned port)
{
   bool rumble_old_state = rwebpad_joypad_data->rumble[port].strong || rwebpad_joypad_data->rumble[port].weak;
   bool rumble_new_state = rwebpad_joypad_data->rumble[port].pending_strong || rwebpad_joypad_data->rumble[port].pending_weak;
   rwebpad_joypad_data->rumble[port].strong = rwebpad_joypad_data->rumble[port].pending_strong;
   rwebpad_joypad_data->rumble[port].weak   = rwebpad_joypad_data->rumble[port].pending_weak;

   if (rumble_new_state)
   {
      EM_ASM({
         try {
            JSEvents?.lastGamepadState?.[$0]?.vibrationActuator?.playEffect?.("dual-rumble", {startDelay: 0, duration: 200, strongMagnitude: $1 / 65536, weakMagnitude: $2 / 65536});
         } catch (e) {}
      }, port, rwebpad_joypad_data->rumble[port].strong, rwebpad_joypad_data->rumble[port].weak);
   }
   else if (rumble_old_state && !rumble_new_state)
   {
      EM_ASM({
         try {
            JSEvents?.lastGamepadState?.[$0]?.vibrationActuator?.reset?.();
         } catch (e) {}
      }, port);
   }
}

static void rwebpad_joypad_do_poll(void *data)
{
   unsigned port;
   emscripten_sample_gamepad_data();
   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      if (!rwebpad_joypad_data->live_pads[port])
         continue;
      emscripten_get_gamepad_status(port, &rwebpad_joypad_data->pads[port]);
      rwebpad_joypad_update_rumble(port);
   }
}

static void rwebpad_joypad_poll(void)
{
   platform_emscripten_run_on_browser_thread_sync(rwebpad_joypad_do_poll, NULL);
}

static bool rwebpad_joypad_query_pad(unsigned port)
{
   return rwebpad_joypad_data->live_pads[port];
}

static bool rwebpad_joypad_set_rumble(unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return false;

   switch (effect)
   {
      case RETRO_RUMBLE_STRONG:
         rwebpad_joypad_data->rumble[port].pending_strong = strength;
         break;
      case RETRO_RUMBLE_WEAK:
         rwebpad_joypad_data->rumble[port].pending_weak = strength;
         break;
      default:
         return false;
   }

   return true;
}

static void rwebpad_joypad_destroy(void) { }

input_device_driver_t rwebpad_joypad = {
   rwebpad_joypad_init,
   rwebpad_joypad_query_pad,
   rwebpad_joypad_destroy,
   rwebpad_joypad_button,
   rwebpad_joypad_state,
   rwebpad_joypad_get_buttons,
   rwebpad_joypad_axis,
   rwebpad_joypad_poll,
   rwebpad_joypad_set_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   rwebpad_joypad_name,
   "rwebpad",
};
