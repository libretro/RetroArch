/**
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andr�s Su�rez (input mapper code)
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 **/

#define _USE_MATH_DEFINES
#include <math.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <clamping.h>
#include <retro_endianness.h>

#include "input_driver.h"
#include "input_keymaps.h"
#include "input_remapping.h"
#include "input_osk.h"
#include "input_types.h"

#ifdef HAVE_CHEEVOS
#include "../cheevos/cheevos.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "../accessibility.h"
#include "../command.h"
#include "../config.def.keybinds.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../verbosity.h"
#include "../configuration.h"
#include "../list_special.h"
#include "../performance_counters.h"
#ifdef HAVE_BSV_MOVIE
#include "../tasks/task_content.h"
#endif
#include "../tasks/tasks_internal.h"

#define HOLD_BTN_DELAY_SEC 2

/* Depends on ASCII character values */
#define ISPRINT(c) (((int)(c) >= ' ' && (int)(c) <= '~') ? 1 : 0)

#define INPUT_REMOTE_KEY_PRESSED(input_st, key, port) (input_st->remote_st_ptr.buttons[(port)] & (UINT64_C(1) << (key)))

#define IS_COMPOSITION(c)       ( (c & 0x0F000000) ? 1 : 0)
#define IS_COMPOSITION_KR(c)    ( (c & 0x01000000) ? 1 : 0)
#define IS_END_COMPOSITION(c)   ( (c & 0xF0000000) ? 1 : 0)

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
#define CHECK_INPUT_DRIVER_BLOCK_HOTKEY(normal_bind, autoconf_bind) \
( \
         (((normal_bind)->key      != RETROK_UNKNOWN) \
      || ((normal_bind)->mbutton   != NO_BTN) \
      || ((normal_bind)->joykey    != NO_BTN) \
      || ((normal_bind)->joyaxis   != AXIS_NONE) \
      || ((autoconf_bind)->key     != RETROK_UNKNOWN) \
      || ((autoconf_bind)->joykey  != NO_BTN) \
      || ((autoconf_bind)->joyaxis != AXIS_NONE)) \
)

/* Human readable order of input binds */
const unsigned input_config_bind_order[24] = {
   RETRO_DEVICE_ID_JOYPAD_UP,
   RETRO_DEVICE_ID_JOYPAD_DOWN,
   RETRO_DEVICE_ID_JOYPAD_LEFT,
   RETRO_DEVICE_ID_JOYPAD_RIGHT,
   RETRO_DEVICE_ID_JOYPAD_B,
   RETRO_DEVICE_ID_JOYPAD_A,
   RETRO_DEVICE_ID_JOYPAD_Y,
   RETRO_DEVICE_ID_JOYPAD_X,
   RETRO_DEVICE_ID_JOYPAD_SELECT,
   RETRO_DEVICE_ID_JOYPAD_START,
   RETRO_DEVICE_ID_JOYPAD_L,
   RETRO_DEVICE_ID_JOYPAD_R,
   RETRO_DEVICE_ID_JOYPAD_L2,
   RETRO_DEVICE_ID_JOYPAD_R2,
   RETRO_DEVICE_ID_JOYPAD_L3,
   RETRO_DEVICE_ID_JOYPAD_R3,
   19, /* Left Analog Up */
   18, /* Left Analog Down */
   17, /* Left Analog Left */
   16, /* Left Analog Right */
   23, /* Right Analog Up */
   22, /* Right Analog Down */
   21, /* Right Analog Left */
   20, /* Right Analog Right */
};

/**************************************/
/* TODO/FIXME - turn these into static global variable */
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
uint64_t lifecycle_state                                        = 0;

static void *input_null_init(const char *joypad_driver) { return (void*)-1; }
static void input_null_poll(void *data) { }
static int16_t input_null_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *retro_keybinds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned index, unsigned id) { return 0; }
static void input_null_free(void *data) { }
static bool input_null_set_sensor_state(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate) { return false; }
static float input_null_get_sensor_input(void *data, unsigned port, unsigned id) { return 0.0; }
static uint64_t input_null_get_capabilities(void *data) { return 0; }
static void input_null_grab_mouse(void *data, bool state) { }
static bool input_null_grab_stdin(void *data) { return false; }
static void input_null_keypress_vibrate(void) { }

static input_driver_t input_null = {
   input_null_init,
   input_null_poll,
   input_null_input_state,
   input_null_free,
   input_null_set_sensor_state,
   input_null_get_sensor_input,
   input_null_get_capabilities,
   "null",
   input_null_grab_mouse,
   input_null_grab_stdin,
   input_null_keypress_vibrate
};

static input_device_driver_t null_joypad = {
   NULL, /* init */
   NULL, /* query_pad */
   NULL, /* destroy */
   NULL, /* button */
   NULL, /* state */
   NULL, /* get_buttons */
   NULL, /* axis */
   NULL, /* poll */
   NULL, /* rumble */
   NULL, /* rumble_gain */
   NULL, /* name */
   "null",
};


#ifdef HAVE_HID
static bool null_hid_joypad_query(void *data, unsigned pad) {
   return pad < MAX_USERS; }
static const char *null_hid_joypad_name(
      void *data, unsigned pad) { return NULL; }
static void null_hid_joypad_get_buttons(void *data,
      unsigned port, input_bits_t *state) { BIT256_CLEAR_ALL_PTR(state); }
static int16_t null_hid_joypad_button(
      void *data, unsigned port, uint16_t joykey) { return 0; }
static bool null_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength) { return false; }
static int16_t null_hid_joypad_axis(
      void *data, unsigned port, uint32_t joyaxis) { return 0; }
static void *null_hid_init(void) { return (void*)-1; }
static void null_hid_free(const void *data) { }
static void null_hid_poll(void *data) { }
static int16_t null_hid_joypad_state(
      void *data,
      rarch_joypad_info_t *joypad_info,
      const void *binds_data,
      unsigned port) { return 0; }

static hid_driver_t null_hid = {
   null_hid_init,               /* init */
   null_hid_joypad_query,       /* joypad_query */
   null_hid_free,               /* free */
   null_hid_joypad_button,      /* button */
   null_hid_joypad_state,       /* state */
   null_hid_joypad_get_buttons, /* get_buttons */
   null_hid_joypad_axis,        /* axis */
   null_hid_poll,               /* poll */
   null_hid_joypad_rumble,      /* rumble */
   null_hid_joypad_name,        /* joypad_name */
   "null",
};
#endif

input_device_driver_t *joypad_drivers[] = {
#ifdef HAVE_XINPUT
   &xinput_joypad,
#endif
#ifdef GEKKO
   &gx_joypad,
#endif
#ifdef WIIU
   &wiiu_joypad,
#endif
#ifdef _XBOX1
   &xdk_joypad,
#endif
#if defined(ORBIS)
   &ps4_joypad,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &ps3_joypad,
#endif
#if defined(PSP) || defined(VITA)
   &psp_joypad,
#endif
#if defined(PS2)
   &ps2_joypad,
#endif
#ifdef _3DS
   &ctr_joypad,
#endif
#ifdef SWITCH
   &switch_joypad,
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
#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
   &sdl_dingux_joypad,
#endif
#ifdef __QNX__
   &qnx_joypad,
#endif
#ifdef HAVE_MFI
   &mfi_joypad,
#endif
#ifdef DJGPP
   &dos_joypad,
#endif
/* Selecting the HID gamepad driver disables the Wii U gamepad. So while
 * we want the HID code to be compiled & linked, we don't want the driver
 * to be selectable in the UI. */
#if defined(HAVE_HID) && !defined(WIIU)
   &hid_joypad,
#endif
#ifdef EMSCRIPTEN
   &rwebpad_joypad,
#endif
   &null_joypad,
   NULL,
};

input_driver_t *input_drivers[] = {
#ifdef ORBIS
   &input_ps4,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP) || defined(VITA)
   &input_psp,
#endif
#if defined(PS2)
   &input_ps2,
#endif
#if defined(_3DS)
   &input_ctr,
#endif
#if defined(SWITCH)
   &input_switch,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef HAVE_WAYLAND
   &input_wayland,
#endif
#ifdef __WINRT__
   &input_uwp,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   &input_winraw,
#endif
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1) || defined(__WINRT__)
   &input_xinput,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &input_sdl,
#endif
#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
   &input_sdl_dingux,
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
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
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

#ifdef HAVE_HID
hid_driver_t *hid_drivers[] = {
#if defined(HAVE_BTSTACK)
   &btstack_hid,
#endif
#if defined(__APPLE__) && defined(HAVE_IOHIDMANAGER)
   &iohidmanager_hid,
#endif
#if defined(HAVE_LIBUSB) && defined(HAVE_THREADS)
   &libusb_hid,
#endif
#ifdef HW_RVL
   &wiiusb_hid,
#endif
#if defined(WIIU)
   &wiiu_hid,
#endif
   &null_hid,
   NULL,
};
#endif

static input_driver_state_t input_driver_st = {0}; /* double alignment */

/**************************************/

input_driver_state_t *input_state_get_ptr(void)
{
   return &input_driver_st;
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
 * Finds first suitable joypad driver and initializes. Used as a fallback by
 * input_joypad_init_driver when no matching driver is found.
 *
 * @param data  joypad state data pointer, which can be NULL and will be
 *              initialized by the new joypad driver, if one is found.
 *
 * @return joypad driver if found and initialized, otherwise NULL.
 **/
static const input_device_driver_t *input_joypad_init_first(void *data)
{
   int i;
   for (i = 0; joypad_drivers[i]; i++)
   {
      if (     joypad_drivers[i]
            && joypad_drivers[i]->init)
      {
         void *ptr = joypad_drivers[i]->init(data);
         if (ptr)
         {
            RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
                  joypad_drivers[i]->ident);
            return joypad_drivers[i];
         }
      }
   }

   return NULL;
}

bool input_driver_set_rumble(
         unsigned port, unsigned joy_idx,
         enum retro_rumble_effect effect, uint16_t strength)
{
   const input_device_driver_t  *primary_joypad;
   const input_device_driver_t      *sec_joypad;
   bool rumble_state   = false;

   if (joy_idx >= MAX_USERS)
      return false;

   primary_joypad = input_driver_st.primary_joypad;
   sec_joypad     = input_driver_st.secondary_joypad;

   if (primary_joypad && primary_joypad->set_rumble)
      rumble_state = primary_joypad->set_rumble(joy_idx, effect, strength);
   /* if sec_joypad exists, this set_rumble() return value will replace primary_joypad's return */
   if (sec_joypad     && sec_joypad->set_rumble)
      rumble_state = sec_joypad->set_rumble(joy_idx, effect, strength);

   return rumble_state;
}

bool input_driver_set_rumble_gain(
         unsigned gain,
         unsigned input_max_users)
{
   int i;

   if (  input_driver_st.primary_joypad
      && input_driver_st.primary_joypad->set_rumble_gain)
   {
      for (i = 0; i < (int)input_max_users; i++)
         input_driver_st.primary_joypad->set_rumble_gain(i, gain);
      return true;
   }
   return false;
}

bool input_driver_set_sensor(
         unsigned port, bool sensors_enable,
         enum retro_sensor_action action, unsigned rate)
{
   const input_driver_t *current_driver;

   if (!input_driver_st.current_data)
      return false;
   /* If sensors are disabled, inhibit any enable
    * actions (but always allow disable actions) */
   if (!sensors_enable &&
       (   (action == RETRO_SENSOR_ACCELEROMETER_ENABLE)
        || (action == RETRO_SENSOR_GYROSCOPE_ENABLE)
        || (action == RETRO_SENSOR_ILLUMINANCE_ENABLE)))
      return false;
   if (   (current_driver = input_driver_st.current_driver)
       &&  current_driver->set_sensor_state)
   {
      void *current_data = input_driver_st.current_data;
      return current_driver->set_sensor_state(current_data,
            port, action, rate);
   }

   return false;
}

/**************************************/

float input_driver_get_sensor(
         unsigned port, bool sensors_enable, unsigned id)
{
   if (input_driver_st.current_data)
   {
      const input_driver_t *current_driver = input_driver_st.current_driver;
      if (sensors_enable && current_driver->get_sensor_input)
      {
         void *current_data = input_driver_st.current_data;
         return current_driver->get_sensor_input(current_data, port, id);
      }
   }

   return 0.0f;
}

const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data)
{
   if (ident && *ident)
   {
      int i;
      for (i = 0; joypad_drivers[i]; i++)
      {
         if (string_is_equal(ident, joypad_drivers[i]->ident)
               && joypad_drivers[i]->init)
         {
            void *ptr = joypad_drivers[i]->init(data);
            if (ptr)
            {
               RARCH_LOG("[Joypad]: Found joypad driver: \"%s\".\n",
                     joypad_drivers[i]->ident);
               return joypad_drivers[i];
            }
         }
      }
   }

   return input_joypad_init_first(data); /* fall back to first available driver */
}

static bool input_driver_button_combo_hold(
      unsigned mode,
      unsigned button,
      retro_time_t current_time,
      input_bits_t *p_input)
{
   rarch_timer_t *timer           = &input_driver_st.combo_timers[mode];
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   static bool enable_hotkey_dupe = false;

   /* Flag current press when button and 'enable_hotkey' are the same,
    * because 'input_hotkey_block_delay' clears the combo button bit. */
   if (     BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
         && BIT256_GET_PTR(p_input, button))
      enable_hotkey_dupe = true;

   /* Ignore press if 'enable_hotkey' is not the combo button. */
   if (      BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
         && !BIT256_GET_PTR(p_input, button)
         && !enable_hotkey_dupe)
      return false;

   /* Allow using the same button for 'enable_hotkey' if set,
    * and stop timer if holding fast-forward or slow-motion */
   if (     !BIT256_GET_PTR(p_input, button)
         && !( BIT256_GET_PTR(p_input, RARCH_ENABLE_HOTKEY)
            && !(input_driver_st.flags & INP_FLAG_BLOCK_HOTKEY)
            && !(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
            && !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
      )
   {
      /* Timer only runs while start is held down */
      enable_hotkey_dupe = false;
      timer->timer_begin = false;
      timer->timer_end   = true;
      timer->timeout_end = 0;
      return false;
   }

   /* User started holding down the start button, start the timer */
   if (!timer->timer_begin)
   {
      timer->timeout_us     = HOLD_BTN_DELAY_SEC * 1000000;
      timer->timeout_end    = current_time + timer->timeout_us;
      timer->timer_begin    = true;
      timer->timer_end      = false;
   }

   timer->current           = current_time;
   timer->timeout_us        = (timer->timeout_end - timer->current);

   if (!timer->timer_end && (timer->timeout_us <= 0))
   {
      /* Start has been held down long enough,
       * stop timer and enter menu */
      enable_hotkey_dupe = false;
      timer->timer_begin = false;
      timer->timer_end   = true;
      timer->timeout_end = 0;
      return true;
   }

   return false;
}

bool input_driver_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t *p_input)
{
   switch (mode)
   {
      case INPUT_COMBO_DOWN_Y_L_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L3_R3:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return true;
         break;
      case INPUT_COMBO_L1_R1_START_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_START_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L3_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L_R:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_DOWN_SELECT:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L2_R2:
         if (   BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L2)
             && BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R2))
            return true;
         break;
      case INPUT_COMBO_HOLD_START:
         return input_driver_button_combo_hold(
               INPUT_COMBO_HOLD_START, RETRO_DEVICE_ID_JOYPAD_START, current_time, p_input);
      case INPUT_COMBO_HOLD_SELECT:
         return input_driver_button_combo_hold(
               INPUT_COMBO_HOLD_SELECT, RETRO_DEVICE_ID_JOYPAD_SELECT, current_time, p_input);
      default:
      case INPUT_COMBO_NONE:
         break;
   }

   return false;
}

static int16_t input_state_wrap(
      input_driver_t *current_input,
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned _port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int16_t ret                   = 0;

   if (!binds)
      return 0;

   /* Do a bitwise OR to combine input states together */

   if (device == RETRO_DEVICE_JOYPAD)
   {
      if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
      {
         if (joypad)
            ret                    |= joypad->state(
                  joypad_info, binds[_port], _port);
         if (sec_joypad)
            ret                    |= sec_joypad->state(
                  joypad_info, binds[_port], _port);
      }
      else
      {
         /* Do a bitwise OR to combine both input
          * states together */
         if (binds[_port][id].valid)
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t bind_joykey     = binds[_port][id].joykey;
            const uint64_t bind_joyaxis    = binds[_port][id].joyaxis;
            const uint64_t autobind_joykey = joypad_info->auto_binds[id].joykey;
            const uint64_t autobind_joyaxis= joypad_info->auto_binds[id].joyaxis;
            uint16_t port                  = joypad_info->joy_idx;
            float axis_threshold           = joypad_info->axis_threshold;
            const uint64_t joykey          = (bind_joykey != NO_BTN)
               ? bind_joykey  : autobind_joykey;
            const uint64_t joyaxis         = (bind_joyaxis != AXIS_NONE)
               ? bind_joyaxis : autobind_joyaxis;

            if (joypad)
            {
               if ((uint16_t)joykey != NO_BTN && joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(joypad->axis(port, (uint32_t)joyaxis))
                      / 0x8000) > axis_threshold)
                  return 1;
            }
            if (sec_joypad)
            {
               if ((uint16_t)joykey != NO_BTN && sec_joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(sec_joypad->axis(port, (uint32_t)joyaxis))
                      / 0x8000) > axis_threshold)
                  return 1;
            }
         }
      }
   }

   if (current_input && current_input->input_state)
      ret |= current_input->input_state(
            data,
            joypad,
            sec_joypad,
            joypad_info,
            binds,
            keyboard_mapping_blocked,
            _port,
            device,
            idx,
            id);
   return ret;
}

static int16_t input_joypad_axis(
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      unsigned port, uint32_t joyaxis, float normal_mag)
{
   int16_t val                    = (joyaxis != AXIS_NONE) ? drv->axis(port, joyaxis) : 0;

   if (input_analog_deadzone)
   {
      /* if analog value is below the deadzone, ignore it
       * normal magnitude is calculated radially for analog sticks
       * and linearly for analog buttons */
      if (normal_mag <= input_analog_deadzone)
         return 0;

      /* due to the way normal_mag is calculated differently for buttons and
       * sticks, this results in either a radial scaled deadzone for sticks
       * or linear scaled deadzone for analog buttons */
      val = val * MAX(1.0f,(1.0f / normal_mag)) * MIN(1.0f,
            ((normal_mag - input_analog_deadzone)
          / (1.0f - input_analog_deadzone)));
   }

   if (input_analog_sensitivity != 1.0f)
   {
      float normalized = (1.0f / 0x7fff) * val;
      int      new_val = 0x7fff * normalized  *
         input_analog_sensitivity;

      if (new_val > 0x7fff)
         return 0x7fff;
      else if (new_val < -0x7fff)
         return -0x7fff;

      return new_val;
   }

   return val;
}

/**
 * input_joypad_analog_button:
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
static int16_t input_joypad_analog_button(
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned ident,
      const struct retro_keybind *bind)
{
   int16_t res                      = 0;
   float normal_mag                 = 0.0f;
   uint32_t axis                    = (bind->joyaxis == AXIS_NONE)
      ? joypad_info->auto_binds[ident].joyaxis
      : bind->joyaxis;

   /* Analog button. */
   if (input_analog_deadzone)
   {
      int16_t mult = 0;
      if (axis != AXIS_NONE)
         if ((mult = drv->axis(
                     joypad_info->joy_idx, axis)) != 0)
            normal_mag   = fabs((1.0f / 0x7fff) * mult);
   }

   /* If the result is zero, it's got a digital button
    * attached to it instead */
   if ((res = abs(input_joypad_axis(
            input_analog_deadzone,
            input_analog_sensitivity,
            drv,
            joypad_info->joy_idx, axis, normal_mag))) == 0)
   {
      uint16_t key = (bind->joykey == NO_BTN)
         ? joypad_info->auto_binds[ident].joykey
         : bind->joykey;

      if (drv->button(joypad_info->joy_idx, key))
         return 0x7fff;
      return 0;
   }

   return res;
}

static int16_t input_joypad_analog_axis(
      unsigned input_analog_dpad_mode,
      float input_analog_deadzone,
      float input_analog_sensitivity,
      const input_device_driver_t *drv,
      rarch_joypad_info_t *joypad_info,
      unsigned idx,
      unsigned ident,
      const struct retro_keybind *binds)
{
   int16_t res                              = 0;
   /* Analog sticks. Either RETRO_DEVICE_INDEX_ANALOG_LEFT
    * or RETRO_DEVICE_INDEX_ANALOG_RIGHT */
   unsigned ident_minus                     = 0;
   unsigned ident_plus                      = 0;
   unsigned ident_x_minus                   = 0;
   unsigned ident_x_plus                    = 0;
   unsigned ident_y_minus                   = 0;
   unsigned ident_y_plus                    = 0;
   const struct retro_keybind *bind_minus   = NULL;
   const struct retro_keybind *bind_plus    = NULL;
   const struct retro_keybind *bind_x_minus = NULL;
   const struct retro_keybind *bind_x_plus  = NULL;
   const struct retro_keybind *bind_y_minus = NULL;
   const struct retro_keybind *bind_y_plus  = NULL;

   /* Skip analog input with analog_dpad_mode */
   switch (input_analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_LEFT)
            return 0;
         break;
      case ANALOG_DPAD_RSTICK:
         if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            return 0;
         break;
      default:
         break;
   }

   input_conv_analog_id_to_bind_id(idx, ident, ident_minus, ident_plus);

   bind_minus                             = &binds[ident_minus];
   bind_plus                              = &binds[ident_plus];

   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_X, ident_x_minus, ident_x_plus);

   bind_x_minus = &binds[ident_x_minus];
   bind_x_plus  = &binds[ident_x_plus];

   if (!bind_x_minus->valid || !bind_x_plus->valid)
      return 0;

   input_conv_analog_id_to_bind_id(idx,
         RETRO_DEVICE_ID_ANALOG_Y, ident_y_minus, ident_y_plus);

   bind_y_minus = &binds[ident_y_minus];
   bind_y_plus  = &binds[ident_y_plus];

   if (!bind_y_minus->valid || !bind_y_plus->valid)
      return 0;

   {
      uint32_t axis_minus            = (bind_minus->joyaxis   == AXIS_NONE)
         ? joypad_info->auto_binds[ident_minus].joyaxis
         : bind_minus->joyaxis;
      uint32_t axis_plus             = (bind_plus->joyaxis    == AXIS_NONE)
         ? joypad_info->auto_binds[ident_plus].joyaxis
         : bind_plus->joyaxis;
      float normal_mag               = 0.0f;

      /* normalized magnitude of stick actuation, needed for scaled
       * radial deadzone */
      if (input_analog_deadzone)
      {
         float x                  = 0.0f;
         float y                  = 0.0f;
         uint32_t x_axis_minus    = (bind_x_minus->joyaxis == AXIS_NONE)
            ? joypad_info->auto_binds[ident_x_minus].joyaxis
            : bind_x_minus->joyaxis;
         uint32_t x_axis_plus     = (bind_x_plus->joyaxis  == AXIS_NONE)
            ? joypad_info->auto_binds[ident_x_plus].joyaxis
            : bind_x_plus->joyaxis;
         uint32_t y_axis_minus    = (bind_y_minus->joyaxis == AXIS_NONE)
            ? joypad_info->auto_binds[ident_y_minus].joyaxis
            : bind_y_minus->joyaxis;
         uint32_t y_axis_plus     = (bind_y_plus->joyaxis  == AXIS_NONE)
            ? joypad_info->auto_binds[ident_y_plus].joyaxis
            : bind_y_plus->joyaxis;
         /* normalized magnitude for radial scaled analog deadzone */
         if (x_axis_plus != AXIS_NONE)
            x                     = drv->axis(
                  joypad_info->joy_idx, x_axis_plus);
         if (x_axis_minus != AXIS_NONE)
            x                    += drv->axis(joypad_info->joy_idx,
                  x_axis_minus);
         if (y_axis_plus != AXIS_NONE)
            y                     = drv->axis(
                  joypad_info->joy_idx, y_axis_plus);
         if (y_axis_minus != AXIS_NONE)
            y                    += drv->axis(
                  joypad_info->joy_idx, y_axis_minus);
         normal_mag               = (1.0f / 0x7fff) * sqrt(x * x + y * y);
      }

      res           = abs(
            input_joypad_axis(
               input_analog_deadzone,
               input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               axis_plus, normal_mag));
      res          -= abs(
            input_joypad_axis(
               input_analog_deadzone,
               input_analog_sensitivity,
               drv, joypad_info->joy_idx,
               axis_minus, normal_mag));
   }

   if (res == 0)
   {
      uint16_t key_minus    = (bind_minus->joykey == NO_BTN)
         ? joypad_info->auto_binds[ident_minus].joykey
         : bind_minus->joykey;
      uint16_t key_plus     = (bind_plus->joykey  == NO_BTN)
         ? joypad_info->auto_binds[ident_plus].joykey
         : bind_plus->joykey;
      if (drv->button(joypad_info->joy_idx, key_plus))
         res  = 0x7fff;
      if (drv->button(joypad_info->joy_idx, key_minus))
         res += -0x7fff;
   }

   return res;
}

