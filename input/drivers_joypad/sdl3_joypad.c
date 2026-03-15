/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Higor Euripedes
 *  Copyright (C)      2023 - Carlo Refice
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
#include <string.h>

#include <SDL3/SDL.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

typedef struct _sdl3_joypad
{
   SDL_Joystick   *joypad;
   SDL_Gamepad    *gamepad;
   SDL_JoystickID  jid;       /* 0 = Invalid ID */
   unsigned        num_axes;
   unsigned        num_buttons;
   unsigned        num_hats;
} sdl3_joypad_t;

/* TODO/FIXME - static globals */
static sdl3_joypad_t sdl3_joypads[MAX_USERS];

static const char *sdl3_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS || !sdl3_joypads[pad].jid)
      return NULL;

   if (sdl3_joypads[pad].gamepad)
      return SDL_GetGamepadName(sdl3_joypads[pad].gamepad);
   else if (sdl3_joypads[pad].joypad)
      return SDL_GetJoystickName(sdl3_joypads[pad].joypad);
   return NULL;
}

static uint8_t sdl3_joypad_get_button(sdl3_joypad_t *pad, unsigned button)
{
   if (pad->gamepad)
      return (uint8_t)SDL_GetGamepadButton(pad->gamepad, (SDL_GamepadButton)button);
   else if (pad->joypad)
      return (uint8_t)SDL_GetJoystickButton(pad->joypad, button);
   return 0;
}

static uint8_t sdl3_joypad_get_hat(sdl3_joypad_t *pad, unsigned hat)
{
   /* Gamepads always have num_hats=0; this is only reached for raw joysticks. */
   return SDL_GetJoystickHat(pad->joypad, hat);
}

static int16_t sdl3_joypad_get_axis(sdl3_joypad_t *pad, unsigned axis)
{
   if (pad->gamepad)
      return SDL_GetGamepadAxis(pad->gamepad, (SDL_GamepadAxis)axis);
   else if (pad->joypad)
      return SDL_GetJoystickAxis(pad->joypad, (int)axis);
   return 0;
}

static void sdl3_joypad_connect(SDL_JoystickID jid)
{
   int i;
   int slot             = -1;
   int32_t vendor       = 0;
   int32_t product      = 0;
   sdl3_joypad_t *pad   = NULL;
   bool success         = false;

   /* Find a free slot */
   for (i = 0; i < MAX_USERS; i++)
   {
      if (!sdl3_joypads[i].jid)
      {
         slot = i;
         break;
      }
   }

   if (slot < 0)
   {
      RARCH_WARN("[SDL3] No free joypad slots for joystick %" SDL_PRIu32 ".\n", jid);
      return;
   }

   pad = &sdl3_joypads[slot];
   pad->jid = jid;

   if (SDL_IsGamepad(jid))
   {
      pad->gamepad = SDL_OpenGamepad(jid);
      if (pad->gamepad)
         pad->joypad = SDL_GetGamepadJoystick(pad->gamepad);
      success = pad->gamepad != NULL && pad->joypad != NULL;
   }
   else
   {
      pad->joypad = SDL_OpenJoystick(jid);
      success     = pad->joypad != NULL;
   }

   if (!success)
   {
      RARCH_ERR("[SDL3] Couldn't open joystick #%d: %s.\n", slot, SDL_GetError());

      if (pad->gamepad)
      {
         SDL_CloseGamepad(pad->gamepad);
         pad->gamepad = NULL;
      }
      else if (pad->joypad)
      {
         SDL_CloseJoystick(pad->joypad);
         pad->joypad = NULL;
      }

      pad->jid = 0;
      return;
   }

   if (pad->gamepad)
   {
      vendor  = SDL_GetGamepadVendor(pad->gamepad);
      product = SDL_GetGamepadProduct(pad->gamepad);
   }
   else
   {
      vendor  = SDL_GetJoystickVendor(pad->joypad);
      product = SDL_GetJoystickProduct(pad->joypad);
   }

   input_autoconfigure_connect(
         sdl3_joypad_name(slot),
         NULL, NULL,
         sdl_joypad.ident,
         slot,
         vendor,
         product);

   if (pad->gamepad)
   {
      /* SDL_Gamepad internally supports all axis/button IDs, even if
       * the controller's mapping does not have a binding for it.
       *
       * So, we can claim to support all axes/buttons, and when we try to poll
       * an unbound ID, SDL simply returns the correct unpressed value.
       *
       * Note that, in addition to 0 trackballs, we also have 0 hats. This is
       * because the d-pad is in the button list, as the last 4 enum entries.
       *
       * -flibit
       */
      pad->num_axes    = SDL_GAMEPAD_AXIS_COUNT;
      pad->num_buttons = SDL_GAMEPAD_BUTTON_COUNT;
      pad->num_hats    = 0;
   }
   else
   {
      pad->num_axes    = SDL_GetNumJoystickAxes(pad->joypad);
      pad->num_buttons = SDL_GetNumJoystickButtons(pad->joypad);
      pad->num_hats    = SDL_GetNumJoystickHats(pad->joypad);
   }
}

