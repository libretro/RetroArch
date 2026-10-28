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

#if defined(__linux__)
#define SDL_DINGUX_HAS_RUMBLE 1
#include <retro_atomic.h>
#include "../common/linux_ff.h"
#include "../common/output_writer.h"
#endif

/* RS-90 devices:
 * - Analog input: No
 * - Menu button:  No
* RetroFW devices:
 * - Analog input: No
 * - Menu button:  Yes
 * Miyoo devices:
 * - Analog input: No
 * - Menu button:  Yes
 * All other OpenDingux devices:
 * - Analog input: Yes
 * - Menu button:  Yes
 */
#if !defined(RS90)
#if !(defined(MIYOO) || defined(RETROFW))
#define SDL_DINGUX_HAS_ANALOG      1
#endif
#define SDL_DINGUX_HAS_MENU_TOGGLE 1
#endif

/* Simple joypad driver designed to rationalise
 * the bizarre keyboard/gamepad hybrid setup
 * of OpenDingux devices */

#define SDL_DINGUX_JOYPAD_NAME "Dingux Gamepad"

/* All digital inputs map to keyboard keys:
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
 *
 * Miyoo devices (Pocketgo, Powkiddy V90 & Q90)
 * have the following alternate mappings:
 * - X:      SDLK_LSHIFT
 * - A:      SDLK_LALT
 * - B:      SDLK_LCTRL
 * - Y:      SDLK_SPACE
 * - Menu:   SDLK_RCTRL
 * - L3:     SDLK_RALT
 * - R3:     SDLK_RSHIFT
 */
#if defined(MIYOO)
#define SDL_DINGUX_SDLK_X      SDLK_LSHIFT
#define SDL_DINGUX_SDLK_A      SDLK_LALT
#define SDL_DINGUX_SDLK_B      SDLK_LCTRL
#define SDL_DINGUX_SDLK_Y      SDLK_SPACE
#else
#define SDL_DINGUX_SDLK_X      SDLK_SPACE
#define SDL_DINGUX_SDLK_A      SDLK_LCTRL
#define SDL_DINGUX_SDLK_B      SDLK_LALT
#define SDL_DINGUX_SDLK_Y      SDLK_LSHIFT
#endif
#define SDL_DINGUX_SDLK_L      SDLK_TAB
#define SDL_DINGUX_SDLK_R      SDLK_BACKSPACE
#define SDL_DINGUX_SDLK_L2     SDLK_PAGEUP
#define SDL_DINGUX_SDLK_R2     SDLK_PAGEDOWN
#define SDL_DINGUX_SDLK_SELECT SDLK_ESCAPE
#define SDL_DINGUX_SDLK_START  SDLK_RETURN
#if defined(MIYOO)
#define SDL_DINGUX_SDLK_L3     SDLK_RALT
#define SDL_DINGUX_SDLK_R3     SDLK_RSHIFT
#else
#define SDL_DINGUX_SDLK_L3     SDLK_KP_DIVIDE
#define SDL_DINGUX_SDLK_R3     SDLK_KP_PERIOD
#endif
#define SDL_DINGUX_SDLK_UP     SDLK_UP
#define SDL_DINGUX_SDLK_RIGHT  SDLK_RIGHT
#define SDL_DINGUX_SDLK_DOWN   SDLK_DOWN
#define SDL_DINGUX_SDLK_LEFT   SDLK_LEFT
#if defined(RETROFW)
#define SDL_DINGUX_SDLK_MENU   SDLK_END
#elif defined(MIYOO)
#define SDL_DINGUX_SDLK_MENU   SDLK_RCTRL
#else
#define SDL_DINGUX_SDLK_MENU   SDLK_HOME
#endif

#if defined(SDL_DINGUX_HAS_RUMBLE)
/* 5 ms period == 200 Hz
 * > Meissner's Corpuscle registers this
 *   as 'fast' motion */
#define SDL_DINGUX_RUMBLE_WEAK_PERIOD 5

/* 142 ms period ~= 7 Hz
 * > Merkel's Cells and Ruffini Ending register
 *   this as 'slow' motion */
#define SDL_DINGUX_RUMBLE_STRONG_PERIOD 142

/* The motor is written from the output writer's thread: the frontend's
 * notes what it wants, the strong strength over the weak in one word,
 * and the gain beside it. The motor's state is the writer's. */