void input_keyboard_line_append(
      struct input_keyboard_line *keyboard_line,
      const char *word, size_t len)
{
   size_t i;
   char *newbuf                = (char*)realloc(
         keyboard_line->buffer,
         keyboard_line->size + len * 2);

   if (!newbuf)
      return;

   memmove(
         newbuf + keyboard_line->ptr + len,
         newbuf + keyboard_line->ptr,
         keyboard_line->size - keyboard_line->ptr + len);

   for (i = 0; i < len; i++)
   {
      newbuf[keyboard_line->ptr]= word[i];
      keyboard_line->ptr++;
      keyboard_line->size++;
   }

   newbuf[keyboard_line->size]  = '\0';

   keyboard_line->buffer        = newbuf;
}

void input_keyboard_line_clear(input_driver_state_t *input_st)
{
   if (input_st->keyboard_line.buffer)
      free(input_st->keyboard_line.buffer);
   input_st->keyboard_line.buffer       = NULL;
   input_st->keyboard_line.ptr          = 0;
   input_st->keyboard_line.size         = 0;
}

void input_keyboard_line_free(input_driver_state_t *input_st)
{
   if (input_st->keyboard_line.buffer)
      free(input_st->keyboard_line.buffer);
   input_st->keyboard_line.buffer       = NULL;
   input_st->keyboard_line.ptr          = 0;
   input_st->keyboard_line.size         = 0;
   input_st->keyboard_line.cb           = NULL;
   input_st->keyboard_line.userdata     = NULL;
   input_st->keyboard_line.enabled      = false;
}

const char **input_keyboard_start_line(
      void *userdata,
      struct input_keyboard_line *keyboard_line,
      input_keyboard_line_complete_t cb)
{
   keyboard_line->buffer    = NULL;
   keyboard_line->ptr       = 0;
   keyboard_line->size      = 0;
   keyboard_line->cb        = cb;
   keyboard_line->userdata  = userdata;
   keyboard_line->enabled   = true;

   return (const char**)&keyboard_line->buffer;
}

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
static bool input_remote_init_network(input_remote_t *handle,
      uint16_t port, unsigned user)
{
   int fd;
   struct addrinfo *res  = NULL;
   port                  = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   if ((fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM, AF_INET)) < 0)
      goto error;

   handle->net_fd[user] = fd;

   if (!socket_nonblock(handle->net_fd[user]))
      goto error;

   if (!socket_bind(handle->net_fd[user], res))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}

void input_remote_free(input_remote_t *handle, unsigned max_users)
{
   int user;
   for (user = 0; user < (int)max_users; user ++)
      socket_close(handle->net_fd[user]);
   free(handle);
}

static input_remote_t *input_remote_new(
      settings_t *settings,
      uint16_t port, unsigned max_users)
{
   int user;
   input_remote_t      *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   for (user = 0; user < (int)max_users; user++)
   {
      handle->net_fd[user] = -1;
      if (settings->bools.network_remote_enable_user[user])
         if (!input_remote_init_network(handle, port, user))
         {
            input_remote_free(handle, max_users);
            return NULL;
         }
   }

   return handle;
}

static void input_remote_parse_packet(
      input_remote_state_t *input_state,
      struct remote_message *msg, unsigned user)
{
   /* Parse message */
   switch (msg->device)
   {
      case RETRO_DEVICE_JOYPAD:
         input_state->buttons[user] &= ~(1 << msg->id);
         if (msg->state)
            input_state->buttons[user] |= 1 << msg->id;
         break;
      case RETRO_DEVICE_ANALOG:
         input_state->analog[msg->index * 2 + msg->id][user] = msg->state;
         break;
   }
}

input_remote_t *input_driver_init_remote(
      settings_t *settings,
      unsigned num_active_users)
{
   unsigned network_remote_base_port = settings->uints.network_remote_base_port;
   return input_remote_new(
         settings,
         network_remote_base_port,
         num_active_users);
}
#endif

static int16_t input_state_device(
      input_driver_state_t *input_st,
      settings_t *settings,
      input_mapper_t *handle,
      unsigned input_analog_dpad_mode,
      int16_t ret,
      unsigned port, unsigned device,
      unsigned idx, unsigned id,
      bool button_mask)
{
   int16_t res  = 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:

         if (id < RARCH_FIRST_META_KEY)
         {
#ifdef HAVE_NETWORKGAMEPAD
            /* Don't process binds if input is coming from Remote RetroPad */
            if (     input_st->remote
                  && INPUT_REMOTE_KEY_PRESSED(input_st, id, port))
               res |= 1;
            else
#endif
            {
               bool bind_valid       = input_st->libretro_input_binds[port]
                  && (*input_st->libretro_input_binds[port])[id].valid;
               unsigned remap_button = settings->uints.input_remap_ids[port][id];

               /* TODO/FIXME: What on earth is this code doing...? */
               if (!(bind_valid && (id != remap_button)))
               {
                  if (button_mask)
                  {
                     if (ret & (1 << id))
                        res |= (1 << id);
                  }
                  else
                     res = ret;
               }

               if (BIT256_GET(handle->buttons[port], id))
                  res = 1;

#ifdef HAVE_OVERLAY
               /* Check if overlay is active and button
                * corresponding to 'id' has been pressed */
               if (  (port == 0)
                   && input_st->overlay_ptr
                   && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE)
                   && BIT256_GET(input_st->overlay_ptr->overlay_state.buttons, id))
               {
#ifdef HAVE_MENU
                  bool menu_driver_alive        = (menu_state_get_ptr()->flags &
                        MENU_ST_FLAG_ALIVE) ? true : false;
#else
                  bool menu_driver_alive        = false;
#endif
                  bool input_remap_binds_enable = settings->bools.input_remap_binds_enable;

                  /* This button has already been processed
                   * inside input_driver_poll() if all the
                   * following are true:
                   * > Menu driver is not running
                   * > Input remaps are enabled
                   * > 'id' is not equal to remapped button index
                   * If these conditions are met, input here
                   * is ignored */
                  if ((menu_driver_alive || !input_remap_binds_enable) ||
                      (id == remap_button))
                     res |= 1;
               }
#endif
            }

            /* Don't allow turbo for D-pad. */
            if (          (id  < RETRO_DEVICE_ID_JOYPAD_UP)
                  || (    (id  > RETRO_DEVICE_ID_JOYPAD_RIGHT)
                       && (id <= RETRO_DEVICE_ID_JOYPAD_R3)))
            {
               /*
                * Apply turbo button if activated.
                */
               unsigned turbo_mode = settings->uints.input_turbo_mode;

               if (turbo_mode > INPUT_TURBO_MODE_CLASSIC)
               {
                  /* Pressing turbo button toggles turbo mode on or off.
                   * Holding the button will
                   * pass through, else the pressed state will be modulated by a
                   * periodic pulse defined by the configured duty cycle.
                   */

                  /* Avoid detecting the turbo button being held as multiple toggles */
                  if (!input_st->turbo_btns.frame_enable[port])
                     input_st->turbo_btns.turbo_pressed[port] &= ~(1 << 31);
                  else if (input_st->turbo_btns.turbo_pressed[port] >= 0)
                  {
                     input_st->turbo_btns.turbo_pressed[port] |= (1 << 31);
                     /* Toggle turbo for selected buttons. */
                     if (input_st->turbo_btns.enable[port]
                           != (1 << settings->uints.input_turbo_default_button))
                     {
                        static const int button_map[]={
                           RETRO_DEVICE_ID_JOYPAD_B,
                           RETRO_DEVICE_ID_JOYPAD_Y,
                           RETRO_DEVICE_ID_JOYPAD_A,
                           RETRO_DEVICE_ID_JOYPAD_X,
                           RETRO_DEVICE_ID_JOYPAD_L,
                           RETRO_DEVICE_ID_JOYPAD_R,
                           RETRO_DEVICE_ID_JOYPAD_L2,
                           RETRO_DEVICE_ID_JOYPAD_R2,
                           RETRO_DEVICE_ID_JOYPAD_L3,
                           RETRO_DEVICE_ID_JOYPAD_R3};
                        input_st->turbo_btns.enable[port] = 1 << button_map[
                           MIN(
                                 ARRAY_SIZE(button_map) - 1,
                                 settings->uints.input_turbo_default_button)];
                     }
                     input_st->turbo_btns.mode1_enable[port] ^= 1;
                  }

                  if (input_st->turbo_btns.turbo_pressed[port] & (1 << 31))
                  {
                     /* Avoid detecting buttons being held as multiple toggles */
                     if (!res)
                        input_st->turbo_btns.turbo_pressed[port] &= ~(1 << id);
                     else if (!(input_st->turbo_btns.turbo_pressed[port] & (1 << id))
                           && turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON)
                     {
                        uint16_t enable_new;
                        input_st->turbo_btns.turbo_pressed[port] |= 1 << id;
                        /* Toggle turbo for pressed button but make
                         * sure at least one button has turbo */
                        enable_new = input_st->turbo_btns.enable[port] ^ (1 << id);
                        if (enable_new)
                           input_st->turbo_btns.enable[port] = enable_new;
                     }
                  }
                  /* Hold mode stops turbo on release */
                  else if (
                           turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON_HOLD
                        && input_st->turbo_btns.enable[port]
                        && input_st->turbo_btns.mode1_enable[port])
                     input_st->turbo_btns.mode1_enable[port] = 0;

                  if (!res && input_st->turbo_btns.mode1_enable[port] &&
                        input_st->turbo_btns.enable[port] & (1 << id))
                  {
                     /* If turbo button is enabled for this key ID */
                     res = ((   input_st->turbo_btns.count
                              % settings->uints.input_turbo_period)
                           < settings->uints.input_turbo_duty_cycle);
                  }
               }
               else
               {
                  /* If turbo button is held, all buttons pressed except
                   * for D-pad will go into a turbo mode. Until the button is
                   * released again, the input state will be modulated by a
                   * periodic pulse defined by the configured duty cycle.
                   */
                  if (res)
                  {
                     if (input_st->turbo_btns.frame_enable[port])
                        input_st->turbo_btns.enable[port] |= (1 << id);

                     if (input_st->turbo_btns.enable[port] & (1 << id))
                        /* if turbo button is enabled for this key ID */
                        res = ((input_st->turbo_btns.count
                                 % settings->uints.input_turbo_period)
                              < settings->uints.input_turbo_duty_cycle);
                  }
                  else
                     input_st->turbo_btns.enable[port] &= ~(1 << id);
               }
            }
         }

         break;


      case RETRO_DEVICE_KEYBOARD:

         res = ret;

         if (id < RETROK_LAST)
         {
#ifdef HAVE_OVERLAY
            if (port == 0)
            {
               if (input_st->overlay_ptr
                     && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE))
               {
                  input_overlay_state_t
                     *ol_state          = &input_st->overlay_ptr->overlay_state;

                  if (OVERLAY_GET_KEY(ol_state, id))
                     res               |= 1;
               }
            }
#endif
            if (MAPPER_GET_KEY(handle, id))
               res |= 1;
         }

         break;


      case RETRO_DEVICE_ANALOG:
         {
#if defined(HAVE_NETWORKGAMEPAD) || defined(HAVE_OVERLAY)
#ifdef HAVE_NETWORKGAMEPAD
            input_remote_state_t
               *input_state         = &input_st->remote_st_ptr;

#endif
            unsigned base           = (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
               ? 2 : 0;
            if (id == RETRO_DEVICE_ID_ANALOG_Y)
               base += 1;
#ifdef HAVE_NETWORKGAMEPAD
            if (     input_st->remote
                  && input_state && input_state->analog[base][port])
               res          = input_state->analog[base][port];
            else
#endif
#endif
            {
               if (id < RARCH_FIRST_META_KEY)
               {
                  bool bind_valid         = input_st->libretro_input_binds[port]
                     && (*input_st->libretro_input_binds[port])[id].valid;

                  if (bind_valid)
                  {
                     /* reset_state - used to reset input state of a button
                      * when the gamepad mapper is in action for that button*/
                     bool reset_state        = false;
                     if (idx < 2 && id < 2)
                     {
                        unsigned offset = RARCH_FIRST_CUSTOM_BIND +
                           (idx * 4) + (id * 2);

                        if (settings->uints.input_remap_ids[port][offset] != offset)
                           reset_state = true;
                        else if (settings->uints.input_remap_ids[port][offset + 1] != (offset+1))
                           reset_state = true;
                     }

                     if (reset_state)
                        res = 0;
                     else
                     {
                        res = ret;

#ifdef HAVE_OVERLAY
                        if (   (input_st->overlay_ptr)
                            && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE)
                            && (port == 0)
                            && (idx != RETRO_DEVICE_INDEX_ANALOG_BUTTON)
                            && !(((input_analog_dpad_mode == ANALOG_DPAD_LSTICK)
                            &&   (idx == RETRO_DEVICE_INDEX_ANALOG_LEFT))
                            || ((input_analog_dpad_mode == ANALOG_DPAD_RSTICK)
                            &&   (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT))))
                        {
                           input_overlay_state_t *ol_state =
                              &input_st->overlay_ptr->overlay_state;
                           int16_t ol_analog               =
                                 ol_state->analog[base];

                           /* Analog values are an integer corresponding
                            * to the extent of the analog motion; these
                            * cannot be OR'd together, we must instead
                            * keep the value with the largest magnitude */
                           if (ol_analog)
                           {
                              if (res == 0)
                                 res = ol_analog;
                              else
                              {
                                 int16_t ol_analog_abs = (ol_analog >= 0) ?
                                       ol_analog : -ol_analog;
                                 int16_t res_abs       = (res >= 0) ?
                                       res : -res;

                                 res = (ol_analog_abs > res_abs) ?
                                       ol_analog : res;
                              }
                           }
                        }
#endif
                     }
                  }
               }
            }

            if (idx < 2 && id < 2)
            {
               unsigned offset = 0 + (idx * 4) + (id * 2);
               int        val1 = handle->analog_value[port][offset];
               int        val2 = handle->analog_value[port][offset+1];

               /* OR'ing these analog values is 100% incorrect,
                * but I have no idea what this code is supposed
                * to be doing (val1 and val2 always seem to be
                * zero), so I will leave it alone... */
               if (val1)
                  res          |= val1;
               else if (val2)
                  res          |= val2;
            }
         }
         break;

      case RETRO_DEVICE_MOUSE:
      case RETRO_DEVICE_LIGHTGUN:
      case RETRO_DEVICE_POINTER:

         if (input_st->flags & INP_FLAG_BLOCK_POINTER_INPUT)
            break;

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = input_st->libretro_input_binds[port]
               && (*input_st->libretro_input_binds[port])[id].valid;

            if (bind_valid)
            {
               if (button_mask)
               {
                  if (ret & (1 << id))
                     res |= (1 << id);
               }
               else
                  res = ret;
            }
         }

         break;
   }

   return res;
}


static int16_t input_state_internal(
      input_driver_state_t *input_st,
      settings_t *settings,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   unsigned mapped_port;
   float input_analog_deadzone             = settings->floats.input_analog_deadzone;
   float input_analog_sensitivity          = settings->floats.input_analog_sensitivity;
   unsigned *input_remap_port_map          = settings->uints.input_remap_port_map[port];
   bool input_driver_analog_requested      = input_st->analog_requested[port];
   const input_device_driver_t *joypad     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t *sec_joypad = input_st->secondary_joypad;
#else
   const input_device_driver_t *sec_joypad = NULL;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st              = menu_state_get_ptr();
   bool input_blocked                      =    (menu_st->input_driver_flushing_input > 0)
                                             || (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT);
#else
   bool input_blocked                      = (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT) ? true : false;
#endif
   bool bitmask_enabled                    = false;
   unsigned max_users                      = settings->uints.input_max_users;
   int16_t result                          = 0;

   device                                 &= RETRO_DEVICE_MASK;
   bitmask_enabled                         =    (device == RETRO_DEVICE_JOYPAD)
                                             && (id == RETRO_DEVICE_ID_JOYPAD_MASK);
   joypad_info.axis_threshold              = settings->floats.input_axis_threshold;

   /* Loop over all 'physical' ports mapped to specified
    * 'virtual' port index */
   while ((mapped_port = *(input_remap_port_map++)) < MAX_USERS)
   {
      int16_t ret                     = 0;
      int16_t port_result             = 0;
      unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[mapped_port];

      joypad_info.joy_idx             = settings->uints.input_joypad_index[mapped_port];
      joypad_info.auto_binds          = input_autoconf_binds[joypad_info.joy_idx];

      /* Skip disabled input devices */
      if (mapped_port >= max_users)
         continue;

      /* If core has requested analog input, disable
       * analog to dpad mapping (unless forced) */
      switch (input_analog_dpad_mode)
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
            if (input_driver_analog_requested)
               input_analog_dpad_mode = ANALOG_DPAD_NONE;
            break;
         case ANALOG_DPAD_LSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_LSTICK;
            break;
         case ANALOG_DPAD_RSTICK_FORCED:
            input_analog_dpad_mode = ANALOG_DPAD_RSTICK;
            break;
         default:
            break;
      }

      /* TODO/FIXME: This code is gibberish - a mess of nested
       * refactors that make no sense whatsoever. The entire
       * thing needs to be rewritten from scratch... */

      ret = input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            joypad,
            sec_joypad,
            &joypad_info,
            (*input_st->libretro_input_binds),
            (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
            mapped_port, device, idx, id);

      if (   (device == RETRO_DEVICE_ANALOG)
          && (ret == 0))
      {
         if (input_st->libretro_input_binds[mapped_port])
         {
            if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
            {
               if (id < RARCH_FIRST_CUSTOM_BIND)
               {
                  /* TODO/FIXME: Analog buttons can only be read as analog
                   * when the default mapping is applied. If the user
                   * remaps any analog buttons, they will become 'digital'
                   * due to the way that mapping is handled elsewhere. We
                   * cannot fix this without rewriting the entire mess that
                   * is the input remapping system... */
                  bool valid_bind = (*input_st->libretro_input_binds[mapped_port])[id].valid &&
                        (id == settings->uints.input_remap_ids[mapped_port][id]);

                  if (valid_bind)
                  {
                     if (sec_joypad)
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              sec_joypad, &joypad_info,
                              id,
                              &(*input_st->libretro_input_binds[mapped_port])[id]);

                     if (joypad && (ret == 0))
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              joypad, &joypad_info,
                              id,
                              &(*input_st->libretro_input_binds[mapped_port])[id]);
                  }
               }
            }
            else
            {
               if (sec_joypad)
                  ret = input_joypad_analog_axis(
                        input_analog_dpad_mode,
                        input_analog_deadzone,
                        input_analog_sensitivity,
                        sec_joypad,
                        &joypad_info,
                        idx,
                        id,
                        (*input_st->libretro_input_binds[mapped_port]));

               if (joypad && (ret == 0))
                  ret = input_joypad_analog_axis(
                        input_analog_dpad_mode,
                        input_analog_deadzone,
                        input_analog_sensitivity,
                        joypad,
                        &joypad_info,
                        idx,
                        id,
                        (*input_st->libretro_input_binds[mapped_port]));
            }
         }
      }

      if (!input_blocked)
      {
         input_mapper_t *handle = &input_st->mapper;

         if (bitmask_enabled)
         {
            unsigned i;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               if (input_state_device(input_st,
                        settings, handle,
                        input_analog_dpad_mode, ret, mapped_port,
                        device, idx, i, true))
                  port_result |= (1 << i);
         }
         else
            port_result = input_state_device(input_st,
                  settings, handle,
                  input_analog_dpad_mode, ret, mapped_port,
                  device, idx, id, false);
      }

      /* Digital values are represented by a bitmap;
       * we can just perform the logical OR of
       * successive samples.
       * Analog values are an integer corresponding
       * to the extent of the analog motion; these
       * cannot be OR'd together, we must instead
       * keep the value with the largest magnitude */
      if (device == RETRO_DEVICE_ANALOG)
      {
         if (result == 0)
            result = port_result;
         else
         {
            int16_t port_result_abs = (port_result >= 0)
               ? port_result : -port_result;
            int16_t result_abs      = (result >= 0)
               ? result      : -result;

            if (port_result_abs > result_abs)
               result = port_result;
         }
      }
      else
         result |= port_result;
   }

   return result;
}


#ifdef HAVE_OVERLAY
/**
 * input_overlay_add_inputs:
 * @desc : pointer to overlay description
 * @ol_state : pointer to overlay state. If valid, inputs
 *             that are actually 'touched' on the overlay
 *             itself will displayed. If NULL, inputs from
 *             the device connected to 'port' will be displayed.
 * @port : when ol_state is NULL, specifies the port of
 *         the input device from which input will be
 *         displayed.
 *
 * Adds inputs from current_input to the overlay, so it's displayed
 * @return true if an input that is pressed will change the overlay
 */
