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

#include <string.h>

#include "input_remapping.h"

#include "../general.h"
#include "../movie.h"
#include "../string_list_special.h"
#include "../verbosity.h"

#ifdef HAVE_COMMAND
#include "../command.h"
#endif

#ifdef HAVE_NETWORK_GAMEPAD
#include "../remote.h"
#endif

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
static rarch_cmd_t *input_driver_command = NULL;
#endif
#ifdef HAVE_NETWORK_GAMEPAD
static rarch_remote_t *input_driver_remote = NULL;
#endif
static const input_driver_t *current_input = NULL;
static void *current_input_data = NULL;

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

int16_t input_driver_state(const struct retro_keybind **retro_keybinds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   return current_input->input_state(current_input_data, retro_keybinds,
         port, device, index, id);
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
   
   input_driver_ctl(RARCH_INPUT_CTL_SET_OWN_DRIVER, NULL);
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

static retro_input_t input_driver_keys_pressed(void)
{
   unsigned key;
   retro_input_t                ret = 0;

   for (key = 0; key < RARCH_BIND_LIST_END; key++)
   {
      bool state = false;
      if ((!input_driver_ctl(RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED, NULL) && ((key < RARCH_FIRST_META_KEY)))
            || !input_driver_ctl(RARCH_INPUT_CTL_IS_HOTKEY_BLOCKED, NULL))
         state = input_driver_ctl(RARCH_INPUT_CTL_KEY_PRESSED, &key);

      if (key >= RARCH_FIRST_META_KEY)
         state |= current_input->meta_key_pressed(current_input_data, key);

#ifdef HAVE_OVERLAY
      state |= input_overlay_key_pressed(key);
#endif

#ifdef HAVE_COMMAND
      if (input_driver_command)
         state |= rarch_cmd_get(input_driver_command, key);
#endif

#ifdef HAVE_NETWORK_GAMEPAD
      if (input_driver_remote)
         state |= input_remote_key_pressed(key,0);
#endif

      if (state)
         ret |= (UINT64_C(1) << key);
   }
   return ret;
}

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
void input_push_analog_dpad(struct retro_keybind *binds, unsigned mode)
{
   unsigned i, j = 0;
   bool inherit_joyaxis = false;

   for (i = RETRO_DEVICE_ID_JOYPAD_UP; i <= RETRO_DEVICE_ID_JOYPAD_RIGHT; i++)
      binds[i].orig_joyaxis = binds[i].joyaxis;

   switch (mode)
   {
      case ANALOG_DPAD_LSTICK:
         /* check if analog left is defined.   *
          * if plus and minus are equal abort. */
         if (!((binds[RARCH_ANALOG_LEFT_X_PLUS].joyaxis == 
               binds[RARCH_ANALOG_LEFT_X_MINUS].joyaxis) || 
               (binds[RARCH_ANALOG_LEFT_Y_PLUS].joyaxis == 
               binds[RARCH_ANALOG_LEFT_Y_MINUS].joyaxis)))
         {
            j = RARCH_ANALOG_LEFT_X_PLUS + 3;
            inherit_joyaxis = true;
         }
         break;
      case ANALOG_DPAD_RSTICK:
         /* check if analog right is defined.  *
          * if plus and minus are equal abort. */
         if (!((binds[RARCH_ANALOG_RIGHT_X_PLUS].joyaxis == 
               binds[RARCH_ANALOG_RIGHT_X_MINUS].joyaxis) || 
               (binds[RARCH_ANALOG_RIGHT_Y_PLUS].joyaxis == 
               binds[RARCH_ANALOG_RIGHT_Y_MINUS].joyaxis)))
         {          
            j = RARCH_ANALOG_RIGHT_X_PLUS + 3;
            inherit_joyaxis = true;
         }
         break;
   }

   if (!inherit_joyaxis)
      return;

   /* Inherit joyaxis from analogs. */
   for (i = RETRO_DEVICE_ID_JOYPAD_UP; i <= RETRO_DEVICE_ID_JOYPAD_RIGHT; i++)
      binds[i].joyaxis = binds[j--].joyaxis;
}

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
void input_pop_analog_dpad(struct retro_keybind *binds)
{
   unsigned i;

   for (i = RETRO_DEVICE_ID_JOYPAD_UP; i <= RETRO_DEVICE_ID_JOYPAD_RIGHT; i++)
      binds[i].joyaxis = binds[i].orig_joyaxis;
}

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
      int16_t *res_screen_y)
{
   int scaled_screen_x, scaled_screen_y, scaled_x, scaled_y;
   struct video_viewport vp = {0};

   if (!video_driver_viewport_info(&vp))
      return false;

   scaled_screen_x = (2 * mouse_x * 0x7fff) / (int)vp.full_width - 0x7fff;
   scaled_screen_y = (2 * mouse_y * 0x7fff) / (int)vp.full_height - 0x7fff;
   if (scaled_screen_x < -0x7fff || scaled_screen_x > 0x7fff)
      scaled_screen_x = -0x8000; /* OOB */
   if (scaled_screen_y < -0x7fff || scaled_screen_y > 0x7fff)
      scaled_screen_y = -0x8000; /* OOB */

   mouse_x -= vp.x;
   mouse_y -= vp.y;

   scaled_x = (2 * mouse_x * 0x7fff) / (int)vp.width - 0x7fff;
   scaled_y = (2 * mouse_y * 0x7fff) / (int)vp.height - 0x7fff;
   if (scaled_x < -0x7fff || scaled_x > 0x7fff)
      scaled_x = -0x8000; /* OOB */
   if (scaled_y < -0x7fff || scaled_y > 0x7fff)
      scaled_y = -0x8000; /* OOB */

   *res_x = scaled_x;
   *res_y = scaled_y;
   *res_screen_x = scaled_screen_x;
   *res_screen_y = scaled_screen_y;

   return true;
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

   input_driver_ctl(RARCH_INPUT_CTL_POLL, NULL);

   for (i = 0; i < MAX_USERS; i++)
      libretro_input_binds[i] = settings->input.binds[i];

#ifdef HAVE_OVERLAY
   input_poll_overlay(settings->input.overlay_opacity);
#endif

#ifdef HAVE_COMMAND
   if (input_driver_command)
      rarch_cmd_ctl(RARCH_CMD_CTL_POLL, input_driver_command);
#endif

#ifdef HAVE_NETWORK_GAMEPAD
   if (input_driver_remote)
      rarch_remote_poll(input_driver_remote);
#endif
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
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   int16_t res                     = 0;
   settings_t *settings            = config_get_ptr();
   

   device &= RETRO_DEVICE_MASK;

   if (bsv_movie_ctl(BSV_MOVIE_CTL_PLAYBACK_ON, NULL))
   {
      int16_t ret;
      if (bsv_movie_ctl(BSV_MOVIE_CTL_GET_INPUT, &ret))
         return ret;

      bsv_movie_ctl(BSV_MOVIE_CTL_SET_END, NULL);
   }

   if (settings->input.remap_binds_enable)
      input_remapping_state(port, &device, &idx, &id);

   if (!input_driver_ctl(RARCH_INPUT_CTL_IS_FLUSHING_INPUT, NULL) 
         && !input_driver_ctl(RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED, NULL))
   {
      if (((id < RARCH_FIRST_META_KEY) || (device == RETRO_DEVICE_KEYBOARD)))
         res = current_input->input_state(
               current_input_data, libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
      input_state_overlay(&res, port, device, idx, id);
#endif

#ifdef HAVE_NETWORK_GAMEPAD
      input_state_remote(&res, port, device, idx, id);
#endif
   }

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
         res = res && ((input_driver_turbo_btns.count % settings->input.turbo_period)
               < settings->input.turbo_duty_cycle);
      }
   }

   if (bsv_movie_ctl(BSV_MOVIE_CTL_PLAYBACK_OFF, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_INPUT, &res);

   return res;
}

