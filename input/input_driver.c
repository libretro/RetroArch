/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_NETWORKGAMEPAD
#include "input_remote.h"
#endif

#include "input_driver.h"
#include "input_keyboard.h"
#include "input_remapping.h"

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/widgets/menu_input_dialog.h"
#endif

#include "../configuration.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../movie.h"
#include "../list_special.h"
#include "../verbosity.h"
#include "../command.h"

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
#ifdef WIIU
   &input_wiiu,
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
#ifdef DJGPP
   &input_dos,
#endif
   &input_null,
   NULL,
};

typedef struct turbo_buttons turbo_buttons_t;

/* Turbo support. */
struct turbo_buttons
{
   bool frame_enable[MAX_USERS];
   uint16_t enable[MAX_USERS];
   unsigned count;
};

static turbo_buttons_t input_driver_turbo_btns;
#ifdef HAVE_COMMAND
static command_t *input_driver_command            = NULL;
#endif
#ifdef HAVE_NETWORKGAMEPAD
static input_remote_t *input_driver_remote        = NULL;
#endif
const input_driver_t *current_input               = NULL;
void *current_input_data                          = NULL;
static bool input_driver_block_hotkey             = false;
static bool input_driver_block_libretro_input     = false;
static bool input_driver_nonblock_state           = false;
static bool input_driver_flushing_input           = false;
static bool input_driver_data_own                 = false;

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

const input_driver_t *input_get_ptr(void)
{
   return current_input;
}

const input_driver_t **input_get_double_ptr(void)
{
   return &current_input;
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
   if (!current_input || !current_input->set_rumble)
      return false;
   return current_input->set_rumble(current_input_data,
         port, effect, strength);
}

const input_device_driver_t *input_driver_get_joypad_driver(void)
{
   if (!current_input || !current_input->get_joypad_driver)
      return NULL;
   return current_input->get_joypad_driver(current_input_data);
}

const input_device_driver_t *input_driver_get_sec_joypad_driver(void)
{
    if (!current_input || !current_input->get_sec_joypad_driver)
       return NULL;
    return current_input->get_sec_joypad_driver(current_input_data);
}

uint64_t input_driver_get_capabilities(void)
{
   if (!current_input || !current_input->get_capabilities)
      return 0;
   return current_input->get_capabilities(current_input_data);
}

void input_driver_set(const input_driver_t **input, void **input_data)
{
   if (input && input_data)
   {
      *input      = current_input;
      *input_data = current_input_data;
   }
   
   input_driver_set_own_driver();
}

void input_driver_keyboard_mapping_set_block(bool value)
{
   if (current_input->keyboard_mapping_set_block)
      current_input->keyboard_mapping_set_block(current_input_data, value);
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
   if (current_input_data &&
         current_input->set_sensor_state)
      return current_input->set_sensor_state(current_input_data,
            port, action, rate);
   return false;
}

float input_sensor_get_input(unsigned port, unsigned id)
{
   if (current_input_data &&
         current_input->get_sensor_input)
      return current_input->get_sensor_input(current_input_data,
            port, id);
   return 0.0f;
}