typedef struct
{
   input_output_writer_t *writer;
   struct linux_ff_motor motor[2]; /* [RETRO_RUMBLE_STRONG], [RETRO_RUMBLE_WEAK] */
   int fd;
   int gain;                       /* written, or -1 */
   uint32_t want;                  /* the frontend's last asked */
   bool has_gain;
} dingux_joypad_rumble_t;

static retro_atomic_int_t dingux_rumble_want;
static retro_atomic_int_t dingux_rumble_want_gain;
#endif

typedef struct
{
#if defined(SDL_DINGUX_HAS_ANALOG)
   SDL_Joystick *device;
#endif
#if defined(SDL_DINGUX_HAS_RUMBLE)
   dingux_joypad_rumble_t rumble;
#endif
   unsigned num_axes;
   uint16_t pad_state;
   int16_t analog_state[2][2];
   bool connected;
#if defined(SDL_DINGUX_HAS_MENU_TOGGLE)
   bool menu_toggle;
#endif
} dingux_joypad_t;

static dingux_joypad_t dingux_joypad;

#if defined(SDL_DINGUX_HAS_RUMBLE)
/* What is wanted of the motor, written if it differs from what was. On
 * the writer's thread; on the frontend's only when there is no writer.
 * @last: the motor is let go. */
static void sdl_dingux_rumble_write(void *data, bool last)
{
   dingux_joypad_rumble_t *rumble = (dingux_joypad_rumble_t*)data;
   uint32_t want;
   int gain;

   if (last)
   {
      linux_ff_motor_erase(rumble->fd, &rumble->motor[RETRO_RUMBLE_STRONG]);
      linux_ff_motor_erase(rumble->fd, &rumble->motor[RETRO_RUMBLE_WEAK]);
      close(rumble->fd);
      rumble->fd = -1;
      return;
   }

   gain = retro_atomic_load_acquire_int(&dingux_rumble_want_gain);
   if (     rumble->has_gain
         && gain != rumble->gain
         && linux_ff_gain(rumble->fd, (unsigned)gain))
      rumble->gain = gain;

   want = (uint32_t)retro_atomic_load_acquire_int(&dingux_rumble_want);
   linux_ff_motor_set(rumble->fd, &rumble->motor[RETRO_RUMBLE_STRONG],
         SDL_DINGUX_RUMBLE_STRONG_PERIOD, (uint16_t)(want >> 16));
   linux_ff_motor_set(rumble->fd, &rumble->motor[RETRO_RUMBLE_WEAK],
         SDL_DINGUX_RUMBLE_WEAK_PERIOD, (uint16_t)want);
}

static void sdl_dingux_rumble_wake(dingux_joypad_rumble_t *rumble)
{
   if (rumble->writer)
      input_output_writer_wake(rumble->writer);
   else
      sdl_dingux_rumble_write(rumble, false);
}

static void sdl_dingux_rumble_init(dingux_joypad_rumble_t *rumble)
{
   rumble->fd = linux_ff_open_sine(2, &rumble->has_gain);
   if (rumble->fd < 0)
      return;
   linux_ff_motor_init(&rumble->motor[RETRO_RUMBLE_STRONG]);
   linux_ff_motor_init(&rumble->motor[RETRO_RUMBLE_WEAK]);
   rumble->gain = -1;
   rumble->want = 0;
   retro_atomic_store_release_int(&dingux_rumble_want, 0);
   retro_atomic_store_release_int(&dingux_rumble_want_gain,
         (int)input_config_get_rumble_gain());
   rumble->writer = input_output_writer_new(sdl_dingux_rumble_write, rumble);
   /* the gain, written as the motor is found */
   sdl_dingux_rumble_wake(rumble);
}

static void sdl_dingux_rumble_deinit(dingux_joypad_rumble_t *rumble)
{
   if (rumble->fd < 0)
      return;
   if (rumble->writer)
      input_output_writer_free(rumble->writer);
   else
      sdl_dingux_rumble_write(rumble, true);
   rumble->writer = NULL;
   rumble->fd     = -1;
}

static bool sdl_dingux_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   dingux_joypad_rumble_t *rumble = &dingux_joypad.rumble;
   uint32_t want;

   if (pad != 0 || rumble->fd < 0)
      return false;

   switch (effect)
   {
      case RETRO_RUMBLE_STRONG:
         want = (rumble->want & 0xFFFFu) | ((uint32_t)strength << 16);
         break;
      case RETRO_RUMBLE_WEAK:
         want = (rumble->want & 0xFFFF0000u) | strength;
         break;
      default:
         return false;
   }

   /* a core asking the same each frame costs nothing */
   if (want != rumble->want)
   {
      rumble->want = want;
      retro_atomic_store_release_int(&dingux_rumble_want, (int)want);
      sdl_dingux_rumble_wake(rumble);
   }
   return true;
}

