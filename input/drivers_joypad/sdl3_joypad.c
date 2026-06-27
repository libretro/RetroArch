/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Higor Euripedes
 *  Copyright (C)      2023 - Carlo Refice
 *  Copyright (C)      2026 - Rob Loach
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
#include <string.h>

#include <SDL3/SDL.h>

#include <streams/file_stream.h>
#include <file/file_path.h>

#include "../input_driver.h"
#include "../../configuration.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

typedef struct _sdl3_joypad
{
   SDL_Joystick   *joypad;
   SDL_Gamepad    *gamepad;
   SDL_JoystickID  jid; /* 0 = Invalid ID */
   unsigned        num_axes;
   unsigned        num_buttons;
   unsigned        num_hats;
   uint16_t        rumble_gain; /* 0-100 */
   uint16_t        rumble[2];   /* raw magnitude per retro_rumble_effect (strong/weak) */
} sdl3_joypad_t;

/**
 * The static global for the active joypads.
 *
 * @todo Move away from static globals.
 */
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
      return (uint8_t)SDL_GetJoystickButton(pad->joypad, (int)button);
   return 0;
}

static uint8_t sdl3_joypad_get_hat(sdl3_joypad_t *pad, unsigned hat)
{
   /* Gamepads don't have hats, so we can pass this in directly for the Joystick. */
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

static bool sdl3_joypad_set_rumble_gain(unsigned pad, unsigned gain)
{
   if (pad >= MAX_USERS)
      return false;

   sdl3_joypads[pad].rumble_gain = (gain > 100) ? 100 : gain;
   return true;
}

static void sdl3_joypad_connect(SDL_JoystickID jid)
{
   int i;
   int slot              = -1;
   int32_t vendor        = 0;
   int32_t product       = 0;
   sdl3_joypad_t *pad    = NULL;
   SDL_Gamepad  *gamepad = NULL;
   SDL_Joystick *joypad  = NULL;

   /* Protect against connecting an already connected device. */
   for (i = 0; i < MAX_USERS; i++)
      if (sdl3_joypads[i].jid == jid)
         return;

   /* Connect to the device. */
   if (SDL_IsGamepad(jid))
   {
      gamepad = SDL_OpenGamepad(jid);
      if (gamepad)
         joypad = SDL_GetGamepadJoystick(gamepad);

      if (!gamepad || !joypad)
      {
         RARCH_ERR("[SDL3] Couldn't open gamepad %" SDL_PRIu32 ": %s.\n", jid, SDL_GetError());
         if (gamepad)
            SDL_CloseGamepad(gamepad);
         return;
      }
   }
   else
   {
      joypad = SDL_OpenJoystick(jid);
      if (!joypad)
      {
         RARCH_ERR("[SDL3] Couldn't open joystick %" SDL_PRIu32 ": %s.\n", jid, SDL_GetError());
         return;
      }
   }

   /* Gamepads allow restoring the player index, so re-use that for the slot if possible. */
   if (gamepad)
   {
      int player = SDL_GetGamepadPlayerIndex(gamepad);
      if (player >= 0 && player < MAX_USERS && !sdl3_joypads[player].jid)
         slot = player;
   }

   /* Fallback to the first free slot. */
   if (slot < 0)
   {
      for (i = 0; i < MAX_USERS; i++)
      {
         if (!sdl3_joypads[i].jid)
         {
            slot = i;
            break;
         }
      }
   }

   /* Fail if a slot is still not found. */
   if (slot < 0)
   {
      RARCH_WARN("[SDL3] No free joypad slots for joystick %" SDL_PRIu32 ".\n", jid);
      if (gamepad)
         SDL_CloseGamepad(gamepad);
      else
         SDL_CloseJoystick(joypad);
      return;
   }

   pad          = &sdl3_joypads[slot];
   pad->jid     = jid;
   pad->gamepad = gamepad;
   pad->joypad  = joypad;

   /* Seed the rumble gain from the saved setting so it applies on connect. */
   {
      settings_t *settings = config_get_ptr();
      if (settings)
         sdl3_joypad_set_rumble_gain((unsigned int)slot, settings->uints.input_rumble_gain);
   }

   if (gamepad)
   {
      vendor  = SDL_GetGamepadVendor(gamepad);
      product = SDL_GetGamepadProduct(gamepad);

      /* Ensure the player index matches the slot. */
      if (SDL_GetGamepadPlayerIndex(gamepad) != slot)
         SDL_SetGamepadPlayerIndex(gamepad, slot);

      /* Set the LED to match the player number. */
      switch (slot) {
         case 0: SDL_SetGamepadLED(gamepad, 255, 0, 0); break;
         case 1: SDL_SetGamepadLED(gamepad, 0, 0, 255); break;
         case 2: SDL_SetGamepadLED(gamepad, 0, 255, 0); break;
         case 3: SDL_SetGamepadLED(gamepad, 255, 255, 0); break;
         case 4: SDL_SetGamepadLED(gamepad, 255, 0, 255); break;
         case 5: SDL_SetGamepadLED(gamepad, 0, 255, 255); break;
         case 6: SDL_SetGamepadLED(gamepad, 255, 128, 0); break;
         case 7: SDL_SetGamepadLED(gamepad, 255, 255, 255); break;
         case 8: SDL_SetGamepadLED(gamepad, 128, 0, 255); break;
         case 9: SDL_SetGamepadLED(gamepad, 0, 128, 255); break;
         case 10: SDL_SetGamepadLED(gamepad, 128, 255, 0); break;
         case 11: SDL_SetGamepadLED(gamepad, 255, 0, 128); break;
         case 12: SDL_SetGamepadLED(gamepad, 128, 0, 0); break;
         case 13: SDL_SetGamepadLED(gamepad, 0, 128, 0); break;
         case 14: SDL_SetGamepadLED(gamepad, 0, 0, 128); break;
         case 15: SDL_SetGamepadLED(gamepad, 128, 128, 128); break;
         default: SDL_SetGamepadLED(gamepad, 0, 0, 0); break;
      }
   }
   else
   {
      vendor = SDL_GetJoystickVendor(joypad);
      product = SDL_GetJoystickProduct(joypad);
   }

   input_autoconfigure_connect_ex(
         sdl3_joypad_name(slot),
         NULL,
         SDL_GetJoystickPath(joypad),
         sdl_joypad.ident,
         slot,
         vendor,
         product,
         gamepad ? AUTOCONF_FLAG_HAS_STANDARD_MAPPING : 0); /* An open SDL_Gamepad always has a normalized mapping */

   if (gamepad)
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
      /* These can return -1 on error, so protect accordingly. */
      int num_axes     = SDL_GetNumJoystickAxes(joypad);
      int num_buttons  = SDL_GetNumJoystickButtons(joypad);
      int num_hats     = SDL_GetNumJoystickHats(joypad);
      pad->num_axes    = (num_axes    > 0) ? (unsigned)num_axes    : 0;
      pad->num_buttons = (num_buttons > 0) ? (unsigned)num_buttons : 0;
      pad->num_hats    = (num_hats    > 0) ? (unsigned)num_hats    : 0;
   }
}