static void sdl3_joypad_disconnect(SDL_JoystickID jid)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (sdl3_joypads[i].jid != jid)
         continue;

      if (sdl3_joypads[i].gamepad)
         SDL_CloseGamepad(sdl3_joypads[i].gamepad);
      else if (sdl3_joypads[i].joypad)
         SDL_CloseJoystick(sdl3_joypads[i].joypad);

      input_autoconfigure_disconnect(i, sdl_joypad.ident);

      memset(&sdl3_joypads[i], 0, sizeof(sdl3_joypads[i]));
      return;
   }
}

static void sdl3_joypad_destroy(void)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (sdl3_joypads[i].gamepad)
         SDL_CloseGamepad(sdl3_joypads[i].gamepad);
      else if (sdl3_joypads[i].joypad)
         SDL_CloseJoystick(sdl3_joypads[i].joypad);
   }

   memset(sdl3_joypads, 0, sizeof(sdl3_joypads));
   SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

static void *sdl3_joypad_init(void *data)
{
   int i, count                 = 0;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   if (sdl_subsystem_flags == 0)
   {
      if (!SDL_Init(SDL_INIT_GAMEPAD))
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_GAMEPAD) == 0)
   {
      if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
         return NULL;
   }

   memset(sdl3_joypads, 0, sizeof(sdl3_joypads));

   {
      SDL_JoystickID *joysticks = SDL_GetJoysticks(&count);

      if (joysticks)
      {
         int n = count;
         if (n > MAX_USERS)
            n = MAX_USERS;

         for (i = 0; i < n; i++)
            sdl3_joypad_connect(joysticks[i]);

         SDL_free(joysticks);
      }
   }

   return (void*)-1;
}

static int32_t sdl3_joypad_button_state(
      sdl3_joypad_t *pad,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      uint8_t  dir;
      uint16_t hat = GET_HAT(joykey);

      if (hat >= pad->num_hats)
         return 0;

      dir = sdl3_joypad_get_hat(pad, hat);

      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (dir & SDL_HAT_UP);
         case HAT_DOWN_MASK:
            return (dir & SDL_HAT_DOWN);
         case HAT_LEFT_MASK:
            return (dir & SDL_HAT_LEFT);
         case HAT_RIGHT_MASK:
            return (dir & SDL_HAT_RIGHT);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < pad->num_buttons)
      return sdl3_joypad_get_button(pad, joykey);

   return 0;
}

static int32_t sdl3_joypad_button(unsigned port, uint16_t joykey)
{
   sdl3_joypad_t *pad;

   if (port >= MAX_USERS)
      return 0;

   pad = &sdl3_joypads[port];
   if (!pad->joypad)
      return 0;

   return sdl3_joypad_button_state(pad, port, joykey);
}

static int16_t sdl3_joypad_axis_state(
      sdl3_joypad_t *pad,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < pad->num_axes)
   {
      int16_t val = sdl3_joypad_get_axis(pad, AXIS_NEG_GET(joyaxis));
      if (val < 0)
      {
         /* Clamp - -0x8000 can cause trouble if we later abs() it. */
         if (val < -0x7fff)
            return -0x7fff;
         return val;
      }
   }
   else if (AXIS_POS_GET(joyaxis) < pad->num_axes)
   {
      int16_t val = sdl3_joypad_get_axis(pad, AXIS_POS_GET(joyaxis));
      if (val > 0)
         return val;
   }

   return 0;
}

static int16_t sdl3_joypad_axis(unsigned port, uint32_t joyaxis)
{
   sdl3_joypad_t *pad;

   if (port >= MAX_USERS)
      return 0;

   pad = &sdl3_joypads[port];
   if (!pad->joypad)
      return 0;

   return sdl3_joypad_axis_state(pad, port, joyaxis);
}

static int16_t sdl3_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   int16_t ret            = 0;
   uint16_t port_idx      = joypad_info->joy_idx;
   sdl3_joypad_t *pad;

   if (port_idx >= MAX_USERS)
      return 0;

   pad = &sdl3_joypads[port_idx];
   if (!pad->joypad)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;

      if (
               (uint16_t)joykey != NO_BTN
            && sdl3_joypad_button_state(pad, port_idx, (uint16_t)joykey)
         )
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(sdl3_joypad_axis_state(pad, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void sdl3_joypad_poll(void)
{
   SDL_Event event;

   SDL_PumpEvents();

   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT,
            SDL_EVENT_JOYSTICK_ADDED, SDL_EVENT_JOYSTICK_REMOVED) > 0)
   {
      switch (event.type)
      {
         case SDL_EVENT_JOYSTICK_ADDED:
            sdl3_joypad_connect(event.jdevice.which);
            break;
         case SDL_EVENT_JOYSTICK_REMOVED:
            sdl3_joypad_disconnect(event.jdevice.which);
            break;
      }
   }

   SDL_UpdateGamepads();
   /* Discard all remaining joystick/gamepad input events (axis, button, hat,
    * etc.) - we sample state directly via SDL_GetGamepadAxis / SDL_GetJoystickButton
    * and don't need the event copies piling up in the queue. */
   SDL_FlushEvents(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_GAMEPAD_REMAPPED);
}