static bool input_overlay_add_inputs_inner(overlay_desc_t *desc,
      input_driver_state_t *input_st,
      settings_t *settings,
      input_overlay_state_t *ol_state, unsigned port)
{
   switch(desc->type)
   {
      case OVERLAY_TYPE_BUTTONS:
         {
            int i;

            /* Check custom binds in the mask */
            for (i = 0; i < CUSTOM_BINDS_U32_COUNT; ++i)
            {
               /* Get bank */
               uint32_t bank_mask = BITS_GET_ELEM(desc->button_mask,i);
               unsigned        id = i * 32;

               /* Worth pursuing? Have we got any bits left in here? */
               while (bank_mask)
               {
                  /* If this bit is set then we need to query the pad
                   * The button must be pressed.*/
                  if (bank_mask & 1)
                  {
                     if (id >= RARCH_CUSTOM_BIND_LIST_END)
                        break;

                     /* Light up the button if pressed */
                     if (     ol_state
                           ? !BIT256_GET(ol_state->buttons, id)
                           : !input_state_internal(input_st, settings, port, RETRO_DEVICE_JOYPAD, 0, id))
                     {
                        /* We need ALL of the inputs to be active,
                         * abort. */
                        desc->touch_mask = 0;
                        return false;
                     }

                     desc->touch_mask   |= (1 << OVERLAY_MAX_TOUCH);
                  }

                  bank_mask >>= 1;
                  ++id;
               }
            }

            return (desc->touch_mask != 0);
         }

      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         if (ol_state)
         {
            unsigned index_offset = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;
            desc->touch_mask     |= (
                   ol_state->analog[index_offset]
                 | ol_state->analog[index_offset + 1]) << OVERLAY_MAX_TOUCH;
         }
         else
         {
            unsigned index        = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT)
               ? RETRO_DEVICE_INDEX_ANALOG_RIGHT
               : RETRO_DEVICE_INDEX_ANALOG_LEFT;
            int16_t analog_x      = input_state_internal(input_st, settings, port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_X);
            int16_t analog_y      = input_state_internal(input_st, settings, port, RETRO_DEVICE_ANALOG,
                  index, RETRO_DEVICE_ID_ANALOG_Y);

            /* Only modify overlay delta_x/delta_y values
             * if we are monitoring input from a physical
             * controller */
            desc->delta_x         = (analog_x / (float)0x8000) * (desc->range_x / 2.0f);
            desc->delta_y         = (analog_y / (float)0x8000) * (desc->range_y / 2.0f);
         }

         /* fall-through */

      case OVERLAY_TYPE_DPAD_AREA:
      case OVERLAY_TYPE_ABXY_AREA:
         return (desc->touch_mask != 0);

      case OVERLAY_TYPE_KEYBOARD:
         {
            bool tmp    = false;
            if (ol_state)
            {
               if (OVERLAY_GET_KEY(ol_state, desc->retro_key_idx))
                  tmp   = true;
            }
            else
               tmp      = input_state_internal(input_st, settings, port,
                     RETRO_DEVICE_KEYBOARD, 0, desc->retro_key_idx);

            if (tmp)
            {
               desc->touch_mask |= (1 << OVERLAY_MAX_TOUCH);
               return true;
            }
         }
         break;

      default:
         break;
   }

   return false;
}

static bool input_overlay_add_inputs(input_overlay_t *ol,
      input_overlay_state_t *ol_state,
      input_driver_state_t *input_st,
      settings_t *settings,
      bool show_touched, unsigned port)
{
   size_t i;
   bool button_pressed      = false;

   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc  = &(ol->active->descs[i]);
      button_pressed       |= input_overlay_add_inputs_inner(
            desc, input_st,
            settings,
            show_touched
            ? ol_state
            : NULL,
            port);
   }

   return button_pressed;
}

static void input_overlay_get_eightway_slope_limits(
      const unsigned diagonal_sensitivity,
      float* low_slope, float* high_slope)
{
   /* Sensitivity setting is the relative size of diagonal zones to
    * cardinal zones. Convert to fraction of 45 deg span (max diagonal).
    */
   float f     =  2.0f * diagonal_sensitivity
             / (100.0f + diagonal_sensitivity);

   float high_angle  /* 67.5 deg max */
               = (f * (0.375 * M_PI) + (1.0f - f) * (0.25 * M_PI));
   float low_angle   /* 22.5 deg min */
               = (f * (0.125 * M_PI) + (1.0f - f) * (0.25 * M_PI));

   *high_slope = tan(high_angle);
   *low_slope  = tan(low_angle);
}

/**
 * input_overlay_set_eightway_diagonal_sensitivity:
 *
 * Gets the slope limits defining each eightway type's diagonal zones.
 */
void input_overlay_set_eightway_diagonal_sensitivity(void)
{
   settings_t           *settings = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   input_overlay_get_eightway_slope_limits(
         settings->uints.input_overlay_dpad_diagonal_sensitivity,
         &input_st->overlay_eightway_dpad_slopes[0],
         &input_st->overlay_eightway_dpad_slopes[1]);

   input_overlay_get_eightway_slope_limits(
         settings->uints.input_overlay_abxy_diagonal_sensitivity,
         &input_st->overlay_eightway_abxy_slopes[0],
         &input_st->overlay_eightway_abxy_slopes[1]);
}

/**
 * input_overlay_get_eightway_state:
 * @desc : overlay descriptor handle for an eightway area
 * @out : current input state to be OR'd with eightway state
 * @x_dist : X offset from eightway area center
 * @y_dist : Y offset from eightway area center
 *
 * Gets the eightway area's current input state based on (@x_dist, @y_dist).
 **/
static INLINE void input_overlay_get_eightway_state(
      const struct overlay_desc *desc,
      overlay_eightway_config_t *eightway,
      input_bits_t *out,
      float x_dist, float y_dist)
{
   uint32_t *data;
   float abs_slope;

   x_dist /= desc->range_x;
   y_dist /= desc->range_y;

   if (x_dist == 0.0f)
      x_dist = 0.0001f;
   abs_slope = fabs(y_dist / x_dist);

   if (x_dist > 0.0f)
   {
      if (y_dist < 0.0f)
      {
         /* Q1 */
         if (abs_slope > *eightway->slope_high)
            data = eightway->up.data;
         else if (abs_slope < *eightway->slope_low)
            data = eightway->right.data;
         else
            data = eightway->up_right.data;
      }
      else
      {
         /* Q4 */
         if (abs_slope > *eightway->slope_high)
            data = eightway->down.data;
         else if (abs_slope < *eightway->slope_low)
            data = eightway->right.data;
         else
            data = eightway->down_right.data;
      }
   }
   else
   {
      if (y_dist < 0.0f)
      {
         /* Q2 */
         if (abs_slope > *eightway->slope_high)
            data = eightway->up.data;
         else if (abs_slope < *eightway->slope_low)
            data = eightway->left.data;
         else
            data = eightway->up_left.data;
      }
      else
      {
         /* Q3 */
         if (abs_slope > *eightway->slope_high)
            data = eightway->down.data;
         else if (abs_slope < *eightway->slope_low)
            data = eightway->left.data;
         else
            data = eightway->down_left.data;
      }
   }

   bits_or_bits(out->data, data, CUSTOM_BINDS_U32_COUNT);
}

/**
 * input_overlay_coords_inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 * @use_range_mod         : Set true to use range_mod hitbox
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox,
 * otherwise false (0).
 **/
static bool input_overlay_coords_inside_hitbox(const struct overlay_desc *desc,
      float x, float y, bool use_range_mod)
{
   float range_x, range_y;

   if (use_range_mod)
   {
      range_x = desc->range_x_mod;
      range_y = desc->range_y_mod;
   }
   else
   {
      range_x = desc->range_x_hitbox;
      range_y = desc->range_y_hitbox;
   }

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipse. */
         float x_dist  = (x - desc->x_hitbox) / range_x;
         float y_dist  = (y - desc->y_hitbox) / range_y;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }
      case OVERLAY_HITBOX_RECT:
         return
               (fabs(x - desc->x_hitbox) <= range_x)
            && (fabs(y - desc->y_hitbox) <= range_y);
      case OVERLAY_HITBOX_NONE:
         break;
   }
   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @touch_idx             : Touch pointer index.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 * @touch_scale           : Overlay scale.
 *
 * Polls input overlay for a single touch pointer.
 *
 * @norm_x and @norm_y are the result of
 * video_driver_translate_coord_viewport().
 **/
static void input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int touch_idx, int16_t norm_x, int16_t norm_y, float touch_scale)
{
   size_t i, j;
   struct overlay_desc *descs = ol->active->descs;
   unsigned int highest_prio  = 0;
   int old_touch_idx          = input_driver_st.old_touch_index_lut[touch_idx];
   bool use_range_mod;

   /* norm_x and norm_y is in [-0x7fff, 0x7fff] range,
    * like RETRO_DEVICE_POINTER. */
   float x = (float)(norm_x + 0x7fff) / 0xffff;
   float y = (float)(norm_y + 0x7fff) / 0xffff;

   x -= ol->active->mod_x;
   y -= ol->active->mod_y;
   x /= ol->active->mod_w;
   y /= ol->active->mod_h;

   x *= touch_scale;
   y *= touch_scale;

   for (i = 0; i < ol->active->size; i++)
   {
      float x_dist, y_dist;
      unsigned int base         = 0;
      unsigned int desc_prio    = 0;
      struct overlay_desc *desc = &descs[i];

      /* Use range_mod if this touch pointer contributed
       * to desc's touch_mask in the previous poll */
      use_range_mod = (old_touch_idx != -1)
            && BIT32_GET(desc->old_touch_mask, old_touch_idx);

      if (!input_overlay_coords_inside_hitbox(desc, x, y, use_range_mod))
         continue;

      /* Check for exclusive hitbox, which blocks other input.
       * range_mod_exclusive has priority over exclusive. */
      if (use_range_mod && (desc->flags & OVERLAY_DESC_RANGE_MOD_EXCLUSIVE))
         desc_prio = 2;
      else if (desc->flags & OVERLAY_DESC_EXCLUSIVE)
         desc_prio = 1;

      if (highest_prio > desc_prio)
         continue;

      if (desc_prio > highest_prio)
      {
         highest_prio = desc_prio;
         memset(out, 0, sizeof(*out));
         for (j = 0; j < i; j++)
            BIT32_CLEAR(descs[j].touch_mask, touch_idx);
      }

      BIT32_SET(desc->touch_mask, touch_idx);
      x_dist = x - desc->x_shift;
      y_dist = y - desc->y_shift;

      switch (desc->type)
      {
         case OVERLAY_TYPE_BUTTONS:
            bits_or_bits(out->buttons.data,
                  desc->button_mask.data,
                  ARRAY_SIZE(desc->button_mask.data));

            if (BIT256_GET(desc->button_mask, RARCH_OVERLAY_NEXT))
               ol->next_index = desc->next_index;
            break;
         case OVERLAY_TYPE_KEYBOARD:
            if (desc->retro_key_idx < RETROK_LAST)
               OVERLAY_SET_KEY(out, desc->retro_key_idx);
            break;
         case OVERLAY_TYPE_DPAD_AREA:
         case OVERLAY_TYPE_ABXY_AREA:
            input_overlay_get_eightway_state(
                  desc, desc->eightway_config,
                  &out->buttons, x_dist, y_dist);
            break;
         case OVERLAY_TYPE_ANALOG_RIGHT:
            base = 2;
            /* fall-through */
         default:
            {
               float x_val           = x_dist / desc->range_x;
               float y_val           = y_dist / desc->range_y;
               float x_val_sat       = x_val / desc->analog_saturate_pct;
               float y_val_sat       = y_val / desc->analog_saturate_pct;
               out->analog[base + 0] = clamp_float(x_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
               out->analog[base + 1] = clamp_float(y_val_sat, -1.0f, 1.0f)
                  * 32767.0f;
            }
            break;
      }

      if (desc->flags & OVERLAY_DESC_MOVABLE)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }
   }

   if (ol->flags & INPUT_OVERLAY_BLOCKED)
      memset(out, 0, sizeof(*out));
}

/**
 * input_overlay_update_desc_geom:
 * @ol                    : overlay handle.
 * @desc                  : overlay descriptors handle.
 *
 * Update input overlay descriptors' vertex geometry.
 **/
static void input_overlay_update_desc_geom(input_overlay_t *ol,
      struct overlay_desc *desc)
{
   if (!desc->image.pixels || !(desc->flags & OVERLAY_DESC_MOVABLE))
      return;

   if (ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

/**
 * input_overlay_post_poll:
 *
 * Called after all the input_overlay_poll() calls to
 * update alpha mods for pressed/unpressed controls
 **/
static void input_overlay_post_poll(
      enum overlay_visibility *visibility,
      input_overlay_t *ol,
      bool show_input, float opacity)
{
   size_t i;

   input_overlay_set_alpha_mod(visibility, ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      if (     desc->touch_mask != 0
            && show_input && desc->image.pixels
            && ol->iface->set_alpha)
         ol->iface->set_alpha(ol->iface_data, desc->image_index,
               desc->alpha_mod * opacity);

      input_overlay_update_desc_geom(ol, desc);

      desc->old_touch_mask = desc->touch_mask;
      desc->touch_mask     = 0;
   }
}

static void input_overlay_desc_init_hitbox(struct overlay_desc *desc)
{
   desc->x_hitbox       =
         ((desc->x_shift + desc->range_x * desc->reach_right) +
          (desc->x_shift - desc->range_x * desc->reach_left)) / 2.0f;

   desc->y_hitbox       =
         ((desc->y_shift + desc->range_y * desc->reach_down) +
          (desc->y_shift - desc->range_y * desc->reach_up)) / 2.0f;

   desc->range_x_hitbox =
         (desc->range_x * desc->reach_right +
          desc->range_x * desc->reach_left) / 2.0f;

   desc->range_y_hitbox =
         (desc->range_y * desc->reach_down +
          desc->range_y * desc->reach_up) / 2.0f;

   desc->range_x_mod    = desc->range_x_hitbox * desc->range_mod;
   desc->range_y_mod    = desc->range_y_hitbox * desc->range_mod;
}

/**
 * input_overlay_scale:
 * @ol                    : Overlay handle.
 * @layout                : Scale + offset factors.
 *
 * Scales the overlay and all its associated descriptors
 * and applies any aspect ratio/offset factors.
 **/
static void input_overlay_scale(struct overlay *ol,
      const overlay_layout_t *layout)
{
   size_t i;

   ol->mod_w = ol->w * layout->x_scale;
   ol->mod_h = ol->h * layout->y_scale;
   ol->mod_x = (ol->center_x + (ol->x - ol->center_x) *
         layout->x_scale) + layout->x_offset;
   ol->mod_y = (ol->center_y + (ol->y - ol->center_y) *
         layout->y_scale) + layout->y_offset;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay_desc *desc = &ol->descs[i];
      float x_shift_offset      = 0.0f;
      float y_shift_offset      = 0.0f;
      float scale_w;
      float scale_h;
      float adj_center_x;
      float adj_center_y;

      /* Apply 'x separation' factor */
      if (desc->x < (0.5f - 0.0001f))
         x_shift_offset = layout->x_separation * -1.0f;
      else if (desc->x > (0.5f + 0.0001f))
         x_shift_offset = layout->x_separation;

      desc->x_shift     = desc->x + x_shift_offset;

      /* Apply 'y separation' factor */
      if (desc->y < (0.5f - 0.0001f))
         y_shift_offset = layout->y_separation * -1.0f;
      else if (desc->y > (0.5f + 0.0001f))
         y_shift_offset = layout->y_separation;

      desc->y_shift     = desc->y + y_shift_offset;

      scale_w           = ol->mod_w * desc->range_x;
      scale_h           = ol->mod_h * desc->range_y;
      adj_center_x      = ol->mod_x + desc->x_shift * ol->mod_w;
      adj_center_y      = ol->mod_y + desc->y_shift * ol->mod_h;

      desc->mod_w       = 2.0f * scale_w;
      desc->mod_h       = 2.0f * scale_h;
      desc->mod_x       = adj_center_x - scale_w;
      desc->mod_y       = adj_center_y - scale_h;

      input_overlay_desc_init_hitbox(desc);
   }
}

static void input_overlay_parse_layout(
      const struct overlay *ol,
      const overlay_layout_desc_t *layout_desc,
      float display_aspect_ratio,
      overlay_layout_t *overlay_layout)
{
   /* Set default values */
   overlay_layout->x_scale      = 1.0f;
   overlay_layout->y_scale      = 1.0f;
   overlay_layout->x_separation = 0.0f;
   overlay_layout->y_separation = 0.0f;
   overlay_layout->x_offset     = 0.0f;
   overlay_layout->y_offset     = 0.0f;

   /* Perform auto-scaling, if required */
   if (layout_desc->auto_scale)
   {
      /* Sanity check - if scaling is blocked,
       * or aspect ratios are invalid, then we
       * can do nothing */
      if (   (ol->flags & OVERLAY_BLOCK_SCALE)
          || (ol->aspect_ratio <= 0.0f)
          || (display_aspect_ratio <= 0.0f))
         return;

      /* If display is wider than overlay,
       * reduce width */
      if (display_aspect_ratio > ol->aspect_ratio)
      {
         overlay_layout->x_scale = ol->aspect_ratio /
               display_aspect_ratio;

         if (overlay_layout->x_scale <= 0.0f)
         {
            overlay_layout->x_scale = 1.0f;
            return;
         }

         /* If auto-scale X separation is enabled, move elements
          * horizontally towards the edges of the screen */
         if (ol->flags & OVERLAY_AUTO_X_SEPARATION)
            overlay_layout->x_separation = ((1.0f / overlay_layout->x_scale) - 1.0f) * 0.5f;
      }
      /* If display is taller than overlay,
       * reduce height */
      else
      {
         overlay_layout->y_scale = display_aspect_ratio /
               ol->aspect_ratio;

         if (overlay_layout->y_scale <= 0.0f)
         {
            overlay_layout->y_scale = 1.0f;
            return;
         }

         /* If auto-scale Y separation is enabled, move elements
          * vertically towards the edges of the screen */
         if (ol->flags & OVERLAY_AUTO_Y_SEPARATION)
            overlay_layout->y_separation = ((1.0f / overlay_layout->y_scale) - 1.0f) * 0.5f;
      }

      return;
   }

   /* Regular 'manual' scaling/position adjustment
    * > Landscape display orientations */
   if (display_aspect_ratio > 1.0f)
   {
      float scale              = layout_desc->scale_landscape;
      float aspect_adjust      = layout_desc->aspect_adjust_landscape;
      /* Note: Y offsets have their sign inverted,
       * since from a usability perspective positive
       * values should move the overlay upwards */
      overlay_layout->x_offset = layout_desc->x_offset_landscape;
      overlay_layout->y_offset = layout_desc->y_offset_landscape * -1.0f;

      if (!(ol->flags & OVERLAY_BLOCK_X_SEPARATION))
         overlay_layout->x_separation = layout_desc->x_separation_landscape;
      if (!(ol->flags & OVERLAY_BLOCK_Y_SEPARATION))
         overlay_layout->y_separation = layout_desc->y_separation_landscape;

      if (!(ol->flags & OVERLAY_BLOCK_SCALE))
      {
         /* In landscape orientations, aspect correction
          * adjusts the overlay width */
         overlay_layout->x_scale = (aspect_adjust >= 0.0f) ?
               (scale * (aspect_adjust + 1.0f)) :
               (scale / ((aspect_adjust * -1.0f) + 1.0f));
         overlay_layout->y_scale = scale;
      }
   }
   /* > Portrait display orientations */
   else
   {
      float scale              = layout_desc->scale_portrait;
      float aspect_adjust      = layout_desc->aspect_adjust_portrait;

      overlay_layout->x_offset = layout_desc->x_offset_portrait;
      overlay_layout->y_offset = layout_desc->y_offset_portrait * -1.0f;

      if (!(ol->flags & OVERLAY_BLOCK_X_SEPARATION))
         overlay_layout->x_separation = layout_desc->x_separation_portrait;
      if (!(ol->flags & OVERLAY_BLOCK_Y_SEPARATION))
         overlay_layout->y_separation = layout_desc->y_separation_portrait;

      if (!(ol->flags & OVERLAY_BLOCK_SCALE))
      {
         /* In portrait orientations, aspect correction
          * adjusts the overlay height */
         overlay_layout->x_scale = scale;
         overlay_layout->y_scale = (aspect_adjust >= 0.0f) ?
               (scale * (aspect_adjust + 1.0f)) :
               (scale / ((aspect_adjust * -1.0f) + 1.0f));
      }
   }
}

static void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (!ol->iface->vertex_geom)
      return;

   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];
      if (desc->image.pixels)
         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
   }
}

/**
 * input_overlay_set_scale_factor:
 * @ol                    : Overlay handle.
 * @layout_desc           : Scale + offset factors.
 *
 * Scales the overlay and applies any aspect ratio/
 * offset factors.
 **/
void input_overlay_set_scale_factor(
      input_overlay_t *ol, const overlay_layout_desc_t *layout_desc,
      unsigned video_driver_width,
      unsigned video_driver_height
)
{
   size_t i;
   float display_aspect_ratio = 0.0f;

   if (!ol || !layout_desc)
      return;

   if (video_driver_height > 0)
      display_aspect_ratio = (float)video_driver_width /
         (float)video_driver_height;

   for (i = 0; i < ol->size; i++)
   {
      struct overlay *current_overlay = &ol->overlays[i];
      overlay_layout_t overlay_layout;

      input_overlay_parse_layout(current_overlay,
            layout_desc, display_aspect_ratio, &overlay_layout);
      input_overlay_scale(current_overlay, &overlay_layout);
   }

   input_overlay_set_vertex_geom(ol);
}

void input_overlay_load_active(
      enum overlay_visibility *visibility,
      input_overlay_t *ol, float opacity)
{
   if (ol->iface->load)
      ol->iface->load(ol->iface_data, ol->active->load_images,
            ol->active->load_images_size);

   input_overlay_set_alpha_mod(visibility, ol, opacity);
   input_overlay_set_vertex_geom(ol);

   if (ol->iface->full_screen)
      ol->iface->full_screen(ol->iface_data,
            (ol->active->flags & OVERLAY_FULL_SCREEN));
}

/**
 * input_overlay_poll_clear:
 * @ol                    : overlay handle
 *
 * Call when there is nothing to poll. Allows overlay to
 * clear certain state.
 **/
static void input_overlay_poll_clear(
      enum overlay_visibility *visibility,
      input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->flags &= ~INPUT_OVERLAY_BLOCKED;

   input_overlay_set_alpha_mod(visibility, ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->old_touch_mask      = desc->touch_mask;
      desc->touch_mask          = 0;

      input_overlay_update_desc_geom(ol, desc);
   }
}

static enum overlay_visibility input_overlay_get_visibility(
      enum overlay_visibility *visibility,
      int overlay_idx)
{
    if (!visibility)
       return OVERLAY_VISIBILITY_DEFAULT;
    if ((overlay_idx < 0) || (overlay_idx >= MAX_VISIBILITY))
       return OVERLAY_VISIBILITY_DEFAULT;
    return visibility[overlay_idx];
}

void input_overlay_set_alpha_mod(
      enum overlay_visibility *visibility,
      input_overlay_t *ol, float mod)
{
   unsigned i;

   if (!ol)
      return;

   for (i = 0; i < ol->active->load_images_size; i++)
   {
      if (input_overlay_get_visibility(visibility, i)
            == OVERLAY_VISIBILITY_HIDDEN)
          ol->iface->set_alpha(ol->iface_data, i, 0.0);
      else
          ol->iface->set_alpha(ol->iface_data, i, mod);
   }
}