static const struct retro_keybind *libretro_input_binds[MAX_USERS];

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
void input_poll(void)
{
   size_t i;
   settings_t *settings           = config_get_ptr();
   unsigned max_users             = settings->input.max_users;
   
   current_input->poll(current_input_data);

   input_driver_turbo_btns.count++;

   for (i = 0; i < max_users; i++)
   {
      libretro_input_binds[i]                 = settings->input.binds[i];
      input_driver_turbo_btns.frame_enable[i] = 0;
   }

   if (!input_driver_block_libretro_input)
   {
      rarch_joypad_info_t joypad_info;
      joypad_info.axis_threshold = settings->input.axis_threshold;

      for (i = 0; i < max_users; i++)
      {
         if (libretro_input_binds[i][RARCH_TURBO_ENABLE].valid)
         {
            joypad_info.joy_idx        = settings->input.joypad_map[i];
            joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];

            input_driver_turbo_btns.frame_enable[i] = current_input->input_state(
                  current_input_data, joypad_info, libretro_input_binds,
                  i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
         }
      }

#ifdef HAVE_OVERLAY
      if (overlay_ptr && input_overlay_is_alive(overlay_ptr))
         input_poll_overlay(
               overlay_ptr,
               settings->input.overlay_opacity,
               settings->input.analog_dpad_mode[0],
               settings->input.axis_threshold);
#endif

#ifdef HAVE_COMMAND
      if (input_driver_command)
         command_poll(input_driver_command);
#endif

#ifdef HAVE_NETWORKGAMEPAD
      if (input_driver_remote)
         input_remote_poll(input_driver_remote,
               settings->input.max_users);
#endif
   }
}

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) 
 * was pressed by the user (assigned to @port).
 **/
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   int16_t res                     = 0;

   device &= RETRO_DEVICE_MASK;

   if (bsv_movie_ctl(BSV_MOVIE_CTL_PLAYBACK_ON, NULL))
   {
      int16_t bsv_result;
      if (bsv_movie_get_input(&bsv_result))
         return bsv_result;

      bsv_movie_ctl(BSV_MOVIE_CTL_SET_END, NULL);
   }

   if (     !input_driver_flushing_input 
         && !input_driver_block_libretro_input)
   {
      settings_t *settings = config_get_ptr();

      if (settings->input.remap_binds_enable)
      {
         switch (device)
         {
            case RETRO_DEVICE_JOYPAD:
               if (id < RARCH_FIRST_CUSTOM_BIND)
                  id = settings->input.remap_ids[port][id];
               break;
            case RETRO_DEVICE_ANALOG:
               if (idx < 2 && id < 2)
               {
                  unsigned new_id = RARCH_FIRST_CUSTOM_BIND + (idx * 2 + id);

                  new_id = settings->input.remap_ids[port][new_id];
                  idx   = (new_id & 2) >> 1;
                  id    = new_id & 1;
               }
               break;
         }
      }

      if (((id < RARCH_FIRST_META_KEY) || (device == RETRO_DEVICE_KEYBOARD)))
      {
         bool bind_valid = libretro_input_binds[port] && libretro_input_binds[port][id].valid;

         if (bind_valid || device == RETRO_DEVICE_KEYBOARD)
         {
            rarch_joypad_info_t joypad_info;

            joypad_info.axis_threshold = settings->input.axis_threshold;
            joypad_info.joy_idx        = settings->input.joypad_map[port];
            joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];

            res = current_input->input_state(
                  current_input_data, joypad_info, libretro_input_binds, port, device, idx, id);
         }
      }

#ifdef HAVE_OVERLAY
      if (overlay_ptr)
         input_state_overlay(overlay_ptr, &res, port, device, idx, id);
#endif

#ifdef HAVE_NETWORKGAMEPAD
      input_remote_state(&res, port, device, idx, id);
#endif

      /* Don't allow turbo for D-pad. */
      if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP ||
               id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         /*
          * Apply turbo button if activated.
          *
          * If turbo button is held, all buttons pressed except
          * for D-pad will go into a turbo mode. Until the button is
          * released again, the input state will be modulated by a 
          * periodic pulse defined by the configured duty cycle. 
          */
         if (res && input_driver_turbo_btns.frame_enable[port])
            input_driver_turbo_btns.enable[port] |= (1 << id);
         else if (!res)
            input_driver_turbo_btns.enable[port] &= ~(1 << id);

         if (input_driver_turbo_btns.enable[port] & (1 << id))
         {
            /* if turbo button is enabled for this key ID */
            res = res && ((input_driver_turbo_btns.count 
                     % settings->input.turbo_period)
                  < settings->input.turbo_duty_cycle);
         }
      }
   }

   if (bsv_movie_ctl(BSV_MOVIE_CTL_PLAYBACK_OFF, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_INPUT, &res);

   return res;
}

/**
 * check_input_driver_block_hotkey:
 *
 * Checks if 'hotkey enable' key is pressed.
 *
 * If we haven't bound anything to this,
 * always allow hotkeys.

 * If we hold ENABLE_HOTKEY button, block all libretro input to allow
 * hotkeys to be bound to same keys as RetroPad.
 **/
