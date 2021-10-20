/**
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <math.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <clamping.h>
#include <retro_assert.h>

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

/**************************************/
/* TODO/FIXME - turn these into static global variable */
uint64_t lifecycle_state                                        = 0;

static void *input_null_init(const char *joypad_driver) { return (void*)-1; }
static void input_null_poll(void *data) { }
static int16_t input_null_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **retro_keybinds,
      bool keyboard_mapping_blocked,
      unsigned port, unsigned device, unsigned index, unsigned id) { return 0; }
static void input_null_free(void *data) { }
static bool input_null_set_sensor_state(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate) { return false; }
static float input_null_get_sensor_input(void *data, unsigned port, unsigned id) { return 0.0; }
static uint64_t input_null_get_capabilities(void *data) { return 0; }
static void input_null_grab_mouse(void *data, bool state) { }
static bool input_null_grab_stdin(void *data) { return false; }

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
   input_null_grab_stdin
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
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &input_sdl,
#endif
#if defined(DINGUX) && defined(HAVE_SDL_DINGUX)
   &input_sdl_dingux,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef __WINRT__
   &input_uwp,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1) || defined(__WINRT__)
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
#if defined(_WIN32) && !defined(_XBOX) && _WIN32_WINNT >= 0x0501 && !defined(__WINRT__)
#ifdef HAVE_WINRAWINPUT
   /* winraw only available since XP */
   &input_winraw,
#endif
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
   unsigned i;

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
   unsigned i;

   if (  input_driver_st.primary_joypad
      && input_driver_st.primary_joypad->set_rumble_gain)
   {
      for (i = 0; i < input_max_users; i++)
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
   void *current_data;

   if (!input_driver_st.current_data)
      return false;

   current_driver = input_driver_st.current_driver;
   current_data   = input_driver_st.current_data;

   /* If sensors are disabled, inhibit any enable
    * actions (but always allow disable actions) */
   if (!sensors_enable &&
       ((action == RETRO_SENSOR_ACCELEROMETER_ENABLE) ||
        (action == RETRO_SENSOR_GYROSCOPE_ENABLE) ||
        (action == RETRO_SENSOR_ILLUMINANCE_ENABLE)))
      return false;

   if (current_driver && current_driver->set_sensor_state)
      return current_driver->set_sensor_state(current_data,
            port, action, rate);

   return false;
}

/**************************************/

float input_driver_get_sensor(
         unsigned port, bool sensors_enable, unsigned id)
{
   const input_driver_t *current_driver;
   void *current_data;

   if (!input_driver_st.current_data)
      return 0.0f;

   current_driver = input_driver_st.current_driver;
   current_data   = input_driver_st.current_data;

   if (sensors_enable && current_driver->get_sensor_input)
      return current_driver->get_sensor_input(current_data, port, id);

   return 0.0f;
}

const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data)
{
   unsigned i;

   if (ident && *ident)
   {
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


bool input_driver_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t* p_input)
{
   retro_assert(p_input  != NULL);

   switch (mode)
   {
      case INPUT_COMBO_DOWN_Y_L_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L3_R3:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return true;
         break;
      case INPUT_COMBO_L1_R1_START_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_START_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L3_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_L_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_COMBO_DOWN_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_COMBO_L2_R2:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L2) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R2))
            return true;
         break;
      case INPUT_COMBO_HOLD_START:
         {
            rarch_timer_t *timer = &input_driver_st.combo_timers[INPUT_COMBO_HOLD_START];

            if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            {
               /* timer only runs while start is held down */
               timer->timer_end   = true;
               timer->timer_begin = false;
               timer->timeout_end = 0;
               return false;
            }

            /* User started holding down the start button, start the timer */
            if (!timer->timer_begin)
            {
               uint64_t current_usec = cpu_features_get_time_usec();
               timer->timeout_us     = HOLD_BTN_DELAY_SEC * 1000000;
               timer->current        = current_usec;
               timer->timeout_end    = timer->current + timer->timeout_us;
               timer->timer_begin    = true;
               timer->timer_end      = false;
            }

            timer->current           = current_time;
            timer->timeout_us        = (timer->timeout_end - timer->current);

            if (!timer->timer_end && (timer->timeout_us <= 0))
            {
               /* start has been held down long enough,
                * stop timer and enter menu */
               timer->timer_end   = true;
               timer->timer_begin = false;
               timer->timeout_end = 0;
               return true;
            }

         }
         break;
      case INPUT_COMBO_HOLD_SELECT:
         {
            rarch_timer_t *timer = &input_driver_st.combo_timers[INPUT_COMBO_HOLD_SELECT];

            if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            {
               /* timer only runs while select is held down */
               timer->timer_end   = true;
               timer->timer_begin = false;
               timer->timeout_end = 0;
               return false;
            }

            /* user started holding down the select button, start the timer */
            if (!timer->timer_begin)
            {
               uint64_t current_usec = cpu_features_get_time_usec();
               timer->timeout_us     = HOLD_BTN_DELAY_SEC * 1000000;
               timer->current        = current_usec;
               timer->timeout_end    = timer->current + timer->timeout_us;
               timer->timer_begin    = true;
               timer->timer_end      = false;
            }

            timer->current           = current_time;
            timer->timeout_us        = (timer->timeout_end - timer->current);

            if (!timer->timer_end && (timer->timeout_us <= 0))
            {
               /* select has been held down long enough,
                * stop timer and enter menu */
               timer->timer_end   = true;
               timer->timer_begin = false;
               timer->timeout_end = 0;
               return true;
            }
         }
         break;
      default:
      case INPUT_COMBO_NONE:
         break;
   }

   return false;
}