/**
 * check_input_driver_block_hotkey:
 * @enable_hotkey        : Is hotkey enable key enabled?
 *
 * Checks if 'hotkey enable' key is pressed.
 **/
static bool check_input_driver_block_hotkey(bool enable_hotkey)
{
   bool use_hotkey_enable;
   settings_t *settings          = config_get_ptr();
   const struct retro_keybind *bind =
      &settings->input.binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *autoconf_bind =
      &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   bool kb_mapping_is_blocked = input_driver_ctl(
         RARCH_INPUT_CTL_KB_MAPPING_IS_BLOCKED, NULL);

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   if (kb_mapping_is_blocked)
      input_driver_ctl(RARCH_INPUT_CTL_SET_HOTKEY_BLOCK, NULL);
   else
      input_driver_ctl(RARCH_INPUT_CTL_UNSET_HOTKEY_BLOCK, NULL);

   /* If we haven't bound anything to this,
    * always allow hotkeys. */
   use_hotkey_enable          =
         (bind->key != RETROK_UNKNOWN)
      || (bind->joykey != NO_BTN)
      || (bind->joyaxis != AXIS_NONE)
      || (autoconf_bind->key != RETROK_UNKNOWN )
      || (autoconf_bind->joykey != NO_BTN)
      || (autoconf_bind->joyaxis != AXIS_NONE);

   if (kb_mapping_is_blocked || (use_hotkey_enable && !enable_hotkey))
      input_driver_ctl(RARCH_INPUT_CTL_SET_HOTKEY_BLOCK, NULL);
   else
      input_driver_ctl(RARCH_INPUT_CTL_UNSET_HOTKEY_BLOCK, NULL);

   /* If we hold ENABLE_HOTKEY button, block all libretro input to allow
    * hotkeys to be bound to same keys as RetroPad. */
   return (use_hotkey_enable && enable_hotkey);
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
retro_input_t input_keys_pressed(void)
{
   unsigned i, key;
   const struct retro_keybind *binds[MAX_USERS];
   retro_input_t             ret = 0;
   settings_t *settings          = config_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
      binds[i] = settings->input.binds[i];

   if (!current_input || !current_input_data)
      return 0;

   input_driver_turbo_btns.count++;

   key = RARCH_ENABLE_HOTKEY;
   
   if (check_input_driver_block_hotkey(input_driver_ctl(RARCH_INPUT_CTL_KEY_PRESSED, &key)))
      input_driver_ctl(RARCH_INPUT_CTL_SET_LIBRETRO_INPUT_BLOCKED, NULL);
   else
      input_driver_ctl(RARCH_INPUT_CTL_UNSET_LIBRETRO_INPUT_BLOCKED, NULL);

   for (i = 0; i < settings->input.max_users; i++)
   {
      input_push_analog_dpad(settings->input.binds[i],
            settings->input.analog_dpad_mode[i]);
      input_push_analog_dpad(settings->input.autoconf_binds[i],
            settings->input.analog_dpad_mode[i]);

      input_driver_turbo_btns.frame_enable[i] = 0;
   }

   if (!input_driver_ctl(RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED, NULL))
   {
      for (i = 0; i < settings->input.max_users; i++)
         input_driver_turbo_btns.frame_enable[i] = input_driver_state(binds,
               i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
   }

   ret = input_driver_keys_pressed();

   for (i = 0; i < settings->input.max_users; i++)
   {
      input_pop_analog_dpad(settings->input.binds[i]);
      input_pop_analog_dpad(settings->input.autoconf_binds[i]);
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

bool input_driver_data_ptr_is_same(void *data)
{
   return (current_input_data == data);
}

bool input_driver_ctl(enum rarch_input_ctl_state state, void *data)
{
   static bool input_driver_block_hotkey             = false;
   static bool input_driver_block_libretro_input     = false;
   static bool input_driver_osk_enabled              = false;
   static bool input_driver_keyboard_linefeed_enable = false;
   static bool input_driver_nonblock_state           = false;
   static bool input_driver_flushing_input           = false;
   static bool input_driver_data_own                 = false;
   settings_t *settings                              = config_get_ptr();

   switch (state)
   {
      case RARCH_INPUT_CTL_KEY_PRESSED:
         {
            unsigned *key = (unsigned*)data;
            if (key && current_input->key_pressed)
               return current_input->key_pressed(current_input_data, *key);
         }
         return false;
      case RARCH_INPUT_CTL_HAS_CAPABILITIES:
         if (!current_input->get_capabilities || !current_input_data)
            return false;
         break;
      case RARCH_INPUT_CTL_POLL:
         current_input->poll(current_input_data);
         break;
      case RARCH_INPUT_CTL_INIT:
         if (current_input)
            current_input_data = current_input->init();

         if (!current_input_data)
            return false;
         break;
      case RARCH_INPUT_CTL_DEINIT:
         if (current_input && current_input->free)
            current_input->free(current_input_data);
         current_input_data = NULL;
         break;
      case RARCH_INPUT_CTL_DESTROY_DATA:
         current_input_data = NULL;
         break;
      case RARCH_INPUT_CTL_DESTROY:
         input_driver_block_hotkey             = false;
         input_driver_block_libretro_input     = false;
         input_driver_keyboard_linefeed_enable = false;
         input_driver_nonblock_state           = false;
         input_driver_flushing_input           = false;
         input_driver_data_own                 = false;
         memset(&input_driver_turbo_btns, 0, sizeof(turbo_buttons_t));
         current_input                         = NULL;
         break;
      case RARCH_INPUT_CTL_GRAB_STDIN:
         if (!current_input->grab_stdin)
            return false;
         return current_input->grab_stdin(current_input_data);
      case RARCH_INPUT_CTL_KB_MAPPING_IS_BLOCKED:
         if (!current_input->keyboard_mapping_is_blocked)
            return false;
         return current_input->keyboard_mapping_is_blocked(
               current_input_data);
      case RARCH_INPUT_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;

            drv.label = "input_driver";
            drv.s     = settings->input.driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = drv.len;

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
               retro_fail(1, "find_input_driver()");
               return false;
            }
         }
         break;
      case RARCH_INPUT_CTL_SET_FLUSHING_INPUT:
         input_driver_flushing_input = true;
         break;
      case RARCH_INPUT_CTL_UNSET_FLUSHING_INPUT:
         input_driver_flushing_input = false;
         break;
      case RARCH_INPUT_CTL_IS_FLUSHING_INPUT:
         return input_driver_flushing_input;
      case RARCH_INPUT_CTL_SET_HOTKEY_BLOCK:
         input_driver_block_hotkey = true;
         break;
      case RARCH_INPUT_CTL_UNSET_HOTKEY_BLOCK:
         input_driver_block_hotkey = false;
         break;
      case RARCH_INPUT_CTL_IS_HOTKEY_BLOCKED:
         return input_driver_block_hotkey;
      case RARCH_INPUT_CTL_SET_LIBRETRO_INPUT_BLOCKED:
         input_driver_block_libretro_input = true;
         break;
      case RARCH_INPUT_CTL_UNSET_LIBRETRO_INPUT_BLOCKED:
         input_driver_block_libretro_input = false;
         break;
      case RARCH_INPUT_CTL_IS_LIBRETRO_INPUT_BLOCKED:
         return input_driver_block_libretro_input;
      case RARCH_INPUT_CTL_SET_NONBLOCK_STATE:
         input_driver_nonblock_state = true;
         break;
      case RARCH_INPUT_CTL_UNSET_NONBLOCK_STATE:
         input_driver_nonblock_state = false;
         break;
      case RARCH_INPUT_CTL_IS_NONBLOCK_STATE:
         return input_driver_nonblock_state;
      case RARCH_INPUT_CTL_SET_OWN_DRIVER:
         input_driver_data_own = true;
         break;
      case RARCH_INPUT_CTL_UNSET_OWN_DRIVER:
         input_driver_data_own = false;
         break;
      case RARCH_INPUT_CTL_OWNS_DRIVER:
         return input_driver_data_own;
      case RARCH_INPUT_CTL_SET_OSK_ENABLED:
         input_driver_osk_enabled = true;
         break;
      case RARCH_INPUT_CTL_UNSET_OSK_ENABLED:
         input_driver_osk_enabled = false;
         break;
      case RARCH_INPUT_CTL_IS_OSK_ENABLED:
         return input_driver_osk_enabled;
      case RARCH_INPUT_CTL_SET_KEYBOARD_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = true;
         break;
      case RARCH_INPUT_CTL_UNSET_KEYBOARD_LINEFEED_ENABLED:
         input_driver_keyboard_linefeed_enable = false;
         break;
      case RARCH_INPUT_CTL_IS_KEYBOARD_LINEFEED_ENABLED:
         return input_driver_keyboard_linefeed_enable;
      case RARCH_INPUT_CTL_COMMAND_INIT:
#ifdef HAVE_COMMAND
         if (!settings->stdin_cmd_enable && !settings->network_cmd_enable)
            return false;

         if (settings->stdin_cmd_enable 
               && input_driver_ctl(RARCH_INPUT_CTL_GRAB_STDIN, NULL))
         {
            RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
                  "Cannot use this command interface.\n");
         }

         input_driver_command = rarch_cmd_new(settings->stdin_cmd_enable
               && !input_driver_ctl(RARCH_INPUT_CTL_GRAB_STDIN, NULL),
               settings->network_cmd_enable, settings->network_cmd_port);

         if (!input_driver_command)
         {
            RARCH_ERR("Failed to initialize command interface.\n");
            return false;
         }
#endif
         break;
      case RARCH_INPUT_CTL_COMMAND_DEINIT:
#ifdef HAVE_COMMAND
         if (input_driver_command)
            rarch_cmd_ctl(RARCH_CMD_CTL_FREE, input_driver_command);
         input_driver_command = NULL;
#endif
         break;
      case RARCH_INPUT_CTL_REMOTE_DEINIT:
#ifdef HAVE_NETWORK_GAMEPAD
         if (input_driver_remote)
            rarch_remote_free(input_driver_remote);
         input_driver_remote = NULL;
#endif
         break;
      case RARCH_INPUT_CTL_REMOTE_INIT:
#ifdef HAVE_NETWORK_GAMEPAD
         if (settings->network_remote_enable)
         {
            input_driver_remote 
               = rarch_remote_new(settings->network_remote_base_port);

            if (!input_driver_remote)
            {
               RARCH_ERR("Failed to initialize remote gamepad interface.\n");
               return false;
            }
         }
#endif
         break;
      case RARCH_INPUT_CTL_GRAB_MOUSE:
         {
            bool *bool_data = (bool*)data;
            if (!current_input || !current_input->grab_mouse)
               return false;

            current_input->grab_mouse(current_input_data, *bool_data);
         }
         break;
      case RARCH_INPUT_CTL_NONE:
      default:
         break;
   }

   return true;
}
