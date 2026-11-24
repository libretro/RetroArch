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
#include <string.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <emscripten/html5.h>
#include "../../frontend/drivers/platform_emscripten.h"

#include "../input_driver.h"
#include "../include/input_pad_view.h"

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
   /* each live pad as the poll left it, lent to the readers
    * (input_pad_view.h): its buttons and its first sixteen axes */
   input_pad_view_t views[DEFAULT_MAX_PADS];
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

/* An axis of the event, one of the first numAxes the pad has and no
 * more than the event holds, as a stick's value; 0 past them */
static int16_t rwebpad_joypad_event_axis(
      const EmscriptenGamepadEvent *gamepad_state, unsigned i)
{
   unsigned axes = (unsigned)gamepad_state->numAxes;
   if (axes > ARRAY_SIZE(gamepad_state->axis))
      axes = ARRAY_SIZE(gamepad_state->axis);
   if (i >= axes)
      return 0;
   return (int16_t)(CLAMPDOUBLE(gamepad_state->axis[i]) * 0x7FFF);
}

/* The view of what the poll read of the pad on @port */
static void rwebpad_joypad_make_view(unsigned port)
{
   unsigned i, buttons;
   const EmscriptenGamepadEvent *gamepad_state =
      &rwebpad_joypad_data->pads[port];
   input_pad_view_t *v = &rwebpad_joypad_data->views[port];

   memset(v, 0, sizeof(*v));
   /* one of the first numButtons the pad has, no more than the event
    * holds */
   buttons = (unsigned)gamepad_state->numButtons;
   if (buttons > ARRAY_SIZE(gamepad_state->digitalButton))
      buttons = ARRAY_SIZE(gamepad_state->digitalButton);
   for (i = 0; i < buttons; i++)
      if (gamepad_state->digitalButton[i])
         BIT256_SET(v->buttons, i);
   for (i = 0; i < INPUT_PAD_VIEW_AXES; i++)
      v->axes[i] = rwebpad_joypad_event_axis(gamepad_state, i);
}

/* The live pad on @port's view; NULL with none there */
static const input_pad_view_t *rwebpad_joypad_view(unsigned port)
{
   if (port >= DEFAULT_MAX_PADS || !rwebpad_joypad_data->live_pads[port])
      return NULL;
   return &rwebpad_joypad_data->views[port];
}

static int32_t rwebpad_joypad_button(unsigned port, uint16_t joykey)
{
   const input_pad_view_t *v = rwebpad_joypad_view(port);
   return v ? input_pad_view_button(v, joykey) : 0;
}

static void rwebpad_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   const input_pad_view_t *v = rwebpad_joypad_view(port);
   if (v)
      *state = v->buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t rwebpad_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t val;
   const input_pad_view_t *v = rwebpad_joypad_view(port);
   if (!v)
      return 0;
   if (     AXIS_NEG_GET(joyaxis) < INPUT_PAD_VIEW_AXES
         || AXIS_POS_GET(joyaxis) < INPUT_PAD_VIEW_AXES)
      return input_pad_view_axis(v, joyaxis);
   /* the axes past the view's, from the event the poll read */
   if (AXIS_NEG_GET(joyaxis) < 0xFFFF)
   {
      val = rwebpad_joypad_event_axis(&rwebpad_joypad_data->pads[port],
            AXIS_NEG_GET(joyaxis));
      return (val < 0) ? val : 0;
   }
   val = rwebpad_joypad_event_axis(&rwebpad_joypad_data->pads[port],
         AXIS_POS_GET(joyaxis));
   return (val > 0) ? val : 0;
}

static int16_t rwebpad_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   unsigned i;
   int16_t ret               = 0;
   uint16_t port_idx         = joypad_info->joy_idx;
   const input_pad_view_t *v = rwebpad_joypad_view(port_idx);
   if (!v)
      return 0;
   /* the common loop, with the axes past the view's read from the
    * event */
   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (joykeys[i] != NO_BTN && input_pad_view_button(v, joykeys[i]))
         ret |= (1 << i);
      else if (joyaxes[i] != AXIS_NONE &&
            ((float)abs(rwebpad_joypad_axis(port_idx, joyaxes[i]))
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
            JSEvents?.lastGamepadState?.[$0]?.vibrationActuator?.playEffect?.("dual-rumble", {"startDelay": 0, "duration": 200, "strongMagnitude": $1 / 65535, "weakMagnitude": $2 / 65535});
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
      rwebpad_joypad_make_view(port);
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
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   rwebpad_joypad_view
};