static void sdl3_joypad_disconnect(SDL_JoystickID jid)
{
   for (int i = 0; i < MAX_USERS; i++)
   {
      if (sdl3_joypads[i].jid != jid)
         continue;

      if (sdl3_joypads[i].gamepad) {
         SDL_SetGamepadPlayerIndex(sdl3_joypads[i].gamepad, -1);
         SDL_CloseGamepad(sdl3_joypads[i].gamepad);
      }
      else if (sdl3_joypads[i].joypad)
         SDL_CloseJoystick(sdl3_joypads[i].joypad);

      input_autoconfigure_disconnect(i, sdl_joypad.ident);

      memset(&sdl3_joypads[i], 0, sizeof(sdl3_joypads[i]));
      return;
   }
}

static void sdl3_joypad_destroy(void)
{

   for (int i = 0; i < MAX_USERS; i++)
   {
      if (sdl3_joypads[i].gamepad)
         SDL_CloseGamepad(sdl3_joypads[i].gamepad);
      else if (sdl3_joypads[i].joypad)
         SDL_CloseJoystick(sdl3_joypads[i].joypad);
   }

   memset(sdl3_joypads, 0, sizeof(sdl3_joypads));
   SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

/**
 * Attempts to load SDL_GameControllerDB from the autoconfig directory.
 *
 * @return The number of loaded configs.
 * @see https://github.com/mdqinc/SDL_GameControllerDB
 */
static int sdl3_joypad_load_gamecontrollerdb(void)
{
   settings_t *settings = config_get_ptr();
   char path[PATH_MAX_LENGTH];
   void *buf = NULL;
   int64_t len = 0;
   int num_mappings = 0;
   SDL_IOStream *io;

   if (     settings == NULL
         || !settings->bools.input_autodetect_enable
         || settings->paths.directory_autoconfig[0] == '\0')
      return 0;

   fill_pathname_join_special(path, settings->paths.directory_autoconfig, "sdl3/gamecontrollerdb.cfg", sizeof(path));
   if (filestream_read_file(path, &buf, &len) == 0 || len == 0)
   {
      RARCH_WARN("[SDL3] Failed to load gamepad mappings from \"%s\".\n", path);
      return 0;
   }

   io = SDL_IOFromConstMem(buf, (size_t)len);
   num_mappings = SDL_AddGamepadMappingsFromIO(io, true);
   if (num_mappings >= 0)
      RARCH_LOG("[SDL3] Loaded %d gamepad mappings from \"%s\".\n", num_mappings, path);
   else
      RARCH_WARN("[SDL3] Failed to load gamepad mappings from \"%s\": %s.\n", path, SDL_GetError());
   free(buf);
   return num_mappings;
}

/**
 * Initializes the SDL3 Joypad system, and loads the GameController DB.
 */
static void *sdl3_joypad_init(void *data)
{
#if SDL_VERSION_ATLEAST(3, 2, 0)
   /* Gamepad driver hints. */
   SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_WII, "1");
#endif

   SDL_InitFlags sdl_subsystem_flags = SDL_WasInit(0);

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

   /* Set the initial state of the joypad system. */
   memset(sdl3_joypads, 0, sizeof(sdl3_joypads));
   sdl3_joypad_load_gamecontrollerdb();

   /* Ensure we know about any already connected devices. */
   {
      int i;
      int num_joysticks = 0;
      SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
      if (joysticks)
      {
         for (i = 0; i < num_joysticks; i++)
            sdl3_joypad_connect(joysticks[i]);
         SDL_free(joysticks);
      }
   }

   return (void*)-1;
}