static bool sdl3_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   sdl3_joypad_t *joypad;
   uint16_t low  = 0;
   uint16_t high = 0;

   if (pad >= MAX_USERS)
      return false;

   joypad = (sdl3_joypad_t*)&sdl3_joypads[pad];

   if (!joypad->joypad)
      return false;

   switch (effect)
   {
      case RETRO_RUMBLE_STRONG:
         low  = strength;
         break;
      case RETRO_RUMBLE_WEAK:
         high = strength;
         break;
      default:
         return false;
   }

   if (joypad->gamepad)
      return SDL_RumbleGamepad(joypad->gamepad, low, high, 5000);
   return SDL_RumbleJoystick(joypad->joypad, low, high, 5000);
}

/**
 * Enables or disables a sensor on the specified gamepad.
 *
 * @param pad    Index of the gamepad.
 * @param action Sensor action to perform (enable/disable gyroscope or accelerometer).
 * @param rate   Requested sensor update rate (unused).
 * @return       true if the sensor state was set successfully, false otherwise.
 */
static bool sdl3_joypad_set_sensor_state(unsigned pad,
   enum retro_sensor_action action, unsigned rate)
{
   sdl3_joypad_t *joypad;

   if (pad >= MAX_USERS)
      return false;

   joypad = (sdl3_joypad_t*)&sdl3_joypads[pad];

   if (!joypad->gamepad)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         if (SDL_GamepadHasSensor(joypad->gamepad, SDL_SENSOR_GYRO))
            return SDL_SetGamepadSensorEnabled(joypad->gamepad, SDL_SENSOR_GYRO,
                  action == RETRO_SENSOR_GYROSCOPE_ENABLE);
         return false;

      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         if (SDL_GamepadHasSensor(joypad->gamepad, SDL_SENSOR_ACCEL))
            return SDL_SetGamepadSensorEnabled(joypad->gamepad, SDL_SENSOR_ACCEL,
                  action == RETRO_SENSOR_ACCELEROMETER_ENABLE);
         return false;

      default:
         return false;
   }
}

/**
 * Retrieves input data from a connected sensor device, such as a gyroscope or accelerometer.
 *
 * @return True if the sensor input was successfully handled by this function, false otherwise.
 */
static bool sdl3_joypad_get_sensor_input(unsigned pad, unsigned id, float *value)
{
   sdl3_joypad_t *joypad;
   SDL_SensorType sensor_type;
   float sensor_data[3];

   if (pad >= MAX_USERS)
      return false;

   joypad = (sdl3_joypad_t*)&sdl3_joypads[pad];

   if (!joypad->gamepad)
      return false;

   if ((id >= RETRO_SENSOR_ACCELEROMETER_X) && (id <= RETRO_SENSOR_ACCELEROMETER_Z))
      sensor_type = SDL_SENSOR_ACCEL;
   else if ((id >= RETRO_SENSOR_GYROSCOPE_X) && (id <= RETRO_SENSOR_GYROSCOPE_Z))
      sensor_type = SDL_SENSOR_GYRO;
   else
      return false;

   if (!SDL_GetGamepadSensorData(joypad->gamepad, sensor_type, sensor_data, 3))
      return false;

   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
         *value = sensor_data[0] / SDL_STANDARD_GRAVITY;
         break;
      case RETRO_SENSOR_ACCELEROMETER_Y:
         *value = sensor_data[2] / SDL_STANDARD_GRAVITY;
         break;
      case RETRO_SENSOR_ACCELEROMETER_Z:
         *value = sensor_data[1] / SDL_STANDARD_GRAVITY;
         break;
      case RETRO_SENSOR_GYROSCOPE_X:
         *value = sensor_data[0];
         break;
      case RETRO_SENSOR_GYROSCOPE_Y:
         *value = -sensor_data[2];
         break;
      case RETRO_SENSOR_GYROSCOPE_Z:
         *value = sensor_data[1];
         break;
   }

   return true;
}

static bool sdl3_joypad_query_pad(unsigned pad)
{
   if (pad >= MAX_USERS || !sdl3_joypads[pad].joypad)
      return false;
   if (sdl3_joypads[pad].gamepad)
      return SDL_GamepadConnected(sdl3_joypads[pad].gamepad);
   return SDL_JoystickConnected(sdl3_joypads[pad].joypad);
}

input_device_driver_t sdl_joypad = {
   sdl3_joypad_init,
   sdl3_joypad_query_pad,
   sdl3_joypad_destroy,
   sdl3_joypad_button,
   sdl3_joypad_state,
   NULL, /* get_buttons */
   sdl3_joypad_axis,
   sdl3_joypad_poll,
   sdl3_joypad_set_rumble,
   NULL, /* set_rumble_gain */
   sdl3_joypad_set_sensor_state,
   sdl3_joypad_get_sensor_input,
   sdl3_joypad_name,
   "sdl3",
};