static void input_overlay_free_overlays(input_overlay_t *ol)
{
   size_t i;

   if (!ol || !ol->overlays)
      return;

   for (i = 0; i < ol->size; i++)
      input_overlay_free_overlay(&ol->overlays[i]);

   free(ol->overlays);
   ol->overlays = NULL;
}

void input_overlay_free_overlay(struct overlay *overlay)
{
   size_t i;

   if (!overlay)
      return;

   for (i = 0; i < overlay->size; i++)
   {
      image_texture_free(&overlay->descs[i].image);
      if (overlay->descs[i].eightway_config)
         free(overlay->descs[i].eightway_config);
      overlay->descs[i].eightway_config = NULL;
   }

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   image_texture_free(&overlay->image);
}

/**
 * input_overlay_free:
 * @ol                    : Overlay handle.
 *
 * Frees overlay handle.
 **/
static void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   input_overlay_free_overlays(ol);

   if (ol->iface && ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

   if (ol->path)
   {
      free(ol->path);
      ol->path = NULL;
   }

   free(ol);
}

void input_overlay_auto_rotate_(
      unsigned video_driver_width,
      unsigned video_driver_height,
      bool input_overlay_enable,
      input_overlay_t *ol)
{
   size_t i;
   enum overlay_orientation screen_orientation         = OVERLAY_ORIENTATION_PORTRAIT;
   enum overlay_orientation active_overlay_orientation = OVERLAY_ORIENTATION_NONE;
   bool tmp                                            = false;

   /* Sanity check */
   if (!ol || !(ol->flags & INPUT_OVERLAY_ALIVE) || !input_overlay_enable)
      return;

   /* Get current screen orientation */
   if (video_driver_width > video_driver_height)
      screen_orientation = OVERLAY_ORIENTATION_LANDSCAPE;

   /* Get orientation of active overlay */
   if (!string_is_empty(ol->active->name))
   {
      if (strstr(ol->active->name, "landscape"))
         active_overlay_orientation = OVERLAY_ORIENTATION_LANDSCAPE;
      else if (strstr(ol->active->name, "portrait"))
         active_overlay_orientation = OVERLAY_ORIENTATION_PORTRAIT;
      else /* Sanity check */
         return;
   }
   else /* Sanity check */
      return;

   /* If screen and overlay have the same orientation,
    * no action is required */
   if (screen_orientation == active_overlay_orientation)
      return;

   /* Attempt to find index of overlay corresponding
    * to opposite orientation */
   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc = &ol->active->descs[i];

      if (!desc)
         continue;

      if (!string_is_empty(desc->next_index_name))
      {
         bool next_overlay_found = false;
         if (active_overlay_orientation == OVERLAY_ORIENTATION_LANDSCAPE)
            next_overlay_found = (strstr(desc->next_index_name, "portrait") != 0);
         else
            next_overlay_found = (strstr(desc->next_index_name, "landscape") != 0);

         if (next_overlay_found)
         {
            /* We have a valid target overlay
             * > Trigger 'overly next' command event
             * Note: tmp == false. This prevents CMD_EVENT_OVERLAY_NEXT
             * from calling input_overlay_auto_rotate_() again */
            ol->next_index     = desc->next_index;
            command_event(CMD_EVENT_OVERLAY_NEXT, &tmp);
            break;
         }
      }
   }
}

/**
 * input_overlay_track_touch_inputs
 * @state : Overlay input state for this poll
 * @old_state : Overlay input state for previous poll
 *
 * Matches current touch inputs to previous poll's, based on distance.
 * Updates old_touch_index_lut and assigns -1 to any new inputs.
 */
static void input_overlay_track_touch_inputs(
      input_overlay_state_t *state, input_overlay_state_t *old_state)
{
   int *const old_index_lut = input_driver_st.old_touch_index_lut;
   int i, j, t, new_idx;
   float x_dist, y_dist, sq_dist, outlier;
   float min_sq_dist[OVERLAY_MAX_TOUCH];

   memset(old_index_lut, -1, sizeof(int) * OVERLAY_MAX_TOUCH);

   /* Compute (squared) distances and match new indexes to old */
   for (i = 0; i < state->touch_count; i++)
   {
      min_sq_dist[i] = 1e10f;

      for (j = 0; j < old_state->touch_count; j++)
      {
         x_dist  = state->touch[i].x - old_state->touch[j].x;
         y_dist  = state->touch[i].y - old_state->touch[j].y;

         sq_dist = x_dist * x_dist + y_dist * y_dist;

         if (sq_dist < min_sq_dist[i])
         {
            min_sq_dist[i]   = sq_dist;
            old_index_lut[i] = j;
         }
      }
   }

   /* If touch_count increased, find the outliers and assign -1 */
   for (t = old_state->touch_count; t < state->touch_count; t++)
   {
      new_idx = OVERLAY_MAX_TOUCH - 1;
      outlier = 0;

      for (i = 0; i < state->touch_count; i++)
         if (     min_sq_dist[i] > outlier
               && old_index_lut[i] != -1)
         {
            outlier = min_sq_dist[i];
            new_idx = i;
         }

      old_index_lut[new_idx] = -1;
   }
}

/*
 * input_poll_overlay:
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
static void input_poll_overlay(
      bool keyboard_mapping_blocked,
      settings_t *settings,
      void *ol_data,
      enum overlay_visibility *overlay_visibility,
      float opacity,
      unsigned analog_dpad_mode,
      float axis_threshold)
{
   input_overlay_state_t old_ol_state;
   int i, j;
   input_overlay_t *ol                     = (input_overlay_t*)ol_data;
   uint16_t key_mod                        = 0;
   bool button_pressed                     = false;
   input_driver_state_t *input_st          = &input_driver_st;
   void *input_data                        = input_st->current_data;
   input_overlay_state_t *ol_state         = &ol->overlay_state;
   input_driver_t *current_input           = input_st->current_driver;
   enum overlay_show_input_type
         input_overlay_show_inputs         = (enum overlay_show_input_type)
               settings->uints.input_overlay_show_inputs;
   unsigned input_overlay_show_inputs_port = settings->uints.input_overlay_show_inputs_port;
   float touch_scale                       = (float)settings->uints.input_touch_scale;
   bool osk_state_changed                  = false;

   if (!ol_state)
      return;

   memcpy(&old_ol_state, ol_state,
         sizeof(old_ol_state));
   memset(ol_state, 0, sizeof(*ol_state));

   if (current_input->input_state)
   {
      rarch_joypad_info_t joypad_info;
      unsigned device                 = (ol->active->flags & OVERLAY_FULL_SCREEN)
         ? RARCH_DEVICE_POINTER_SCREEN
         : RETRO_DEVICE_POINTER;
      const input_device_driver_t
         *joypad                      = input_st->primary_joypad;
#ifdef HAVE_MFI
      const input_device_driver_t
         *sec_joypad                  = input_st->secondary_joypad;
#else
      const input_device_driver_t
         *sec_joypad                  = NULL;
#endif

      joypad_info.joy_idx             = 0;
      joypad_info.auto_binds          = NULL;
      joypad_info.axis_threshold      = 0.0f;

      /* Get driver input */
      for (i = 0;
            current_input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_PRESSED)
                  && i < OVERLAY_MAX_TOUCH;
            i++)
      {
         ol_state->touch[i].x = current_input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_X);
         ol_state->touch[i].y = current_input->input_state(
               input_data,
               joypad,
               sec_joypad,
               &joypad_info,
               NULL,
               keyboard_mapping_blocked,
               0,
               device,
               i,
               RETRO_DEVICE_ID_POINTER_Y);
      }
      ol_state->touch_count = i;

      /* Update lookup table of new to old touch indexes */
      input_overlay_track_touch_inputs(ol_state, &old_ol_state);

      /* Poll overlay */
      for (i = 0; i < ol_state->touch_count; i++)
      {
         input_overlay_state_t polled_data;
         memset(&polled_data, 0, sizeof(struct input_overlay_state));

         if (ol->flags & INPUT_OVERLAY_ENABLE)
            input_overlay_poll(ol, &polled_data, i,
                  ol_state->touch[i].x, ol_state->touch[i].y, touch_scale);
         else
            ol->flags &= ~INPUT_OVERLAY_BLOCKED;

         bits_or_bits(ol_state->buttons.data,
               polled_data.buttons.data,
               ARRAY_SIZE(polled_data.buttons.data));

         for (j = 0; j < (int)ARRAY_SIZE(ol_state->keys); j++)
            ol_state->keys[j] |= polled_data.keys[j];

         /* Fingers pressed later take priority and matched up
          * with overlay poll priorities. */
         for (j = 0; j < 4; j++)
            if (polled_data.analog[j])
               ol_state->analog[j] = polled_data.analog[j];
      }
   }

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = (int)ARRAY_SIZE(ol_state->keys); i-- > 0;)
   {
      if (ol_state->keys[i] != old_ol_state.keys[i])
      {
         uint32_t orig_bits = old_ol_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];
         osk_state_changed  = true;

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
            {
               unsigned rk = i * 32 + j;
               uint32_t c  = input_keymaps_translate_rk_to_ascii(rk, key_mod);
               input_keyboard_event(new_bits & (1 << j),
                     rk, c, key_mod, RETRO_DEVICE_POINTER);
            }
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if ((BIT256_GET(ol->overlay_state.buttons, bind_plus)))
         ol_state->analog[j] += 0x7fff;
      if ((BIT256_GET(ol->overlay_state.buttons, bind_minus)))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (analog_dpad_mode)
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (analog_dpad_mode == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  axis_threshold)
            BIT256_SET(ol_state->buttons, RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   button_pressed = input_overlay_add_inputs(ol, ol_state, input_st,
         settings,
         (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_TOUCHED),
         input_overlay_show_inputs_port);

   if (button_pressed)
      input_st->flags |=  INP_FLAG_BLOCK_POINTER_INPUT;
   else
      input_st->flags &= ~INP_FLAG_BLOCK_POINTER_INPUT;

   if (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_NONE)
      button_pressed = false;

   if (button_pressed || ol_state->touch_count)
      input_overlay_post_poll(overlay_visibility, ol,
            button_pressed, opacity);
   else
      input_overlay_poll_clear(overlay_visibility, ol, opacity);

   /* Create haptic feedback for any change in button/key state,
    * unless touch_count decreased. */
   if (     current_input->keypress_vibrate
         && settings->bools.vibrate_on_keypress
         && ol_state->touch_count
         && ol_state->touch_count >= old_ol_state.touch_count
         && !(ol->flags & INPUT_OVERLAY_BLOCKED))
   {
      if (     osk_state_changed
            || bits_any_different(
                     ol_state->buttons.data,
                     old_ol_state.buttons.data,
                     ARRAY_SIZE(old_ol_state.buttons.data))
         )
         current_input->keypress_vibrate();
   }
}
#endif

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates string representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str, size_t len)
{
   size_t i;
   if (len == 1 && ISALPHA((int)*str))
      return (enum retro_key)(RETROK_a + (TOLOWER((int)*str) - (int)'a'));
   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }
   return RETROK_UNKNOWN;
}

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (string_is_equal(str, input_config_bind_map[i].base))
         return i;

   return RARCH_BIND_LIST_END;
}

void input_config_get_bind_string(
      void *settings_data,
      char *buf,
      const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind,
      size_t size)
{
   settings_t *settings                 = (settings_t*)settings_data;
   int delim                            = 0;
   bool  input_descriptor_label_show    =
      settings->bools.input_descriptor_label_show;

   *buf                                 = '\0';

   if      (bind      && bind->joykey  != NO_BTN)
      input_config_get_bind_string_joykey(
            input_descriptor_label_show,
            buf, "", bind, size);
   else if (bind      && bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            buf, "", bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(
            input_descriptor_label_show,
            buf, "(Auto)", auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            buf, "(Auto)", auto_bind, size);

   if (*buf)
      delim = 1;

#ifndef RARCH_CONSOLE
   {
      char key[64];
      key[0] = '\0';

      input_keymaps_translate_rk_to_str(bind->key, key, sizeof(key));
      if (     key[0] == 'n'
            && key[1] == 'u'
            && key[2] == 'l'
            && key[3] == '\0'
         )
         *key = '\0';
      /*empty?*/
      else if (*key != '\0')
      {
         char keybuf[64];

         keybuf[0] = '\0';

         if (delim)
            strlcat(buf, ", ", size);
         snprintf(keybuf, sizeof(keybuf),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEY), key);
         strlcat(buf, keybuf, size);
         delim = 1;
      }
   }
#endif

   if (bind->mbutton != NO_BTN)
   {
      int tag = 0;
      switch (bind->mbutton)
      {
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT;
            break;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT;
            break;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4;
            break;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP;
            break;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            tag = MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN;
            break;
      }

      if (tag != 0)
      {
         if (delim)
            strlcat(buf, ", ", size);
         strlcat(buf, msg_hash_to_str((enum msg_hash_enums)tag), size);
      }
   }

   /*completely empty?*/
   if (*buf == '\0')
      strlcat(buf, "---", size);
}

void input_config_get_bind_string_joykey(
      bool input_descriptor_label_show,
      char *buf, const char *suffix,
      const struct retro_keybind *bind, size_t size)
{
   if (GET_HAT_DIR(bind->joykey))
   {
      if (      bind->joykey_label
            && !string_is_empty(bind->joykey_label)
            && input_descriptor_label_show)
         fill_pathname_join_delim(buf,
               bind->joykey_label, suffix, ' ', size);
      else
      {
         size_t len  = snprintf(buf, size,
               "Hat #%u ", (unsigned)GET_HAT(bind->joykey));

         switch (GET_HAT_DIR(bind->joykey))
         {
            case HAT_UP_MASK:
               strlcpy(buf + len, "Up",    size - len);
               break;
            case HAT_DOWN_MASK:
               strlcpy(buf + len, "Down",  size - len);
               break;
            case HAT_LEFT_MASK:
               strlcpy(buf + len, "Left",  size - len);
               break;
            case HAT_RIGHT_MASK:
               strlcpy(buf + len, "Right", size - len);
               break;
            default:
               strlcpy(buf + len, "?",     size - len);
               break;
         }
      }
   }
   else
   {
      if (      bind->joykey_label
            && !string_is_empty(bind->joykey_label)
            && input_descriptor_label_show)
         fill_pathname_join_delim(buf,
               bind->joykey_label, suffix, ' ', size);
      else
         snprintf(buf, size, "%s%u",
               "Button ", (unsigned)bind->joykey);
   }
}

void input_config_get_bind_string_joyaxis(
      bool input_descriptor_label_show,
      char *buf, const char *suffix,
      const struct retro_keybind *bind, size_t size)
{
   if (      bind->joyaxis_label
         && !string_is_empty(bind->joyaxis_label)
         && input_descriptor_label_show)
      fill_pathname_join_delim(buf,
            bind->joyaxis_label, suffix, ' ', size);
   else
   {
      size_t _len = strlcpy(buf, "Axis ", size);
      if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
         snprintf(buf + _len, size - _len, "-%u",
               (unsigned)AXIS_NEG_GET(bind->joyaxis));
      else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
         snprintf(buf + _len, size - _len, "+%u",
               (unsigned)AXIS_POS_GET(bind->joyaxis));
   }
}

void osk_update_last_codepoint(
      unsigned *last_codepoint,
      unsigned *last_codepoint_len,
      const char *word)
{
   const char *letter         = word;
   const char    *pos         = letter;

   if (word[0] == 0)
   {
      *last_codepoint         = 0;
      *last_codepoint_len     = 0;
      return;
   }

   for (;;)
   {
      unsigned codepoint      = utf8_walk(&letter);
      if (letter[0] == 0)
      {
         *last_codepoint      = codepoint;
         *last_codepoint_len  = (unsigned)(letter - pos);
         break;
      }
      pos                     = letter;
   }
}

#ifdef HAVE_LANGEXTRA
/* combine 3 korean elements. make utf8 character */
static unsigned get_kr_utf8( int c1,int c2,int c3)
{
   int  uv = c1 * (28 * 21) + c2 * 28 + c3 + 0xac00;
   int  tv = (uv >> 12) | ((uv & 0x0f00) << 2) | ((uv & 0xc0) << 2) | ((uv & 0x3f) << 16);
   return  (tv | 0x8080e0);
}

/* utf8 korean composition */
static unsigned get_kr_composition( char* pcur, char* padd)
{
   size_t _len;
   static char cc1[] = {"ㄱㄱㄲ ㄷㄷㄸ ㅂㅂㅃ ㅅㅅㅆ ㅈㅈㅉ"};
   static char cc2[] = {"ㅗㅏㅘ ㅗㅐㅙ ㅗㅣㅚ ㅜㅓㅝ ㅜㅔㅞ ㅜㅣㅟ ㅡㅣㅢ"};
   static char cc3[] = {"ㄱㄱㄲ ㄱㅅㄳ ㄴㅈㄵ ㄴㅎㄶ ㄹㄱㄺ ㄹㅁㄻ ㄹㅂㄼ ㄹㅅㄽ ㄹㅌㄾ ㄹㅍㄿ ㄹㅎㅀ ㅂㅅㅄ ㅅㅅㅆ"};
   static char s1[]  = {"ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣㆍㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅅㅆㅇㅈㅊㅋㅌㅍㅎ"};
   char *tmp1        = NULL;
   char *tmp2        = NULL;
   int c1            = -1;
   int c2            = -1;
   int c3            =  0;
   int nv            = -1;
   char utf8[8]      = {0, 0, 0, 0, 0, 0, 0, 0};
   unsigned ret      =  *((unsigned*)pcur);

   /* check korean */
   if (!pcur[0] || !pcur[1] || !pcur[2] || pcur[3])
      return ret;
   if (!padd[0] || !padd[1] || !padd[2] || padd[3])
      return ret;
   if ((tmp1 = strstr(s1, pcur)))
      c1 = (int)((tmp1 - s1) / 3);
   if ((tmp1 = strstr(s1, padd)))
      nv = (int)((tmp1 - s1) / 3);
   if (nv == -1 || nv >= 19 + 21)
      return ret;

   /* single element composition  */
   _len = strlcpy(utf8, pcur, sizeof(utf8));
   strlcpy(utf8 + _len, padd, sizeof(utf8) - _len);

   if ((tmp2 = strstr(cc1, utf8)))
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }
   else if ((tmp2 = strstr(cc2, utf8)))
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }
   if (tmp2 && tmp2 < cc2 + sizeof(cc2) - 10)
   {
      *((unsigned*)padd) = *((unsigned*)(tmp2 + 6)) & 0xffffff;
      return 0;
   }

   if (c1 >= 19)
      return ret;

   if (c1 == -1)
   {
      int tv = ((pcur[0] & 0x0f) << 12) | ((pcur[1] & 0x3f) << 6) | (pcur[2] & 0x03f);
      tv     = tv  - 0xac00;
      c1     = tv  / (28 * 21);
      c2     = (tv % (28 * 21)) / 28;
      c3     = (tv % (28));
      if (c1 < 0 || c1 >= 19 || c2 < 0 || c2 > 21 || c3 < 0 || c3 > 28)
         return ret;
   }

   if (c1 == -1 && c2 == -1 && c3 == 0)
      return ret;

   if (c2 == -1 && c3 == 0)
   {
      /* 2nd element attach */
      if (nv < 19)
         return ret;
      c2  = nv - 19;
   }
   else
      if (c2 >= 0 && c3 == 0)
      {
         if (nv < 19)
         {
            /* 3rd element attach */
            if (!(tmp1 = strstr(s1 + (19 + 21) * 3, padd)))
               return ret;
            c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
         }
         else
         {
            /* 2nd element transform */
            strlcpy(utf8, s1 + (19 + c2) * 3, 4);
            utf8[3] = 0;
            strlcat(utf8, padd, sizeof(utf8));
            if (    !(tmp2 = strstr(cc2, utf8))
                  || (tmp2 >= cc2 + sizeof(cc2) - 10))
               return ret;
            strlcpy(utf8, tmp2 + 6, 4);
            utf8[3] = 0;
            if (!(tmp1 = strstr(s1 + (19) * 3, utf8)))
               return ret;
            c2 = (int)((tmp1 - s1) / 3 - 19);
         }
      }
      else
         if (c3 > 0)
         {
            strlcpy(utf8, s1 + (19 + 21 + c3) * 3, 4);
            utf8[3] = 0;
            if (nv < 19)
            {
               /* 3rd element transform */
               strlcat(utf8, padd, sizeof(utf8));
               if (    !(tmp2 = strstr(cc3, utf8))
                     || (tmp2 >= cc3 + sizeof(cc3) - 10))
                     return ret;
               strlcpy(utf8, tmp2 + 6, 4);
               utf8[3] = 0;
               if (!(tmp1 = strstr(s1 + (19 + 21) * 3, utf8)))
                  return ret;
               c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
            }
            else
            {
               int tv = 0;
               if ((tmp2 = strstr(cc3, utf8)))
                  tv = (tmp2 - cc3) % 10;
               if (tv == 6)
               {
                  /*  complex 3rd element -> disassemble */
                  strlcpy(utf8, tmp2 - 3, 4);
                  if (!(tmp1 = strstr(s1, utf8)))
                     return ret;
                  tv = (int)((tmp1 - s1) / 3);
                  strlcpy(utf8, tmp2 - 6, 4);
                  if (!(tmp1 = strstr(s1 + (19 + 21) * 3, utf8)))
                     return ret;
                  c3 = (int)((tmp1 - s1) / 3 - 19 - 21);
               }
               else
               {
                  if (!(tmp1 = strstr(s1, utf8)) || (tmp1 - s1) >= 19 * 3)
                     return ret;
                  tv = (int)((tmp1 - s1) / 3);
                  c3 = 0;
               }
               *((unsigned*)padd) = get_kr_utf8(tv, nv - 19, 0);
               return get_kr_utf8(c1, c2, c3);
            }
         }
         else
            return ret;
   *((unsigned*)padd) = get_kr_utf8(c1, c2, c3);
   return 0;
}
#endif

/**
 * input_keyboard_line_event:
 * @state                    : Input keyboard line handle.
 * @character                : Inputted character.
 *
 * Called on every keyboard character event.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool input_keyboard_line_event(
      input_driver_state_t *input_st,
      input_keyboard_line_t *state, uint32_t character)
{
   char array[2];
   bool            ret         = false;
   const char            *word = NULL;
   char            c           = (character >= 128) ? '?' : character;

#ifdef HAVE_LANGEXTRA
   static uint32_t composition = 0;
   /* reset composition, when edit box is opened. */
   if (state->size == 0)
      composition = 0;
   /* reset composition, when 1 byte(=english) input */
   if (character && character < 0xff)
      composition = 0;
   if (IS_COMPOSITION(character) || IS_END_COMPOSITION(character))
   {
      size_t len = strlen((char*)&composition);
      if (composition && state->buffer && state->size >= len && state->ptr >= len)
      {
         memmove(state->buffer + state->ptr-len, state->buffer + state->ptr, len + 1);
         state->ptr  -= len;
         state->size -= len;
      }
      if (IS_COMPOSITION_KR(character) && composition)
      {
         unsigned new_comp;
         character   = character & 0xffffff;
         new_comp    = get_kr_composition((char*)&composition, (char*)&character);
         if (new_comp)
            input_keyboard_line_append(state, (char*)&new_comp, 3);
         composition = character;
      }
      else
      {
         if (IS_END_COMPOSITION(character))
            composition = 0;
         else
            composition = character & 0xffffff;
         character     &= 0xffffff;
      }
      if (len && composition == 0)
         word = state->buffer;
      if (character)
         input_keyboard_line_append(state, (char*)&character, strlen((char*)&character));
      word = state->buffer;
   }
   else
