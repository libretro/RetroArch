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

#include <string.h>

#include "../general.h"
#include "../string_list_special.h"
#include "../verbosity.h"

static const input_driver_t *input_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   &input_psp,
#endif
#if defined(_3DS)
   &input_ctr,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &input_sdl,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
   &input_xinput,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef ANDROID
   &input_android,
#endif
#ifdef HAVE_UDEV
   &input_udev,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
   &input_cocoa,
#endif
#ifdef __QNX__
   &input_qnx,
#endif
#ifdef EMSCRIPTEN
   &input_rwebinput,
#endif
   &input_null,
   NULL,
};

/**
 * input_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to input driver at index. Can be NULL
 * if nothing found.
 **/
const void *input_driver_find_handle(int idx)
{
   const void *drv = input_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * input_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of input driver at index. Can be NULL
 * if nothing found.
 **/
const char *input_driver_find_ident(int idx)
{
   const input_driver_t *drv = input_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_DRIVERS, NULL);
}

void find_input_driver(void)
{
   driver_t *driver = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   int i = find_driver_index("input_driver", settings->input.driver);

   if (i >= 0)
      driver->input = (const input_driver_t*)input_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any input driver named \"%s\"\n",
            settings->input.driver);
      RARCH_LOG_OUTPUT("Available input drivers are:\n");
      for (d = 0; input_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", input_driver_find_ident(d));
      RARCH_WARN("Going to default to first input driver...\n");

      driver->input = (const input_driver_t*)input_driver_find_handle(0);

      if (!driver->input)
         retro_fail(1, "find_input_driver()");
   }
}

const input_driver_t *input_get_ptr(void *data)
{
   driver_t *driver = (driver_t*)data;
   if (!driver)
      return NULL;
   return driver->input;
}

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
      enum retro_rumble_effect effect, uint16_t strength)
{
   driver_t            *driver = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   if (input->set_rumble)
      return input->set_rumble(driver->input_data,
            port, effect, strength);
   return false;
}

int16_t input_driver_state(const struct retro_keybind **retro_keybinds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   driver_t            *driver = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   return input->input_state(driver->input_data, retro_keybinds,
         port, device, index, id);
}

const input_device_driver_t *input_driver_get_joypad_driver(void)
{
   driver_t            *driver = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   if (input->get_joypad_driver)
      return input->get_joypad_driver(driver->input_data);
   return NULL;
}

const input_device_driver_t *input_driver_get_sec_joypad_driver(void)
{
    driver_t            *driver = driver_get_ptr();
    const input_driver_t *input = input_get_ptr(driver);
    
    if (input->get_sec_joypad_driver)
        return input->get_sec_joypad_driver(driver->input_data);
    return NULL;
}

uint64_t input_driver_get_capabilities(void)
{
   driver_t            *driver = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   if (input->get_capabilities)
      return input->get_capabilities(driver->input_data);
   return 0; 
}

bool input_driver_grab_mouse(bool state)
{
   driver_t            *driver = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   if (input->grab_mouse)
   {
      input->grab_mouse(driver->input_data, state);
      return true;
   }
   return false;
}

void input_driver_set(const input_driver_t **input, void **input_data)
{
   driver_t *driver               = driver_get_ptr();

   if (input && input_data)
   {
      *input      = driver->input;
      *input_data = driver->input_data;
   }

   driver->input_data_own = true;
}

void input_driver_destroy(void)
{
   driver_t *driver = driver_get_ptr();

   if (driver)
      driver->input_data = NULL;
}

void input_driver_keyboard_mapping_set_block(bool value)
{
   driver_t *driver               = driver_get_ptr();
   const input_driver_t *input = input_get_ptr(driver);

   if (input->keyboard_mapping_set_block)
      driver->input->keyboard_mapping_set_block(driver->input_data, value);
}

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
      enum retro_sensor_action action, unsigned rate)
{
   driver_t *driver = driver_get_ptr();

   if (driver->input && driver->input_data &&
         driver->input->set_sensor_state)
      return driver->input->set_sensor_state(driver->input_data,
            port, action, rate);
   return false;
}

float input_sensor_get_input(unsigned port, unsigned id)
{
   driver_t *driver = driver_get_ptr();

   if (driver->input && driver->input_data &&
         driver->input->get_sensor_input)
      return driver->input->get_sensor_input(driver->input_data,
            port, id);
   return 0.0f;
}

bool input_driver_ctl(enum rarch_input_ctl_state state, void *data)
{
   driver_t                   *driver = driver_get_ptr();
   const input_driver_t        *input = input_get_ptr(driver);
   settings_t               *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_INPUT_CTL_INIT:
         if (driver && driver->input)
            driver->input_data = driver->input->init();

         if (!driver->input_data)
            return false;
         return true;
      case RARCH_INPUT_CTL_DEINIT:
         if (!driver || !driver->input)
            return false;
         driver->input->free(driver->input_data);
         return true;
      case RARCH_INPUT_CTL_GRAB_STDIN:
         if (input->grab_stdin)
            return input->grab_stdin(driver->input_data);
         return false;
      case RARCH_INPUT_CTL_KB_MAPPING_IS_BLOCKED:
         if (input->keyboard_mapping_is_blocked)
            return driver->input->keyboard_mapping_is_blocked(
                  driver->input_data);
         return false;
      case RARCH_INPUT_CTL_NONE:
      default:
         break;
   }

   return false;
}