static bool sdl_dingux_joypad_set_rumble_gain(unsigned pad, unsigned gain)
{
   dingux_joypad_rumble_t *rumble = &dingux_joypad.rumble;

   if (pad != 0 || rumble->fd < 0 || !rumble->has_gain)
      return false;
   if (gain > 100)
      gain = 100;
   retro_atomic_store_release_int(&dingux_rumble_want_gain, (int)gain);
   sdl_dingux_rumble_wake(rumble);
   return true;
}
#endif

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

#if defined(SDL_DINGUX_HAS_ANALOG)
   /* Open joypad device */
   if (SDL_NumJoysticks() > 0)
      joypad->device = SDL_JoystickOpen(0);

   /* If joypad exists, get number of axes */
   if (joypad->device)
      joypad->num_axes = SDL_JoystickNumAxes(joypad->device);
#endif

#if defined(SDL_DINGUX_HAS_RUMBLE)
   sdl_dingux_rumble_init(&joypad->rumble);
#endif

   /* 'Register' joypad connection via
    * autoconfig task */
   input_autoconfigure_connect(
         sdl_dingux_joypad_name(0), /* name */
         NULL, NULL,                /* display_names */
         sdl_dingux_joypad.ident,   /* driver */
         0,                         /* port */
         0,                         /* vid */
         0);                        /* pid */

   joypad->connected = true;
}

static void sdl_dingux_joypad_disconnect(void)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

#if defined(SDL_DINGUX_HAS_ANALOG)
   if (joypad->device)
      SDL_JoystickClose(joypad->device);
#endif

   if (joypad->connected)
      input_autoconfigure_disconnect(0, sdl_dingux_joypad.ident);

#if defined(SDL_DINGUX_HAS_RUMBLE)
   sdl_dingux_rumble_deinit(&joypad->rumble);
#endif

   memset(joypad, 0, sizeof(dingux_joypad_t));
#if defined(SDL_DINGUX_HAS_RUMBLE)
   joypad->rumble.fd = -1;
#endif
}

static void sdl_dingux_joypad_destroy(void)
{
   SDL_Event event;

   /* Disconnect joypad */
   sdl_dingux_joypad_disconnect();

   /* Flush out all pending events */
   while (SDL_PollEvent(&event));

#if defined(SDL_DINGUX_HAS_MENU_TOGGLE)
   input_driver_set_platform_menu_button(false);
#endif
}

static void *sdl_dingux_joypad_init(void *data)
{
   dingux_joypad_t *joypad      = (dingux_joypad_t*)&dingux_joypad;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   memset(joypad, 0, sizeof(dingux_joypad_t));
#if defined(SDL_DINGUX_HAS_RUMBLE)
   joypad->rumble.fd = -1;
#endif
#if defined(SDL_DINGUX_HAS_MENU_TOGGLE)
   input_driver_set_platform_menu_button(false);
#endif

#if defined(SDL_DINGUX_HAS_ANALOG)
   /* Initialise joystick subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_JOYSTICK) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
         return NULL;
   }
#endif

   /* Connect joypad */
   sdl_dingux_joypad_connect();

   return (void*)-1;
}

static bool sdl_dingux_joypad_query_pad(unsigned port)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;

   return (port == 0) && joypad->connected;
}

static int32_t sdl_dingux_joypad_button(unsigned port, uint16_t joykey)
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
#if defined(SDL_DINGUX_HAS_ANALOG)
   if (port == 0)
   {
      dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;
      if (AXIS_NEG_GET(joyaxis) < 4)
      {
         int16_t val  = 0;
         int16_t axis = AXIS_NEG_GET(joyaxis);
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
         if (val < 0)
            return val;
      }
      else if (AXIS_POS_GET(joyaxis) < 4)
      {
         int16_t val  = 0;
         int16_t axis = AXIS_POS_GET(joyaxis);
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
         if (val > 0)
            return val;
      }
   }
#endif
   return 0;
}

static int16_t sdl_dingux_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port != 0)
      return 0;

   return sdl_dingux_joypad_axis_state(port, joyaxis);
}

static int16_t sdl_dingux_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
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
      const uint64_t joykey  = joykeys[i];
#if defined(SDL_DINGUX_HAS_ANALOG)
      const uint32_t joyaxis = joyaxes[i];