#endif

   /* Treat extended chars as ? as we cannot support
    * printable characters for unicode stuff. */

   if (c == '\r' || c == '\n')
   {
      state->cb(state->userdata, state->buffer);

      array[0] = c;
      array[1] = 0;

      ret      = true;
      word     = array;
   }
   else if (c == '\b' || c == '\x7f') /* 0x7f is ASCII for del */
   {
      if (state->ptr)
      {
         unsigned i;

         for (i = 0; i < input_st->osk_last_codepoint_len; i++)
         {
            memmove(state->buffer + state->ptr - 1,
                  state->buffer + state->ptr,
                  state->size - state->ptr + 1);
            state->ptr--;
            state->size--;
         }

         word     = state->buffer;
      }
   }
   else if (ISPRINT(c))
   {
      /* Handle left/right here when suitable */
      char *newbuf = (char*)
         realloc(state->buffer, state->size + 2);
      if (!newbuf)
         return false;

      memmove(newbuf + state->ptr + 1,
            newbuf + state->ptr,
            state->size - state->ptr + 1);
      newbuf[state->ptr] = c;
      state->ptr++;
      state->size++;
      newbuf[state->size] = '\0';

      state->buffer = newbuf;

      array[0] = c;
      array[1] = 0;

      word     = array;
   }

   /* OSK - update last character */
   if (word)
      osk_update_last_codepoint(
            &input_st->osk_last_codepoint,
            &input_st->osk_last_codepoint_len,
            word);

   return ret;
}


void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      unsigned *osk_last_codepoint,
      unsigned *osk_last_codepoint_len,
      int ptr,
      bool show_symbol_pages,
      const char *word,
      size_t word_len)
{
#ifdef HAVE_LANGEXTRA
   if (string_is_equal(word, "\xe2\x87\xa6")) /* backspace character */
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(word, "\xe2\x8f\x8e")) /* return character */
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else
   if (string_is_equal(word, "\xe2\x87\xa7")) /* up arrow */
      *osk_idx = OSK_UPPERCASE_LATIN;
   else if (string_is_equal(word, "\xe2\x87\xa9")) /* down arrow */
      *osk_idx = OSK_LOWERCASE_LATIN;
   else if (string_is_equal(word,"\xe2\x8a\x95")) /* plus sign (next button) */
   {
      if (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE) == RETRO_LANGUAGE_KOREAN   )
      {
         static int prv_osk = OSK_TYPE_UNKNOWN+1;
         if (*osk_idx < OSK_KOREAN_PAGE1 )
         {
            prv_osk = *osk_idx;
            *osk_idx =  OSK_KOREAN_PAGE1;
         }
         else
            *osk_idx = (enum osk_type)prv_osk;
      }
      else
      if (*osk_idx < (show_symbol_pages ? OSK_TYPE_LAST - 1 : OSK_SYMBOLS_PAGE1))
         *osk_idx = (enum osk_type)(*osk_idx + 1);
      else
         *osk_idx = ((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
   }
   else if (*osk_idx == OSK_KOREAN_PAGE1 && word && word_len == 3)
   {
      unsigned character = *((unsigned*)word) | 0x01000000;
      input_keyboard_line_event(&input_driver_st,  keyboard_line, character);
   }
#else
   if (string_is_equal(word, "Bksp"))
      input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
   else if (string_is_equal(word, "Enter"))
      input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
   else
   if (string_is_equal(word, "Upper"))
      *osk_idx = OSK_UPPERCASE_LATIN;
   else if (string_is_equal(word, "Lower"))
      *osk_idx = OSK_LOWERCASE_LATIN;
   else if (string_is_equal(word, "Next"))
      if (*osk_idx < (show_symbol_pages ? OSK_TYPE_LAST - 1 : OSK_SYMBOLS_PAGE1))
         *osk_idx = (enum osk_type)(*osk_idx + 1);
      else
         *osk_idx = ((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
#endif
   else
   {
      input_keyboard_line_append(keyboard_line, word, word_len);
      osk_update_last_codepoint(
            osk_last_codepoint,
            osk_last_codepoint_len,
            word);
   }
}

void *input_driver_init_wrap(input_driver_t *input, const char *name)
{
   void *ret                   = NULL;
   if (!input)
      return NULL;
   if ((ret = input->init(name)))
   {
      input_driver_init_joypads();
      return ret;
   }
   return NULL;
}

bool input_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled)
{
   int i                = (int)driver_find_index(
         "input_driver",
         settings->arrays.input_driver);

   if (i >= 0)
   {
      input_driver_st.current_driver = (input_driver_t*)input_drivers[i];
      RARCH_LOG("[Input]: Found %s: \"%s\".\n", prefix,
            input_driver_st.current_driver->ident);
   }
   else
   {
      input_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.input_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; input_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", input_drivers[d]->ident);
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (input_driver_t*)input_drivers[0];
      if (!tmp)
         return false;
      input_driver_st.current_driver = tmp;
   }

   return true;
}

void input_mapper_reset(void *data)
{
   unsigned i;
   input_mapper_t *handle = (input_mapper_t*)data;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;
      for (j = 0; j < 8; j++)
      {
         handle->analog_value[i][j]           = 0;
         handle->buttons[i].data[j]           = 0;
         handle->buttons[i].analogs[j]        = 0;
         handle->buttons[i].analog_buttons[j] = 0;
      }
   }
   for (i = 0; i < RETROK_LAST; i++)
      handle->key_button[i]         = 0;
   for (i = 0; i < (RETROK_LAST / 32 + 1); i++)
      handle->keys[i]               = 0;
}

/**
 * Sets the sensor state. Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 *
 * @param port
 * @param action
 * @param rate
 *
 * @return true if the sensor state has been successfully set
 **/
bool input_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   settings_t *settings        = config_get_ptr();
   bool input_sensors_enable   = settings->bools.input_sensors_enable;
   return input_driver_set_sensor(
      port, input_sensors_enable, action, rate);
}

const char *joypad_driver_name(unsigned i)
{
   if (!input_driver_st.primary_joypad || !input_driver_st.primary_joypad->name)
      return NULL;
   return input_driver_st.primary_joypad->name(i);
}

void joypad_driver_reinit(void *data, const char *joypad_driver_name)
{
   if (input_driver_st.primary_joypad)
   {
      const input_device_driver_t *tmp  = input_driver_st.primary_joypad;
      input_driver_st.primary_joypad    = NULL;
      /* Run poll one last time in order to detect disconnections */
      tmp->poll();
      tmp->destroy();
   }
#ifdef HAVE_MFI
   if (input_driver_st.secondary_joypad)
   {
      const input_device_driver_t *tmp  = input_driver_st.secondary_joypad;
      input_driver_st.secondary_joypad  = NULL;
      tmp->poll();
      tmp->destroy();
   }
#endif
   if (!input_driver_st.primary_joypad)
      input_driver_st.primary_joypad    = input_joypad_init_driver(joypad_driver_name, data);
#ifdef HAVE_MFI
   if (!input_driver_st.secondary_joypad)
      input_driver_st.secondary_joypad  = input_joypad_init_driver("mfi", data);
#endif
}

/**
 * Retrieves the sensor state associated with the provided port and ID.
 *
 * @param port
 * @param id    Sensor ID
 *
 * @return The current state associated with the port and ID as a float
 **/
float input_get_sensor_state(unsigned port, unsigned id)
{
   settings_t *settings                   = config_get_ptr();
   bool input_sensors_enable              = settings->bools.input_sensors_enable;

   return input_driver_get_sensor(port, input_sensors_enable, id);
}

/**
 * Sets the rumble state. Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 *
 * @param port      User number.
 * @param effect    Rumble effect.
 * @param strength  Strength of rumble effect.
 *
 * @return true if the rumble state has been successfully set
 **/
bool input_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   settings_t *settings                   = config_get_ptr();
   unsigned joy_idx                       = settings->uints.input_joypad_index[port];
   uint16_t scaled_strength               = strength;

   /* If gain setting is not suported, do software gain control */
   if (input_driver_st.primary_joypad)
   {
      if (!input_driver_st.primary_joypad->set_rumble_gain)
      {
         unsigned rumble_gain = settings->uints.input_rumble_gain;
         scaled_strength      = (rumble_gain * strength) / 100.0;
      }
   }

   return input_driver_set_rumble(
      port, joy_idx, effect, scaled_strength);
}

/**
 * Sets the rumble gain. Used by MENU_ENUM_LABEL_INPUT_RUMBLE_GAIN.
 *
 * @param gain  Rumble gain, 0-100 [%]
 *
 * @return true if the rumble gain has been successfully set
 **/
bool input_set_rumble_gain(unsigned gain)
{
   settings_t           *settings         = config_get_ptr();
   if (input_driver_set_rumble_gain(
            gain, settings->uints.input_max_users))
      return true;
   return false;
}

uint64_t input_driver_get_capabilities(void)
{
   if (  !input_driver_st.current_driver ||
         !input_driver_st.current_driver->get_capabilities)
      return 0;
   return input_driver_st.current_driver->get_capabilities(input_driver_st.current_data);
}

void input_driver_init_joypads(void)
{
   settings_t                   *settings    = config_get_ptr();
   if (!input_driver_st.primary_joypad)
      input_driver_st.primary_joypad        = input_joypad_init_driver(
         settings->arrays.input_joypad_driver,
         input_driver_st.current_data);
#ifdef HAVE_MFI
   if (!input_driver_st.secondary_joypad)
      input_driver_st.secondary_joypad      = input_joypad_init_driver(
            "mfi",
            input_driver_st.current_data);
#endif
}

bool input_key_pressed(int key, bool keyboard_pressed)
{
   /* If a keyboard key is pressed then immediately return
    * true, otherwise call button_is_pressed to determine
    * if the input comes from another input device */
   if (!(
            (key < RARCH_BIND_LIST_END)
            && keyboard_pressed
        )
      )
   {
      settings_t           *settings = config_get_ptr();
      const input_device_driver_t
         *joypad                     = (const input_device_driver_t*)
         input_driver_st.primary_joypad;
      const uint64_t bind_joykey     = input_config_binds[0][key].joykey;
      const uint64_t bind_joyaxis    = input_config_binds[0][key].joyaxis;
      const uint64_t autobind_joykey = input_autoconf_binds[0][key].joykey;
      const uint64_t autobind_joyaxis= input_autoconf_binds[0][key].joyaxis;
      uint16_t port                  = 0;
      float axis_threshold           = settings->floats.input_axis_threshold;
      const uint64_t joykey          = (bind_joykey != NO_BTN)
         ? bind_joykey  : autobind_joykey;
      const uint64_t joyaxis         = (bind_joyaxis != AXIS_NONE)
         ? bind_joyaxis : autobind_joyaxis;

      if ((uint16_t)joykey != NO_BTN && joypad->button(
               port, (uint16_t)joykey))
         return true;
      if (joyaxis != AXIS_NONE &&
            ((float)abs(joypad->axis(port, (uint32_t)joyaxis))
             / 0x8000) > axis_threshold)
         return true;
      return false;
   }
   return true;
}

bool video_driver_init_input(
      input_driver_t *tmp,
      settings_t *settings,
      bool verbosity_enabled)
{
   void              *new_data    = NULL;
   input_driver_t         **input = &input_driver_st.current_driver;
   if (*input)
      return true;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("[Video]: Graphics driver did not initialize an input driver."
         " Attempting to pick a suitable driver.\n");

   if (tmp)
      *input = tmp;
   else
   {
      if (!(input_driver_find_driver(
            settings, "input driver",
            verbosity_enabled)))
      {
         RARCH_ERR("[Video]: Cannot find input driver. Exiting ...\n");
         return false;
      }
   }

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (  !input_driver_st.current_driver ||
         !(new_data = input_driver_init_wrap(
               input_driver_st.current_driver,
               settings->arrays.input_joypad_driver)))
   {
      RARCH_ERR("[Video]: Cannot initialize input driver. Exiting ...\n");
      return false;
   }

   input_driver_st.current_data = new_data;

   return true;
}

bool input_driver_grab_mouse(void)
{
   if (!input_driver_st.current_driver || !input_driver_st.current_driver->grab_mouse)
      return false;
   input_driver_st.current_driver->grab_mouse(
         input_driver_st.current_data, true);
   return true;
}

bool input_driver_ungrab_mouse(void)
{
   if (!input_driver_st.current_driver || !input_driver_st.current_driver->grab_mouse)
      return false;
   input_driver_st.current_driver->grab_mouse(input_driver_st.current_data, false);
   return true;
}

void input_config_reset(void)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;

   memcpy(input_config_binds[0], retro_keybinds_1, sizeof(retro_keybinds_1));

   for (i = 1; i < MAX_USERS; i++)
      memcpy(input_config_binds[i], retro_keybinds_rest,
            sizeof(retro_keybinds_rest));

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Note: Don't use input_config_clear_device_name()
       * here, since this will re-index devices each time
       * (not required - we are setting all 'name indices'
       * to zero manually) */
      input_st->input_device_info[i].name[0]          = '\0';
      input_st->input_device_info[i].display_name[0]  = '\0';
      input_st->input_device_info[i].config_name[0]   = '\0';
      input_st->input_device_info[i].joypad_driver[0] = '\0';
      input_st->input_device_info[i].vid              = 0;
      input_st->input_device_info[i].pid              = 0;
      input_st->input_device_info[i].autoconfigured   = false;
      input_st->input_device_info[i].name_index       = 0;

      input_config_reset_autoconfig_binds(i);

      input_st->libretro_input_binds[i] = (const retro_keybind_set *)&input_config_binds[i];
   }
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t        *settings = config_get_ptr();

   if (settings && (port < MAX_USERS))
      configuration_set_uint(settings,
            settings->uints.input_libretro_device[port], id);
}

unsigned input_config_get_device(unsigned port)
{
   settings_t             *settings = config_get_ptr();

   if (settings && (port < MAX_USERS))
      return settings->uints.input_libretro_device[port];

   return RETRO_DEVICE_NONE;
}

const struct retro_keybind *input_config_get_bind_auto(
      unsigned port, unsigned id)
{
   settings_t        *settings = config_get_ptr();
   unsigned        joy_idx     = settings->uints.input_joypad_index[port];

   if (joy_idx < MAX_USERS)
      return &input_autoconf_binds[joy_idx][id];
   return NULL;
}

unsigned *input_config_get_device_ptr(unsigned port)
{
   settings_t             *settings = config_get_ptr();

   if (settings && (port < MAX_USERS))
      return &settings->uints.input_libretro_device[port];

   return NULL;
}

/* Adds an index to devices with the same name,
 * so they can be uniquely identified in the
 * frontend */
static void input_config_reindex_device_names(input_driver_state_t *input_st)
{
   unsigned i;
   unsigned j;
   unsigned name_index;

   /* Reset device name indices */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
      input_st->input_device_info[i].name_index       = 0;

   /* Scan device names */
   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      const char *device_name = input_config_get_device_name(i);

      /* If current device name is empty, or a non-zero
       * name index has already been assigned, continue
       * to the next device */
      if (
               string_is_empty(device_name)
            || input_st->input_device_info[i].name_index != 0)
         continue;

      /* > Uniquely named devices have a name index
       *   of 0
       * > Devices with the same name have a name
       *   index starting from 1 */
      name_index = 1;

      /* Loop over all devices following the current
       * selection */
      for (j = i + 1; j < MAX_INPUT_DEVICES; j++)
      {
         const char *next_device_name = input_config_get_device_name(j);

         if (string_is_empty(next_device_name))
            continue;

         /* Check if names match */
         if (string_is_equal(device_name, next_device_name))
         {
            /* If this is the first match, set a starting
             * index for the current device selection */
            if (input_st->input_device_info[i].name_index == 0)
               input_st->input_device_info[i].name_index       = name_index++;

            /* Set name index for the next device
             * (will keep incrementing as more matches
             *  are found) */
            input_st->input_device_info[j].name_index          = name_index++;
         }
      }
   }
}

const char *input_config_get_device_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_device_info[port].name))
      return NULL;
   return input_st->input_device_info[port].name;
}

const char *input_config_get_device_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_device_info[port].display_name))
      return NULL;
   return input_st->input_device_info[port].display_name;
}

const char *input_config_get_device_config_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_device_info[port].config_name))
      return NULL;
   return input_st->input_device_info[port].config_name;
}

const char *input_config_get_device_joypad_driver(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_device_info[port].joypad_driver))
      return NULL;
   return input_st->input_device_info[port].joypad_driver;
}

uint16_t input_config_get_device_vid(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].vid;
}

uint16_t input_config_get_device_pid(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].pid;
}

bool input_config_get_device_autoconfigured(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].autoconfigured;
}

unsigned input_config_get_device_name_index(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].name_index;
}

/* TODO/FIXME: This is required by linuxraw_joypad.c
 * and parport_joypad.c. These input drivers should
 * be refactored such that this dubious low-level
 * access is not required */
char *input_config_get_device_name_ptr(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return input_st->input_device_info[port].name;
}

size_t input_config_get_device_name_size(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   return sizeof(input_st->input_device_info[port].name);
}

void input_config_set_device_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(name))
      return;

   strlcpy(input_st->input_device_info[port].name, name,
         sizeof(input_st->input_device_info[port].name));

   input_config_reindex_device_names(input_st);
}

void input_config_set_device_display_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!string_is_empty(name))
      strlcpy(input_st->input_device_info[port].display_name, name,
            sizeof(input_st->input_device_info[port].display_name));
}

void input_config_set_device_config_name(unsigned port, const char *name)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!string_is_empty(name))
      strlcpy(input_st->input_device_info[port].config_name, name,
            sizeof(input_st->input_device_info[port].config_name));
}

void input_config_set_device_joypad_driver(unsigned port, const char *driver)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (!string_is_empty(driver))
      strlcpy(input_st->input_device_info[port].joypad_driver, driver,
            sizeof(input_st->input_device_info[port].joypad_driver));
}

void input_config_set_device_vid(unsigned port, uint16_t vid)
{
   input_driver_state_t *input_st        = &input_driver_st;
   input_st->input_device_info[port].vid = vid;
}

void input_config_set_device_pid(unsigned port, uint16_t pid)
{
   input_driver_state_t *input_st        = &input_driver_st;
   input_st->input_device_info[port].pid = pid;
}

void input_config_set_device_autoconfigured(unsigned port, bool autoconfigured)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].autoconfigured = autoconfigured;
}

void input_config_set_device_name_index(unsigned port, unsigned name_index)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].name_index = name_index;
}

void input_config_clear_device_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].name[0] = '\0';
   input_config_reindex_device_names(input_st);
}

void input_config_clear_device_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].display_name[0] = '\0';
}

void input_config_clear_device_config_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].config_name[0] = '\0';
}

void input_config_clear_device_joypad_driver(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].joypad_driver[0] = '\0';
}

const char *input_config_get_mouse_display_name(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_mouse_info[port].display_name))
      return NULL;
   return input_st->input_mouse_info[port].display_name;
}

void input_config_set_mouse_display_name(unsigned port, const char *name)
{
   char name_ascii[NAME_MAX_LENGTH];
   input_driver_state_t *input_st = &input_driver_st;

   name_ascii[0] = '\0';

   /* Strip non-ASCII characters */
   if (!string_is_empty(name))
      string_copy_only_ascii(name_ascii, name);

   if (!string_is_empty(name_ascii))
      strlcpy(input_st->input_mouse_info[port].display_name, name_ascii,
            sizeof(input_st->input_mouse_info[port].display_name));
}

void input_keyboard_mapping_bits(unsigned mode, unsigned key)
{
   input_driver_state_t *input_st = &input_driver_st;
   switch (mode)
   {
      case 0:
         BIT512_CLEAR_PTR(&input_st->keyboard_mapping_bits, key);
         break;
      case 1:
         BIT512_SET_PTR(&input_st->keyboard_mapping_bits, key);
         break;
      default:
         break;
   }
}

void config_read_keybinds_conf(void *data)
{
   unsigned i;
   config_file_t            *conf = (config_file_t*)data;
   bool key_store[RETROK_LAST]    = {0};

   if (!conf)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;

      for (j = 0; input_config_bind_map_get_valid(j); j++)
      {
         char str[NAME_MAX_LENGTH];
         const struct input_bind_map *keybind =
            (const struct input_bind_map*)INPUT_CONFIG_BIND_MAP_GET(j);
         struct retro_keybind *bind = &input_config_binds[i][j];
         bool meta                  = false;
         const char *prefix         = NULL;
         const char *btn            = NULL;
         struct config_entry_list
            *entry                  = NULL;


         if (!bind || !bind->valid || !keybind)
            continue;
         if (!keybind->valid)
            continue;
         meta                       = keybind->meta;
         btn                        = keybind->base;
         prefix                     = input_config_get_prefix(i, meta);
         if (!btn || !prefix)
            continue;

         fill_pathname_join_delim(str, prefix, btn,  '_', sizeof(str));

         /* Clear old mapping bit unless just recently set */
         if (!key_store[bind->key])
            input_keyboard_mapping_bits(0, bind->key);

         entry                      = config_get_entry(conf, str);
         if (entry && !string_is_empty(entry->value))
            bind->key               = input_config_translate_str_to_rk(
                  entry->value, strlen(entry->value));

         /* Store new mapping bit and remember it for a while
          * so that next clear leaves the new key alone */
         input_keyboard_mapping_bits(1, bind->key);
         key_store[bind->key]       = true;

         input_config_parse_joy_button  (str, conf, prefix, btn, bind);
         input_config_parse_joy_axis    (str, conf, prefix, btn, bind);
         input_config_parse_mouse_button(str, conf, prefix, btn, bind);
      }
   }
}

#ifdef HAVE_COMMAND
void input_driver_init_command(input_driver_state_t *input_st,
      settings_t *settings)
{
#ifdef HAVE_STDIN_CMD
   bool input_stdin_cmd_enable       = settings->bools.stdin_cmd_enable;

   if (input_stdin_cmd_enable)
   {
      input_driver_state_t *input_st = &input_driver_st;
      bool grab_stdin                =
         input_st->current_driver->grab_stdin &&
         input_st->current_driver->grab_stdin(input_st->current_data);
      if (grab_stdin)
      {
         RARCH_WARN("stdin command interface is desired, "
               "but input driver has already claimed stdin.\n"
               "Cannot use this command interface.\n");
      }
      else
      {
         input_st->command[0] = command_stdin_new();
         if (!input_st->command[0])
            RARCH_ERR("Failed to initialize the stdin command interface.\n");
      }
   }
#endif

   /* Initialize the network command interface */
#ifdef HAVE_NETWORK_CMD
   {
      bool input_network_cmd_enable = settings->bools.network_cmd_enable;
      if (input_network_cmd_enable)
      {
         unsigned network_cmd_port  = settings->uints.network_cmd_port;
         if (!(input_st->command[1] = command_network_new(network_cmd_port)))
            RARCH_ERR("Failed to initialize the network command interface.\n");
      }
   }
#endif

#ifdef HAVE_LAKKA
   if (!(input_st->command[2] = command_uds_new()))
      RARCH_ERR("Failed to initialize the UDS command interface.\n");
#endif
}