#define check_input_driver_block_hotkey(normal_bind, autoconf_bind) \
( \
         (((normal_bind)->key      != RETROK_UNKNOWN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((autoconf_bind)->key     != RETROK_UNKNOWN ) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)

static const unsigned buttons[] = {
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_B,
};

/**
 * state_tracker_update_input:
 *
 * Updates 16-bit input in same format as libretro API itself.
 **/
void state_tracker_update_input(uint16_t *input1, uint16_t *input2)
{
   unsigned i;
   const struct retro_keybind *binds[MAX_USERS];
   settings_t *settings = config_get_ptr();
   unsigned max_users   = settings->input.max_users;

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = settings->input.binds[i];
      struct retro_keybind *auto_binds    = settings->input.autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->input.analog_dpad_mode[i];
      binds[i]                            = settings->input.binds[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_push_analog_dpad(general_binds, dpad_mode);
      input_push_analog_dpad(auto_binds,    dpad_mode);
   }

   if (!input_driver_block_libretro_input)
   {
      rarch_joypad_info_t joypad_info;
      joypad_info.axis_threshold = settings->input.axis_threshold;

      for (i = 4; i < 16; i++)
      {
         unsigned id     = buttons[i - 4];

         if (binds[0][id].valid)
         {
            joypad_info.joy_idx        = settings->input.joypad_map[0];
            joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];
            *input1 |= (current_input->input_state(current_input_data, joypad_info,
                     binds,
                     0, RETRO_DEVICE_JOYPAD, 0, id) ? 1 : 0) << i;
         }
         if (binds[1][id].valid)
         {
            joypad_info.joy_idx        = settings->input.joypad_map[1];
            joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];
            *input2 |= (current_input->input_state(current_input_data, joypad_info,
                     binds,
                     1, RETRO_DEVICE_JOYPAD, 0, id) ? 1 : 0) << i;
         }
      }
   }

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = settings->input.binds[i];
      struct retro_keybind *auto_binds    = settings->input.autoconf_binds[i];

      input_pop_analog_dpad(general_binds);
      input_pop_analog_dpad(auto_binds);
   }
}

#ifdef HAVE_MENU
static INLINE bool input_menu_keys_pressed_internal(
      const struct retro_keybind **binds,
      settings_t *settings,
      rarch_joypad_info_t joypad_info,
      unsigned i,
      unsigned max_users,
      bool bind_valid, bool all_users_control_menu)
{
   if (
         (((!input_driver_block_libretro_input && ((i < RARCH_FIRST_META_KEY)))
           || !input_driver_block_hotkey)) && bind_valid
      )
   {
      unsigned port;
      unsigned port_max = all_users_control_menu ? max_users : 1;

      for (port = 0; port < port_max; port++)
      {
         const input_device_driver_t *first = current_input->get_joypad_driver 
            ? current_input->get_joypad_driver(current_input_data) : NULL;
         const input_device_driver_t *sec   = current_input->get_sec_joypad_driver 
            ? current_input->get_sec_joypad_driver(current_input_data) : NULL;

         joypad_info.joy_idx        = settings->input.joypad_map[port];
         joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];
         joypad_info.axis_threshold = settings->input.axis_threshold;

         if (sec   && input_joypad_pressed(sec,
                  joypad_info, port, settings->input.binds[0], i))
            return true;
         if (first && input_joypad_pressed(first,
                  joypad_info, port, settings->input.binds[0], i))
            return true;
      }
   }

   if (i >= RARCH_FIRST_META_KEY)
   {
      if (current_input->meta_key_pressed(current_input_data, i))
         return true;
   }

#ifdef HAVE_OVERLAY
   if (overlay_ptr && input_overlay_key_pressed(overlay_ptr, i))
      return true;
#endif

#ifdef HAVE_COMMAND
   if (input_driver_command)
   {
      command_handle_t handle;

      handle.handle = input_driver_command;
      handle.id     = i;

      if (command_get(&handle))
         return true;
   }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote)
   {
      if (input_remote_key_pressed(i, 0))
         return true;
   }
#endif

   return false;
}

static bool input_driver_toggle_button_combo(
      unsigned mode, uint64_t *trigger_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT64_GET(*trigger_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      default:
      case INPUT_TOGGLE_NONE:
         return false;
   }
   
   return true;
}

