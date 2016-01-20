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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <string/stdstring.h>

#include "input_keymaps.h"
#include "../general.h"
#include "../string_list_special.h"
#include "../verbosity.h"

static input_device_driver_t *joypad_drivers[] = {
#ifdef __CELLOS_LV2__
   &ps3_joypad,
#endif
#ifdef HAVE_XINPUT
   &xinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef _XBOX
   &xdk_joypad,
#endif
#if defined(PSP) || defined(VITA)
   &psp_joypad,
#endif
#ifdef _3DS
   &ctr_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#ifdef HAVE_UDEV
   &udev_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_PARPORT
   &parport_joypad,
#endif
#ifdef ANDROID
   &android_joypad,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &sdl_joypad,
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
#ifdef HAVE_MFI
   &mfi_joypad,
#endif
   &hid_joypad,
   &null_joypad,
   NULL,
};

/**
 * joypad_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to joypad driver at index. Can be NULL
 * if nothing found.
 **/
const void *joypad_driver_find_handle(int idx)
{
   const void *drv = joypad_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * joypad_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of joypad driver at index. Can be NULL
 * if nothing found.
 **/
const char *joypad_driver_find_ident(int idx)
{
   const input_device_driver_t *drv = joypad_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_JOYPAD_DRIVERS, NULL);
}

/**
 * input_joypad_init_driver:
 * @ident                           : identifier of driver to initialize.
 *
 * Initialize a joypad driver of name @ident.
 *
 * If ident points to NULL or a zero-length string, 
 * equivalent to calling input_joypad_init_first().
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_driver(const char *ident, void *data)
{
   unsigned i;
   if (!ident || !*ident)
      return input_joypad_init_first(data);

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (string_is_equal(ident, joypad_drivers[i]->ident)
            && joypad_drivers[i]->init(data))
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return input_joypad_init_first(data);
}

/**
 * input_joypad_init_first:
 *
 * Finds first suitable joypad driver and initializes.
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_first(void *data)
{
   unsigned i;

   for (i = 0; joypad_drivers[i]; i++)
   {
      if (joypad_drivers[i]->init(data))
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n",
               joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

/**
 * input_joypad_name:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 *
 * Gets name of the joystick (@port).
 *
 * Returns: name of joystick #port.
 **/
const char *input_joypad_name(const input_device_driver_t *drv,
      unsigned port)
{
   if (!drv)
      return NULL;
   return drv->name(port);
}

/**
 * input_joypad_set_rumble:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @effect                  : Rumble effect to set.
 * @strength                : Strength of rumble effect.
 *
 * Sets rumble effect @effect with strength @strength.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_joypad_set_rumble(const input_device_driver_t *drv,
      unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = settings->input.joypad_map[port];
   
   if (!drv || !drv->set_rumble)
      return false;

   if (joy_idx >= MAX_USERS)
      return false;

   return drv->set_rumble(joy_idx, effect, strength);
}

/**
 * input_joypad_is_pressed:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @binds                   : Binds of user.
 * @key                     : Identifier of key.
 *
 * Checks if key (@key) was being pressed by user
 * with number @port with provided keybinds (@binds).
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_joypad_pressed(
      const input_device_driver_t *drv,
      unsigned port,
      const void *binds_data,
      unsigned key)
{
   float scaled_axis;
   int16_t  axis;
   uint32_t joyaxis;
   uint64_t joykey;
   const struct retro_keybind *auto_binds = NULL;
   const struct retro_keybind *binds = (const struct retro_keybind*)
      binds_data;
   settings_t *settings = config_get_ptr();
   unsigned joy_idx = settings->input.joypad_map[port];

   if (joy_idx >= MAX_USERS)
      return false;
   if (!drv || !binds[key].valid)
      return false;

   /* Auto-binds are per joypad, not per user. */
   auto_binds = settings->input.autoconf_binds[joy_idx];

   joykey = binds[key].joykey;
   if (joykey == NO_BTN)
      joykey = auto_binds[key].joykey;

   if (drv->button(joy_idx, (uint16_t)joykey))
      return true;

   joyaxis = binds[key].joyaxis;
   if (joyaxis == AXIS_NONE)
      joyaxis = auto_binds[key].joyaxis;

   axis        = drv->axis(joy_idx, joyaxis);
   scaled_axis = (float)abs(axis) / 0x8000;
   return scaled_axis > settings->input.axis_threshold;
}

