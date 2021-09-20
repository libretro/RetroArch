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

#include "input_driver.h"
#include "input_keymaps.h"
#include "input_remapping.h"
#include "input_osk.h"

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#include "../command.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../verbosity.h"
#include "../configuration.h"
#include "../list_special.h"
#include "../performance_counters.h"

#define HOLD_BTN_DELAY_SEC 2

/**************************************/

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
   &null_hid,
   NULL,
};
#endif

/**************************************/

/* private function prototypes */

static const input_device_driver_t *input_joypad_init_first(void *data);


/**************************************/

bool input_driver_set_rumble(
         input_driver_state_t *driver_state, unsigned port, unsigned joy_idx, 
         enum retro_rumble_effect effect, uint16_t strength)
{
   const input_device_driver_t  *primary_joypad;
   const input_device_driver_t      *sec_joypad;
   bool rumble_state   = false;

   if (!driver_state || (joy_idx >= MAX_USERS))
      return false;

   primary_joypad = driver_state->primary_joypad;
   sec_joypad     = driver_state->secondary_joypad;

   if (primary_joypad && primary_joypad->set_rumble)
      rumble_state = primary_joypad->set_rumble(joy_idx, effect, strength);
   
   /* if sec_joypad exists, this set_rumble() return value will replace primary_joypad's return */
   if (sec_joypad     && sec_joypad->set_rumble)
      rumble_state = sec_joypad->set_rumble(joy_idx, effect, strength);

   return rumble_state;
}

/**************************************/

bool input_driver_set_rumble_gain(
         input_driver_state_t *driver_state, unsigned gain,
         unsigned input_max_users)
{
   unsigned i;

   if (driver_state->primary_joypad
      && driver_state->primary_joypad->set_rumble_gain)
   {
      for (i = 0; i < input_max_users; i++)
         driver_state->primary_joypad->set_rumble_gain(i, gain);
      return true;
   }
   return false;
}

/**************************************/

bool input_driver_set_sensor(
         input_driver_state_t *driver_state, unsigned port, bool sensors_enable,
         enum retro_sensor_action action, unsigned rate)
{
   const input_driver_t *current_driver;
   void *current_data;

   if (!driver_state || !driver_state->current_data)
      return false;

   current_driver = driver_state->current_driver;
   current_data = driver_state->current_data;

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
         input_driver_state_t *driver_state,
         unsigned port, bool sensors_enable, unsigned id)
{
   const input_driver_t *current_driver;
   void *current_data;

   if (!driver_state || !driver_state->current_data)
      return 0.0f;

   current_driver = driver_state->current_driver;
   current_data   = driver_state->current_data;

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

bool input_driver_toggle_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t* p_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return true;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_TOGGLE_L3_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_TOGGLE_L_R:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return true;
         break;
      case INPUT_TOGGLE_DOWN_SELECT:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return true;
         break;
      case INPUT_TOGGLE_L2_R2:
         if (BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L2) &&
             BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R2))
            return true;
         break;
      case INPUT_TOGGLE_HOLD_START:
         {
            static rarch_timer_t timer = {0};

            if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            {
               /* timer only runs while start is held down */
               timer.timer_end   = true;
               timer.timer_begin = false;
               timer.timeout_end = 0;
               return false;
            }

            /* User started holding down the start button, start the timer */
            if (!timer.timer_begin)
            {
               uint64_t current_usec = cpu_features_get_time_usec();
               timer.timeout_us      = HOLD_BTN_DELAY_SEC * 1000000;
               timer.current         = current_usec;
               timer.timeout_end     = timer.current + timer.timeout_us;
               timer.timer_begin     = true;
               timer.timer_end       = false;
            }

            timer.current            = current_time;
            timer.timeout_us         = (timer.timeout_end - timer.current);

            if (!timer.timer_end && (timer.timeout_us <= 0))
            {
               /* start has been held down long enough,
                * stop timer and enter menu */
               timer.timer_end   = true;
               timer.timer_begin = false;
               timer.timeout_end = 0;
               return true;
            }

         }
         break;
      case INPUT_TOGGLE_HOLD_SELECT:
         {
            static rarch_timer_t timer = {0};

            if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            {
               /* timer only runs while select is held down */
               timer.timer_end   = true;
               timer.timer_begin = false;
               timer.timeout_end = 0;
               return false;
            }

            /* user started holding down the select button, start the timer */
            if (!timer.timer_begin)
            {
               uint64_t current_usec = cpu_features_get_time_usec();
               timer.timeout_us      = HOLD_BTN_DELAY_SEC * 1000000;
               timer.current         = current_usec;
               timer.timeout_end     = timer.current + timer.timeout_us;
               timer.timer_begin     = true;
               timer.timer_end       = false;
            }

            timer.current            = current_time;
            timer.timeout_us         = (timer.timeout_end - timer.current);

            if (!timer.timer_end && (timer.timeout_us <= 0))
            {
               /* select has been held down long enough,
                * stop timer and enter menu */
               timer.timer_end   = true;
               timer.timer_begin = false;
               timer.timeout_end = 0;
               return true;
            }
         }
         break;
      default:
      case INPUT_TOGGLE_NONE:
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
      input_driver_state_t *input_driver_state,
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled)
{
   int i                = (int)driver_find_index(
         "input_driver",
         settings->arrays.input_driver);

   if (i >= 0)
   {
      input_driver_state->current_driver = (input_driver_t*)input_drivers[i];
      RARCH_LOG("[Input]: Found %s: \"%s\".\n", prefix,
            input_driver_state->current_driver->ident);
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
      input_driver_state->current_driver = tmp;
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
