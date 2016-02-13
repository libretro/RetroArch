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

#ifndef __INPUT_DRIVER__H
#define __INPUT_DRIVER__H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <boolean.h>

#include "input_joypad_driver.h"

#ifdef HAVE_OVERLAY
#include "input_overlay.h"
#endif

#ifndef MAX_USERS
#define MAX_USERS 16
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t retro_input_t;

enum input_device_type
{
   INPUT_DEVICE_TYPE_NONE = 0,
   INPUT_DEVICE_TYPE_KEYBOARD,
   INPUT_DEVICE_TYPE_JOYPAD
};

enum rarch_input_ctl_state
{
   RARCH_INPUT_CTL_NONE = 0,
   RARCH_INPUT_CTL_INIT,
   RARCH_INPUT_CTL_DEINIT,
   RARCH_INPUT_CTL_DESTROY,
   RARCH_INPUT_CTL_DESTROY_DATA,
   RARCH_INPUT_CTL_HAS_CAPABILITIES,
   RARCH_INPUT_CTL_POLL,
   RARCH_INPUT_CTL_FIND_DRIVER,
   RARCH_INPUT_CTL_GRAB_STDIN,
   RARCH_INPUT_CTL_KB_MAPPING_IS_BLOCKED,
   RARCH_INPUT_CTL_SET_FLUSHING_INPUT,
   RARCH_INPUT_CTL_UNSET_FLUSHING_INPUT,
   RARCH_INPUT_CTL_IS_FLUSHING_INPUT,
   RARCH_INPUT_CTL_SET_HOTKEY_BLOCK,
   RARCH_INPUT_CTL_UNSET_HOTKEY_BLOCK,
   RARCH_INPUT_CTL_IS_HOTKEY_BLOCKED,
   RARCH_INPUT_CTL_SET_LIBRETRO_INPUT_BLOCKED,
   RARCH_INPUT_CTL_UNSET_LIBRETRO_INPUT_BLOCKED,
   RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED,
   RARCH_INPUT_CTL_SET_NONBLOCK_STATE,
   RARCH_INPUT_CTL_UNSET_NONBLOCK_STATE,
   RARCH_INPUT_CTL_IS_NONBLOCK_STATE,
   RARCH_INPUT_CTL_SET_OWN_DRIVER,
   RARCH_INPUT_CTL_UNSET_OWN_DRIVER,
   RARCH_INPUT_CTL_OWNS_DRIVER,
   RARCH_INPUT_CTL_SET_OSK_ENABLED,
   RARCH_INPUT_CTL_UNSET_OSK_ENABLED,
   RARCH_INPUT_CTL_IS_OSK_ENABLED,
   RARCH_INPUT_CTL_SET_KEYBOARD_LINEFEED_ENABLED,
   RARCH_INPUT_CTL_UNSET_KEYBOARD_LINEFEED_ENABLED,
   RARCH_INPUT_CTL_IS_KEYBOARD_LINEFEED_ENABLED,
   RARCH_INPUT_CTL_COMMAND_INIT,
   RARCH_INPUT_CTL_COMMAND_DEINIT,
   RARCH_INPUT_CTL_REMOTE_INIT,
   RARCH_INPUT_CTL_REMOTE_DEINIT,
   RARCH_INPUT_CTL_KEY_PRESSED,
   RARCH_INPUT_CTL_GRAB_MOUSE,
   RARCH_INPUT_CTL_IS_DATA_PTR_SAME
};

struct retro_keybind
{
   bool valid;
   unsigned id;
   const char *desc;
   enum retro_key key;

   uint64_t joykey;
   /* Default key binding value - for resetting bind to default */
   uint64_t def_joykey;

   uint32_t joyaxis;
   uint32_t def_joyaxis;

   /* Used by input_{push,pop}_analog_dpad(). */
   uint32_t orig_joyaxis;

   char     joykey_label[256];
   char     joyaxis_label[256];
};