#endif

      if ((uint16_t)joykey != NO_BTN &&
            (joypad->pad_state & (1 << (uint16_t)joykey)))
         ret |= (1 << i);
#if defined(SDL_DINGUX_HAS_ANALOG)
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(sdl_dingux_joypad_axis_state(port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
#endif
   }

   return ret;
}

static void sdl_dingux_joypad_poll(void)
{
   dingux_joypad_t *joypad = (dingux_joypad_t*)&dingux_joypad;
   SDL_Event event;

#if defined(SDL_DINGUX_HAS_MENU_TOGGLE)
   /* Note: The menu toggle key is an awkward special
    * case - the press/release events happen almost
    * instantaneously, and since we only sample once
    * per frame the input is often 'missed'.
    * If the toggle key gets pressed, we therefore have
    * to wait until the *next* frame to release it */
   if (joypad->menu_toggle)
   {
      input_driver_set_platform_menu_button(false);
      joypad->menu_toggle = false;
   }
#endif

   /* All digital inputs map to keyboard keys */
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_KEYDOWN:
            switch (event.key.keysym.sym)
            {
               case SDL_DINGUX_SDLK_X:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_X);
                  break;
               case SDL_DINGUX_SDLK_A:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_A);
                  break;
               case SDL_DINGUX_SDLK_B:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_B);
                  break;
               case SDL_DINGUX_SDLK_Y:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_Y);
                  break;
               case SDL_DINGUX_SDLK_L:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L);
                  break;
               case SDL_DINGUX_SDLK_R:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R);
                  break;
               case SDL_DINGUX_SDLK_L2:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L2);
                  break;
               case SDL_DINGUX_SDLK_R2:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R2);
                  break;
               case SDL_DINGUX_SDLK_SELECT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_SELECT);
                  break;
               case SDL_DINGUX_SDLK_START:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_START);
                  break;
               case SDL_DINGUX_SDLK_L3:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L3);
                  break;
               case SDL_DINGUX_SDLK_R3:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R3);
                  break;
               case SDL_DINGUX_SDLK_UP:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_UP);
                  break;
               case SDL_DINGUX_SDLK_RIGHT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
                  break;
               case SDL_DINGUX_SDLK_DOWN:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_DOWN);
                  break;
               case SDL_DINGUX_SDLK_LEFT:
                  BIT16_SET(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_LEFT);
                  break;
#if defined(SDL_DINGUX_HAS_MENU_TOGGLE)
               case SDL_DINGUX_SDLK_MENU:
                  input_driver_set_platform_menu_button(true);
                  joypad->menu_toggle = true;
                  break;
#endif
               default:
                  break;
            }
            break;
			case SDL_KEYUP:
            switch (event.key.keysym.sym)
            {
               case SDL_DINGUX_SDLK_X:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_X);
                  break;
               case SDL_DINGUX_SDLK_A:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_A);
                  break;
               case SDL_DINGUX_SDLK_B:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_B);
                  break;
               case SDL_DINGUX_SDLK_Y:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_Y);
                  break;
               case SDL_DINGUX_SDLK_L:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L);
                  break;
               case SDL_DINGUX_SDLK_R:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R);
                  break;
               case SDL_DINGUX_SDLK_L2:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L2);
                  break;
               case SDL_DINGUX_SDLK_R2:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R2);
                  break;
               case SDL_DINGUX_SDLK_SELECT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_SELECT);
                  break;
               case SDL_DINGUX_SDLK_START:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_START);
                  break;
               case SDL_DINGUX_SDLK_L3:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_L3);
                  break;
               case SDL_DINGUX_SDLK_R3:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_R3);
                  break;
               case SDL_DINGUX_SDLK_UP:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_UP);
                  break;
               case SDL_DINGUX_SDLK_RIGHT:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
                  break;
               case SDL_DINGUX_SDLK_DOWN:
                  BIT16_CLEAR(joypad->pad_state, RETRO_DEVICE_ID_JOYPAD_DOWN);
                  break;
               case SDL_DINGUX_SDLK_LEFT:
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

#if defined(SDL_DINGUX_HAS_ANALOG)
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
#endif
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
#if defined(SDL_DINGUX_HAS_RUMBLE)
   sdl_dingux_joypad_set_rumble,
   sdl_dingux_joypad_set_rumble_gain,
#else
   NULL,
   NULL,
#endif
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   sdl_dingux_joypad_name,
   "sdl_dingux",
};