static int32_t sdl3_joypad_button_state(sdl3_joypad_t *pad, uint16_t joykey)
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
   if (port >= MAX_USERS)
      return 0;

   if (!sdl3_joypads[port].joypad)
      return 0;

   return sdl3_joypad_button_state(&sdl3_joypads[port], joykey);
}

static int16_t sdl3_joypad_axis_state(sdl3_joypad_t *pad, uint32_t joyaxis)
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
   if (port >= MAX_USERS)
      return 0;

   if (!sdl3_joypads[port].joypad)
      return 0;

   return sdl3_joypad_axis_state(&sdl3_joypads[port], joyaxis);
}

static int16_t sdl3_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   int16_t ret = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   if (port_idx >= MAX_USERS)
      return 0;

   if (!sdl3_joypads[port_idx].joypad)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;

      if (
               (uint16_t)joykey != NO_BTN
            && sdl3_joypad_button_state(&sdl3_joypads[port_idx], (uint16_t)joykey)
         )
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(sdl3_joypad_axis_state(&sdl3_joypads[port_idx], joyaxis))
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

   /* Flush all remaining gamepad/joystick input events, since we handle it directly. */
   SDL_FlushEvents(SDL_EVENT_JOYSTICK_AXIS_MOTION, SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED);
}

static bool sdl3_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   uint16_t low, high;

   if (pad >= MAX_USERS)
      return false;

   switch (effect)
   {
      case RETRO_RUMBLE_STRONG:
      case RETRO_RUMBLE_WEAK:
         break;
      default:
         return false;
   }

   /* Send both effects, while not clobbering the other effect. */
   sdl3_joypads[pad].rumble[effect] = strength;
   low  = (uint16_t)((sdl3_joypads[pad].rumble[RETRO_RUMBLE_STRONG] * sdl3_joypads[pad].rumble_gain) / 100);
   high = (uint16_t)((sdl3_joypads[pad].rumble[RETRO_RUMBLE_WEAK] * sdl3_joypads[pad].rumble_gain) / 100);

   /* The frontend re-issues rumble every frame, so just use an arbitrary duration. */
   if (sdl3_joypads[pad].gamepad)
      return SDL_RumbleGamepad(sdl3_joypads[pad].gamepad, low, high, 5000);
   else if (sdl3_joypads[pad].joypad)
      return SDL_RumbleJoystick(sdl3_joypads[pad].joypad, low, high, 5000);

   return false;
}