void input_driver_deinit_command(input_driver_state_t *input_st)
{
   int i;
   for (i = 0; i < (int)ARRAY_SIZE(input_st->command); i++)
   {
      if (input_st->command[i])
         input_st->command[i]->destroy(
            input_st->command[i]);

      input_st->command[i] = NULL;
    }
}
#endif

void input_game_focus_free(void)
{
   input_game_focus_state_t *game_focus_st = &input_driver_st.game_focus_state;

   /* Ensure that game focus mode is disabled */
   if (game_focus_st->enabled)
   {
      enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_OFF;
      command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);
   }

   game_focus_st->enabled        = false;
   game_focus_st->core_requested = false;
}

#ifdef HAVE_OVERLAY
static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!video_st->current_video || !video_st->current_video->overlay_interface)
      return false;
   video_st->current_video->overlay_interface(video_st->data, iface);
   return true;
}

static void input_overlay_enable_(bool enable)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;
   float opacity                  = (ol && (ol->flags & INPUT_OVERLAY_IS_OSK))
      ? settings->floats.input_osk_overlay_opacity
      : settings->floats.input_overlay_opacity;
   bool auto_rotate               = settings->bools.input_overlay_auto_rotate;
   bool hide_mouse_cursor         = !settings->bools.input_overlay_show_mouse_cursor
         && settings->bools.video_fullscreen;

   if (!ol)
      return;

   if (enable)
   {
      /* Set video interface */
      ol->iface_data = video_st->data;
      if (!video_driver_overlay_interface(&ol->iface) || !ol->iface)
      {
         RARCH_ERR("Overlay interface is not present in video driver.\n");
         ol->flags &= ~INPUT_OVERLAY_ALIVE;
         return;
      }

      /* Load last-active overlay */
      input_overlay_load_active(input_st->overlay_visibility, ol, opacity);

      /* Adjust to current settings */
      command_event(CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, NULL);

      if (auto_rotate)
         input_overlay_auto_rotate_(
               video_st->width, video_st->height, true, ol);

      /* Enable */
      if (ol->iface->enable)
         ol->iface->enable(ol->iface_data, true);

      ol->flags |= (INPUT_OVERLAY_ENABLE | INPUT_OVERLAY_BLOCKED);

      if (     hide_mouse_cursor
            && video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, false);
   }
   else
   {
      /* Disable and clear input state */
      ol->flags &= ~INPUT_OVERLAY_ENABLE;

      if (ol->iface && ol->iface->enable)
         ol->iface->enable(ol->iface_data, false);
      ol->iface = NULL;

      memset(&ol->overlay_state, 0, sizeof(input_overlay_state_t));
   }
}

static void input_overlay_deinit(void)
{
   input_overlay_free(input_driver_st.overlay_ptr);
   input_driver_st.overlay_ptr = NULL;

   input_overlay_free(input_driver_st.overlay_cache_ptr);
   input_driver_st.overlay_cache_ptr = NULL;
}

static void input_overlay_move_to_cache(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol       = input_st->overlay_ptr;

   if (!ol)
      return;

   /* Free existing cache */
   input_overlay_free(input_driver_st.overlay_cache_ptr);

   /* Disable current overlay */
   input_overlay_enable_(false);

   /* Move to cache */
   input_st->overlay_cache_ptr = ol;
   input_st->overlay_ptr       = NULL;
}

static void input_overlay_swap_with_cached(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol;

   /* Disable current overlay */
   input_overlay_enable_(false);

   /* Swap with cached */
   ol                          = input_st->overlay_cache_ptr;
   input_st->overlay_cache_ptr = input_st->overlay_ptr;
   input_st->overlay_ptr       = ol;

   /* Enable and update to current settings */
   input_overlay_enable_(true);
}

void input_overlay_unload(void)
{
   settings_t      *settings   = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   /* Free if overlays disabled or initing/deiniting core */
   if (     !settings->bools.input_overlay_enable
         || !(runloop_st->flags & RUNLOOP_FLAG_IS_INITED)
         ||  (runloop_st->flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
      input_overlay_deinit();
   else
      input_overlay_move_to_cache();
}

void input_overlay_set_visibility(int overlay_idx,
      enum overlay_visibility vis)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t      *ol       = input_st->overlay_ptr;

   if (!input_st->overlay_visibility)
   {
      unsigned i;
      input_st->overlay_visibility = (enum overlay_visibility *)calloc(
            MAX_VISIBILITY, sizeof(enum overlay_visibility));

      for (i = 0; i < MAX_VISIBILITY; i++)
         input_st->overlay_visibility[i] = OVERLAY_VISIBILITY_DEFAULT;
   }

   input_st->overlay_visibility[overlay_idx] = vis;

   if (!ol)
      return;
   if (vis == OVERLAY_VISIBILITY_HIDDEN)
      ol->iface->set_alpha(ol->iface_data, overlay_idx, 0.0);
}

static bool input_overlay_want_hidden(void)
{
   settings_t *settings = config_get_ptr();
   bool hide            = false;

#ifdef HAVE_MENU
   if (settings->bools.input_overlay_hide_in_menu)
      hide = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) != 0;
#endif
   if (settings->bools.input_overlay_hide_when_gamepad_connected)
      hide = hide || (input_config_get_device_name(0) != NULL);

   return hide;
}

void input_overlay_check_mouse_cursor(void)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   video_driver_state_t *video_st = video_state_get_ptr();
   input_overlay_t *ol            = input_st->overlay_ptr;

   if (     ol && (ol->flags & INPUT_OVERLAY_ENABLE)
         && video_st->poke
         && video_st->poke->show_mouse)
   {
      if (settings->bools.input_overlay_show_mouse_cursor)
         video_st->poke->show_mouse(video_st->data, true);
      else if (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE)
         video_st->poke->show_mouse(video_st->data, false);
   }
}

/* task_data = overlay_task_data_t* */
static void input_overlay_loaded(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   settings_t           *settings = config_get_ptr();
   overlay_task_data_t  *data     = (overlay_task_data_t*)task_data;
   input_overlay_t      *ol       = NULL;
   input_driver_state_t *input_st = &input_driver_st;
   bool enable_overlay            = !input_overlay_want_hidden()
         && settings->bools.input_overlay_enable;
#ifdef HAVE_MENU
   uint16_t overlay_types;
#endif

   if (err)
      return;

   ol              = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays    = data->overlays;
   ol->size        = data->size;
   ol->active      = data->active;
   ol->path        = data->overlay_path;
   ol->next_index  = (unsigned)((ol->index + 1) % ol->size);
   ol->flags      |= INPUT_OVERLAY_ALIVE;
   if (data->flags & OVERLAY_LOADER_IS_OSK)
      ol->flags   |= INPUT_OVERLAY_IS_OSK;
#ifdef HAVE_MENU
   overlay_types   = data->overlay_types;
#endif

   free(data);

   /* Due to the asynchronous nature of overlay loading
    * it is possible for overlay_ptr to be non-NULL here
    * > Ensure it is free()'d before assigning new pointer */
   if (input_st->overlay_ptr)
      input_overlay_free(input_st->overlay_ptr);
   input_st->overlay_ptr = ol;

   /* Enable or disable the overlay */
   input_overlay_enable_(enable_overlay);

   /* Abort if enable failed */
   if (!(ol->flags & INPUT_OVERLAY_ALIVE))
   {
      input_st->overlay_ptr = NULL;
      input_overlay_free(ol);
      return;
   }

   /* Cache or free if hidden */
   if (!enable_overlay)
      input_overlay_unload();

   input_overlay_set_eightway_diagonal_sensitivity();

#ifdef HAVE_MENU
   /* Update menu entries if this is the main overlay */
   if (!(ol->flags & INPUT_OVERLAY_IS_OSK))
   {
      struct menu_state *menu_st = menu_state_get_ptr();

      if (menu_st->overlay_types != overlay_types)
      {
         menu_st->overlay_types = overlay_types;
         menu_st->flags        |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
      }
   }
#endif
}

void input_overlay_init(void)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;
   input_overlay_t *ol            = input_st->overlay_ptr;
   input_overlay_t *ol_cache      = input_st->overlay_cache_ptr;
   bool want_osk                  =
            (input_st->flags & INP_FLAG_KB_LINEFEED_ENABLE)
         && !string_is_empty(settings->paths.path_osk_overlay);
   const char *path_overlay       = want_osk
         ? settings->paths.path_osk_overlay
         : settings->paths.path_overlay;
   bool want_hidden               = input_overlay_want_hidden();
   bool overlay_shown             = ol
         && (ol->flags & INPUT_OVERLAY_ENABLE)
         && string_is_equal(path_overlay, ol->path);
   bool overlay_cached            = ol_cache
         && (ol_cache->flags & INPUT_OVERLAY_ALIVE)
         && string_is_equal(path_overlay, ol_cache->path);
   bool overlay_hidden            = !ol && overlay_cached;

#if defined(GEKKO)
   /* Avoid a crash at startup or even when toggling overlay in rgui */
   if (frontend_driver_get_free_memory() < (3 * 1024 * 1024))
      return;
#endif

   /* Cancel if overlays disabled or task already done */
   if (     !settings->bools.input_overlay_enable
         || ( want_hidden && overlay_hidden)
         || (!want_hidden && overlay_shown))
      return;

   /* Restore if cached */
   if (!want_hidden && overlay_cached)
   {
      input_overlay_swap_with_cached();
      return;
   }

   /* Cache current overlay when loading a different type */
   if (want_osk != (ol && (ol->flags & INPUT_OVERLAY_IS_OSK)))
      input_overlay_unload();
   else
      input_overlay_deinit();

   /* Start task */
   task_push_overlay_load_default(
         input_overlay_loaded, path_overlay, want_osk, NULL);
}
#endif

void input_pad_connect(unsigned port, input_device_driver_t *driver)
{
   if (port >= MAX_USERS || !driver)
   {
      RARCH_ERR("[Input]: input_pad_connect: bad parameters\n");
      return;
   }

   input_autoconfigure_connect(driver->name(port), NULL, driver->ident,
          port, 0, 0);
}

static bool input_keys_pressed_other_sources(
      input_driver_state_t *input_st,
      unsigned i,
      input_bits_t* p_new_state)
{
#ifdef HAVE_COMMAND
   int j;
   for (j = 0; j < (int)ARRAY_SIZE(input_st->command); j++)
      if ((i < RARCH_BIND_LIST_END) && input_st->command[j]
         && input_st->command[j]->state[i])
         return true;
#endif

#ifdef HAVE_OVERLAY
   if (               input_st->overlay_ptr &&
         ((BIT256_GET(input_st->overlay_ptr->overlay_state.buttons, i))))
      return true;
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Only process key presses related to game input if using Remote RetroPad */
   if (i < RARCH_CUSTOM_BIND_LIST_END
         && input_st->remote
         && INPUT_REMOTE_KEY_PRESSED(input_st, i, 0))
      return true;
#endif

   return false;
}

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 */
static void input_keys_pressed(
      unsigned port,
      bool is_menu,
      unsigned input_hotkey_block_delay,
      input_bits_t *p_new_state,
      const retro_keybind_set *binds,
      const struct retro_keybind *binds_norm,
      const struct retro_keybind *binds_auto,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;
   bool block_hotkey[RARCH_BIND_LIST_END];
   bool libretro_hotkey_set       =
            binds[port][RARCH_ENABLE_HOTKEY].joykey                 != NO_BTN
         || binds[port][RARCH_ENABLE_HOTKEY].joyaxis                != AXIS_NONE
         || input_autoconf_binds[port][RARCH_ENABLE_HOTKEY].joykey  != NO_BTN
         || input_autoconf_binds[port][RARCH_ENABLE_HOTKEY].joyaxis != AXIS_NONE;
   bool keyboard_hotkey_set       =
         binds[port][RARCH_ENABLE_HOTKEY].key != RETROK_UNKNOWN;

   if (!binds)
      return;

   if (     binds[port][RARCH_ENABLE_HOTKEY].valid
         && CHECK_INPUT_DRIVER_BLOCK_HOTKEY(binds_norm, binds_auto))
   {
      if (input_state_wrap(
            input_st->current_driver,
            input_st->current_data,
            input_st->primary_joypad,
            sec_joypad,
            joypad_info,
            binds,
            (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
            port, RETRO_DEVICE_JOYPAD, 0,
            RARCH_ENABLE_HOTKEY))
      {
         if (input_st->input_hotkey_block_counter < input_hotkey_block_delay)
            input_st->input_hotkey_block_counter++;
         else
            input_st->flags |= INP_FLAG_BLOCK_LIBRETRO_INPUT;
      }
      else
      {
         input_st->input_hotkey_block_counter = 0;
         input_st->flags |= INP_FLAG_BLOCK_HOTKEY;
      }
   }

   if (!is_menu && binds[port][RARCH_GAME_FOCUS_TOGGLE].valid)
   {
      const struct retro_keybind *focus_binds_auto =
            &input_autoconf_binds[port][RARCH_GAME_FOCUS_TOGGLE];
      const struct retro_keybind *focus_normal     =
            &binds[port][RARCH_GAME_FOCUS_TOGGLE];

      /* Allows Game Focus toggle hotkey to still work
       * even though every hotkey is blocked */
      if (CHECK_INPUT_DRIVER_BLOCK_HOTKEY(focus_normal, focus_binds_auto))
      {
         if (input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               input_st->primary_joypad,
               sec_joypad,
               joypad_info,
               binds,
               (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
               port, RETRO_DEVICE_JOYPAD, 0,
               RARCH_GAME_FOCUS_TOGGLE))
            input_st->flags &= ~INP_FLAG_BLOCK_HOTKEY;
      }
   }

   {
      int16_t ret                 = 0;
      bool libretro_input_pressed = false;

      /* Check libretro input if emulated device type is active,
       * except device type must be always active in menu. */
      if (     !(input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT)
            && !(!is_menu && !input_config_get_device(port)))
         ret = input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               input_st->primary_joypad,
               sec_joypad,
               joypad_info,
               binds,
               (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
               port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK);

      for (i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         if (     (ret & (UINT64_C(1) << i))
               || input_keys_pressed_other_sources(input_st, i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
            libretro_input_pressed = true;
         }
      }

      if (!libretro_input_pressed)
      {
         bool keyboard_menu_pressed = false;

         /* Ignore keyboard menu toggle button and check
          * joypad menu toggle button for pressing
          * it without 'enable_hotkey', because Guide button
          * is not part of the usual buttons. */
         i = RARCH_MENU_TOGGLE;

         keyboard_menu_pressed = binds[port][i].valid
               && input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_KEYBOARD, 0,
                     input_config_binds[port][i].key);

         if (!keyboard_menu_pressed)
         {
            bool bit_pressed = binds[port][i].valid
                  && input_state_wrap(
                        input_st->current_driver,
                        input_st->current_data,
                        input_st->primary_joypad,
                        sec_joypad,
                        joypad_info,
                        binds,
                        (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                        port, RETRO_DEVICE_JOYPAD, 0, i);

            if (
                     bit_pressed
                  || BIT64_GET(lifecycle_state, i)
                  || input_keys_pressed_other_sources(input_st, i, p_new_state))
            {
               BIT256_SET_PTR(p_new_state, i);
            }
         }
      }
   }

   /* Hotkeys are only relevant for first port */
   if (port > 0)
      return;

   /* Check hotkeys to block keyboard and joypad hotkeys separately.
    * This looks complicated because hotkeys must be unblocked based
    * on the device type depending if 'enable_hotkey' is set or not.. */
   if (     input_st->flags & INP_FLAG_BLOCK_HOTKEY
         && (libretro_hotkey_set && keyboard_hotkey_set))
   {
      /* Block everything when hotkey bind exists for both device types */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
         block_hotkey[i] = true;
   }
   else if (input_st->flags & INP_FLAG_BLOCK_HOTKEY
         && (!libretro_hotkey_set || !keyboard_hotkey_set))
   {
      /* Block selectively when hotkey bind exists for either device type */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         bool keyboard_hotkey_pressed = false;
         bool libretro_hotkey_pressed = false;

         /* Default */
         block_hotkey[i]              = true;

         /* No 'enable_hotkey' in joypad */
         if (!libretro_hotkey_set)
         {
            if (     binds[port][i].joykey  != NO_BTN
                  || binds[port][i].joyaxis != AXIS_NONE)
            {
               /* Allow blocking if keyboard hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_KEYBOARD, 0,
                     input_config_binds[port][i].key))
               {
                  keyboard_hotkey_pressed = true;

                  /* Always block */
                  block_hotkey[i] = true;
               }

               /* Deny blocking if joypad hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_JOYPAD, 0,
                     i))
               {
                  libretro_hotkey_pressed = true;

                  /* Only deny block if keyboard is not pressed */
                  if (!keyboard_hotkey_pressed)
                     block_hotkey[i] = false;
               }
            }
         }

         /* No 'enable_hotkey' in keyboard */
         if (!keyboard_hotkey_set)
         {
            if (binds[port][i].key != RETROK_UNKNOWN)
            {
               /* Deny blocking if keyboard hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_KEYBOARD, 0,
                     input_config_binds[port][i].key))
               {
                  keyboard_hotkey_pressed = true;

                  /* Only deny block if joypad is not pressed */
                  if (!libretro_hotkey_pressed)
                     block_hotkey[i] = false;
               }

               /* Allow blocking if joypad hotkey is pressed */
               if (input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_JOYPAD, 0,
                     i))
               {

                  /* Only block if keyboard is not pressed */
                  if (!keyboard_hotkey_pressed)
                     block_hotkey[i] = true;
               }
            }
         }
      }
   }
   else
   {
      /* Clear everything */
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
         block_hotkey[i] = false;
   }

   {
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         bool other_pressed = input_keys_pressed_other_sources(input_st, i, p_new_state);
         bool bit_pressed   = binds[port][i].valid
               && input_state_wrap(
                     input_st->current_driver,
                     input_st->current_data,
                     input_st->primary_joypad,
                     sec_joypad,
                     joypad_info,
                     binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     port, RETRO_DEVICE_JOYPAD, 0,
                     i);

         if (     bit_pressed
               || other_pressed
               || BIT64_GET(lifecycle_state, i))
         {
            if (libretro_hotkey_set || keyboard_hotkey_set)
            {
               /* Do not block "other source" (input overlay) presses */
               if (block_hotkey[i] && !other_pressed)
                  continue;
            }

            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }
}

#ifdef HAVE_BSV_MOVIE
/* Forward declaration */
void bsv_movie_free(bsv_movie_t*);

void bsv_movie_enqueue(input_driver_state_t *input_st, bsv_movie_t * state, enum bsv_flags flags)
{ 
   if (input_st->bsv_movie_state_next_handle)
      bsv_movie_free(input_st->bsv_movie_state_next_handle);
   input_st->bsv_movie_state_next_handle    = state;
   input_st->bsv_movie_state.flags          = flags;
}

void bsv_movie_deinit(input_driver_state_t *input_st)
{
   if (input_st->bsv_movie_state_handle)
      bsv_movie_free(input_st->bsv_movie_state_handle);
   input_st->bsv_movie_state_handle = NULL;
}

void bsv_movie_deinit_full(input_driver_state_t *input_st)
{
   bsv_movie_deinit(input_st);
   if (input_st->bsv_movie_state_next_handle)
      bsv_movie_free(input_st->bsv_movie_state_next_handle);
   input_st->bsv_movie_state_next_handle = NULL;
}

void bsv_movie_frame_rewind(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   bsv_movie_t         *handle    = input_st->bsv_movie_state_handle;

   if (!handle)
      return;

   handle->did_rewind = true;

   if (     (handle->frame_ptr <= 1)
         && (handle->frame_pos[0] == handle->min_file_pos))
   {
      /* If we're at the beginning... */
      handle->frame_ptr = 0;
      intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
   else
   {
      /* First time rewind is performed, the old frame is simply replayed.
       * However, playing back that frame caused us to read data, and push
       * data to the ring buffer.
       *
       * Sucessively rewinding frames, we need to rewind past the read data,
       * plus another. */
      handle->frame_ptr = (handle->frame_ptr -
            (handle->first_rewind ? 1 : 2)) & handle->frame_mask;
      intfstream_seek(handle->file,
            (int)handle->frame_pos[handle->frame_ptr], SEEK_SET);
   }

   if (intfstream_tell(handle->file) <= (long)handle->min_file_pos)
   {
      /* We rewound past the beginning. */

      if (!handle->playback)
      {
         retro_ctx_serialize_info_t serial_info;

         /* If recording, we simply reset
          * the starting point. Nice and easy. */

         intfstream_seek(handle->file, 4 * sizeof(uint32_t), SEEK_SET);

         serial_info.data = handle->state;
         serial_info.size = handle->state_size;

         core_serialize(&serial_info);

         intfstream_write(handle->file, handle->state, handle->state_size);
      }
      else
         intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
}

/* Zero out key events when playing back or recording */
static void bsv_movie_handle_clear_key_events(bsv_movie_t *movie)
{
   movie->key_event_count = 0;
}

void bsv_movie_handle_push_key_event(bsv_movie_t *movie,
      uint8_t down, uint16_t mod, uint32_t code, uint32_t character)
{
   bsv_key_data_t data;
   data.down                                 = down;
   data.mod                                  = swap_if_big16(mod);
   data.code                                 = swap_if_big32(code);
   data.character                            = swap_if_big32(character);
   movie->key_events[movie->key_event_count] = data;
   movie->key_event_count++;
}

void bsv_movie_finish_rewind(input_driver_state_t *input_st)
{
   bsv_movie_t *handle  = input_st->bsv_movie_state_handle;
   if (!handle)
      return;
   handle->frame_ptr    = (handle->frame_ptr + 1) & handle->frame_mask;
   handle->first_rewind = !handle->did_rewind;
   handle->did_rewind   = false;
}

void bsv_movie_next_frame(input_driver_state_t *input_st)
{
   settings_t *settings           = config_get_ptr();
   unsigned checkpoint_interval   = settings->uints.replay_checkpoint_interval;
   /* if bsv_movie_state_next_handle is not null, deinit and set
      bsv_movie_state_handle to bsv_movie_state_next_handle and clear
      next_handle */
   bsv_movie_t         *handle    = input_st->bsv_movie_state_handle;
   if (input_st->bsv_movie_state_next_handle)
   {
      if(handle)
         bsv_movie_deinit(input_st);
      handle = input_st->bsv_movie_state_next_handle;
      input_st->bsv_movie_state_handle = handle;
      input_st->bsv_movie_state_next_handle = NULL;
   }

   if (!handle)
      return;
#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed())
      return;
#endif

   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
   {
      int i;
      /* write key events, frame is over */
      intfstream_write(handle->file, &(handle->key_event_count), 1);
      for (i = 0; i < handle->key_event_count; i++)
         intfstream_write(handle->file, &(handle->key_events[i]), sizeof(bsv_key_data_t));
      bsv_movie_handle_clear_key_events(handle);

      /* Maybe record checkpoint */
      if (checkpoint_interval != 0 && handle->frame_ptr > 0 && (handle->frame_ptr % (checkpoint_interval*60) == 0))
      {
         retro_ctx_serialize_info_t serial_info;
         uint8_t frame_tok = REPLAY_TOKEN_CHECKPOINT_FRAME;
         size_t info_size  = core_serialize_size();
         uint64_t size     = swap_if_big64(info_size);
         uint8_t *st       = (uint8_t*)malloc(info_size);
         serial_info.data  = st;
         serial_info.size  = info_size;
         core_serialize(&serial_info);
         /* "next frame is a checkpoint" */
         intfstream_write(handle->file, (uint8_t *)(&frame_tok), sizeof(uint8_t));
         intfstream_write(handle->file, &size, sizeof(uint64_t));
         intfstream_write(handle->file, st, info_size);
         free(st);
      }
      else
      {
         uint8_t frame_tok = REPLAY_TOKEN_REGULAR_FRAME;
         /* write "next frame is not a checkpoint" */
         intfstream_write(handle->file, (uint8_t *)(&frame_tok), sizeof(uint8_t));
      }
   }

   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
   {
      /* read next key events, a frame happened for sure? but don't apply them yet */
      if (handle->key_event_count != 0)
      {
         RARCH_ERR("[Replay] Keyboard replay reading next frame while some unused keys still in queue\n");
      }
      if (intfstream_read(handle->file, &(handle->key_event_count), 1) == 1)
      {
         int i;
         for (i = 0; i < handle->key_event_count; i++)
         {
            if (intfstream_read(handle->file, &(handle->key_events[i]), sizeof(bsv_key_data_t)) != sizeof(bsv_key_data_t))
            {
               /* Unnatural EOF */
               RARCH_ERR("[Replay] Keyboard replay ran out of keyboard inputs too early\n");
               input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
               return;
            }
         }
      }
      else
      {
         RARCH_LOG("[Replay] EOF after buttons\n",handle->key_event_count);
         /* Natural(?) EOF */
         input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
         return;
      }

      {
        uint8_t next_frame_type=REPLAY_TOKEN_INVALID;
        if (intfstream_read(handle->file, (uint8_t *)(&next_frame_type), sizeof(uint8_t)) != sizeof(uint8_t))
        {
           /* Unnatural EOF */
           RARCH_ERR("[Replay] Replay ran out of frames\n");
           input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
           return;
        }
        else if (next_frame_type == REPLAY_TOKEN_REGULAR_FRAME)
        {
           /* do nothing */
        }
        else if (next_frame_type == REPLAY_TOKEN_CHECKPOINT_FRAME)
        {
           uint64_t size;
           uint8_t *st;
           retro_ctx_serialize_info_t serial_info;

           if (intfstream_read(handle->file, &(size), sizeof(uint64_t)) != sizeof(uint64_t))
           {
              RARCH_ERR("[Replay] Replay ran out of frames\n");
              input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
              return;
           }

           size = swap_if_big64(size);
           st   = (uint8_t*)malloc(size);
           if (intfstream_read(handle->file, st, size) != (int64_t)size)
           {
              RARCH_ERR("[Replay] Replay checkpoint truncated\n");
              input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
              free(st);
              return;
           }

           serial_info.data_const = st;
           serial_info.size       = size;
           core_unserialize(&serial_info);
           free(st);
        }
      }
   }
   handle->frame_pos[handle->frame_ptr] = intfstream_tell(handle->file);
}

size_t replay_get_serialize_size(void)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_PLAYBACK))
      return sizeof(int32_t)+intfstream_tell(input_st->bsv_movie_state_handle->file);
   return 0;
}

