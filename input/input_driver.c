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

#include <string/stdstring.h>

#include "input_driver.h"

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

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
   NULL,
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

         return false;
      }
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

         return false;
      }
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