typedef struct input_driver
{
   void *(*init)(void);
   void (*poll)(void *data);
   int16_t (*input_state)(void *data,
         const struct retro_keybind **retro_keybinds,
         unsigned port, unsigned device, unsigned index, unsigned id);
   bool (*key_pressed)(void *data, int key);
   bool (*meta_key_pressed)(void *data, int key);
   void (*free)(void *data);
   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);
   uint64_t (*get_capabilities)(void *data);
   const char *ident;

   void (*grab_mouse)(void *data, bool state);
   bool (*grab_stdin)(void *data);
   bool (*set_rumble)(void *data, unsigned port,
         enum retro_rumble_effect effect, uint16_t state);
   const input_device_driver_t *(*get_joypad_driver)(void *data);
   const input_device_driver_t *(*get_sec_joypad_driver)(void *data);
   bool (*keyboard_mapping_is_blocked)(void *data);
   void (*keyboard_mapping_set_block)(void *data, bool value);
} input_driver_t;

extern input_driver_t input_android;
extern input_driver_t input_sdl;
extern input_driver_t input_dinput;
extern input_driver_t input_x;
extern input_driver_t input_wayland;
extern input_driver_t input_ps3;
extern input_driver_t input_psp;
extern input_driver_t input_ctr;
extern input_driver_t input_xenon360;
extern input_driver_t input_gx;
extern input_driver_t input_xinput;
extern input_driver_t input_linuxraw;
extern input_driver_t input_udev;
extern input_driver_t input_cocoa;
extern input_driver_t input_qnx;
extern input_driver_t input_rwebinput;
extern input_driver_t input_null;

/**
 * input_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to input driver at index. Can be NULL
 * if nothing found.
 **/
const void *input_driver_find_handle(int index);

/**
 * input_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of input driver at index. Can be NULL
 * if nothing found.
 **/
const char *input_driver_find_ident(int index);

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void);

/**
 * input_driver_set_rumble_state:
 * @port               : User number.
 * @effect             : Rumble effect.
 * @strength           : Strength of rumble effect.
 *
 * Sets the rumble state.
 * Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 **/
bool input_driver_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

int16_t input_driver_state(const struct retro_keybind **retro_keybinds,
      unsigned port, unsigned device, unsigned index, unsigned id);

uint64_t input_driver_get_capabilities(void);

const input_device_driver_t * input_driver_get_joypad_driver(void);

const input_device_driver_t * input_driver_get_sec_joypad_driver(void);

void input_driver_keyboard_mapping_set_block(bool value);

void input_driver_set(const input_driver_t **input, void **input_data);

/**
 * input_sensor_set_state:
 * @port               : User number.
 * @effect             : Sensor action.
 * @rate               : Sensor rate update.
 *
 * Sets the sensor state.
 * Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 **/
bool input_sensor_set_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);

float input_sensor_get_input(unsigned port, unsigned id);

/**
 * input_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool input_translate_coord_viewport(int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x,
      int16_t *res_screen_y);

/**
 * input_push_analog_dpad:
 * @binds                          : Binds to modify.
 * @mode                           : Which analog stick to bind D-Pad to.
 *                                   E.g:
 *                                   ANALOG_DPAD_LSTICK
 *                                   ANALOG_DPAD_RSTICK
 *
 * Push analog to D-Pad mappings to binds.
 **/
void input_push_analog_dpad(struct retro_keybind *binds, unsigned mode);

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
void input_pop_analog_dpad(struct retro_keybind *binds);

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
void input_poll(void);

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);

retro_input_t input_keys_pressed(void);

bool input_driver_ctl(enum rarch_input_ctl_state state, void *data);

void *input_driver_get_data(void);

const input_driver_t *input_get_ptr(void);

const input_driver_t **input_get_double_ptr(void);

void **input_driver_get_data_ptr(void);

#ifdef __cplusplus
}
#endif

#endif
