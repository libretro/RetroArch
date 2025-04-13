/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stdlib.h>

#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_inline.h>

#ifdef HAVE_XI2
#include <X11/extensions/XInput2.h>
#define MAX_MOUSE_IDX MAX_INPUT_DEVICES
#else
#define MAX_MOUSE_IDX 1
#endif

#include "../input_keymaps.h"
#include "../input_defines.h"

#include "../common/input_x11_common.h"
#include "../common/linux_common.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct x11_input
{
   Display *display;
   Window win;

#ifdef HAVE_XI2
   int mouse_dev_list[MAX_MOUSE_IDX];
#endif
   int mouse_x[MAX_MOUSE_IDX];
   int mouse_y[MAX_MOUSE_IDX];
   int mouse_delta_x[MAX_MOUSE_IDX];
   int mouse_delta_y[MAX_MOUSE_IDX];
   bool mouse_grabbed;
   char state[32];
   bool mouse_l[MAX_MOUSE_IDX];
   bool mouse_r[MAX_MOUSE_IDX];
   bool mouse_m[MAX_MOUSE_IDX];
#ifdef HAVE_XI2
   bool mouse_4[MAX_MOUSE_IDX];
   bool mouse_5[MAX_MOUSE_IDX];
   XIDeviceInfo *di;
#endif

#ifdef __linux__
   /* X11 is mostly used on Linux, but not exclusively. */
   linux_illuminance_sensor_t *illuminance_sensor;
#endif
} x11_input_t;

/* Public global variable */
extern bool g_x11_entered;

static void *x_input_init(const char *joypad_driver)
{
   x11_input_t *x11;
#ifdef HAVE_XI2
   XIDeviceInfo *dev;
   XIButtonClassInfo *classinfo;
   int i, j = 0;
   int cnt;
#endif

   /* Currently active window is not an X11 window. Cannot use this driver. */
   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
      return NULL;
   if (!(x11 = (x11_input_t*)calloc(1, sizeof(*x11))))
      return NULL;

   /* Borrow the active X window ... */
   x11->display = (Display*)video_driver_display_get();
   x11->win     = (Window)video_driver_window_get();

   input_keymaps_init_keyboard_lut(rarch_key_map_x11);

#ifdef HAVE_XI2
   x11->di = XIQueryDevice(x11->display, XIAllDevices, &cnt);
   for (i = 0; i < cnt; i++)
   {
      x11->mouse_dev_list[i] = -1;
      dev = &(x11->di[i]);
      RARCH_DBG("[X11]: Device detected, %d \"%s\" attached to %d\n", i, dev->name, dev->attachment);
      if (dev->use == XIMasterPointer)
      {
         RARCH_LOG("[X11]: Master pointer, %d \"%s\"\n",dev->deviceid,dev->name);
         input_config_set_mouse_display_name(j, dev->name);
         x11->mouse_dev_list[j++] = dev->deviceid;
      }
   }
#else
   RARCH_DBG("[X11]: XInput2 support not compiled in, using only 1 mouse.\n");
#endif
   return x11;
}

static bool x_keyboard_pressed(x11_input_t *x11, unsigned key)
{
   int keycode = rarch_keysym_lut[(enum retro_key)key];
   return x11->state[keycode >> 3] & (1 << (keycode & 7));
}

static bool x_mouse_button_pressed(
      x11_input_t *x11, unsigned port, unsigned key)
{
   unsigned mouse_port = port;
#ifndef HAVE_XI2
   mouse_port = 0;
#endif

   switch (key)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return x11->mouse_l[mouse_port];
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return x11->mouse_r[mouse_port];
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return x11->mouse_m[mouse_port];
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
#ifdef HAVE_XI2
         return x11->mouse_4[mouse_port];
#endif
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
#ifdef HAVE_XI2
         return x11->mouse_5[mouse_port];
#endif
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return x_mouse_state_wheel(key);
   }

   return false;
}