bool replay_get_serialized_data(void* buffer)
{
   input_driver_state_t *input_st = &input_driver_st;
   bsv_movie_t *handle            = input_st->bsv_movie_state_handle;

   if (input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_RECORDING | BSV_FLAG_MOVIE_PLAYBACK))
   {
      long file_end           = intfstream_tell(handle->file);
      long read_amt           = 0;
      long file_end_lil       = swap_if_big32(file_end);
      uint8_t *file_end_bytes = (uint8_t *)(&file_end_lil);
      uint8_t *buf            = buffer;
      buf[0]                  = file_end_bytes[0];
      buf[1]                  = file_end_bytes[1];
      buf[2]                  = file_end_bytes[2];
      buf[3]                  = file_end_bytes[3];
      buf                    += 4;
      intfstream_rewind(handle->file);
      read_amt                = intfstream_read(handle->file, (void *)buf, file_end);
      if (read_amt != file_end)
         RARCH_ERR("[Replay] Failed to write correct number of replay bytes into state file: %d / %d\n", read_amt, file_end);
   }
   return true;
}

bool replay_set_serialized_data(void* buf)
{
   uint8_t *buffer                = buf;
   input_driver_state_t *input_st = &input_driver_st;
   bool playback                  = (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)  ? true : false;
   bool recording                 = (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING) ? true : false;

   /* If there is no current replay, ignore this entirely.
      TODO/FIXME: Later, consider loading up the replay
      and allow the user to continue it?
      Or would that be better done from the replay hotkeys?
    */
   if (!(playback || recording))
      return true;

   if (!buffer)
   {
      if (recording)
      {
         const char *str = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT);
         runloop_msg_queue_push(str,
            1, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         RARCH_ERR("[Replay] %s.\n", str);
         return false;
      }

      if (playback)
      {
         const char *str = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT);
         runloop_msg_queue_push(str,
            1, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
         RARCH_WARN("[Replay] %s.\n", str);
         movie_stop(input_st);
      }
   }
   else
   {
      int32_t loaded_len       = swap_if_big32(((int32_t *)buffer)[0]);
      /* TODO: should factor the next few lines away, magic numbers ahoy */
      uint32_t *header         = (uint32_t *)(buffer+sizeof(int32_t));
      int64_t *identifier_spot = (int64_t *)(header+4);
      int64_t identifier       = swap_if_big64(*identifier_spot);
      int64_t handle_idx       = intfstream_tell(input_st->bsv_movie_state_handle->file);
      bool is_compatible       = identifier == input_st->bsv_movie_state_handle->identifier;

      if (is_compatible)
      {
         /* If the state is part of this replay, go back to that state
            and rewind the replay; otherwise
            halt playback and go to that state normally.

            If the savestate movie is after the current replay
            length we can replace the current replay data with it,
            but if it's earlier we can rewind the replay to the
            savestate movie time point.

            This can truncate the current recording, so beware!

            TODO/FIXME: Figure out what to do about rewinding across load
         */
         if (loaded_len > handle_idx)
         {
            /* TODO: Really, to be very careful, we should be
               checking that the events in the loaded state are the
               same up to handle_idx. Right? */
            intfstream_rewind(input_st->bsv_movie_state_handle->file);
            intfstream_write(input_st->bsv_movie_state_handle->file, buffer+sizeof(int32_t), loaded_len);
         }
         else
            intfstream_seek(input_st->bsv_movie_state_handle->file, loaded_len, SEEK_SET);
      }
      else
      {
         if (recording)
         {
            const char *str = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT);
            runloop_msg_queue_push(str,
                                   1, 180, true,
                                   NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
            RARCH_ERR("[Replay] %s.\n", str);
            return false;
         }

         if (playback)
         {
            const char *str = msg_hash_to_str(MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT);
            runloop_msg_queue_push(str,
                                   1, 180, true,
                                   NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
            RARCH_WARN("[Replay] %s.\n", str);
            movie_stop(input_st);
         }
      }
   }
   return true;
}
#endif

void input_driver_poll(void)
{
   size_t i, j;
   rarch_joypad_info_t joypad_info[MAX_USERS];
   input_driver_state_t *input_st = &input_driver_st;
   settings_t *settings           = config_get_ptr();
   const input_device_driver_t
      *joypad                     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t
      *sec_joypad                 = input_st->secondary_joypad;
#else
   const input_device_driver_t
      *sec_joypad                 = NULL;
#endif
   bool input_remap_binds_enable  = settings->bools.input_remap_binds_enable;
   uint8_t max_users              = (uint8_t)settings->uints.input_max_users;

   if (     joypad && joypad->poll)
      joypad->poll();
   if (     sec_joypad && sec_joypad->poll)
      sec_joypad->poll();
   if (     input_st->current_driver
         && input_st->current_driver->poll)
      input_st->current_driver->poll(input_st->current_data);

   input_st->turbo_btns.count++;

   if (input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT)
   {
      for (i = 0; i < max_users; i++)
         input_st->turbo_btns.frame_enable[i] = 0;
      return;
   }

   /* This rarch_joypad_info_t struct contains the device index + autoconfig binds for the
    * controller to be queried, and also (for unknown reasons) the analog axis threshold
    * when mapping analog stick to dpad input. */
   for (i = 0; i < max_users; i++)
   {
      joypad_info[i].axis_threshold           = settings->floats.input_axis_threshold;
      joypad_info[i].joy_idx                  = settings->uints.input_joypad_index[i];
      joypad_info[i].auto_binds               = input_autoconf_binds[joypad_info[i].joy_idx];

      input_st->turbo_btns.frame_enable[i]    = (*input_st->libretro_input_binds[i])[RARCH_TURBO_ENABLE].valid ?
         input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info[i],
               (*input_st->libretro_input_binds),
               (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
               (unsigned)i,
               RETRO_DEVICE_JOYPAD,
               0,
               RARCH_TURBO_ENABLE) : 0;
   }

#ifdef HAVE_OVERLAY
   if (      input_st->overlay_ptr
         && (input_st->overlay_ptr->flags & INPUT_OVERLAY_ALIVE))
   {
      unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[0];
      float input_overlay_opacity     = (input_st->overlay_ptr->flags & INPUT_OVERLAY_IS_OSK)
         ? settings->floats.input_osk_overlay_opacity
         : settings->floats.input_overlay_opacity;

      switch (input_analog_dpad_mode)
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
            {
               unsigned mapped_port      = settings->uints.input_remap_ports[0];
               if (input_st->analog_requested[mapped_port])
                  input_analog_dpad_mode = ANALOG_DPAD_NONE;
            }
            break;
         case ANALOG_DPAD_LSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_LSTICK;
            break;
         case ANALOG_DPAD_RSTICK_FORCED:
            input_analog_dpad_mode       = ANALOG_DPAD_RSTICK;
            break;
         default:
            break;
      }

      input_poll_overlay(
            (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
            settings,
            input_st->overlay_ptr,
            input_st->overlay_visibility,
            input_overlay_opacity,
            input_analog_dpad_mode,
            settings->floats.input_axis_threshold);
   }
#endif

#ifdef HAVE_MENU
   if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
#endif
   if (input_remap_binds_enable)
   {
#ifdef HAVE_OVERLAY
      input_overlay_t *overlay_pointer   = (input_overlay_t*)input_st->overlay_ptr;
      bool poll_overlay                  = (overlay_pointer &&
            (overlay_pointer->flags & INPUT_OVERLAY_ALIVE));
#endif
      input_mapper_t *handle             = &input_st->mapper;
      float input_analog_deadzone        = settings->floats.input_analog_deadzone;
      float input_analog_sensitivity     = settings->floats.input_analog_sensitivity;

      for (i = 0; i < max_users; i++)
      {
         input_bits_t current_inputs;
         unsigned mapped_port            = settings->uints.input_remap_ports[i];
         unsigned device                 = settings->uints.input_libretro_device[mapped_port]
                                           & RETRO_DEVICE_MASK;
         input_bits_t *p_new_state       = (input_bits_t*)&current_inputs;
         unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[i];

         switch (input_analog_dpad_mode)
         {
            case ANALOG_DPAD_LSTICK:
            case ANALOG_DPAD_RSTICK:
               if (input_st->analog_requested[mapped_port])
                  input_analog_dpad_mode = ANALOG_DPAD_NONE;
               break;
            case ANALOG_DPAD_LSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_LSTICK;
               break;
            case ANALOG_DPAD_RSTICK_FORCED:
               input_analog_dpad_mode    = ANALOG_DPAD_RSTICK;
               break;
            default:
               break;
         }

         switch (device)
         {
            case RETRO_DEVICE_KEYBOARD:
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_ANALOG:
               BIT256_CLEAR_ALL_PTR(&current_inputs);
               if (joypad)
               {
                  unsigned k, j;
                  int16_t ret = input_state_wrap(
                        input_st->current_driver,
                        input_st->current_data,
                        input_st->primary_joypad,
                        sec_joypad,
                        &joypad_info[i],
                        (*input_st->libretro_input_binds),
                        (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                        (unsigned)i, RETRO_DEVICE_JOYPAD,
                        0, RETRO_DEVICE_ID_JOYPAD_MASK);

                  for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
                  {
                     if (ret & (1 << k))
                     {
                        bool valid_bind  =
                           (*input_st->libretro_input_binds[i])[k].valid;

                        if (valid_bind)
                        {
                           int16_t   val =
                              input_joypad_analog_button(
                                    input_analog_deadzone,
                                    input_analog_sensitivity,
                                    joypad,
                                    &joypad_info[i],
                                    k,
                                    &(*input_st->libretro_input_binds[i])[k]
                                    );
                           if (val)
                              p_new_state->analog_buttons[k] = val;
                        }

                        BIT256_SET_PTR(p_new_state, k);
                     }
                  }

                  /* This is the analog joypad index -
                   * handles only the two analog axes */
                  for (k = 0; k < 2; k++)
                  {
                     /* This is the analog joypad ident */
                     for (j = 0; j < 2; j++)
                     {
                        unsigned offset = 0 + (k * 4) + (j * 2);
                        int16_t     val = input_joypad_analog_axis(
                              input_analog_dpad_mode,
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              joypad,
                              &joypad_info[i],
                              k,
                              j,
                              (*input_st->libretro_input_binds[i]));

                        if (val >= 0)
                           p_new_state->analogs[offset]   = val;
                        else
                           p_new_state->analogs[offset+1] = val;
                     }
                  }
               }
               break;
            default:
               break;
         }

         /* mapper */
         switch (device)
         {
            /* keyboard to gamepad remapping */
            case RETRO_DEVICE_KEYBOARD:
               for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
               {
                  unsigned current_button_value;
                  unsigned remap_key =
                        settings->uints.input_keymapper_ids[i][j];

                  if (remap_key == RETROK_UNKNOWN)
                     continue;

                  if (j >= RARCH_FIRST_CUSTOM_BIND && j < RARCH_ANALOG_BIND_LIST_END)
                  {
                     int16_t current_axis_value = p_new_state->analogs[j - RARCH_FIRST_CUSTOM_BIND];
                     current_button_value = abs(current_axis_value) >
                           settings->floats.input_axis_threshold
                            * 32767;
                  }
                  else
                  {
                     current_button_value =
                        BIT256_GET_PTR(p_new_state, j);
                  }

#ifdef HAVE_OVERLAY
                  if (poll_overlay && i == 0)
                  {
                     input_overlay_state_t *ol_state  =
                        overlay_pointer
                        ? &overlay_pointer->overlay_state
                        : NULL;
                     if (ol_state)
                        current_button_value |=
                           BIT256_GET(ol_state->buttons, j);
                  }
#endif
                  /* Press */
                  if ((current_button_value == 1)
                        && !MAPPER_GET_KEY(handle, remap_key))
                  {
                     handle->key_button[remap_key] = (unsigned)j;

                     MAPPER_SET_KEY(handle, remap_key);
                     input_keyboard_event(true,
                           remap_key,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                  }
                  /* Release */
                  else if ((current_button_value == 0)
                        && MAPPER_GET_KEY(handle, remap_key))
                  {
                     if (handle->key_button[remap_key] != j)
                        continue;

                     input_keyboard_event(false,
                           remap_key,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                     MAPPER_UNSET_KEY(handle, remap_key);
                  }
               }
               break;

               /* gamepad remapping */
            case RETRO_DEVICE_JOYPAD:
            case RETRO_DEVICE_ANALOG:
               /* this loop iterates on all users and all buttons,
                * and checks if a pressed button is assigned to any
                * other button than the default one, then it sets
                * the bit on the mapper input bitmap, later on the
                * original input is cleared in input_state */
               BIT256_CLEAR_ALL(handle->buttons[i]);

               for (j = 0; j < 8; j++)
                  handle->analog_value[i][j] = 0;

               for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
               {
                  bool remap_valid;
                  unsigned remap_button         =
                        settings->uints.input_remap_ids[i][j];
                  unsigned current_button_value =
                        BIT256_GET_PTR(p_new_state, j);

#ifdef HAVE_OVERLAY
                  if (poll_overlay && i == 0)
                  {
                     input_overlay_state_t *ol_state  =
                        overlay_pointer
                        ? &overlay_pointer->overlay_state
                        : NULL;
                     if (ol_state)
                        current_button_value            |=
                           BIT256_GET(ol_state->buttons, j);
                  }
#endif
                  remap_valid                   =
                        (current_button_value == 1)
                     && (j != remap_button)
                     && (remap_button != RARCH_UNMAPPED);

#ifdef HAVE_ACCESSIBILITY
                  /* gamepad override */
                  if (     (i == 0)
                        && input_st->gamepad_input_override & (1 << j))
                  {
                     BIT256_SET(handle->buttons[i], j);
                  }
#endif

                  if (remap_valid)
                  {
                     if (remap_button < RARCH_FIRST_CUSTOM_BIND)
                     {
                        BIT256_SET(handle->buttons[i], remap_button);
                     }
                     else
                     {
                        int invert = 1;

                        if (remap_button % 2 != 0)
                           invert = -1;

                        handle->analog_value[i][
                           remap_button - RARCH_FIRST_CUSTOM_BIND] =
                              (p_new_state->analog_buttons[j]
                               ? p_new_state->analog_buttons[j]
                               : 32767) * invert;
                     }
                  }
               }

               for (j = 0; j < 8; j++)
               {
                  unsigned k                 = (unsigned)j + RARCH_FIRST_CUSTOM_BIND;
                  int16_t current_axis_value = p_new_state->analogs[j];
                  unsigned remap_axis        = settings->uints.input_remap_ids[i][k];

                  if (
                        (
                            abs(current_axis_value) > 0
                        && (k != remap_axis)
                        && (remap_axis != RARCH_UNMAPPED)
                        )
                     )
                  {
                     if (remap_axis < RARCH_FIRST_CUSTOM_BIND &&
                           abs(current_axis_value) >
                           settings->floats.input_axis_threshold
                            * 32767)
                     {
                        BIT256_SET(handle->buttons[i], remap_axis);
                     }
                     else
                     {
                        unsigned remap_axis_bind =
                           remap_axis - RARCH_FIRST_CUSTOM_BIND;

                        if (remap_axis_bind < sizeof(handle->analog_value[i]))
                        {
                           int invert = 1;
                           if (  (k % 2 == 0 && remap_axis % 2 != 0) ||
                                 (k % 2 != 0 && remap_axis % 2 == 0)
                              )
                              invert = -1;

                           handle->analog_value[i][
                              remap_axis_bind] =
                                 current_axis_value * invert;
                        }
                     }
                  }

               }
               break;
            default:
               break;
         }
      }
   }

#ifdef HAVE_COMMAND
   for (i = 0; i < ARRAY_SIZE(input_st->command); i++)
   {
      if (input_st->command[i])
      {
         memset(input_st->command[i]->state,
                0, sizeof(input_st->command[i]->state));

         input_st->command[i]->poll(
            input_st->command[i]);
      }
   }
#endif

#ifdef HAVE_NETWORKGAMEPAD
   /* Poll remote */
   if (input_st->remote)
   {
      unsigned user;

      for (user = 0; user < max_users; user++)
      {
         if (settings->bools.network_remote_enable_user[user])
         {
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
            fd_set fds;
            ssize_t ret;
            struct remote_message msg;

            if (input_st->remote->net_fd[user] < 0)
               return;

            FD_ZERO(&fds);
            FD_SET(input_st->remote->net_fd[user], &fds);

            ret = recvfrom(input_st->remote->net_fd[user],
                  (char*)&msg,
                  sizeof(msg), 0, NULL, NULL);

            if (ret == sizeof(msg))
               input_remote_parse_packet(&input_st->remote_st_ptr, &msg, user);
            else if ((ret != -1) || ((errno != EAGAIN) && (errno != ENOENT)))
#endif
            {
               input_remote_state_t *input_state  = &input_st->remote_st_ptr;
               input_state->buttons[user]         = 0;
               input_state->analog[0][user]       = 0;
               input_state->analog[1][user]       = 0;
               input_state->analog[2][user]       = 0;
               input_state->analog[3][user]       = 0;
            }
         }
      }
   }
#endif
#ifdef HAVE_BSV_MOVIE
   if (BSV_MOVIE_IS_PLAYBACK_ON())
   {
      runloop_state_t *runloop_st   = runloop_state_get_ptr();
      retro_keyboard_event_t *key_event                 = &runloop_st->key_event;

      if (*key_event && *key_event == runloop_st->frontend_key_event)
      {
         int i;
         bsv_key_data_t k;
         for (i = 0; i < input_st->bsv_movie_state_handle->key_event_count; i++)
         {
#ifdef HAVE_CHEEVOS
            rcheevos_pause_hardcore();
#endif
            k = input_st->bsv_movie_state_handle->key_events[i];
            input_keyboard_event(k.down, swap_if_big32(k.code), swap_if_big32(k.character), swap_if_big16(k.mod), RETRO_DEVICE_KEYBOARD);
         }
         /* Have to clear here so we don't double-apply key events */
         bsv_movie_handle_clear_key_events(input_st->bsv_movie_state_handle);
      }
   }
#endif
}

int16_t input_driver_state_wrapper(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   input_driver_state_t
      *input_st                = &input_driver_st;
   settings_t *settings        = config_get_ptr();
   int16_t result              = 0;
#ifdef HAVE_BSV_MOVIE
   /* Load input from BSV record, if enabled */
   if (BSV_MOVIE_IS_PLAYBACK_ON())
   {
      int16_t bsv_result = 0;
      if (intfstream_read(
               input_st->bsv_movie_state_handle->file,
               &bsv_result, 2) == 2)
      {
#ifdef HAVE_CHEEVOS
         rcheevos_pause_hardcore();
#endif
         return swap_if_big16(bsv_result);
      }

      input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_END;
   }
#endif

   /* Read input state */
   result = input_state_internal(input_st, settings, port, device, idx, id);

   /* Register any analog stick input requests for
    * this 'virtual' (core) port */
   if (     (device == RETRO_DEVICE_ANALOG)
       && ( (idx    == RETRO_DEVICE_INDEX_ANALOG_LEFT)
       ||   (idx    == RETRO_DEVICE_INDEX_ANALOG_RIGHT)))
      input_st->analog_requested[port] = true;

#ifdef HAVE_BSV_MOVIE
   /* Save input to BSV record, if enabled */
   if (BSV_MOVIE_IS_RECORDING())
   {
      result = swap_if_big16(result);
      intfstream_write(input_st->bsv_movie_state_handle->file, &result, 2);
   }
#endif

   return result;
}

#ifdef HAVE_HID
void *hid_driver_get_data(void)
{
   return (void *)input_driver_st.hid_data;
}

/* This is only to be called after we've invoked free() on the
 * HID driver; the memory will have already been freed, so we need to
 * reset the pointer.
 */
void hid_driver_reset_data(void)
{
   input_driver_st.hid_data = NULL;
}

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_HID_DRIVERS, NULL);
}

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;

   for (i = 0; hid_drivers[i]; i++)
   {
      input_st->hid_data = hid_drivers[i]->init();

      if (input_st->hid_data)
      {
         RARCH_LOG("[Input]: Found HID driver: \"%s\".\n",
               hid_drivers[i]->ident);
         return hid_drivers[i];
      }
   }

   return NULL;
}
#endif

void input_remapping_cache_global_config(void)
{
   unsigned i;
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Libretro device type is always set to
       * RETRO_DEVICE_JOYPAD globally *unless*
       * an override has been set via the command
       * line interface */
      unsigned device = RETRO_DEVICE_JOYPAD;

      if (retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &i))
         device = settings->uints.input_libretro_device[i];

      input_st->old_analog_dpad_mode[i] = settings->uints.input_analog_dpad_mode[i];
      input_st->old_libretro_device[i]  = device;
   }

   input_st->flags |= INP_FLAG_OLD_ANALOG_DPAD_MODE_SET
                    | INP_FLAG_OLD_LIBRETRO_DEVICE_SET;
}

