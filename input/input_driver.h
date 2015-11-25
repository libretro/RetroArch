/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "../libretro.h"

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

void find_input_driver(void);

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

bool input_driver_grab_mouse(bool state);

bool input_driver_grab_stdin(void);

void input_driver_destroy(void);

bool input_driver_keyboard_mapping_is_blocked(void);

void input_driver_keyboard_mapping_set_block(bool value);

const input_driver_t *input_get_ptr(void *data);

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

bool input_driver_ctl(enum rarch_input_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
