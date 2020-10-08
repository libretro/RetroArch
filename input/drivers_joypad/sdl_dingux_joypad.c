/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2020 - Daniel De Matteis
 *  Copyright (C) 2019-2020 - James Leaver
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

#include <SDL/SDL.h>

#include <libretro.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

/* Simple joypad driver designed to rationalise
 * the bizarre keyboard/gamepad hybrid setup
 * of OpenDingux devices */

#define SDL_DINGUX_JOYPAD_NAME "Dingux Gamepad"

typedef struct
{
   SDL_Joystick *device;
   uint16_t pad_state;
   int16_t analog_state[2][2];
   unsigned num_axes;
   bool connected;
   bool menu_toggle;
} dingux_joypad_t;

/* TODO/FIXME - global referenced outside */
extern uint64_t lifecycle_state;

static dingux_joypad_t dingux_joypad;

static const char *sdl_dingux_joypad_name(unsigned port)
{
   const char *joypad_name = NULL;

   if (port != 0)
      return NULL;

   return SDL_DINGUX_JOYPAD_NAME;
}

static void sdl_dingux_joypad_connect(void)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   /* Open joypad device */
   if (SDL_NumJoysticks() > 0)
      joypad->device = SDL_JoystickOpen(0);

   /* If joypad exists, get number of axes */
   if (joypad->device)
      joypad->num_axes = SDL_JoystickNumAxes(joypad->device);

   /* 'Register' joypad connection via
    * autoconfig task */
   input_autoconfigure_connect(
         sdl_dingux_joypad_name(0), /* name */
         NULL,                      /* display_name */
         sdl_dingux_joypad.ident,   /* driver */
         0,                         /* port */
         0,                         /* vid */
         0);                        /* pid */

   joypad->connected = true;
}

static void sdl_dingux_joypad_disconnect(void)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   if (joypad->device)
      SDL_JoystickClose(joypad->device);

   if (joypad->connected)
      input_autoconfigure_disconnect(0, sdl_dingux_joypad.ident);

   memset(joypad, 0, sizeof(dingux_joypad_t));
}

static void sdl_dingux_joypad_destroy(void)
{
   SDL_Event event;

   /* Disconnect joypad */
   sdl_dingux_joypad_disconnect();

   /* De-initialise joystick subsystem */
   SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

   /* Flush out all pending events */
   while (SDL_PollEvent(&event));

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
}

static void *sdl_dingux_joypad_init(void *data)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   memset(joypad, 0, sizeof(dingux_joypad_t));
   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

   /* Initialise joystick subsystem */
   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
      return NULL;

   /* Connect joypad */
   sdl_dingux_joypad_connect();

   return (void*)-1;
}

static bool sdl_dingux_joypad_query_pad(unsigned port)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   return (port == 0) && joypad->connected;
}

static int16_t sdl_dingux_joypad_button(unsigned port, uint16_t joykey)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   if (port != 0)
      return 0;

   return (joypad->pad_state & (1 << joykey));
}

static void sdl_dingux_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   /* Macros require braces here... */
   if (port == 0)
   {
      BITS_COPY16_PTR(state, joypad->pad_state);
   }
   else
   {
      BIT256_CLEAR_ALL_PTR(state);
   }
}

static int16_t sdl_dingux_joypad_axis_state(unsigned port, uint32_t joyaxis)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;
   int val                 = 0;
   int axis                = -1;
   bool is_neg             = false;
   bool is_pos             = false;

   if (port != 0)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
      case 1:
         val = joypad->analog_state[0][axis];
         break;
      case 2:
      case 3:
         val = joypad->analog_state[1][axis - 2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;

   return val;
}

static int16_t sdl_dingux_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port != 0)
      return 0;

   return sdl_dingux_joypad_axis_state(port, joyaxis);
}