static int16_t x_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{

   if (port < MAX_USERS)
   {
      unsigned mouse_port = port;
      
#ifndef HAVE_XI2
      mouse_port = 0;
#endif
      x11_input_t *x11     = (x11_input_t*)data;
      settings_t *settings = config_get_ptr();

      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
            {
               unsigned i;
               int16_t ret = 0;

               if (settings->uints.input_mouse_index[port] == 0)
               {
                  for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                  {
                     if (binds[port][i].valid)
                     {
                        if (x_mouse_button_pressed(x11, port, binds[port][i].mbutton))
                           ret |= (1 << i);
                     }
                  }
               }

               if (!keyboard_mapping_blocked)
               {
                  for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
                  {
                     if (binds[port][i].valid)
                     {
                        if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                              && x_keyboard_pressed(x11, binds[port][i].key))
                           ret |= (1 << i);
                     }
                  }
               }

               return ret;
            }

            if (id < RARCH_BIND_LIST_END)
            {
               if (binds[port][id].valid)
               {
                  if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                        && x_keyboard_pressed(x11, binds[port][id].key)
                        && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                     )
                     return 1;
                  else if (settings->uints.input_mouse_index[port] == 0)
                  {
                     if (x_mouse_button_pressed(x11, port, binds[port][id].mbutton))
                        return 1;
                  }
               }
            }
            break;
         case RETRO_DEVICE_ANALOG:
            if (binds)
            {
               int id_minus_key      = 0;
               int id_plus_key       = 0;
               unsigned id_minus     = 0;
               unsigned id_plus      = 0;
               int16_t pressed_minus = 0;
               int16_t pressed_plus  = 0;
               int16_t ret           = 0;
               bool id_plus_valid    = false;
               bool id_minus_valid   = false;

               input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

               id_minus_valid        = binds[port][id_minus].valid;
               id_plus_valid         = binds[port][id_plus].valid;
               id_minus_key          = binds[port][id_minus].key;
               id_plus_key           = binds[port][id_plus].key;

               if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
               {
                  unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
                  if (x11->state[sym >> 3] & (1 << (sym & 7)))
                     ret = 0x7fff;
               }
               if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
               {
                  unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
                  if (x11->state[sym >> 3] & (1 << (sym & 7)))
                     ret += -0x7fff;
               }

               return ret;
            }
            break;
         case RETRO_DEVICE_KEYBOARD:
            return (id && id < RETROK_LAST) && x_keyboard_pressed(x11, id);
         case RETRO_DEVICE_MOUSE:
         case RARCH_DEVICE_MOUSE_SCREEN:
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return x11->mouse_x[mouse_port];
                  return x11->mouse_delta_x[mouse_port];
               case RETRO_DEVICE_ID_MOUSE_Y:
                  if (device == RARCH_DEVICE_MOUSE_SCREEN)
                     return x11->mouse_y[mouse_port];
                  return x11->mouse_delta_y[mouse_port];
               case RETRO_DEVICE_ID_MOUSE_LEFT:
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return x_mouse_button_pressed(x11, mouse_port, id);
            }
            break;
         case RETRO_DEVICE_POINTER:
         case RARCH_DEVICE_POINTER_SCREEN:
            /* Map up to 3 touches to mouse buttons. */
            if (idx < 3)
            {
               struct video_viewport vp    = {0};
               bool screen                 =
                  (device == RARCH_DEVICE_POINTER_SCREEN);
               int16_t res_x               = 0;
               int16_t res_y               = 0;
               int16_t res_screen_x        = 0;
               int16_t res_screen_y        = 0;

               if (video_driver_translate_coord_viewport_confined_wrap(
                        &vp, x11->mouse_x[mouse_port], x11->mouse_y[mouse_port],
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
               {
                  if (screen)
                  {
                     res_x = res_screen_x;
                     res_y = res_screen_y;
                  }

                  switch (id)
                  {
                     case RETRO_DEVICE_ID_POINTER_X:
                        return res_x;
                     case RETRO_DEVICE_ID_POINTER_Y:
                        return res_y;
                     case RETRO_DEVICE_ID_POINTER_PRESSED:
                        if (idx == 0)
                           return (x11->mouse_l[mouse_port]
                                 | x11->mouse_r[mouse_port]
                                 | x11->mouse_m[mouse_port]);
                        else if (idx == 1)
                           return (x11->mouse_r[mouse_port]
                                 | x11->mouse_m[mouse_port]);
                        else if (idx == 2)
                           return x11->mouse_m[mouse_port];
                     case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                        return input_driver_pointer_is_offscreen(res_x, res_y);
                  }
               }
            }
            break;
         case RETRO_DEVICE_LIGHTGUN:
            switch ( id )
            {
               /*aiming*/
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
               case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
               case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                  {
                     struct video_viewport vp    = {0};
                     int16_t res_x               = 0;
                     int16_t res_y               = 0;
                     int16_t res_screen_x        = 0;
                     int16_t res_screen_y        = 0;

                     if (video_driver_translate_coord_viewport_wrap(&vp,
                              x11->mouse_x[mouse_port], x11->mouse_y[mouse_port],
                              &res_x, &res_y, &res_screen_x, &res_screen_y))
                     {
                        switch ( id )
                        {
                           case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                              return res_x;
                           case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                              return res_y;
                           case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                              return input_driver_pointer_is_offscreen(res_x, res_y);
                           default:
                              break;
                        }
                     }
                  }
                  break;
                  /*buttons*/
               case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
               case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
               case RETRO_DEVICE_ID_LIGHTGUN_START:
               case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
               case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
               case RETRO_DEVICE_ID_LIGHTGUN_PAUSE: /* deprecated */
                  {
                     unsigned new_id                = input_driver_lightgun_id_convert(id);
                     const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                     const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                     const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                     const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                     uint16_t joyport               = joypad_info->joy_idx;
                     float axis_threshold           = joypad_info->axis_threshold;
                     const uint64_t joykey          = (bind_joykey != NO_BTN)
                        ? bind_joykey  : autobind_joykey;
                     const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                        ? bind_joyaxis : autobind_joyaxis;

                     if (binds[port][new_id].valid)
                     {
                        if ((uint16_t)joykey != NO_BTN && joypad->button(
                                 joyport, (uint16_t)joykey))
                           return 1;
                        if (joyaxis != AXIS_NONE &&
                              ((float)abs(joypad->axis(joyport, joyaxis))
                               / 0x8000) > axis_threshold)
                           return 1;
                        else if ((binds[port][new_id].key && binds[port][new_id].key < RETROK_LAST)
                              && !keyboard_mapping_blocked
                              && x_keyboard_pressed(x11, binds[port][new_id].key)
                           )
                           return 1;
                        else if (x_mouse_button_pressed(x11, port, binds[port][new_id].mbutton))
                              return 1;
                     }
                  }
                  break;
                  /*deprecated*/
               case RETRO_DEVICE_ID_LIGHTGUN_X:
                  return x11->mouse_delta_x[mouse_port];
               case RETRO_DEVICE_ID_LIGHTGUN_Y:
                  return x11->mouse_delta_y[mouse_port];
            }
            break;
      }
   }

   return 0;
}