void input_remapping_restore_global_config(bool clear_cache)
{
   unsigned i;
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = &input_driver_st;

   if (!(input_st->flags & INP_FLAG_REMAPPING_CACHE_ACTIVE))
      goto end;

   for (i = 0; i < MAX_USERS; i++)
   {
      if ((input_st->flags & INP_FLAG_OLD_ANALOG_DPAD_MODE_SET)
          && (settings->uints.input_analog_dpad_mode[i] !=
               input_st->old_analog_dpad_mode[i]))
         configuration_set_uint(settings,
               settings->uints.input_analog_dpad_mode[i],
               input_st->old_analog_dpad_mode[i]);

      if (   (input_st->flags & INP_FLAG_OLD_LIBRETRO_DEVICE_SET)
          && (settings->uints.input_libretro_device[i] !=
               input_st->old_libretro_device[i]))
         configuration_set_uint(settings,
               settings->uints.input_libretro_device[i],
               input_st->old_libretro_device[i]);
   }

end:
   if (clear_cache)
      input_st->flags &= ~(INP_FLAG_OLD_ANALOG_DPAD_MODE_SET
                         | INP_FLAG_OLD_LIBRETRO_DEVICE_SET
                         | INP_FLAG_REMAPPING_CACHE_ACTIVE);
}

void input_remapping_update_port_map(void)
{
   unsigned i, j;
   settings_t *settings               = config_get_ptr();
   unsigned port_map_index[MAX_USERS] = {0};

   /* First pass: 'reset' port map */
   for (i = 0; i < MAX_USERS; i++)
      for (j = 0; j < (MAX_USERS + 1); j++)
         settings->uints.input_remap_port_map[i][j] = MAX_USERS;

   /* Second pass: assign port indices from
    * 'input_remap_ports' */
   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned remap_port = settings->uints.input_remap_ports[i];

      if (remap_port < MAX_USERS)
      {
         /* 'input_remap_port_map' provides a list of
          * 'physical' ports for each 'virtual' port
          * sampled in input_state().
          * (Note: in the following explanation, port
          * index starts from 0, rather than the frontend
          * display convention of 1)
          * For example - the following remap configuration
          * will map input devices 0+1 to port 0, and input
          * device 2 to port 1
          * > input_remap_ports[0] = 0;
          *   input_remap_ports[1] = 0;
          *   input_remap_ports[2] = 1;
          * This gives a port map of:
          * > input_remap_port_map[0] = { 0, 1, MAX_USERS, ... };
          *   input_remap_port_map[1] = { 2, MAX_USERS, ... }
          *   input_remap_port_map[2] = { MAX_USERS, ... }
          *   ...
          * A port map value of MAX_USERS indicates the end
          * of the 'physical' port list */
         settings->uints.input_remap_port_map[remap_port]
               [port_map_index[remap_port]] = i;
         port_map_index[remap_port]++;
      }
   }
}

void input_remapping_deinit(bool save_remap)
{
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   if (runloop_st->name.remapfile)
   {
      if (save_remap)
         input_remapping_save_file(runloop_st->name.remapfile);

      free(runloop_st->name.remapfile);
   }
   runloop_st->name.remapfile   = NULL;
   runloop_st->flags           &= ~(RUNLOOP_FLAG_REMAPS_CORE_ACTIVE
                               |    RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE
                               |    RUNLOOP_FLAG_REMAPS_GAME_ACTIVE);
}

void input_remapping_set_defaults(bool clear_cache)
{
   unsigned i, j;
   settings_t *settings        = config_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      /* Button/keyboard remaps */
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
      {
         const struct retro_keybind *keybind = &input_config_binds[i][j];

         configuration_set_uint(settings,
               settings->uints.input_remap_ids[i][j],
                     keybind ? keybind->id : RARCH_UNMAPPED);

         configuration_set_uint(settings,
               settings->uints.input_keymapper_ids[i][j], RETROK_UNKNOWN);
      }

      /* Analog stick remaps */
      for (j = RARCH_FIRST_CUSTOM_BIND; j < (RARCH_FIRST_CUSTOM_BIND + 8); j++)
         configuration_set_uint(settings,
               settings->uints.input_remap_ids[i][j], j);

      /* Controller port remaps */
      configuration_set_uint(settings,
            settings->uints.input_remap_ports[i], i);
   }

   /* Need to call 'input_remapping_update_port_map()'
    * whenever 'settings->uints.input_remap_ports'
    * is modified */
   input_remapping_update_port_map();

   /* Restore 'global' settings that were cached on
    * the last core init
    * > Prevents remap changes from 'bleeding through'
    *   into the main config file */
   input_remapping_restore_global_config(clear_cache);
}

void input_driver_collect_system_input(input_driver_state_t *input_st,
      settings_t *settings, input_bits_t *current_bits)
{
   int port;
   rarch_joypad_info_t joypad_info;
   unsigned block_delay                = settings->uints.input_hotkey_block_delay;
   const input_device_driver_t *joypad = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t
      *sec_joypad                      = input_st->secondary_joypad;
#else
   const input_device_driver_t
      *sec_joypad                      = NULL;
#endif
#ifdef HAVE_MENU
   bool display_kb                     = menu_input_dialog_get_display_kb();
   bool menu_is_alive                  = (menu_state_get_ptr()->flags &
         MENU_ST_FLAG_ALIVE) ? true : false;
   bool menu_input_active              = menu_is_alive &&
         !(settings->bools.menu_unified_controls && !display_kb);
#endif
   input_driver_t *current_input       = input_st->current_driver;
   unsigned max_users                  = settings->uints.input_max_users;
   bool all_users_control_menu         = settings->bools.input_all_users_control_menu;
   joypad_info.axis_threshold          = settings->floats.input_axis_threshold;

   /* Gather input from each (enabled) joypad */
   for (port = 0; port < (int)max_users; port++)
   {
      const struct retro_keybind *binds_norm = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds_auto = &input_autoconf_binds[port][RARCH_ENABLE_HOTKEY];
      struct retro_keybind *auto_binds       = input_autoconf_binds[port];
      struct retro_keybind *general_binds    = input_config_binds[port];
      joypad_info.joy_idx                    = settings->uints.input_joypad_index[port];
      joypad_info.auto_binds                 = input_autoconf_binds[joypad_info.joy_idx];

#ifdef HAVE_MENU
      if (menu_is_alive)
      {
         int k;
         int s;

         /* Remember original analog D-pad binds. */
         for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
         {
            (auto_binds)[k].orig_joyaxis    = (auto_binds)[k].joyaxis;
            (general_binds)[k].orig_joyaxis = (general_binds)[k].joyaxis;
         }

         /* Read input from both analog sticks. */
         for (s = RETRO_DEVICE_INDEX_ANALOG_LEFT; s <= RETRO_DEVICE_INDEX_ANALOG_RIGHT; s++)
         {
            unsigned x_plus  = RARCH_ANALOG_LEFT_X_PLUS;
            unsigned y_plus  = RARCH_ANALOG_LEFT_Y_PLUS;
            unsigned x_minus = RARCH_ANALOG_LEFT_X_MINUS;
            unsigned y_minus = RARCH_ANALOG_LEFT_Y_MINUS;

            if (s == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
            {
               x_plus  = RARCH_ANALOG_RIGHT_X_PLUS;
               y_plus  = RARCH_ANALOG_RIGHT_Y_PLUS;
               x_minus = RARCH_ANALOG_RIGHT_X_MINUS;
               y_minus = RARCH_ANALOG_RIGHT_Y_MINUS;
            }

            if (!INHERIT_JOYAXIS(auto_binds))
            {
               unsigned j = x_plus + 3;
               /* Inherit joyaxis from analogs. */
               for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
               {
                  if ((auto_binds)[j].joyaxis != AXIS_NONE &&
                        ((float)abs(joypad->axis(port, (uint32_t)(auto_binds)[j].joyaxis))
                         / 0x8000) > joypad_info.axis_threshold)
                     (auto_binds)[k].joyaxis = (auto_binds)[j].joyaxis;
                  j--;
               }
            }

            if (!INHERIT_JOYAXIS(general_binds))
            {
               unsigned j = x_plus + 3;
               /* Inherit joyaxis from analogs. */
               for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
               {
                  if ((general_binds)[j].joyaxis != AXIS_NONE &&
                        ((float)abs(joypad->axis(port, (uint32_t)(general_binds)[j].joyaxis))
                         / 0x8000) > joypad_info.axis_threshold)
                     (general_binds)[k].joyaxis = (general_binds)[j].joyaxis;
                  j--;
               }
            }
         }
      }
#endif /* HAVE_MENU */

      input_keys_pressed(port,
#ifdef HAVE_MENU
            menu_is_alive,
#else
            false,
#endif
            block_delay,
            current_bits,
            (const retro_keybind_set *)input_config_binds,
            binds_norm,
            binds_auto,
            joypad,
            sec_joypad,
            &joypad_info);

#ifdef HAVE_MENU
      if (menu_is_alive)
      {
         int k;

         /* Restore analog D-pad binds temporarily overridden. */
         for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
         {
            (auto_binds)[k].joyaxis    = (auto_binds)[k].orig_joyaxis;
            (general_binds)[k].joyaxis = (general_binds)[k].orig_joyaxis;
         }

         if (!all_users_control_menu)
            break;
      }
#endif /* HAVE_MENU */
   }

#ifdef HAVE_MENU
   if (menu_input_active)
   {
      /* Gather keyboard input, if enabled
       * Note: Keyboard input always read from
       * port 0 */
      if (     !display_kb
            &&  current_input
            &&  current_input->input_state)
      {
         unsigned i;
         unsigned ids[][2] =
         {
            {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_B      },
            {RETROK_DELETE,    RETRO_DEVICE_ID_JOYPAD_Y      },
            {RETROK_SLASH,     RETRO_DEVICE_ID_JOYPAD_X      },
            {RETROK_SPACE,     RETRO_DEVICE_ID_JOYPAD_START  },
            {RETROK_RSHIFT,    RETRO_DEVICE_ID_JOYPAD_SELECT },
            {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP     },
            {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN   },
            {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT   },
            {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT  },
            {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L      },
            {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R      },
            {RETROK_HOME,      RETRO_DEVICE_ID_JOYPAD_L3     },
            {RETROK_END,       RETRO_DEVICE_ID_JOYPAD_R3     },
            {0,                RARCH_QUIT_KEY                }, /* 14 */
            {0,                RARCH_FULLSCREEN_TOGGLE_KEY   },
            {0,                RARCH_UI_COMPANION_TOGGLE     },
            {0,                RARCH_FPS_TOGGLE              },
            {0,                RARCH_NETPLAY_HOST_TOGGLE     },
            {0,                RARCH_MENU_TOGGLE             },
         };

         ids[14][0] = input_config_binds[0][RARCH_QUIT_KEY].key;
         ids[15][0] = input_config_binds[0][RARCH_FULLSCREEN_TOGGLE_KEY].key;
         ids[16][0] = input_config_binds[0][RARCH_UI_COMPANION_TOGGLE].key;
         ids[17][0] = input_config_binds[0][RARCH_FPS_TOGGLE].key;
         ids[18][0] = input_config_binds[0][RARCH_NETPLAY_HOST_TOGGLE].key;
         ids[19][0] = input_config_binds[0][RARCH_MENU_TOGGLE].key;

         if (settings->bools.input_menu_swap_ok_cancel_buttons)
         {
            ids[0][1] = RETRO_DEVICE_ID_JOYPAD_B;
            ids[1][1] = RETRO_DEVICE_ID_JOYPAD_A;
         }

         for (i = 0; i < ARRAY_SIZE(ids); i++)
         {
            if (current_input->input_state(
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     &joypad_info,
                     (const retro_keybind_set *)input_config_binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
               BIT256_SET_PTR(current_bits, ids[i][1]);
         }
      }
      else if (display_kb
            && current_input
            && current_input->input_state)
      {
         /* Allow arrows, LCtrl as OK, character map switches,
          * and set RetroPad Select bit when pressing Escape
          * in order to clear the input window and close it. */
         unsigned i;
         unsigned ids[][2] =
         {
            {RETROK_LCTRL,     RETRO_DEVICE_ID_JOYPAD_A      },
            {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP     },
            {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN   },
            {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT   },
            {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT  },
            {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L      },
            {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R      },
            {RETROK_ESCAPE,    RETRO_DEVICE_ID_JOYPAD_SELECT },
         };

         if (settings->bools.input_menu_swap_ok_cancel_buttons)
            ids[0][1] = RETRO_DEVICE_ID_JOYPAD_B;

         for (i = 0; i < ARRAY_SIZE(ids); i++)
         {
            if (current_input->input_state(
                     input_st->current_data,
                     joypad,
                     sec_joypad,
                     &joypad_info,
                     (const retro_keybind_set *)input_config_binds,
                     (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED) ? true : false,
                     0,
                     RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
               BIT256_SET_PTR(current_bits, ids[i][1]);
         }
      }
   }
   else
#endif /* HAVE_MENU */
   {
#if defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE)
      if (settings->bools.ai_service_enable)
      {
         int i;
         input_st->gamepad_input_override = 0;
         for (i = 0; i < MAX_USERS; i++)
         {
            /* Set gamepad input override */
            if (input_st->ai_gamepad_state[i] == 2)
               input_st->gamepad_input_override |= (1 << i);
            input_st->ai_gamepad_state[i] = 0;
         }
      }
#endif /* defined(HAVE_ACCESSIBILITY) && defined(HAVE_TRANSLATE) */
   }
}

#if defined(HAVE_MENU) && defined(HAVE_ACCESSIBILITY)
static const char *accessibility_lut_name(char key)
{
   switch (key)
   {
#if 0
      /* TODO/FIXME - overlaps with tilde */
      case '`':
         return "left quote";
#endif
      case '`':
         return "tilde";
      case '!':
         return "exclamation point";
      case '@':
         return "at sign";
      case '#':
         return "hash sign";
      case '$':
         return "dollar sign";
      case '%':
         return "percent sign";
      case '^':
         return "carrot";
      case '&':
         return "ampersand";
      case '*':
         return "asterisk";
      case '(':
         return "left bracket";
      case ')':
         return "right bracket";
      case '-':
         return "minus";
      case '_':
         return "underscore";
      case '=':
         return "equals";
      case '+':
         return "plus";
      case '[':
         return "left square bracket";
      case '{':
         return "left curl bracket";
      case ']':
         return "right square bracket";
      case '}':
         return "right curl bracket";
      case '\\':
         return "back slash";
      case '|':
         return "pipe";
      case ';':
         return "semicolon";
      case ':':
         return "colon";
      case '\'':
         return "single quote";
      case '\"':
         return "double quote";
      case ',':
         return "comma";
      case '<':
         return "left angle bracket";
      case '.':
         return "period";
      case '>':
         return "right angle bracket";
      case '/':
         return "front slash";
      case '?':
         return "question mark";
      case ' ':
         return "space";
      default:
         break;
   }
   return NULL;
}
#endif

void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   runloop_state_t *runloop_st   = runloop_state_get_ptr();
   retro_keyboard_event_t
      *key_event                 = &runloop_st->key_event;
   input_driver_state_t
      *input_st                  = &input_driver_st;
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st     = access_state_get_ptr();
   settings_t *settings          = config_get_ptr();
   bool accessibility_enable     = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed
                                 = settings->uints.accessibility_narrator_speech_speed;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st    = menu_state_get_ptr();

   /* If screensaver is active, then it should be
    * disabled if:
    * - Key is down AND
    * - OSK is active, OR:
    * - Key is *not* mapped to RetroPad input (these
    *   inputs are handled in menu_event() - if we
    *   allow mapped RetroPad keys to toggle off
    *   the screensaver, then we end up with a 'duplicate'
    *   input that will trigger unwanted menu action)
    * - For extra amusement, a number of keyboard keys
    *   are hard-coded to RetroPad inputs (while the menu
    *   is running) in such a way that they cannot be
    *   detected via the regular 'keyboard_mapping_bits'
    *   record. We therefore have to check each of these
    *   explicitly...
    * Otherwise, input is ignored whenever screensaver
    * is active */
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      if (   (down)
          && (code != RETROK_UNKNOWN)
          && (menu_input_dialog_get_display_kb() ||
              !((code == RETROK_SPACE)    || /* RETRO_DEVICE_ID_JOYPAD_START */
                (code == RETROK_SLASH)    || /* RETRO_DEVICE_ID_JOYPAD_X */
                (code == RETROK_RSHIFT)   || /* RETRO_DEVICE_ID_JOYPAD_SELECT */
                (code == RETROK_RIGHT)    || /* RETRO_DEVICE_ID_JOYPAD_RIGHT */
                (code == RETROK_LEFT)     || /* RETRO_DEVICE_ID_JOYPAD_LEFT */
                (code == RETROK_DOWN)     || /* RETRO_DEVICE_ID_JOYPAD_DOWN */
                (code == RETROK_UP)       || /* RETRO_DEVICE_ID_JOYPAD_UP */
                (code == RETROK_PAGEUP)   || /* RETRO_DEVICE_ID_JOYPAD_L */
                (code == RETROK_PAGEDOWN) || /* RETRO_DEVICE_ID_JOYPAD_R */
                (code == RETROK_BACKSPACE)|| /* RETRO_DEVICE_ID_JOYPAD_B */
                (code == RETROK_RETURN)   || /* RETRO_DEVICE_ID_JOYPAD_A */
                (code == RETROK_DELETE)   || /* RETRO_DEVICE_ID_JOYPAD_Y */
                 BIT512_GET(input_st->keyboard_mapping_bits, code))))
      {
         struct menu_state *menu_st  = menu_state_get_ptr();
         menu_st->flags             &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
         menu_st->input_last_time_us = menu_st->current_time_us;
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
                  NULL, menu_st->userdata);
      }
      return;
   }

   if (down)
      menu_st->input_last_time_us = menu_st->current_time_us;

#ifdef HAVE_ACCESSIBILITY
   if (menu_input_dialog_get_display_kb()
         && down && is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
   {
      if (code != 303 && code != 0)
      {
         char* say_char = (char*)malloc(sizeof(char)+1);

         if (say_char)
         {
            char c      = (char)character;
            *say_char   = c;
            say_char[1] = '\0';

            if (character == 127 || character == 8)
               accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     "backspace", 10);
            else
            {
               const char *lut_name = accessibility_lut_name(c);
               if (lut_name)
                  accessibility_speak_priority(
                        accessibility_enable,
                        accessibility_narrator_speech_speed,
                        lut_name, 10);
               else if (character != 0)
                  accessibility_speak_priority(
                        accessibility_enable,
                        accessibility_narrator_speech_speed,
                        say_char, 10);
            }
            free(say_char);
         }
      }
   }
#endif
#endif

   if (input_st->flags & INP_FLAG_DEFERRED_WAIT_KEYS)
   {
      if (down)
         return;
      input_st->keyboard_press_cb    = NULL;
      input_st->keyboard_press_data  = NULL;
      input_st->flags               &= ~(INP_FLAG_KB_MAPPING_BLOCKED
                                     |   INP_FLAG_DEFERRED_WAIT_KEYS
                                       );
   }
   else if (input_st->keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (input_st->keyboard_press_cb(input_st->keyboard_press_data, code))
         return;
      input_st->flags               |= INP_FLAG_DEFERRED_WAIT_KEYS;
   }
   else if (input_st->keyboard_line.enabled)
   {
      if (!down)
         return;

      if (!input_keyboard_line_event(input_st,
            &input_st->keyboard_line, character))
         return;

      /* Line is complete, can free it now. */
      input_keyboard_line_free(input_st);

      /* Unblock all hotkeys. */
      input_st->flags                     &= ~INP_FLAG_KB_MAPPING_BLOCKED;
   }
   else
   {
      if (code == RETROK_UNKNOWN)
         return;

      /* Check if keyboard events should be blocked when
       * pressing hotkeys and RetroPad binds, but
       * - not with Game Focus
       * - not from keyboard device type mappings
       * - not from overlay keyboard input
       * - with 'enable_hotkey' modifier set and unpressed. */
      if (     !input_st->game_focus_state.enabled
            && BIT512_GET(input_st->keyboard_mapping_bits, code)
            && device != RETRO_DEVICE_POINTER)
      {
         settings_t *settings        = config_get_ptr();
         unsigned max_users          = settings->uints.input_max_users;
         unsigned j;
         bool hotkey_pressed         = (input_st->input_hotkey_block_counter > 0);
         bool block_key_event        = false;

         /* Loop enabled ports for keycode dupes. */
         for (j = 0; j < max_users; j++)
         {
            unsigned k;
            unsigned hotkey_code = input_config_binds[0][RARCH_ENABLE_HOTKEY].key;

            /* Block hotkey key events based on 'enable_hotkey' modifier,
             * and only when modifier is a keyboard key. */
            if (     j == 0
                  && !block_key_event
                  && !(    !hotkey_pressed
                        && hotkey_code != RETROK_UNKNOWN
                        && hotkey_code != code))
            {
               for (k = RARCH_FIRST_META_KEY; k < RARCH_BIND_LIST_END; k++)
               {
                  if (input_config_binds[j][k].key == code)
                  {
                     block_key_event = true;
                     break;
                  }
               }
            }

            /* RetroPad blocking needed only when emulated device type is active. */
            if (     input_config_get_device(j)
                  && !block_key_event)
            {
               for (k = 0; k < RARCH_FIRST_META_KEY; k++)
               {
                  if (input_config_binds[j][k].key == code)
                  {
                     block_key_event = true;
                     break;
                  }
               }
            }
         }

         /* No blocking when event comes from emulated keyboard device type */
         if (MAPPER_GET_KEY(&input_st->mapper, code))
            block_key_event = false;

         if (block_key_event)
            return;
      }

      if (*key_event)
      {
         if (*key_event == runloop_st->frontend_key_event)
         {
#ifdef HAVE_BSV_MOVIE
            /* Save input to BSV record, if recording */
            if (BSV_MOVIE_IS_RECORDING())
            {
               bsv_movie_handle_push_key_event(input_st->bsv_movie_state_handle, down, mod, code, character);
            }
#endif
         }
         (*key_event)(down, code, character, mod);
      }
   }
}