int16_t input_state_wrap(
      input_driver_t *current_input,
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **binds,
      bool keyboard_mapping_blocked,
      unsigned _port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int16_t ret                   = 0;

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
            const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
               ? bind_joyaxis : autobind_joyaxis;

            if (joypad)
            {
               if ((uint16_t)joykey != NO_BTN && joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(joypad->axis(port, joyaxis))
                      / 0x8000) > axis_threshold)
                  return 1;
            }
            if (sec_joypad)
            {
               if ((uint16_t)joykey != NO_BTN && sec_joypad->button(
                        port, (uint16_t)joykey))
                  return 1;
               if (joyaxis != AXIS_NONE &&
                     ((float)abs(sec_joypad->axis(port, joyaxis))
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

int16_t input_joypad_axis(
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

int16_t input_joypad_analog_button(
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

int16_t input_joypad_analog_axis(
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

bool input_keyboard_line_append(
      struct input_keyboard_line *keyboard_line,
      const char *word)
{
   unsigned i                  = 0;
   unsigned len                = (unsigned)strlen(word);
   char *newbuf                = (char*)realloc(
         keyboard_line->buffer,
         keyboard_line->size + len * 2);

   if (!newbuf)
      return false;

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
   return true;
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

   port = port + user;

   if (!network_init())
      return false;

   RARCH_LOG("Bringing up remote interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
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
   unsigned user;
   for (user = 0; user < max_users; user ++)
      socket_close(handle->net_fd[user]);

   free(handle);
}

static input_remote_t *input_remote_new(
      settings_t *settings,
      uint16_t port, unsigned max_users)
{
   unsigned user;
   input_remote_t      *handle = (input_remote_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   for (user = 0; user < max_users; user ++)
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

void input_remote_parse_packet(
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

#ifdef HAVE_OVERLAY
bool input_overlay_add_inputs_inner(overlay_desc_t *desc,
      input_overlay_state_t *ol_state, unsigned port)
{
   switch(desc->type)
   {
      case OVERLAY_TYPE_BUTTONS:
         {
            unsigned i;
            bool all_buttons_pressed        = false;

            /* Check each bank of the mask */
            for (i = 0; i < ARRAY_SIZE(desc->button_mask.data); ++i)
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
                     /* Light up the button if pressed */
                     if (ol_state ?
                           !BIT256_GET(ol_state->buttons, id) :
                           !input_state_internal(port, RETRO_DEVICE_JOYPAD, 0, id))
                     {
                        /* We need ALL of the inputs to be active,
                         * abort. */
                        desc->updated    = false;
                        return false;
                     }

                     all_buttons_pressed = true;
                     desc->updated       = true;
                  }

                  bank_mask >>= 1;
                  ++id;
               }
            }

            return all_buttons_pressed;
         }

      case OVERLAY_TYPE_ANALOG_LEFT:
      case OVERLAY_TYPE_ANALOG_RIGHT:
         {
            float analog_x;
            float analog_y;
            float dx;
            float dy;

            if (ol_state)
            {
               unsigned index_offset = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ? 2 : 0;
               analog_x              = (float)ol_state->analog[index_offset];
               analog_y              = (float)ol_state->analog[index_offset + 1];
            }
            else
            {
               unsigned index        = (desc->type == OVERLAY_TYPE_ANALOG_RIGHT) ?
                  RETRO_DEVICE_INDEX_ANALOG_RIGHT : RETRO_DEVICE_INDEX_ANALOG_LEFT;

               analog_x              = input_state_internal(port, RETRO_DEVICE_ANALOG,
                     index, RETRO_DEVICE_ID_ANALOG_X);
               analog_y              = input_state_internal(port, RETRO_DEVICE_ANALOG,
                     index, RETRO_DEVICE_ID_ANALOG_Y);
            }

            dx = (analog_x / (float)0x8000) * (desc->range_x / 2.0f);
            dy = (analog_y / (float)0x8000) * (desc->range_y / 2.0f);

            /* Only modify overlay delta_x/delta_y values
             * if we are monitoring input from a physical
             * controller */
            if (!ol_state)
            {
               desc->delta_x = dx;
               desc->delta_y = dy;
            }

            /* Maybe use some option here instead of 0, only display
             * changes greater than some magnitude */
            if ((dx * dx) > 0 || (dy * dy) > 0)
               return true;
         }
         break;

      case OVERLAY_TYPE_KEYBOARD:
         if (ol_state ?
               OVERLAY_GET_KEY(ol_state, desc->retro_key_idx) :
               input_state_internal(port, RETRO_DEVICE_KEYBOARD, 0, desc->retro_key_idx))
         {
            desc->updated  = true;
            return true;
         }
         break;

      default:
         break;
   }

   return false;
}

bool input_overlay_add_inputs(input_overlay_t *ol,
      bool show_touched, unsigned port)
{
   unsigned i;
   bool button_pressed             = false;
   input_overlay_state_t *ol_state = &ol->overlay_state;

   if (!ol_state)
      return false;

   for (i = 0; i < ol->active->size; i++)
   {
      overlay_desc_t *desc  = &(ol->active->descs[i]);
      button_pressed       |= input_overlay_add_inputs_inner(desc,
            show_touched ? ol_state : NULL, port);
   }

   return button_pressed;
}

/**
 * inside_hitbox:
 * @desc                  : Overlay descriptor handle.
 * @x                     : X coordinate value.
 * @y                     : Y coordinate value.
 *
 * Check whether the given @x and @y coordinates of the overlay
 * descriptor @desc is inside the overlay descriptor's hitbox.
 *
 * Returns: true (1) if X, Y coordinates are inside a hitbox,
 * otherwise false (0).
 **/
static bool inside_hitbox(const struct overlay_desc *desc, float x, float y)
{
   if (!desc)
      return false;

   switch (desc->hitbox)
   {
      case OVERLAY_HITBOX_RADIAL:
      {
         /* Ellipsis. */
         float x_dist  = (x - desc->x_shift) / desc->range_x_mod;
         float y_dist  = (y - desc->y_shift) / desc->range_y_mod;
         float sq_dist = x_dist * x_dist + y_dist * y_dist;
         return (sq_dist <= 1.0f);
      }

      case OVERLAY_HITBOX_RECT:
         return
            (fabs(x - desc->x_shift) <= desc->range_x_mod) &&
            (fabs(y - desc->y_shift) <= desc->range_y_mod);
   }

   return false;
}

/**
 * input_overlay_poll:
 * @out                   : Polled output data.
 * @norm_x                : Normalized X coordinate.
 * @norm_y                : Normalized Y coordinate.
 *
 * Polls input overlay.
 *
 * @norm_x and @norm_y are the result of
 * input_translate_coord_viewport().
 **/
void input_overlay_poll(
      input_overlay_t *ol,
      input_overlay_state_t *out,
      int16_t norm_x, int16_t norm_y, float touch_scale)
{
   size_t i;

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
      struct overlay_desc *desc = &ol->active->descs[i];

      if (!inside_hitbox(desc, x, y))
         continue;

      desc->updated = true;
      x_dist        = x - desc->x_shift;
      y_dist        = y - desc->y_shift;

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

      if (desc->movable)
      {
         desc->delta_x = clamp_float(x_dist, -desc->range_x, desc->range_x)
            * ol->active->mod_w;
         desc->delta_y = clamp_float(y_dist, -desc->range_y, desc->range_y)
            * ol->active->mod_h;
      }
   }

   if (!bits_any_set(out->buttons.data, ARRAY_SIZE(out->buttons.data)))
      ol->blocked = false;
   else if (ol->blocked)
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
   if (!desc->image.pixels || !desc->movable)
      return;

   if (ol->iface->vertex_geom)
      ol->iface->vertex_geom(ol->iface_data, desc->image_index,
            desc->mod_x + desc->delta_x, desc->mod_y + desc->delta_y,
            desc->mod_w, desc->mod_h);

   desc->delta_x = 0.0f;
   desc->delta_y = 0.0f;
}

void input_overlay_post_poll(
      enum overlay_visibility *visibility,
      input_overlay_t *ol,
      bool show_input, float opacity)
{
   size_t i;

   input_overlay_set_alpha_mod(visibility, ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;

      if (desc->updated)
      {
         /* If pressed this frame, change the hitbox. */
         desc->range_x_mod *= desc->range_mod;
         desc->range_y_mod *= desc->range_mod;

         if (show_input && desc->image.pixels)
         {
            if (ol->iface->set_alpha)
               ol->iface->set_alpha(ol->iface_data, desc->image_index,
                     desc->alpha_mod * opacity);
         }
      }

      input_overlay_update_desc_geom(ol, desc);
      desc->updated = false;
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

void input_overlay_scale(struct overlay *ol,
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

      desc->x_shift = desc->x + x_shift_offset;

      /* Apply 'y separation' factor */
      if (desc->y < (0.5f - 0.0001f))
         y_shift_offset = layout->y_separation * -1.0f;
      else if (desc->y > (0.5f + 0.0001f))
         y_shift_offset = layout->y_separation;

      desc->y_shift = desc->y + y_shift_offset;

      scale_w       = ol->mod_w * desc->range_x;
      scale_h       = ol->mod_h * desc->range_y;
      adj_center_x  = ol->mod_x + desc->x_shift * ol->mod_w;
      adj_center_y  = ol->mod_y + desc->y_shift * ol->mod_h;

      desc->mod_w   = 2.0f * scale_w;
      desc->mod_h   = 2.0f * scale_h;
      desc->mod_x   = adj_center_x - scale_w;
      desc->mod_y   = adj_center_y - scale_h;
   }
}

void input_overlay_parse_layout(
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
      if (ol->block_scale ||
          (ol->aspect_ratio <= 0.0f) ||
          (display_aspect_ratio <= 0.0f))
         return;

      /* If display is wider than overlay,
       * reduce width */
      if (display_aspect_ratio >
            ol->aspect_ratio)
      {
         overlay_layout->x_scale = ol->aspect_ratio /
               display_aspect_ratio;

         if (overlay_layout->x_scale <= 0.0f)
         {
            overlay_layout->x_scale = 1.0f;
            return;
         }

         /* If X separation is permitted, move elements
          * horizontally towards the edges of the screen */
         if (!ol->block_x_separation)
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

         /* If Y separation is permitted and display has
          * a *landscape* orientation, move elements
          * vertically towards the edges of the screen
          * > Portrait overlays typically have all elements
          *   below the centre line, so Y separation
          *   provides no real benefit */
         if ((display_aspect_ratio > 1.0f) &&
             !ol->block_y_separation)
            overlay_layout->y_separation = ((1.0f / overlay_layout->y_scale) - 1.0f) * 0.5f;
      }

      return;
   }

   /* Regular 'manual' scaling/position adjustment
    * > Landscape display orientations */
   if (display_aspect_ratio > 1.0f)
   {
      float scale         = layout_desc->scale_landscape;
      float aspect_adjust = layout_desc->aspect_adjust_landscape;

      /* Note: Y offsets have their sign inverted,
       * since from a usability perspective positive
       * values should move the overlay upwards */
      overlay_layout->x_offset = layout_desc->x_offset_landscape;
      overlay_layout->y_offset = layout_desc->y_offset_landscape * -1.0f;

      if (!ol->block_x_separation)
         overlay_layout->x_separation = layout_desc->x_separation_landscape;
      if (!ol->block_y_separation)
         overlay_layout->y_separation = layout_desc->y_separation_landscape;

      if (!ol->block_scale)
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
      float scale         = layout_desc->scale_portrait;
      float aspect_adjust = layout_desc->aspect_adjust_portrait;

      overlay_layout->x_offset = layout_desc->x_offset_portrait;
      overlay_layout->y_offset = layout_desc->y_offset_portrait * -1.0f;

      if (!ol->block_x_separation)
         overlay_layout->x_separation = layout_desc->x_separation_portrait;
      if (!ol->block_y_separation)
         overlay_layout->y_separation = layout_desc->y_separation_portrait;

      if (!ol->block_scale)
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

void input_overlay_set_vertex_geom(input_overlay_t *ol)
{
   size_t i;

   if (ol->active->image.pixels)
      ol->iface->vertex_geom(ol->iface_data, 0,
            ol->active->mod_x, ol->active->mod_y,
            ol->active->mod_w, ol->active->mod_h);

   if (ol->iface->vertex_geom)
      for (i = 0; i < ol->active->size; i++)
      {
         struct overlay_desc *desc = &ol->active->descs[i];

         if (!desc->image.pixels)
            continue;

         ol->iface->vertex_geom(ol->iface_data, desc->image_index,
               desc->mod_x, desc->mod_y, desc->mod_w, desc->mod_h);
      }
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
      ol->iface->full_screen(ol->iface_data, ol->active->full_screen);
}

void input_overlay_poll_clear(
      enum overlay_visibility *visibility,
      input_overlay_t *ol, float opacity)
{
   size_t i;

   ol->blocked = false;

   input_overlay_set_alpha_mod(visibility, ol, opacity);

   for (i = 0; i < ol->active->size; i++)
   {
      struct overlay_desc *desc = &ol->active->descs[i];

      desc->range_x_mod = desc->range_x;
      desc->range_y_mod = desc->range_y;
      desc->updated     = false;

      desc->delta_x     = 0.0f;
      desc->delta_y     = 0.0f;
      input_overlay_update_desc_geom(ol, desc);
   }
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

enum overlay_visibility input_overlay_get_visibility(
      enum overlay_visibility *visibility,
      int overlay_idx)
{
    if (!visibility)
       return OVERLAY_VISIBILITY_DEFAULT;
    if ((overlay_idx < 0) || (overlay_idx >= MAX_VISIBILITY))
       return OVERLAY_VISIBILITY_DEFAULT;
    return visibility[overlay_idx];
}

void input_overlay_free_overlays(input_overlay_t *ol)
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
      image_texture_free(&overlay->descs[i].image);

   if (overlay->load_images)
      free(overlay->load_images);
   overlay->load_images = NULL;
   if (overlay->descs)
      free(overlay->descs);
   overlay->descs       = NULL;
   image_texture_free(&overlay->image);
}

void input_overlay_free(input_overlay_t *ol)
{
   if (!ol)
      return;

   input_overlay_free_overlays(ol);

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, false);

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
   if (!ol || !ol->alive || !input_overlay_enable)
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
   }

   /* Sanity check */
   if (active_overlay_orientation == OVERLAY_ORIENTATION_NONE)
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

void input_poll_overlay(
      bool keyboard_mapping_blocked,
      settings_t *settings,
      void *ol_data,
      enum overlay_visibility *overlay_visibility,
      float opacity,
      unsigned analog_dpad_mode,
      float axis_threshold)
{
   input_overlay_state_t old_key_state;
   unsigned i, j;
   input_overlay_t *ol                     = (input_overlay_t*)ol_data;
   uint16_t key_mod                        = 0;
   bool polled                             = false;
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

   if (!ol_state)
      return;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   if (current_input->input_state)
   {
      rarch_joypad_info_t joypad_info;
      unsigned device                 = ol->active->full_screen
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
               RETRO_DEVICE_ID_POINTER_PRESSED);
            i++)
      {
         input_overlay_state_t polled_data;
         int16_t x = current_input->input_state(
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
         int16_t y = current_input->input_state(
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

         memset(&polled_data, 0, sizeof(struct input_overlay_state));

         if (ol->enable)
            input_overlay_poll(ol, &polled_data, x, y, touch_scale);
         else
            ol->blocked = false;

         bits_or_bits(ol_state->buttons.data,
               polled_data.buttons.data,
               ARRAY_SIZE(polled_data.buttons.data));

         for (j = 0; j < ARRAY_SIZE(ol_state->keys); j++)
            ol_state->keys[j] |= polled_data.keys[j];

         /* Fingers pressed later take priority and matched up
          * with overlay poll priorities. */
         for (j = 0; j < 4; j++)
            if (polled_data.analog[j])
               ol_state->analog[j] = polled_data.analog[j];

         polled = true;
      }
   }

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
       OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (  OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(ol_state->keys); i++)
   {
      if (ol_state->keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
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

   if (input_overlay_show_inputs != OVERLAY_SHOW_INPUT_NONE)
      button_pressed = input_overlay_add_inputs(ol,
            (input_overlay_show_inputs == OVERLAY_SHOW_INPUT_TOUCHED),
            input_overlay_show_inputs_port);

   if (button_pressed || polled)
      input_overlay_post_poll(overlay_visibility, ol,
            button_pressed, opacity);
   else
      input_overlay_poll_clear(overlay_visibility, ol, opacity);
}
#endif

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str)
{
   size_t i;
   if (strlen(str) == 1 && ISALPHA((int)*str))
      return (enum retro_key)(RETROK_a + (TOLOWER((int)*str) - (int)'a'));
   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }

   RARCH_WARN("[Input]: Key name \"%s\" not found.\n", str);
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
            input_descriptor_label_show, buf, "", bind, size);
   else if (bind      && bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            buf, "", bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(
            input_descriptor_label_show, buf, "Auto: ", auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(
            input_descriptor_label_show,
            buf, "Auto: ", auto_bind, size);

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
      if (*key != '\0')
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
      char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   if (GET_HAT_DIR(bind->joykey))
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label)
            && input_descriptor_label_show)
         fill_pathname_join_delim_concat(buf, prefix,
               bind->joykey_label, ' ', " (hat)", size);
      else
      {
         const char *dir = "?";

         switch (GET_HAT_DIR(bind->joykey))
         {
            case HAT_UP_MASK:
               dir = "up";
               break;
            case HAT_DOWN_MASK:
               dir = "down";
               break;
            case HAT_LEFT_MASK:
               dir = "left";
               break;
            case HAT_RIGHT_MASK:
               dir = "right";
               break;
            default:
               break;
         }
         snprintf(buf, size, "%sHat #%u %s (%s)", prefix,
               (unsigned)GET_HAT(bind->joykey), dir,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
      }
   }
   else
   {
      if (bind->joykey_label &&
            !string_is_empty(bind->joykey_label)
            && input_descriptor_label_show)
         fill_pathname_join_delim_concat(buf, prefix,
               bind->joykey_label, ' ', " (btn)", size);
      else
         snprintf(buf, size, "%s%u (%s)", prefix, (unsigned)bind->joykey,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

void input_config_get_bind_string_joyaxis(
      bool input_descriptor_label_show,
      char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   if (bind->joyaxis_label &&
         !string_is_empty(bind->joyaxis_label)
         && input_descriptor_label_show)
      fill_pathname_join_delim_concat(buf, prefix,
            bind->joyaxis_label, ' ', " (axis)", size);
   else
   {
      unsigned axis        = 0;
      char dir             = '\0';
      if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '-';
         axis = AXIS_NEG_GET(bind->joyaxis);
      }
      else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
      {
         dir = '+';
         axis = AXIS_POS_GET(bind->joyaxis);
      }
      snprintf(buf, size, "%s%c%u (%s)", prefix, dir, axis,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
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

void input_event_osk_append(
      input_keyboard_line_t *keyboard_line,
      enum osk_type *osk_idx,
      unsigned *osk_last_codepoint,
      unsigned *osk_last_codepoint_len,
      int ptr,
      bool show_symbol_pages,
      const char *word)
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
#endif
      if (*osk_idx < (show_symbol_pages ? OSK_TYPE_LAST - 1 : OSK_SYMBOLS_PAGE1))
         *osk_idx = (enum osk_type)(*osk_idx + 1);
      else
         *osk_idx = ((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
   else
   {
      input_keyboard_line_append(keyboard_line, word);
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
      tmp->destroy();
   }
#ifdef HAVE_MFI
   if (input_driver_st.secondary_joypad)
   {
      const input_device_driver_t *tmp  = input_driver_st.secondary_joypad;
      input_driver_st.secondary_joypad  = NULL;
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
      const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
         ? bind_joyaxis : autobind_joyaxis;

      if ((uint16_t)joykey != NO_BTN && joypad->button(
               port, (uint16_t)joykey))
         return true;
      if (joyaxis != AXIS_NONE &&
            ((float)abs(joypad->axis(port, joyaxis))
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

   retro_assert(sizeof(input_config_binds[0]) >= sizeof(retro_keybinds_1));
   retro_assert(sizeof(input_config_binds[1]) >= sizeof(retro_keybinds_rest));

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
      input_st->input_device_info[i].config_path[0]   = '\0';
      input_st->input_device_info[i].config_name[0]   = '\0';
      input_st->input_device_info[i].joypad_driver[0] = '\0';
      input_st->input_device_info[i].vid              = 0;
      input_st->input_device_info[i].pid              = 0;
      input_st->input_device_info[i].autoconfigured   = false;
      input_st->input_device_info[i].name_index       = 0;

      input_config_reset_autoconfig_binds(i);

      input_st->libretro_input_binds[i] = input_config_binds[i];
   }
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t        *settings = config_get_ptr();

   if (settings)
      configuration_set_uint(settings,
      settings->uints.input_libretro_device[port], id);
}

unsigned input_config_get_device(unsigned port)
{
   settings_t             *settings = config_get_ptr();
   return settings->uints.input_libretro_device[port];
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
   return &settings->uints.input_libretro_device[port];
}

unsigned input_config_get_device_count(void)
{
   unsigned num_devices;
   input_driver_state_t *input_st = &input_driver_st;

   for (num_devices = 0; num_devices < MAX_INPUT_DEVICES; ++num_devices)
   {
      if (string_is_empty(input_st->input_device_info[num_devices].name))
         break;
   }
   return num_devices;
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


const char *input_config_get_device_config_path(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   if (string_is_empty(input_st->input_device_info[port].config_path))
      return NULL;
   return input_st->input_device_info[port].config_path;
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

void input_config_set_device_config_path(unsigned port, const char *path)
{
   if (!string_is_empty(path))
   {
      char parent_dir_name[128];
      input_driver_state_t *input_st = &input_driver_st;

      parent_dir_name[0] = '\0';

      if (fill_pathname_parent_dir_name(parent_dir_name,
               path, sizeof(parent_dir_name)))
         fill_pathname_join(input_st->input_device_info[port].config_path,
               parent_dir_name, path_basename(path),
               sizeof(input_st->input_device_info[port].config_path));
   }
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

void input_config_clear_device_config_path(unsigned port)
{
   input_driver_state_t *input_st = &input_driver_st;
   input_st->input_device_info[port].config_path[0] = '\0';
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
   input_driver_state_t *input_st = &input_driver_st;
   if (!string_is_empty(name))
      strlcpy(input_st->input_mouse_info[port].display_name, name,
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
   input_driver_state_t *input_st = &input_driver_st;
   config_file_t            *conf = (config_file_t*)data;

   if (!conf)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      unsigned j;

      for (j = 0; input_config_bind_map_get_valid(j); j++)
      {
         char str[256];
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

         str[0]                     = '\0';

         fill_pathname_join_delim(str, prefix, btn,  '_', sizeof(str));

         /* Clear old mapping bit */
         BIT512_CLEAR_PTR(&input_st->keyboard_mapping_bits, bind->key);

         entry                      = config_get_entry(conf, str);
         if (entry && !string_is_empty(entry->value))
            bind->key               = input_config_translate_str_to_rk(
                  entry->value);
         /* Store mapping bit */
         BIT512_SET_PTR(&input_st->keyboard_mapping_bits, bind->key);

         input_config_parse_joy_button  (str, conf, prefix, btn, bind);
         input_config_parse_joy_axis    (str, conf, prefix, btn, bind);
         input_config_parse_mouse_button(str, conf, prefix, btn, bind);
      }
   }
}

#ifdef HAVE_COMMAND
void input_driver_init_command(
      input_driver_state_t *input_st,
      settings_t *settings)
{
   bool input_network_cmd_enable     = settings->bools.network_cmd_enable;
   unsigned network_cmd_port         = settings->uints.network_cmd_port;
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
   if (input_network_cmd_enable)
   {
      input_st->command[1] = command_network_new(network_cmd_port);
      if (!input_st->command[1])
         RARCH_ERR("Failed to initialize the network command interface.\n");
   }
#endif

#ifdef HAVE_LAKKA
   input_st->command[2] = command_uds_new();
   if (!input_st->command[2])
      RARCH_ERR("Failed to initialize the UDS command interface.\n");
#endif
}

void input_driver_deinit_command(input_driver_state_t *input_st)
{
   int i;
   for (i = 0; i < ARRAY_SIZE(input_st->command); i++)
   {
      if (input_st->command[i])
         input_st->command[i]->destroy(
            input_st->command[i]);

      input_st->command[i] = NULL;
    }
}
#endif

bool input_keyboard_line_event(
      input_driver_state_t *input_st,
      input_keyboard_line_t *state, uint32_t character)
{
   char array[2];
   bool            ret         = false;
   const char            *word = NULL;
   char            c           = (character >= 128) ? '?' : character;

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
void input_overlay_deinit(void)
{
   input_overlay_free(input_driver_st.overlay_ptr);
   input_driver_st.overlay_ptr = NULL;
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

static bool video_driver_overlay_interface(
      const video_overlay_interface_t **iface)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!video_st->current_video || !video_st->current_video->overlay_interface)
      return false;
   video_st->current_video->overlay_interface(video_st->data, iface);
   return true;
}

/* task_data = overlay_task_data_t* */
static void input_overlay_loaded(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   size_t i;
   overlay_task_data_t              *data = (overlay_task_data_t*)task_data;
   input_overlay_t                    *ol = NULL;
   const video_overlay_interface_t *iface = NULL;
   settings_t *settings                   = config_get_ptr();
   bool input_overlay_show_mouse_cursor   = settings->bools.input_overlay_show_mouse_cursor;
   bool inp_overlay_auto_rotate           = settings->bools.input_overlay_auto_rotate;
   bool input_overlay_enable              = settings->bools.input_overlay_enable;
   video_driver_state_t *video_st         = video_state_get_ptr();
   input_driver_state_t *input_st         = &input_driver_st;
   if (err)
      return;

   if (data->overlay_enable)
   {
#ifdef HAVE_MENU
      /* We can't display when the menu is up */
      if (data->hide_in_menu && menu_state_get_ptr()->alive)
         goto abort_load;
#endif

      /* If 'hide_when_gamepad_connected' is enabled,
       * we can't display when a gamepad is connected */
      if (data->hide_when_gamepad_connected &&
          (input_config_get_device_name(0) != NULL))
         goto abort_load;
   }

   if (  !data->overlay_enable                   ||
         !video_driver_overlay_interface(&iface) ||
         !iface)
   {
      RARCH_ERR("Overlay interface is not present in video driver,"
            " or not enabled.\n");
      goto abort_load;
   }

   ol             = (input_overlay_t*)calloc(1, sizeof(*ol));
   ol->overlays   = data->overlays;
   ol->size       = data->size;
   ol->active     = data->active;
   ol->iface      = iface;
   ol->iface_data = video_st->data;

   input_overlay_load_active(input_st->overlay_visibility,
         ol, data->overlay_opacity);

   /* Enable or disable the overlay. */
   ol->enable = data->overlay_enable;

   if (ol->iface->enable)
      ol->iface->enable(ol->iface_data, data->overlay_enable);

   input_overlay_set_scale_factor(ol, &data->layout_desc,
         video_st->width, video_st->height);

   ol->next_index = (unsigned)((ol->index + 1) % ol->size);
   ol->state      = OVERLAY_STATUS_NONE;
   ol->alive      = true;

   /* Due to the asynchronous nature of overlay loading
    * it is possible for overlay_ptr to be non-NULL here
    * > Ensure it is free()'d before assigning new pointer */
   if (input_st->overlay_ptr)
   {
      input_overlay_free_overlays(input_st->overlay_ptr);
      free(input_st->overlay_ptr);
   }
   input_st->overlay_ptr = ol;

   free(data);

   if (!input_overlay_show_mouse_cursor)
      video_driver_hide_mouse();

   /* Attempt to automatically rotate overlay, if required */
   if (inp_overlay_auto_rotate)
      input_overlay_auto_rotate_(
            video_st->width,
            video_st->height,
            input_overlay_enable,
            input_st->overlay_ptr);

   return;

abort_load:
   for (i = 0; i < data->size; i++)
      input_overlay_free_overlay(&data->overlays[i]);

   free(data->overlays);
   free(data);
}

void input_overlay_init(void)
{
   settings_t *settings                     = config_get_ptr();
   bool input_overlay_enable                = settings->bools.input_overlay_enable;
   bool input_overlay_auto_scale            = settings->bools.input_overlay_auto_scale;
   const char *path_overlay                 = settings->paths.path_overlay;
   float overlay_opacity                    = settings->floats.input_overlay_opacity;
   float overlay_scale_landscape            = settings->floats.input_overlay_scale_landscape;
   float overlay_aspect_adjust_landscape    = settings->floats.input_overlay_aspect_adjust_landscape;
   float overlay_x_separation_landscape     = settings->floats.input_overlay_x_separation_landscape;
   float overlay_y_separation_landscape     = settings->floats.input_overlay_y_separation_landscape;
   float overlay_x_offset_landscape         = settings->floats.input_overlay_x_offset_landscape;
   float overlay_y_offset_landscape         = settings->floats.input_overlay_y_offset_landscape;
   float overlay_scale_portrait             = settings->floats.input_overlay_scale_portrait;
   float overlay_aspect_adjust_portrait     = settings->floats.input_overlay_aspect_adjust_portrait;
   float overlay_x_separation_portrait      = settings->floats.input_overlay_x_separation_portrait;
   float overlay_y_separation_portrait      = settings->floats.input_overlay_y_separation_portrait;
   float overlay_x_offset_portrait          = settings->floats.input_overlay_x_offset_portrait;
   float overlay_y_offset_portrait          = settings->floats.input_overlay_y_offset_portrait;
   float overlay_touch_scale                = (float)settings->uints.input_touch_scale;

   bool load_enabled                        = input_overlay_enable;
#ifdef HAVE_MENU
   bool overlay_hide_in_menu                = settings->bools.input_overlay_hide_in_menu;
#else
   bool overlay_hide_in_menu                = false;
#endif
   bool overlay_hide_when_gamepad_connected = settings->bools.input_overlay_hide_when_gamepad_connected;
#if defined(GEKKO)
   /* Avoid a crash at startup or even when toggling overlay in rgui */
   uint64_t memory_free                     = frontend_driver_get_free_memory();
   if (memory_free < (3 * 1024 * 1024))
      return;
#endif

   input_overlay_deinit();

#ifdef HAVE_MENU
   /* Cancel load if 'hide_in_menu' is enabled and
    * menu is currently active */
   if (overlay_hide_in_menu)
      load_enabled = load_enabled && !menu_state_get_ptr()->alive;
#endif

   /* Cancel load if 'hide_when_gamepad_connected' is
    * enabled and a gamepad is currently connected */
   if (overlay_hide_when_gamepad_connected)
      load_enabled = load_enabled && (input_config_get_device_name(0) == NULL);

   if (load_enabled)
   {
      overlay_layout_desc_t layout_desc;

      layout_desc.scale_landscape         = overlay_scale_landscape;
      layout_desc.aspect_adjust_landscape = overlay_aspect_adjust_landscape;
      layout_desc.x_separation_landscape  = overlay_x_separation_landscape;
      layout_desc.y_separation_landscape  = overlay_y_separation_landscape;
      layout_desc.x_offset_landscape      = overlay_x_offset_landscape;
      layout_desc.y_offset_landscape      = overlay_y_offset_landscape;
      layout_desc.scale_portrait          = overlay_scale_portrait;
      layout_desc.aspect_adjust_portrait  = overlay_aspect_adjust_portrait;
      layout_desc.x_separation_portrait   = overlay_x_separation_portrait;
      layout_desc.y_separation_portrait   = overlay_y_separation_portrait;
      layout_desc.x_offset_portrait       = overlay_x_offset_portrait;
      layout_desc.y_offset_portrait       = overlay_y_offset_portrait;
      layout_desc.touch_scale             = overlay_touch_scale;
      layout_desc.auto_scale              = input_overlay_auto_scale;

      task_push_overlay_load_default(input_overlay_loaded,
            path_overlay,
            overlay_hide_in_menu,
            overlay_hide_when_gamepad_connected,
            input_overlay_enable,
            overlay_opacity,
            &layout_desc,
            NULL);
   }
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

bool input_keys_pressed_other_sources(
      input_driver_state_t *input_st,
      unsigned i,
      input_bits_t* p_new_state)
{
#ifdef HAVE_COMMAND
   int j;
   for (j = 0; j < ARRAY_SIZE(input_st->command); j++)
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

void input_keys_pressed(
      unsigned port,
      bool is_menu,
      int input_hotkey_block_delay,
      input_bits_t *p_new_state,
      const struct retro_keybind **binds,
      const struct retro_keybind *binds_norm,
      const struct retro_keybind *binds_auto,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info)
{
   unsigned i;
   input_driver_state_t *input_st = &input_driver_st;

   if (CHECK_INPUT_DRIVER_BLOCK_HOTKEY(binds_norm, binds_auto))
   {
      if (  input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               input_st->primary_joypad,
               sec_joypad,
               joypad_info,
               &binds[port],
               input_st->keyboard_mapping_blocked,
               port, RETRO_DEVICE_JOYPAD, 0,
               RARCH_ENABLE_HOTKEY))
      {
         if (input_st->input_hotkey_block_counter < input_hotkey_block_delay)
            input_st->input_hotkey_block_counter++;
         else
            input_st->block_libretro_input    = true;
      }
      else
      {
         input_st->input_hotkey_block_counter = 0;
         input_st->block_hotkey               = true;
      }
   }

   if (     !is_menu
         && binds[port][RARCH_GAME_FOCUS_TOGGLE].valid)
   {
      const struct retro_keybind *focus_binds_auto =
         &input_autoconf_binds[port][RARCH_GAME_FOCUS_TOGGLE];
      const struct retro_keybind *focus_normal     =
         &binds[port][RARCH_GAME_FOCUS_TOGGLE];

      /* Allows rarch_focus_toggle hotkey to still work
       * even though every hotkey is blocked */
      if (CHECK_INPUT_DRIVER_BLOCK_HOTKEY(
               focus_normal, focus_binds_auto))
      {
         if (input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  input_st->primary_joypad,
                  sec_joypad,
                  joypad_info,
                  &binds[port],
                  input_st->keyboard_mapping_blocked,
                  port,
                  RETRO_DEVICE_JOYPAD, 0, RARCH_GAME_FOCUS_TOGGLE))
            input_st->block_hotkey = false;
      }
   }

   {
      int16_t ret = 0;

      /* Check the libretro input first */
      if (!input_st->block_libretro_input)
         ret = input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               input_st->primary_joypad,
               sec_joypad,
               joypad_info, &binds[port],
               input_st->keyboard_mapping_blocked,
               port, RETRO_DEVICE_JOYPAD, 0,
               RETRO_DEVICE_ID_JOYPAD_MASK);

      for (i = 0; i < RARCH_FIRST_META_KEY; i++)
      {
         if (
               (ret & (UINT64_C(1) <<  i)) ||
               input_keys_pressed_other_sources(input_st,
                  i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }

   /* Check the hotkeys */
   if (input_st->block_hotkey)
   {
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         if (
                  BIT64_GET(lifecycle_state, i)
               || input_keys_pressed_other_sources(input_st,
                  i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }
   else
   {
      for (i = RARCH_FIRST_META_KEY; i < RARCH_BIND_LIST_END; i++)
      {
         bool bit_pressed = binds[port][i].valid
            && input_state_wrap(
                  input_st->current_driver,
                  input_st->current_data,
                  input_st->primary_joypad,
                  sec_joypad,
                  joypad_info,
                  &binds[port],
                  input_st->keyboard_mapping_blocked,
                  port, RETRO_DEVICE_JOYPAD, 0, i);
         if (     bit_pressed
               || BIT64_GET(lifecycle_state, i)
               || input_keys_pressed_other_sources(input_st,
                  i, p_new_state))
         {
            BIT256_SET_PTR(p_new_state, i);
         }
      }
   }
}

int16_t input_state_device(
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
                  && input_st->libretro_input_binds[port][id].valid;
               unsigned remap_button = settings->uints.input_remap_ids[port][id];

               /* TODO/FIXME: What on earth is this code doing...? */
               if (!
                     (      bind_valid
                            && id != remap_button
                     )
                  )
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
               if ((port == 0) &&
                   input_st->overlay_ptr &&
                   input_st->overlay_ptr->alive &&
                   BIT256_GET(input_st->overlay_ptr->overlay_state.buttons, id))
               {
#ifdef HAVE_MENU
                  bool menu_driver_alive        = menu_state_get_ptr()->alive;
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
            if (     (id  < RETRO_DEVICE_ID_JOYPAD_UP)    ||
                  (    (id  > RETRO_DEVICE_ID_JOYPAD_RIGHT) &&
                       (id <= RETRO_DEVICE_ID_JOYPAD_R3)))
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
                  else if (input_st->turbo_btns.turbo_pressed[port]>=0)
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
                     else if (!(input_st->turbo_btns.turbo_pressed[port] & (1 << id)) &&
                           turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON)
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
                  else if (turbo_mode == INPUT_TURBO_MODE_SINGLEBUTTON_HOLD &&
                        input_st->turbo_btns.enable[port] &&
                        input_st->turbo_btns.mode1_enable[port])
                  {
                     /* Hold mode stops turbo on release */
                     input_st->turbo_btns.mode1_enable[port] = 0;
                  }

                  if (!res && input_st->turbo_btns.mode1_enable[port] &&
                        input_st->turbo_btns.enable[port] & (1 << id))
                  {
                     /* if turbo button is enabled for this key ID */
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
               if (input_st->overlay_ptr && input_st->overlay_ptr->alive)
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
                     && input_st->libretro_input_binds[port][id].valid;

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
                        else if (settings->uints.input_remap_ids[port][offset+1] != (offset+1))
                           reset_state = true;
                     }

                     if (reset_state)
                        res = 0;
                     else
                     {
                        res = ret;

#ifdef HAVE_OVERLAY
                        if (input_st->overlay_ptr &&
                            input_st->overlay_ptr->alive &&
                            (port == 0) &&
                            !(((input_analog_dpad_mode == ANALOG_DPAD_LSTICK) &&
                                 (idx == RETRO_DEVICE_INDEX_ANALOG_LEFT)) ||
                             ((input_analog_dpad_mode == ANALOG_DPAD_RSTICK) &&
                                 (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT))))
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

         if (id < RARCH_FIRST_META_KEY)
         {
            bool bind_valid = input_st->libretro_input_binds[port]
               && input_st->libretro_input_binds[port][id].valid;

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
#ifdef HAVE_OVERLAY
   float input_overlay_opacity    = settings->floats.input_overlay_opacity;
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

   if (input_st->block_libretro_input)
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
      joypad_info[i].axis_threshold              = settings->floats.input_axis_threshold;
      joypad_info[i].joy_idx                     = settings->uints.input_joypad_index[i];
      joypad_info[i].auto_binds                  = input_autoconf_binds[joypad_info[i].joy_idx];

      input_st->turbo_btns.frame_enable[i]       = input_st->libretro_input_binds[i][RARCH_TURBO_ENABLE].valid ?
         input_state_wrap(
               input_st->current_driver,
               input_st->current_data,
               joypad,
               sec_joypad,
               &joypad_info[i],
               input_st->libretro_input_binds,
               input_st->keyboard_mapping_blocked,
               (unsigned)i,
               RETRO_DEVICE_JOYPAD,
               0,
               RARCH_TURBO_ENABLE) : 0;
   }

#ifdef HAVE_OVERLAY
   if (input_st->overlay_ptr && input_st->overlay_ptr->alive)
   {
      unsigned input_analog_dpad_mode = settings->uints.input_analog_dpad_mode[0];

      switch (input_analog_dpad_mode)
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
            {
               unsigned mapped_port = settings->uints.input_remap_ports[0];
               if (input_st->analog_requested[mapped_port])
                  input_analog_dpad_mode = ANALOG_DPAD_NONE;
            }
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

      input_poll_overlay(
            input_st->keyboard_mapping_blocked,
            settings,
            input_st->overlay_ptr,
            input_st->overlay_visibility,
            input_overlay_opacity,
            input_analog_dpad_mode,
            settings->floats.input_axis_threshold);
   }
#endif

#ifdef HAVE_MENU
   if (!menu_state_get_ptr()->alive)
#endif
   if (input_remap_binds_enable)
   {
#ifdef HAVE_OVERLAY
      input_overlay_t *overlay_pointer = (input_overlay_t*)input_st->overlay_ptr;
      bool poll_overlay                = (input_st->overlay_ptr && input_st->overlay_ptr->alive);
#endif
      input_mapper_t *handle           = &input_st->mapper;
      float input_analog_deadzone      = settings->floats.input_analog_deadzone;
      float input_analog_sensitivity   = settings->floats.input_analog_sensitivity;

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
               input_analog_dpad_mode = ANALOG_DPAD_LSTICK;
               break;
            case ANALOG_DPAD_RSTICK_FORCED:
               input_analog_dpad_mode = ANALOG_DPAD_RSTICK;
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
                        input_st->libretro_input_binds,
                        input_st->keyboard_mapping_blocked,
                        (unsigned)i, RETRO_DEVICE_JOYPAD,
                        0, RETRO_DEVICE_ID_JOYPAD_MASK);

                  for (k = 0; k < RARCH_FIRST_CUSTOM_BIND; k++)
                  {
                     if (ret & (1 << k))
                     {
                        bool valid_bind  =
                           input_st->libretro_input_binds[i][k].valid;

                        if (valid_bind)
                        {
                           int16_t   val =
                              input_joypad_analog_button(
                                    input_analog_deadzone,
                                    input_analog_sensitivity,
                                    joypad,
                                    &joypad_info[i],
                                    k,
                                    &input_st->libretro_input_binds[i][k]
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
                              input_st->libretro_input_binds[i]);

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
                     (current_button_value == 1) &&
                     (j != remap_button)         &&
                     (remap_button != RARCH_UNMAPPED);

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
                        (abs(current_axis_value) > 0 &&
                         (k != remap_axis)            &&
                         (remap_axis != RARCH_UNMAPPED)
                        ))
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
}

#ifdef HAVE_BSV_MOVIE
#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

#define BSV_MAGIC          0x42535631

static bool bsv_movie_init_playback(
      bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   content_crc               = content_get_crc();

   if (content_crc != 0)
      if (swap_if_big32(header[CRC_INDEX]) != content_crc)
         RARCH_WARN("%s.\n", msg_hash_to_str(MSG_CRC32_CHECKSUM_MISMATCH));

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }

      core_serialize_size( &info);

      if (info.size == state_size)
      {
         serial_info.data_const = handle->state;
         serial_info.size       = state_size;
         core_unserialize(&serial_info);
      }
      else
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(
      bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);

   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;
      uint8_t *st      = (uint8_t*)malloc(state_size);

      if (!st)
         return false;

      handle->state    = st;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

static void bsv_movie_free(bsv_movie_t *handle)
{
   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path,
      enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   if (handle)
      bsv_movie_free(handle);
   return NULL;
}

static bool runloop_check_movie_init(input_driver_state_t *input_st,
      settings_t *settings)
{
   char msg[16384], path[8192];
   bsv_movie_t *state          = NULL;
   int state_slot              = settings->ints.state_slot;

   msg[0] = path[0]            = '\0';

   configuration_set_uint(settings, settings->uints.rewind_granularity, 1);

   if (state_slot > 0)
      snprintf(path, sizeof(path), "%s%d.bsv",
            input_st->bsv_movie_state.movie_path,
            state_slot);
   else
      snprintf(path, sizeof(path), "%s.bsv",
            input_st->bsv_movie_state.movie_path);

   snprintf(msg, sizeof(msg), "%s \"%s\".",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   state = bsv_movie_init_internal(path, RARCH_MOVIE_RECORD);

   if (!state)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
      return false;
   }

   input_st->bsv_movie_state_handle         = state;

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   return true;
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

bool bsv_movie_init(input_driver_state_t *input_st)
{
   bsv_movie_t *state = NULL;
   if (input_st->bsv_movie_state.movie_start_playback)
   {
      if (!(state = bsv_movie_init_internal(
               input_st->bsv_movie_state.movie_start_path,
               RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               input_st->bsv_movie_state.movie_start_path);
         return false;
      }

      input_st->bsv_movie_state_handle        = state;
      input_st->bsv_movie_state.movie_playback = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

      return true;
   }
   else if (input_st->bsv_movie_state.movie_start_recording)
   {
      char msg[8192];

      if (!(state = bsv_movie_init_internal(
               input_st->bsv_movie_state.movie_start_path,
               RARCH_MOVIE_RECORD)))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("%s.\n",
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return false;
      }

      input_st->bsv_movie_state_handle         = state;
      snprintf(msg, sizeof(msg),
            "%s \"%s\".",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            input_st->bsv_movie_state.movie_start_path);

      runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            input_st->bsv_movie_state.movie_start_path);

      return true;
   }

   return false;
}

void bsv_movie_deinit(input_driver_state_t *input_st)
{
   if (input_st->bsv_movie_state_handle)
      bsv_movie_free(input_st->bsv_movie_state_handle);
   input_st->bsv_movie_state_handle = NULL;
}

bool bsv_movie_check(input_driver_state_t *input_st,
      settings_t *settings)
{
   if (!input_st->bsv_movie_state_handle)
      return runloop_check_movie_init(input_st, settings);

   if (input_st->bsv_movie_state.movie_playback)
   {
      /* Checks if movie is being played back. */
      if (!input_st->bsv_movie_state.movie_end)
         return false;
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED), 2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

      bsv_movie_deinit(input_st);

      input_st->bsv_movie_state.movie_end      = false;
      input_st->bsv_movie_state.movie_playback = false;

      return true;
   }

   /* Checks if movie is being recorded. */
   if (!input_st->bsv_movie_state_handle)
      return false;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED), 2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

   bsv_movie_deinit(input_st);

   return true;
}
#endif

int16_t input_state_internal(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   rarch_joypad_info_t joypad_info;
   unsigned mapped_port;
   input_driver_state_t *input_st          = &input_driver_st;
   settings_t *settings                    = config_get_ptr();
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
   bool input_blocked                      = (menu_st->input_driver_flushing_input > 0) ||
                                             input_st->block_libretro_input;
#else
   bool input_blocked                      = input_st->block_libretro_input;
#endif
   bool bitmask_enabled                    = false;
   unsigned max_users                      = settings->uints.input_max_users;
   int16_t result                          = 0;

   device                                 &= RETRO_DEVICE_MASK;
   bitmask_enabled                         = (device == RETRO_DEVICE_JOYPAD) &&
                                             (id == RETRO_DEVICE_ID_JOYPAD_MASK);
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
            input_st->libretro_input_binds,
            input_st->keyboard_mapping_blocked,
            mapped_port, device, idx, id);

      if ((device == RETRO_DEVICE_ANALOG) &&
          (ret == 0))
      {
         if (input_st->libretro_input_binds[mapped_port])
         {
            if (idx == RETRO_DEVICE_INDEX_ANALOG_BUTTON)
            {
               if (id < RARCH_FIRST_CUSTOM_BIND)
               {
                  bool valid_bind = input_st->libretro_input_binds[mapped_port][id].valid;

                  if (valid_bind)
                  {
                     if (sec_joypad)
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              sec_joypad, &joypad_info,
                              id,
                              &input_st->libretro_input_binds[mapped_port][id]);

                     if (joypad && (ret == 0))
                        ret = input_joypad_analog_button(
                              input_analog_deadzone,
                              input_analog_sensitivity,
                              joypad, &joypad_info,
                              id,
                              &input_st->libretro_input_binds[mapped_port][id]);
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
                        input_st->libretro_input_binds[mapped_port]);

               if (joypad && (ret == 0))
                  ret = input_joypad_analog_axis(
                        input_analog_dpad_mode,
                        input_analog_deadzone,
                        input_analog_sensitivity,
                        joypad,
                        &joypad_info,
                        idx,
                        id,
                        input_st->libretro_input_binds[mapped_port]);
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
            int16_t port_result_abs = (port_result >= 0) ?
               port_result : -port_result;
            int16_t result_abs      = (result >= 0) ?
               result : -result;

            if (port_result_abs > result_abs)
               result = port_result;
         }
      }
      else
         result |= port_result;
   }

#ifdef HAVE_BSV_MOVIE
   /* Save input to BSV record, if enabled */
   if (BSV_MOVIE_IS_PLAYBACK_OFF())
   {
      result = swap_if_big16(result);
      intfstream_write(
            input_st->bsv_movie_state_handle->file, &result, 2);
   }
#endif

   return result;
}

int16_t input_driver_state_wrapper(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   input_driver_state_t 
      *input_st                = &input_driver_st;
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

      input_st->bsv_movie_state.movie_end = true;
   }
#endif

   /* Read input state */
   result = input_state_internal(port, device, idx, id);

   /* Register any analog stick input requests for
    * this 'virtual' (core) port */
   if (     (device == RETRO_DEVICE_ANALOG) &&
       (    (idx    == RETRO_DEVICE_INDEX_ANALOG_LEFT) ||
            (idx    == RETRO_DEVICE_INDEX_ANALOG_RIGHT)))
      input_st->analog_requested[port] = true;

#ifdef HAVE_BSV_MOVIE
   /* Save input to BSV record, if enabled */
   if (BSV_MOVIE_IS_PLAYBACK_OFF())
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
   settings_t *settings        = config_get_ptr();
   global_t *global            = global_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      global->old_analog_dpad_mode[i] = settings->uints.input_analog_dpad_mode[i];
      global->old_libretro_device[i]  = settings->uints.input_libretro_device[i];
   }

   global->old_analog_dpad_mode_set = true;
   global->old_libretro_device_set  = true;
}

void input_remapping_enable_global_config_restore(void)
{
   global_t *global               = global_get_ptr();
   global->remapping_cache_active = true;
}

void input_remapping_restore_global_config(bool clear_cache)
{
   unsigned i;
   settings_t *settings        = config_get_ptr();
   global_t *global            = global_get_ptr();

   if (!global->remapping_cache_active)
      goto end;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (global->old_analog_dpad_mode_set &&
          (settings->uints.input_analog_dpad_mode[i] !=
               global->old_analog_dpad_mode[i]))
         configuration_set_uint(settings,
               settings->uints.input_analog_dpad_mode[i],
               global->old_analog_dpad_mode[i]);

      if (global->old_libretro_device_set &&
          (settings->uints.input_libretro_device[i] !=
               global->old_libretro_device[i]))
         configuration_set_uint(settings,
               settings->uints.input_libretro_device[i],
               global->old_libretro_device[i]);
   }

end:
   if (clear_cache)
   {
      global->old_analog_dpad_mode_set = false;
      global->old_libretro_device_set  = false;
      global->remapping_cache_active   = false;
   }
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

void input_remapping_deinit(void)
{
   global_t *global                        = global_get_ptr();
   runloop_state_t *runloop_st             = runloop_state_get_ptr();
   if (global->name.remapfile)
      free(global->name.remapfile);
   global->name.remapfile                  = NULL;
   runloop_st->remaps_core_active          = false;
   runloop_st->remaps_content_dir_active   = false;
   runloop_st->remaps_game_active          = false;
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