static void x_input_free(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (x11)
   {
#ifdef HAVE_XI2
      XIFreeDeviceInfo(x11->di);
#endif
#ifdef __linux__
      linux_close_illuminance_sensor(x11->illuminance_sensor);
#endif
      free(x11);
   }
}

static bool x_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (!x11)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
         /* If already disabled, then do nothing */
#ifdef __linux__
         linux_close_illuminance_sensor(x11->illuminance_sensor); /* noop if NULL */
         x11->illuminance_sensor = NULL;
#endif
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         /** Unimplemented sensor actions that probably shouldn't fail */
         return true;

#ifdef __linux__
      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
         if (x11->illuminance_sensor)
           /* If we already have a sensor, just set the rate */
           linux_set_illuminance_sensor_rate(x11->illuminance_sensor, rate);
         else
           x11->illuminance_sensor = linux_open_illuminance_sensor(rate);

         return x11->illuminance_sensor != NULL;
#endif
      default:
         break;
   }

   return false;
}

static float x_get_sensor_input(void *data, unsigned port, unsigned id)
{
   x11_input_t *x11 = (x11_input_t*)data;

   if (!x11)
      return 0.0f;

   switch (id)
   {
#ifdef __linux__
      case RETRO_SENSOR_ILLUMINANCE:
         if (x11->illuminance_sensor)
            return linux_get_illuminance_reading(x11->illuminance_sensor);
#endif
      default:
         break;
   }

   return 0.0f;
}