/**
 * input_menu_keys_pressed:
 *
 * Grab an input sample for this frame. We exclude
 * keyboard input here.
 *
 * TODO: In case RARCH_BIND_LIST_END starts exceeding 64,
 * and you need a bitmask of more than 64 entries, reimplement
 * it to use something like rarch_bits_t.
 *
 * Returns: Input sample containg a mask of all pressed keys.
 */
uint64_t input_menu_keys_pressed(
      uint64_t old_input,
      uint64_t *last_input,
      uint64_t *trigger_input,
      bool runloop_paused)
{
   uint64_t             ret                     = 0;

   if (current_input && current_input_data)
   {
      unsigned i;
      rarch_joypad_info_t joypad_info;
      const struct retro_keybind *binds[MAX_USERS] = {NULL};
      settings_t     *settings                     = config_get_ptr();
      const struct retro_keybind *binds_norm       = NULL;
      const struct retro_keybind *binds_auto       = NULL;
      unsigned max_users                           = settings->input.max_users;

      if (settings->menu.unified_controls && !menu_input_dialog_get_display_kb())
         return input_keys_pressed(old_input, last_input,
               trigger_input, runloop_paused);

      for (i = 0; i < max_users; i++)
      {
         struct retro_keybind *auto_binds    = settings->input.autoconf_binds[i];

         input_push_analog_dpad(auto_binds, ANALOG_DPAD_LSTICK);
      }

      joypad_info.axis_threshold = settings->input.axis_threshold;

      input_driver_block_libretro_input            = false;
      input_driver_block_hotkey                    = false;

      if (current_input->keyboard_mapping_is_blocked 
            && current_input->keyboard_mapping_is_blocked(current_input_data))
         input_driver_block_hotkey = true;

      binds_norm = &settings->input.binds[0][RARCH_ENABLE_HOTKEY];
      binds_auto = &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];

      for (i = 0; i < max_users; i++)
         binds[i] = settings->input.binds[i];

      if (check_input_driver_block_hotkey(binds_norm, binds_auto))
      {
         joypad_info.joy_idx        = settings->input.joypad_map[0];
         joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];

         if (settings->input.binds[0][RARCH_ENABLE_HOTKEY].valid 
               && current_input->input_state(current_input_data, joypad_info,
                  &binds[0], 0, RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
            input_driver_block_libretro_input = true;
         else
            input_driver_block_hotkey         = true;
      }

      if (  ((settings->input.menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
            input_driver_toggle_button_combo(
               settings->input.menu_toggle_gamepad_combo, &old_input))
            || input_menu_keys_pressed_internal(
               binds, settings, joypad_info, RARCH_MENU_TOGGLE, max_users,
               settings->input.binds[0][RARCH_MENU_TOGGLE].valid,
               settings->input.all_users_control_menu))
         ret |= (UINT64_C(1) << RARCH_MENU_TOGGLE);

      for (i = 0; i < RARCH_BIND_LIST_END; i++)
      {
         if (i != RARCH_MENU_TOGGLE &&
               input_menu_keys_pressed_internal(binds,
                  settings, joypad_info, i, max_users,
                  settings->input.binds[0][i].valid,
                  settings->input.all_users_control_menu))
            ret |= (UINT64_C(1) << i);
         
      }

      for (i = 0; i < max_users; i++)
      {
         struct retro_keybind *auto_binds    = settings->input.autoconf_binds[i];
         input_pop_analog_dpad(auto_binds);
      }

      if (!menu_input_dialog_get_display_kb())
      {
         unsigned ids[13][2];

         ids[0][0]  = RETROK_SPACE;
         ids[0][1]  = RETRO_DEVICE_ID_JOYPAD_START;
         ids[1][0]  = RETROK_SLASH;
         ids[1][1]  = RETRO_DEVICE_ID_JOYPAD_X;
         ids[2][0]  = RETROK_RSHIFT;
         ids[2][1]  = RETRO_DEVICE_ID_JOYPAD_SELECT;
         ids[3][0]  = RETROK_RIGHT;
         ids[3][1]  = RETRO_DEVICE_ID_JOYPAD_RIGHT;
         ids[4][0]  = RETROK_LEFT;
         ids[4][1]  = RETRO_DEVICE_ID_JOYPAD_LEFT;
         ids[5][0]  = RETROK_DOWN;
         ids[5][1]  = RETRO_DEVICE_ID_JOYPAD_DOWN;
         ids[6][0]  = RETROK_UP;
         ids[6][1]  = RETRO_DEVICE_ID_JOYPAD_UP;
         ids[7][0]  = RETROK_PAGEUP;
         ids[7][1]  = RETRO_DEVICE_ID_JOYPAD_L;
         ids[8][0]  = RETROK_PAGEDOWN;
         ids[8][1]  = RETRO_DEVICE_ID_JOYPAD_R;
         ids[9][0]  = settings->input.binds[0][RARCH_QUIT_KEY].key;
         ids[9][1]  = RARCH_QUIT_KEY;
         ids[10][0] = settings->input.binds[0][RARCH_FULLSCREEN_TOGGLE_KEY].key;
         ids[10][1] = RARCH_FULLSCREEN_TOGGLE_KEY;
         ids[11][0] = RETROK_BACKSPACE;
         ids[11][1] = RETRO_DEVICE_ID_JOYPAD_B;
         ids[12][0] = RETROK_RETURN;
         ids[12][1] = RETRO_DEVICE_ID_JOYPAD_A;

         if (settings->input.menu_swap_ok_cancel_buttons)
         {
            ids[11][1] = RETRO_DEVICE_ID_JOYPAD_A;
            ids[12][1] = RETRO_DEVICE_ID_JOYPAD_B;
         }

         for (i = 0; i < 13; i++)
         {
            if (current_input->input_state(current_input_data, joypad_info, binds, 0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
               BIT64_SET(ret, ids[i][1]);
         }
      }
   }

   *trigger_input = ret & ~old_input;
   *last_input    = ret;

   if (input_driver_flushing_input)
   { 
      input_driver_flushing_input = false; 
      if (ret) 
      {
         ret = 0;
         if (runloop_paused)
            BIT64_SET(ret, RARCH_PAUSE_TOGGLE);
         input_driver_flushing_input = true; 
      }
   }

   if (menu_driver_is_binding_state())
      *trigger_input = 0;

   return ret;
}
#endif

static INLINE bool input_keys_pressed_internal(
      settings_t *settings,
      rarch_joypad_info_t joypad_info,
      unsigned i,
      const struct retro_keybind *binds)
{
   if (((!input_driver_block_libretro_input && ((i < RARCH_FIRST_META_KEY)))
            || !input_driver_block_hotkey))
   {
      bool bind_valid = binds[i].valid;

      joypad_info.joy_idx        = settings->input.joypad_map[0];
      joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];

      if (bind_valid && current_input->input_state(current_input_data,
               joypad_info, &binds,
               0, RETRO_DEVICE_JOYPAD, 0, i))
         return true;
   }

   if (i >= RARCH_FIRST_META_KEY)
   {
      if (current_input->meta_key_pressed(current_input_data, i))
         return true;
   }

#ifdef HAVE_OVERLAY
   if (overlay_ptr && input_overlay_key_pressed(overlay_ptr, i))
      return true;
#endif

#ifdef HAVE_COMMAND
   if (input_driver_command)
   {
      command_handle_t handle;

      handle.handle = input_driver_command;
      handle.id     = i;

      if (command_get(&handle))
         return true;
   }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote)
   {
      if (input_remote_key_pressed(i, 0))
         return true;
   }
#endif

   return false;
}


/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * TODO: In case RARCH_BIND_LIST_END starts exceeding 64,
 * and you need a bitmask of more than 64 entries, reimplement
 * it to use something like rarch_bits_t.
 *
 * Returns: Input sample containg a mask of all pressed keys.
 */
uint64_t input_keys_pressed(
      uint64_t old_input,
      uint64_t *last_input,
      uint64_t *trigger_input,
      bool runloop_paused)
{
   unsigned i;
   rarch_joypad_info_t joypad_info;
   uint64_t                      ret            = 0;
   settings_t              *settings            = config_get_ptr();
   const struct retro_keybind *binds            = settings->input.binds[0];
   const struct retro_keybind *binds_auto       = &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *binds_norm       = &binds[RARCH_ENABLE_HOTKEY];

   const struct retro_keybind *focus_binds_auto = &settings->input.autoconf_binds[0][RARCH_GAME_FOCUS_TOGGLE];
   const struct retro_keybind *focus_normal     = &binds[RARCH_GAME_FOCUS_TOGGLE];
   bool enable_hotkey_valid                     = settings->input.binds[0][RARCH_ENABLE_HOTKEY].valid;
   bool game_focus_toggle_valid                 = false;

   joypad_info.axis_threshold                   = settings->input.axis_threshold;
   
   input_driver_block_libretro_input            = false;
   input_driver_block_hotkey                    = false;

   if (     current_input->keyboard_mapping_is_blocked 
         && current_input->keyboard_mapping_is_blocked(current_input_data))
      input_driver_block_hotkey = true;

   if (check_input_driver_block_hotkey(binds_norm, binds_auto))
   {
      joypad_info.joy_idx        = settings->input.joypad_map[0];
      joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];
      if (     enable_hotkey_valid 
            && current_input->input_state(
               current_input_data, joypad_info, &binds, 0,
               RETRO_DEVICE_JOYPAD, 0, RARCH_ENABLE_HOTKEY))
            input_driver_block_libretro_input = true;
         else
            input_driver_block_hotkey         = true;
   }

   game_focus_toggle_valid                      = binds[RARCH_GAME_FOCUS_TOGGLE].valid;

   /* Allows rarch_focus_toggle hotkey to still work 
    * even though every hotkey is blocked */
   if (check_input_driver_block_hotkey(
            focus_normal, focus_binds_auto) && game_focus_toggle_valid)
   {
      joypad_info.joy_idx        = settings->input.joypad_map[0];
      joypad_info.auto_binds     = settings->input.autoconf_binds[joypad_info.joy_idx];
      if (current_input->input_state(current_input_data, joypad_info, &binds, 0,
               RETRO_DEVICE_JOYPAD, 0, RARCH_GAME_FOCUS_TOGGLE))
         input_driver_block_hotkey = false;
   }

   if (
         ((settings->input.menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
         input_driver_toggle_button_combo(settings->input.menu_toggle_gamepad_combo, &old_input))
         || input_keys_pressed_internal(settings, joypad_info, RARCH_MENU_TOGGLE, binds))
      ret |= (UINT64_C(1) << RARCH_MENU_TOGGLE);

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      if (  (i != RARCH_MENU_TOGGLE) && 
            input_keys_pressed_internal(settings, joypad_info, i, binds))
         ret |= (UINT64_C(1) << i);
   }

   *trigger_input = ret & ~old_input;
   *last_input    = ret;

   if (input_driver_flushing_input)
   { 
      input_driver_flushing_input = false; 
      if (ret) 
      {
         ret = 0;
         if (runloop_paused)
            BIT64_SET(ret, RARCH_PAUSE_TOGGLE);
         input_driver_flushing_input = true; 
      }
   }

   return ret;
}