static int16_t sdl_dingux_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;
   uint16_t port_idx       = joypad_info->joy_idx;
   int16_t ret             = 0;
   size_t i;

   if (port_idx != 0)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      
      if ((uint16_t)joykey != NO_BTN && 
            (joypad->pad_state & (1 << (uint16_t)joykey)))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(sdl_dingux_joypad_axis_state(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void sdl_dingux_joypad_poll(void)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;
   SDL_Event event;

   /* Note: The menu toggle key is an awkward special
    * case - the press/release events happen almost
    * instantaneously, and since we only sample once
    * per frame the input is often 'missed'.
    * If the toggle key gets pressed, we therefore have
    * to wait until the *next* frame to release it */
   if (joypad->menu_toggle)
   {
      BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
      joypad->menu_toggle = false;
   }

   /* All digital inputs map to keyboard keys
    * - X:      SDLK_SPACE
    * - A:      SDLK_LCTRL
    * - B:      SDLK_LALT
    * - Y:      SDLK_LSHIFT
    * - L:      SDLK_TAB
    * - R:      SDLK_BACKSPACE
    * - L2:     SDLK_PAGEUP
    * - R2:     SDLK_PAGEDOWN
    * - Select: SDLK_ESCAPE
    * - Start:  SDLK_RETURN
    * - L3:     SDLK_KP_DIVIDE
    * - R3:     SDLK_KP_PERIOD
    * - Up:     SDLK_UP
    * - Right:  SDLK_RIGHT
    * - Down:   SDLK_DOWN
    * - Left:   SDLK_LEFT
    * - Menu:   SDLK_HOME
    */
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
               case SDLK_SPACE:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_X);
                  break;
               case SDLK_LCTRL:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_A);
                  break;
               case SDLK_LALT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_B);
                  break;
               case SDLK_LSHIFT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_Y);
                  break;
               case SDLK_TAB:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L);
                  break;
               case SDLK_BACKSPACE:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R);
                  break;
               case SDLK_PAGEUP:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L2);
                  break;
               case SDLK_PAGEDOWN:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R2);
                  break;
               case SDLK_ESCAPE:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_SELECT);
                  break;
               case SDLK_RETURN:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_START);
                  break;
               case SDLK_KP_DIVIDE:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L3);
                  break;
               case SDLK_KP_PERIOD:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R3);
                  break;
               case SDLK_UP:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_UP);
                  break;
               case SDLK_RIGHT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
                  break;
               case SDLK_DOWN:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_DOWN);
                  break;
               case SDLK_LEFT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_LEFT);
                  break;
               case SDLK_HOME:
                  BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
                  joypad->menu_toggle = true;
                  break;
               default:
                  break;
            }
            break;
			case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
               case SDLK_SPACE:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_X);
                  break;
               case SDLK_LCTRL:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_A);
                  break;
               case SDLK_LALT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_B);
                  break;
               case SDLK_LSHIFT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_Y);
                  break;
               case SDLK_TAB:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L);
                  break;
               case SDLK_BACKSPACE:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R);
                  break;
               case SDLK_PAGEUP:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L2);
                  break;
               case SDLK_PAGEDOWN:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R2);
                  break;
               case SDLK_ESCAPE:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_SELECT);
                  break;
               case SDLK_RETURN:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_START);
                  break;
               case SDLK_KP_DIVIDE:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L3);
                  break;
               case SDLK_KP_PERIOD:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R3);
                  break;
               case SDLK_UP:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_UP);
                  break;
               case SDLK_RIGHT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
                  break;
               case SDLK_DOWN:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_DOWN);
                  break;
               case SDLK_LEFT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_LEFT);
                  break;
               default:
                  break;
            }
				break;
			default:
				break;
		}
	}

   /* Analog inputs come from the joypad device,
    * if connected */
   if (joypad->device)
   {
      int16_t axis_value;

      SDL_JoystickUpdate();

      if (joypad->num_axes > 0)
      {
         axis_value = SDL_JoystickGetAxis(joypad->device, 0);
         /* -0x8000 can cause trouble if we later abs() it */
         joypad->analog_state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] =
               (axis_value < -0x7FFF) ? -0x7FFF : axis_value;
      }

      if (joypad->num_axes > 1)
      {
         axis_value = SDL_JoystickGetAxis(joypad->device, 1);
         joypad->analog_state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] =
               (axis_value < -0x7FFF) ? -0x7FFF : axis_value;
      }

      if (joypad->num_axes > 2)
      {
         axis_value = SDL_JoystickGetAxis(joypad->device, 2);
         joypad->analog_state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] =
               (axis_value < -0x7FFF) ? -0x7FFF : axis_value;
      }

      if (joypad->num_axes > 3)
      {
         axis_value = SDL_JoystickGetAxis(joypad->device, 3);
         joypad->analog_state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] =
               (axis_value < -0x7FFF) ? -0x7FFF : axis_value;
      }
   }
}

input_device_driver_t sdl_dingux_joypad = {
   sdl_dingux_joypad_init,
   sdl_dingux_joypad_query_pad,
   sdl_dingux_joypad_destroy,
   sdl_dingux_joypad_button,
   sdl_dingux_joypad_state,
   sdl_dingux_joypad_get_buttons,
   sdl_dingux_joypad_axis,
   sdl_dingux_joypad_poll,
   NULL,                          /* set_rumble */
   sdl_dingux_joypad_name,
   "sdl_dingux",
};