static void x_input_poll(void *data)
{
   Window root_win;
   Window child_win;
   x11_input_t *x11         = (x11_input_t*)data;
   bool video_has_focus     = video_driver_has_focus();
#ifdef HAVE_XI2
   double root_x            = 0;
   double root_y            = 0;
   double win_x             = 0;
   double win_y             = 0;
   XIButtonState buttons_return;
   XIModifierState modifiers_return;
   XIGroupState group_return;
   settings_t *settings = config_get_ptr();
   unsigned mouse_dev_idx;
#else
   int root_x               = 0;
   int root_y               = 0;
   int win_x                = 0;
   int win_y                = 0;
   unsigned mask            = 0;
#endif
   unsigned mouse_port;

   /* If window loses focus, 'reset' keyboard
    * and ignore mouse input */
   if (!video_has_focus)
   {
      memset(x11->state,         0, sizeof(x11->state));
      memset(x11->mouse_delta_x, 0, sizeof(x11->mouse_delta_x));
      memset(x11->mouse_delta_y, 0, sizeof(x11->mouse_delta_y));
      memset(x11->mouse_l,       0, sizeof(x11->mouse_l));
      memset(x11->mouse_m,       0, sizeof(x11->mouse_m));
      memset(x11->mouse_r,       0, sizeof(x11->mouse_r));
#ifdef HAVE_XI2
      memset(x11->mouse_4,       0, sizeof(x11->mouse_4));
      memset(x11->mouse_5,       0, sizeof(x11->mouse_5));
#endif
      return;
   }

   /* Process keyboard */
   XQueryKeymap(x11->display, x11->state);

   /* If pointer is not inside the application
    * window, ignore mouse input */
   if (!g_x11_entered)
   {
      memset(x11->mouse_delta_x, 0, sizeof(x11->mouse_delta_x));
      memset(x11->mouse_delta_y, 0, sizeof(x11->mouse_delta_y));
      memset(x11->mouse_l,       0, sizeof(x11->mouse_l));
      memset(x11->mouse_m,       0, sizeof(x11->mouse_m));
      memset(x11->mouse_r,       0, sizeof(x11->mouse_r));
#ifdef HAVE_XI2
      memset(x11->mouse_4,       0, sizeof(x11->mouse_4));
      memset(x11->mouse_5,       0, sizeof(x11->mouse_5));
#endif
      return;
   }

   for(mouse_port = 0; mouse_port < MAX_MOUSE_IDX; mouse_port++)
   {
#ifdef HAVE_XI2
      mouse_dev_idx = settings->uints.input_mouse_index[mouse_port];
      if (mouse_dev_idx >= MAX_INPUT_DEVICES || x11->mouse_dev_list[mouse_dev_idx] < 0)
         return;

      /* Process mouse */
      if (!XIQueryPointer( x11->display,
                     x11->mouse_dev_list[mouse_dev_idx],
                     x11->win,
                     &root_win, &child_win,
                     &root_x, &root_y,
                     &win_x, &win_y,
                     &buttons_return,
                     &modifiers_return,
                     &group_return))
         return;
#else
      if (!XQueryPointer(x11->display,
            x11->win,
            &root_win, &child_win,
            &root_x, &root_y,
            &win_x, &win_y,
            &mask))
         return;
#endif

#ifdef HAVE_XI2
      /* > Mouse buttons - fixed map (1,2,3,8,9) */
      x11->mouse_l[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<1 : 0;
      x11->mouse_m[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<2 : 0;
      x11->mouse_r[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<3 : 0;
      x11->mouse_4[mouse_port] = buttons_return.mask_len > 1 ? buttons_return.mask[1] & 1<<0 : 0;
      x11->mouse_5[mouse_port] = buttons_return.mask_len > 1 ? buttons_return.mask[1] & 1<<1 : 0;
#else
      x11->mouse_l[mouse_port] = mask & Button1Mask;
      x11->mouse_m[mouse_port] = mask & Button2Mask;
      x11->mouse_r[mouse_port] = mask & Button3Mask;
      /* Buttons 4 and 5 are not returned here, so they are handled elsewhere. */
#endif

      /* > Mouse pointer */
      if (!x11->mouse_grabbed)
      {
         /* Mouse is not grabbed - this corresponds
          * to 'conventional' pointer input, using
          * absolute screen coordinates */
      int mouse_last_x               = x11->mouse_x[mouse_port];
      int mouse_last_y               = x11->mouse_y[mouse_port];

      x11->mouse_x[mouse_port]       = win_x;
      x11->mouse_y[mouse_port]       = win_y;

      x11->mouse_delta_x[mouse_port] = x11->mouse_x[mouse_port] - mouse_last_x;
      x11->mouse_delta_y[mouse_port] = x11->mouse_y[mouse_port] - mouse_last_y;
      }
      else
      {
         /* Mouse is grabbed - all pointer movement
          * must be considered 'relative' */
         XWindowAttributes win_attr;
         int centre_x, centre_y;
         int warp_x            = win_x;
         int warp_y            = win_y;
         bool do_warp          = false;

         /* Get dimensions/centre coordinates of
          * application window */
         if (!XGetWindowAttributes(x11->display, x11->win, &win_attr))
         {
            x11->mouse_delta_x[mouse_port] = 0;
            x11->mouse_delta_y[mouse_port] = 0;
            return;
         }

         centre_x              = win_attr.width  >> 1;
         centre_y              = win_attr.height >> 1;

         /* Get relative movement delta since last
          * poll event */
         x11->mouse_delta_x[mouse_port] = win_x - centre_x;
         x11->mouse_delta_y[mouse_port] = win_y - centre_y;

         /* Get effective 'absolute' pointer location
          * (last position + delta, bounded by current
          * application window dimensions) */
         x11->mouse_x[mouse_port]     += x11->mouse_delta_x[mouse_port];
         x11->mouse_y[mouse_port]     += x11->mouse_delta_y[mouse_port];

         /* Clamp X */
         if (x11->mouse_x[mouse_port] < 0)
            x11->mouse_x[mouse_port] = 0;
         if (x11->mouse_x[mouse_port] >= win_attr.width)
            x11->mouse_x[mouse_port] = (win_attr.width - 1);

         /* Clamp Y */
         if (x11->mouse_y[mouse_port] < 0)
            x11->mouse_y[mouse_port] = 0;
         if (x11->mouse_y[mouse_port] >= win_attr.height)
            x11->mouse_y[mouse_port] = (win_attr.height - 1);

         /* Hack/workaround:
          * - X11 gives absolute pointer coordinates
          * - Once the pointer reaches a screen edge
          *   it cannot go any further
          * - To achieve 'relative' motion, we therefore
          *   have to reset the hardware cursor to the
          *   centre of the screen after polling each
          *   movement delta, such that it is always
          *   free to move in all directions during the
          *   time interval until the next poll event */
         if (win_x != centre_x)
         {
            warp_x  = centre_x;
            do_warp = true;
         }

         if (win_y != centre_y)
         {
            warp_y  = centre_y;
            do_warp = true;
         }

         if (do_warp)
         {
            XWarpPointer(x11->display, None,
                  x11->win, 0, 0, 0, 0,
                  warp_x, warp_y);
            XSync(x11->display, False);
         }
      }
   }
}

static void x_grab_mouse(void *data, bool state)
{
   x11_input_t *x11 = (x11_input_t*)data;
   if (x11)
      x11->mouse_grabbed = state;
}

static uint64_t x_input_get_capabilities(void *data)
{
   return
           (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_LIGHTGUN)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_x = {
   x_input_init,
   x_input_poll,
   x_input_state,
   x_input_free,
#ifdef __linux__
   /* Right now this driver only supports the illuminance sensor on Linux. */
   x_set_sensor_state,
   x_get_sensor_input,
#else
   NULL,
   NULL,
#endif
   x_input_get_capabilities,
   "x",
   x_grab_mouse,
   NULL,
   NULL
};