void *input_driver_get_data(void)
{
   return current_input_data;
}

void **input_driver_get_data_ptr(void)
{
   return (void**)&current_input_data;
}

bool input_driver_has_capabilities(void)
{
   if (!current_input->get_capabilities || !current_input_data)
      return false;
   return true;
}

void input_driver_poll(void)
{
   current_input->poll(current_input_data);
}

bool input_driver_init(void)
{
   unsigned i;
   settings_t *settings       = config_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
      libretro_input_binds[i] = settings->input.binds[i];
   if (current_input)
      current_input_data      = current_input->init(settings->input.joypad_driver);

   if (!current_input_data)
      return false;
   return true;
}

void input_driver_deinit(void)
{
   if (current_input && current_input->free)
      current_input->free(current_input_data);
   current_input_data = NULL;
}

void input_driver_destroy_data(void)
{
   current_input_data = NULL;
}

void input_driver_destroy(void)
{
   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_DESTROY, NULL);
   input_driver_block_hotkey             = false;
   input_driver_block_libretro_input     = false;
   input_driver_nonblock_state           = false;
   input_driver_flushing_input           = false;
   input_driver_data_own                 = false;
   memset(&input_driver_turbo_btns, 0, sizeof(turbo_buttons_t));
   current_input                         = NULL;
}