/**
 * input_joypad_analog:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.: 
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
int16_t input_joypad_analog(const input_device_driver_t *drv,
      unsigned port, unsigned idx, unsigned ident,
      const void *binds_data)
{
   uint32_t axis_minus, axis_plus;
   uint64_t key_minus, key_plus;
   int16_t  pressed_minus, pressed_plus, res;
   unsigned ident_minus = 0, ident_plus = 0;
   int16_t digital_left = 0, digital_right = 0;
   const struct retro_keybind *auto_binds = NULL;
   const struct retro_keybind *bind_minus = NULL;
   const struct retro_keybind *bind_plus  = NULL;
   const struct retro_keybind *binds = (const struct retro_keybind*)
      binds_data;
   settings_t *settings = config_get_ptr();
   unsigned joy_idx = settings->input.joypad_map[port];

   if (!drv)
      return 0;

   if (joy_idx >= MAX_USERS)
      return 0;

   /* Auto-binds are per joypad, not per user. */
   auto_binds = settings->input.autoconf_binds[joy_idx];

   input_conv_analog_id_to_bind_id(idx, ident, &ident_minus, &ident_plus);

   bind_minus = &binds[ident_minus];
   bind_plus  = &binds[ident_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   axis_minus = bind_minus->joyaxis;
   axis_plus  = bind_plus->joyaxis;
   if (axis_minus == AXIS_NONE)
      axis_minus = auto_binds[ident_minus].joyaxis;
   if (axis_plus == AXIS_NONE)
      axis_plus = auto_binds[ident_plus].joyaxis;

   pressed_minus = abs(drv->axis(joy_idx, axis_minus));
   pressed_plus  = abs(drv->axis(joy_idx, axis_plus));
   res           = pressed_plus - pressed_minus;

   if (res != 0)
      return res;

   key_minus = bind_minus->joykey;
   key_plus  = bind_plus->joykey;
   if (key_minus == NO_BTN)
      key_minus = auto_binds[ident_minus].joykey;
   if (key_plus == NO_BTN)
      key_plus = auto_binds[ident_plus].joykey;

   if (drv->button(joy_idx, (uint16_t)key_minus))
      digital_left  = -0x7fff;
   if (drv->button(joy_idx, (uint16_t)key_plus))
      digital_right = 0x7fff;
   return digital_right + digital_left;
}

/**
 * input_joypad_axis_raw:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @axis                    : Identifier of axis.
 *
 * Checks if axis (@axis) was being pressed by user   
 * with joystick number @port.
 *
 * Returns: true (1) if axis was pressed, otherwise
 * false (0).
 **/
int16_t input_joypad_axis_raw(const input_device_driver_t *drv,
      unsigned port, unsigned axis)
{
   if (!drv)
      return 0;
   return drv->axis(port, AXIS_POS(axis)) +
      drv->axis(port, AXIS_NEG(axis));
}

/**
 * input_joypad_button_raw:
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @button                  : Identifier of key.
 *
 * Checks if key (@button) was being pressed by user
 * with joystick number @port.
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_joypad_button_raw(const input_device_driver_t *drv,
      unsigned port, unsigned button)
{
   if (!drv)
      return false;
   return drv->button(port, button);
}

bool input_joypad_hat_raw(const input_device_driver_t *drv,
      unsigned port, unsigned hat_dir, unsigned hat)
{
   if (!drv)
      return false;
   return drv->button(port, HAT_MAP(hat, hat_dir));
}

/**
 * input_conv_analog_id_to_bind_id:
 * @idx                     : Analog key index.
 *                            E.g.: 
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @ident_minus             : Bind ID minus, will be set by function.
 * @ident_plus              : Bind ID plus,  will be set by function.
 *
 * Takes as input analog key identifiers and converts
 * them to corresponding bind IDs @ident_minus and @ident_plus.
 **/
void input_conv_analog_id_to_bind_id(unsigned idx, unsigned ident,
      unsigned *ident_minus, unsigned *ident_plus)
{
   switch ((idx << 1) | ident)
   {
      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *ident_minus = RARCH_ANALOG_LEFT_X_MINUS;
         *ident_plus  = RARCH_ANALOG_LEFT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *ident_minus = RARCH_ANALOG_LEFT_Y_MINUS;
         *ident_plus  = RARCH_ANALOG_LEFT_Y_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X:
         *ident_minus = RARCH_ANALOG_RIGHT_X_MINUS;
         *ident_plus  = RARCH_ANALOG_RIGHT_X_PLUS;
         break;

      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y:
         *ident_minus = RARCH_ANALOG_RIGHT_Y_MINUS;
         *ident_plus  = RARCH_ANALOG_RIGHT_Y_PLUS;
         break;
   }
}