/**
 * Enables or disables a sensor on the specified gamepad.
 *
 * @param pad Index of the gamepad.
 * @param action Sensor action to perform (enable/disable gyroscope or accelerometer).
 * @param rate Requested sensor update rate (unused).
 * @return true if the sensor state was set successfully, false otherwise.
 */
static bool sdl3_joypad_set_sensor_state(unsigned pad,
   enum retro_sensor_action action, unsigned rate)
{
   if (pad >= MAX_USERS)
      return false;

   if (!sdl3_joypads[pad].gamepad)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         if (SDL_GamepadHasSensor(sdl3_joypads[pad].gamepad, SDL_SENSOR_GYRO))
            return SDL_SetGamepadSensorEnabled(sdl3_joypads[pad].gamepad, SDL_SENSOR_GYRO,
                  action == RETRO_SENSOR_GYROSCOPE_ENABLE);
         return false;

      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         if (SDL_GamepadHasSensor(sdl3_joypads[pad].gamepad, SDL_SENSOR_ACCEL))
            return SDL_SetGamepadSensorEnabled(sdl3_joypads[pad].gamepad, SDL_SENSOR_ACCEL,
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
   SDL_SensorType sensor_type;
   float sensor_data[3];

   if (pad >= MAX_USERS)
      return false;

   if (!sdl3_joypads[pad].gamepad)
      return false;

   if ((id >= RETRO_SENSOR_ACCELEROMETER_X) && (id <= RETRO_SENSOR_ACCELEROMETER_Z))
      sensor_type = SDL_SENSOR_ACCEL;
   else if ((id >= RETRO_SENSOR_GYROSCOPE_X) && (id <= RETRO_SENSOR_GYROSCOPE_Z))
      sensor_type = SDL_SENSOR_GYRO;
   else
      return false;

   if (!SDL_GetGamepadSensorData(sdl3_joypads[pad].gamepad, sensor_type, sensor_data, 3))
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

/**
 * Queries whether or not the joypad is still connected.
 *
 * We check against the NULL state of the joypad, since that is managed
 * by the event poll. SDL_GamepadConnected() or SDL_JoystickConnected()
 * would be redundant.
 */
static bool sdl3_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && sdl3_joypads[pad].joypad != NULL;
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
   sdl3_joypad_set_rumble_gain,
   sdl3_joypad_set_sensor_state,
   sdl3_joypad_get_sensor_input,
   sdl3_joypad_name,
   "sdl3",
};