bool input_driver_grab_stdin(void)
{
   if (!current_input->grab_stdin)
      return false;
   return current_input->grab_stdin(current_input_data);
}

bool input_driver_keyboard_mapping_is_blocked(void)
{
   return current_input->keyboard_mapping_is_blocked(
         current_input_data);
}

bool input_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

   drv.label            = "input_driver";
   drv.s                = settings->input.driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i                    = drv.len;

   if (i >= 0)
      current_input = (const input_driver_t*)
         input_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any input driver named \"%s\"\n",
            settings->input.driver);
      RARCH_LOG_OUTPUT("Available input drivers are:\n");
      for (d = 0; input_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", input_driver_find_ident(d));
      RARCH_WARN("Going to default to first input driver...\n");

      current_input = (const input_driver_t*)
         input_driver_find_handle(0);

      if (current_input)
         return true;
      retroarch_fail(1, "find_input_driver()");
      return false;
   }

   return true;
}

void input_driver_set_flushing_input(void)
{
   input_driver_flushing_input = true;
}

void input_driver_unset_hotkey_block(void)
{
   input_driver_block_hotkey = true;
}

void input_driver_set_hotkey_block(void)
{
   input_driver_block_hotkey = true;
}

void input_driver_set_libretro_input_blocked(void)
{
   input_driver_block_libretro_input = true;
}

void input_driver_unset_libretro_input_blocked(void)
{
   input_driver_block_libretro_input = false;
}

bool input_driver_is_libretro_input_blocked(void)
{
   return input_driver_block_libretro_input;
}

void input_driver_set_nonblock_state(void)
{
   input_driver_nonblock_state = true;
}

void input_driver_unset_nonblock_state(void)
{
   input_driver_nonblock_state = false;
}

bool input_driver_is_nonblock_state(void)
{
   return input_driver_nonblock_state;
}

void input_driver_set_own_driver(void)
{
   input_driver_data_own = true;
}

void input_driver_unset_own_driver(void)
{
   input_driver_data_own = false;
}

bool input_driver_owns_driver(void)
{
   return input_driver_data_own;
}

bool input_driver_init_command(void)
{
#ifdef HAVE_COMMAND
   settings_t *settings = config_get_ptr();
   if (     !settings->stdin_cmd_enable 
         && !settings->network_cmd_enable)
      return false;

   if (settings->stdin_cmd_enable 
         && input_driver_grab_stdin())
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   input_driver_command = command_new(false);
   
   if (command_network_new(
            input_driver_command,
            settings->stdin_cmd_enable
            && !input_driver_grab_stdin(),
            settings->network_cmd_enable,
            settings->network_cmd_port))
      return true;

   RARCH_ERR("Failed to initialize command interface.\n");
#endif
   return false;
}

void input_driver_deinit_command(void)
{
#ifdef HAVE_COMMAND
   if (input_driver_command)
      command_free(input_driver_command);
   input_driver_command = NULL;
#endif
}

void input_driver_deinit_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   if (input_driver_remote)
   {
      settings_t *settings = config_get_ptr();

      input_remote_free(input_driver_remote,
            settings->input.max_users);
   }
   input_driver_remote = NULL;
#endif
}

bool input_driver_init_remote(void)
{
#ifdef HAVE_NETWORKGAMEPAD
   settings_t *settings = config_get_ptr();

   if (!settings->network_remote_enable)
      return false;

   input_driver_remote = input_remote_new(
         settings->network_remote_base_port,
         settings->input.max_users);

   if (input_driver_remote)
      return true;

   RARCH_ERR("Failed to initialize remote gamepad interface.\n");
#endif
   return false;
}

bool input_driver_grab_mouse(void)
{
   if (!current_input || !current_input->grab_mouse)
      return false;

   current_input->grab_mouse(current_input_data, true);
   return true;
}

bool input_driver_ungrab_mouse(void)
{
   if (!current_input || !current_input->grab_mouse)
      return false;

   current_input->grab_mouse(current_input_data, false);
   return true;
}

bool input_driver_is_data_ptr_same(void *data)
{
   return (current_input_data == data);
}
