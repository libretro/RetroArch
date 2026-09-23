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
#include <string.h>

#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#include <boolean.h>
#include <compat/strl.h>
#include <retro_inline.h>
#include <retro_atomic.h>

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
#ifdef HAVE_XI2
   /* Each master pointer's position in the window, as of the last
    * event on event_display. */
   double ptr_x[MAX_MOUSE_IDX];
   double ptr_y[MAX_MOUSE_IDX];
#endif
   Display *display;
   /* The driver's own connection, carrying the window's key and focus
    * events and, with XInput 2, its pointer events; NULL when it
    * could not be opened. */
   Display *event_display;
   Window win;

#ifdef HAVE_XI2
   /* First request after a warp: older events are from before it. */
   unsigned long ptr_warp_serial[MAX_MOUSE_IDX];
   int mouse_dev_list[MAX_MOUSE_IDX];
   int xi_opcode;
   /* Buttons held, bit n for button n. */
   unsigned ptr_buttons[MAX_MOUSE_IDX];
#endif
   int mouse_x[MAX_MOUSE_IDX];
   int mouse_y[MAX_MOUSE_IDX];
   int mouse_delta_x[MAX_MOUSE_IDX];
   int mouse_delta_y[MAX_MOUSE_IDX];
   bool mouse_grabbed;
   char state[32];
   /* Keys held, kept from the events on event_display. */
   char keys[32];
#ifdef HAVE_XI2
   bool ptr_inside[MAX_MOUSE_IDX];
   bool ptr_warp_pending[MAX_MOUSE_IDX];
   /* The pointer is read from events rather than queried. */
   bool ptr_events;
#endif
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

/* Public global variables, owned by x11_common.c */
extern retro_atomic_int_t g_x11_entered;
extern retro_atomic_int_t g_x11_size;
extern Window             g_x11_win;

/* The poll reads the keyboard from events on a connection of the
 * driver's own. Draining it is a non-blocking read, where a query on
 * the shared connection is a round trip queued behind whatever the
 * video thread is presenting through it. Events are drained at the
 * poll, so a key reads as of the poll. */
static Display *x_events_open(x11_input_t *x11)
{
   Display *dpy = XOpenDisplay(DisplayString(x11->display));

   if (!dpy)
      return NULL;

   /* A held key repeats as presses alone, so no read of the socket
    * can end between a repeat's release and its press. */
   XkbSetDetectableAutoRepeat(dpy, True, NULL);

   /* KeymapNotify follows every FocusIn and EnterNotify with the
    * whole key vector. */
   XSelectInput(dpy, x11->win, KeyPressMask | KeyReleaseMask
         | FocusChangeMask | KeymapStateMask);

   /* Keys already down. Events selected above that the query saw
    * are applied again in order, which leaves each key at its last
    * event. */
   XQueryKeymap(dpy, x11->keys);
   return dpy;
}

#ifdef HAVE_XI2
static Display *x_select_dpy;
static int      x_select_error_code;
static int    (*x_select_prev)(Display*, XErrorEvent*);

/* XI_ButtonPress is one client's per window; a refusal on the
 * driver's own connection is reported here rather than ending the
 * process. Errors on other connections go to the handler before. */
static int x_select_error(Display *dpy, XErrorEvent *event)
{
   if (dpy == x_select_dpy)
   {
      x_select_error_code = event->error_code;
      return 0;
   }
   return x_select_prev ? x_select_prev(dpy, event) : 0;
}

static unsigned x_button_word(const XIButtonState *state)
{
   unsigned word = 0;
   if (state->mask_len > 0)
      word  = state->mask[0];
   if (state->mask_len > 1)
      word |= (unsigned)state->mask[1] << 8;
   return word;
}

static int x_pointer_index(const x11_input_t *x11, int deviceid)
{
   int i;
   for (i = 0; i < MAX_MOUSE_IDX; i++)
      if (x11->mouse_dev_list[i] == deviceid)
         return i;
   return -1;
}

/* The master pointers' motion, buttons and crossings on the window
 * come to event_display, which then holds the window's implicit grab
 * and gets the button events the pump would; the wheel is latched
 * from here instead. Each pointer is asked once, here, where it
 * starts. */
static bool x_pointer_open(x11_input_t *x11, int xi_opcode)
{
   int i;
   XIEventMask event_mask;
   XWindowAttributes attr;
   unsigned char mask[XIMaskLen(XI_LASTEVENT)];
   Display *dpy = x11->event_display;

   memset(mask, 0, sizeof(mask));
   XISetMask(mask, XI_Motion);
   XISetMask(mask, XI_ButtonPress);
   XISetMask(mask, XI_ButtonRelease);
   XISetMask(mask, XI_Enter);
   XISetMask(mask, XI_Leave);
   event_mask.deviceid = XIAllMasterDevices;
   event_mask.mask_len = sizeof(mask);
   event_mask.mask     = mask;

   x_select_dpy        = dpy;
   x_select_error_code = 0;
   x_select_prev       = XSetErrorHandler(x_select_error);
   XISelectEvents(dpy, x11->win, &event_mask, 1);
   XSync(dpy, False);
   XSetErrorHandler(x_select_prev);
   x_select_prev       = NULL;
   x_select_dpy        = NULL;

   if (x_select_error_code || !XGetWindowAttributes(dpy, x11->win, &attr))
   {
      event_mask.mask_len = 0;
      XISelectEvents(dpy, x11->win, &event_mask, 1);
      return false;
   }

   for (i = 0; i < MAX_MOUSE_IDX; i++)
   {
      Window root_win, child_win;
      double root_x, root_y, win_x, win_y;
      XIButtonState buttons;
      XIModifierState mods;
      XIGroupState group;

      if (x11->mouse_dev_list[i] < 0)
         continue;
      if (XIQueryPointer(dpy, x11->mouse_dev_list[i], x11->win,
               &root_win, &child_win, &root_x, &root_y,
               &win_x, &win_y, &buttons, &mods, &group))
      {
         x11->ptr_x[i]       = win_x;
         x11->ptr_y[i]       = win_y;
         x11->ptr_buttons[i] = x_button_word(&buttons);
         x11->ptr_inside[i]  =    win_x >= 0 && win_x < attr.width
                               && win_y >= 0 && win_y < attr.height;
         /* Allocated for the caller, on success only */
         XFree(buttons.mask);
      }
   }

   x11->xi_opcode = xi_opcode;
   return true;
}

/* A position older than the last warp is one the warp replaced. */
static void x_pointer_move(x11_input_t *x11, int i,
      unsigned long serial, double x, double y)
{
   if (x11->ptr_warp_pending[i])
   {
      if ((long)(serial - x11->ptr_warp_serial[i]) < 0)
         return;
      x11->ptr_warp_pending[i] = false;
   }
   x11->ptr_x[i] = x;
   x11->ptr_y[i] = y;
}

static void x_pointer_event(x11_input_t *x11, XGenericEventCookie *cookie)
{
   int i;
   XIDeviceEvent *de = (XIDeviceEvent*)cookie->data;

   if (de->event != x11->win || (i = x_pointer_index(x11, de->deviceid)) < 0)
      return;

   switch (de->evtype)
   {
      case XI_Motion:
         x_pointer_move(x11, i, de->serial, de->event_x, de->event_y);
         x11->ptr_buttons[i] = x_button_word(&de->buttons);
         break;

      case XI_ButtonPress:
      case XI_ButtonRelease:
         x_pointer_move(x11, i, de->serial, de->event_x, de->event_y);
         if (de->detail > 0 && de->detail < 16)
         {
            if (de->evtype == XI_ButtonPress)
               x11->ptr_buttons[i] |=  (1u << de->detail);
            else
               x11->ptr_buttons[i] &= ~(1u << de->detail);
         }
         /* Wheel notches and buttons 8 and 9 as the pump latches
          * them from core events. */
         if (     de->detail >= 4 && de->detail <= 9
               && (de->evtype == XI_ButtonPress || de->detail >= 8))
         {
            XButtonEvent button;
            memset(&button, 0, sizeof(button));
            button.type   = (de->evtype == XI_ButtonPress)
               ? ButtonPress : ButtonRelease;
            button.button = (unsigned)de->detail;
            x_input_poll_wheel(&button, true);
         }
         break;

      case XI_Enter:
      case XI_Leave:
         {
            XIEnterEvent *ee = (XIEnterEvent*)de;
            x_pointer_move(x11, i, ee->serial, ee->event_x, ee->event_y);
            x11->ptr_buttons[i] = x_button_word(&ee->buttons);
            x11->ptr_inside[i]  = (ee->evtype == XI_Enter);
         }
         break;

      default:
         break;
   }
}
#endif

static void x_events_drain(x11_input_t *x11)
{
   Display *dpy = x11->event_display;

   while (XPending(dpy))
   {
      XEvent event;
      unsigned keycode;

      XNextEvent(dpy, &event);

      switch (event.type)
      {
         case KeyPress:
            keycode = event.xkey.keycode & 0xFF;
            x11->keys[keycode >> 3] |= (char)(1 << (keycode & 7));
            break;

         case KeyRelease:
            keycode = event.xkey.keycode & 0xFF;
            x11->keys[keycode >> 3] &= (char)~(1 << (keycode & 7));
            break;

         case KeymapNotify:
            /* Xlib fills key_vector from index 1; keycodes 0-7 do
             * not exist. */
            memcpy(x11->keys, event.xkeymap.key_vector, sizeof(x11->keys));
            x11->keys[0] = 0;
            break;

         /* Key events stop arriving; the next FocusIn brings a
          * KeymapNotify. Under another client's grab keys stay as
          * they were, as the window keeps focus, until the FocusIn
          * that ends it. */
         case FocusOut:
            if (     event.xfocus.mode   != NotifyGrab
                  && event.xfocus.detail != NotifyInferior)
               memset(x11->keys, 0, sizeof(x11->keys));
            break;

#ifdef HAVE_XI2
         case GenericEvent:
            if (     x11->ptr_events
                  && event.xcookie.extension == x11->xi_opcode
                  && XGetEventData(dpy, &event.xcookie))
            {
               x_pointer_event(x11, &event.xcookie);
               XFreeEventData(dpy, &event.xcookie);
            }
            break;
#endif

         default:
            break;
      }
   }
}

static void *x_input_init(const char *joypad_driver)
{
   x11_input_t *x11;
#ifdef HAVE_XI2
   XIDeviceInfo *dev;
   int i, j = 0;
   int cnt  = 0;
   int xi_opcode, xi_event, xi_error;
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

   if (x11->win != None)
      x11->event_display = x_events_open(x11);

#ifdef HAVE_XI2
   for (i = 0; i < MAX_MOUSE_IDX; i++)
      x11->mouse_dev_list[i] = -1;
   /* x11->di stays NULL on a server without XInput 2, and the
    * driver reads the core pointer as a single mouse instead */
   if (XQueryExtension(x11->display, "XInputExtension",
            &xi_opcode, &xi_event, &xi_error))
      x11->di = XIQueryDevice(x11->display, XIAllDevices, &cnt);
   if (!x11->di)
      RARCH_LOG("[X11] XInput 2 unavailable on this display, using the core pointer as one mouse.\n");
   for (i = 0; i < cnt && j < MAX_MOUSE_IDX; i++)
   {
      dev = &(x11->di[i]);
      RARCH_DBG("[X11] Device detected, %d \"%s\" attached to %d.\n", i, dev->name, dev->attachment);
      if (dev->use == XIMasterPointer)
      {
         RARCH_LOG("[X11] Master pointer, %d \"%s\".\n", dev->deviceid, dev->name);
         input_config_set_mouse_display_name(j, dev->name);
         x11->mouse_dev_list[j++] = dev->deviceid;
      }
   }
   if (j && x11->event_display)
      x11->ptr_events = x_pointer_open(x11, xi_opcode);
#else
   RARCH_DBG("[X11] XInput2 support not compiled in, using only 1 mouse.\n");
#endif
   return x11;
}

/* Core protocol pointer query: one mouse, reported in mouse slot 0.
 * Buttons 4 and 5 are not in the mask; they arrive as button events
 * and are read through x_mouse_state_wheel(). */
static bool x_query_core_pointer(x11_input_t *x11, int *win_x, int *win_y)
{
   Window root_win;
   Window child_win;
   int root_x    = 0;
   int root_y    = 0;
   unsigned mask = 0;

   if (!XQueryPointer(x11->display, x11->win,
            &root_win, &child_win,
            &root_x, &root_y,
            win_x, win_y,
            &mask))
      return false;

   x11->mouse_l[0] = (mask & Button1Mask) != 0;
   x11->mouse_m[0] = (mask & Button2Mask) != 0;
   x11->mouse_r[0] = (mask & Button3Mask) != 0;
   return true;
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
#ifdef HAVE_XI2
   if (!x11->di)
      mouse_port = 0;
#else
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
         if (x11->di)
            return x11->mouse_4[mouse_port];
#endif
         /* fall through */
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
#ifdef HAVE_XI2
         if (x11->di)
            return x11->mouse_5[mouse_port];
#endif
         /* fall through */
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
      unsigned mouse_port  = port;
      x11_input_t *x11     = (x11_input_t*)data;
      settings_t *settings = config_get_ptr();

#ifdef HAVE_XI2
      if (!x11->di)
         mouse_port = 0;
#else
      mouse_port = 0;
#endif

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
                     if (RETRO_KEYBIND_VALID(&binds[port][i]))
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
                     if (RETRO_KEYBIND_VALID(&binds[port][i]))
                     {
                        if (     (RETRO_KEYBIND_KEY(&binds[port][i]) && RETRO_KEYBIND_KEY(&binds[port][i]) < RETROK_LAST)
                              && x_keyboard_pressed(x11, RETRO_KEYBIND_KEY(&binds[port][i])))
                           ret |= (1 << i);
                     }
                  }
               }

               return ret;
            }

            if (id < RARCH_BIND_LIST_END)
            {
               if (RETRO_KEYBIND_VALID(&binds[port][id]))
               {
                  if (     (RETRO_KEYBIND_KEY(&binds[port][id]) && RETRO_KEYBIND_KEY(&binds[port][id]) < RETROK_LAST)
                        && x_keyboard_pressed(x11, RETRO_KEYBIND_KEY(&binds[port][id]))
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
               int16_t ret           = 0;
               bool id_plus_valid    = false;
               bool id_minus_valid   = false;

               input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

               id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
               id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
               id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
               id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

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

                     if (RETRO_KEYBIND_VALID(&binds[port][new_id]))
                     {
                        if ((uint16_t)joykey != NO_BTN && joypad->button(
                                 joyport, (uint16_t)joykey))
                           return 1;
                        if (joyaxis != AXIS_NONE &&
                              ((float)abs(joypad->axis(joyport, joyaxis))
                               / 0x8000) > axis_threshold)
                           return 1;
                        else if ((RETRO_KEYBIND_KEY(&binds[port][new_id]) && RETRO_KEYBIND_KEY(&binds[port][new_id]) < RETROK_LAST)
                              && !keyboard_mapping_blocked
                              && x_keyboard_pressed(x11, RETRO_KEYBIND_KEY(&binds[port][new_id]))
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
      /* NULL when the server lacks XInput 2 */
      if (x11->di)
         XIFreeDeviceInfo(x11->di);
#endif
#ifdef __linux__
      linux_close_illuminance_sensor(x11->illuminance_sensor);
#endif
      if (x11->event_display)
         XCloseDisplay(x11->event_display);
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
   x11_input_t *x11         = (x11_input_t*)data;
   bool video_has_focus     = video_driver_has_focus();
#ifdef HAVE_XI2
   Window root_win;
   Window child_win;
   double root_x            = 0;
   double root_y            = 0;
   double win_x             = 0;
   double win_y             = 0;
   int core_x               = 0;
   int core_y               = 0;
   XIButtonState buttons_return;
   XIModifierState modifiers_return;
   XIGroupState group_return;
   settings_t *settings     = config_get_ptr();
   unsigned mouse_dev_idx   = 0;
   unsigned mouse_ports     = x11->di ? MAX_MOUSE_IDX : 1;
#else
   int win_x                = 0;
   int win_y                = 0;
   unsigned mouse_ports     = 1;
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
   if (x11->event_display)
   {
      x_events_drain(x11);
      memcpy(x11->state, x11->keys, sizeof(x11->state));
   }
   else
      XQueryKeymap(x11->display, x11->state);

   /* If pointer is not inside the application
    * window, ignore mouse input. From events, each
    * pointer is tested on its own below. */
   if (
#ifdef HAVE_XI2
            !x11->ptr_events &&
#endif
            !retro_atomic_load_relaxed_int(&g_x11_entered))
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

   for (mouse_port = 0; mouse_port < mouse_ports; mouse_port++)
   {
#ifdef HAVE_XI2
      if (!x11->di)
      {
         if (!x_query_core_pointer(x11, &core_x, &core_y))
            return;
         win_x = core_x;
         win_y = core_y;
      }
      else
      {
         mouse_dev_idx = settings->uints.input_mouse_index[mouse_port];
         if (mouse_dev_idx >= MAX_INPUT_DEVICES || x11->mouse_dev_list[mouse_dev_idx] < 0)
            return;

         if (x11->ptr_events)
         {
            unsigned buttons = x11->ptr_buttons[mouse_dev_idx];

            if (!x11->ptr_inside[mouse_dev_idx])
            {
               x11->mouse_delta_x[mouse_port] = 0;
               x11->mouse_delta_y[mouse_port] = 0;
               x11->mouse_l[mouse_port]       = false;
               x11->mouse_m[mouse_port]       = false;
               x11->mouse_r[mouse_port]       = false;
               x11->mouse_4[mouse_port]       = false;
               x11->mouse_5[mouse_port]       = false;
               continue;
            }

            win_x = x11->ptr_x[mouse_dev_idx];
            win_y = x11->ptr_y[mouse_dev_idx];
            /* > Mouse buttons - fixed map (1,2,3,8,9) */
            x11->mouse_l[mouse_port] = (buttons & (1u << 1)) != 0;
            x11->mouse_m[mouse_port] = (buttons & (1u << 2)) != 0;
            x11->mouse_r[mouse_port] = (buttons & (1u << 3)) != 0;
            x11->mouse_4[mouse_port] = (buttons & (1u << 8)) != 0;
            x11->mouse_5[mouse_port] = (buttons & (1u << 9)) != 0;
         }
         else
         {
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

            /* > Mouse buttons - fixed map (1,2,3,8,9) */
            x11->mouse_l[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<1 : 0;
            x11->mouse_m[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<2 : 0;
            x11->mouse_r[mouse_port] = buttons_return.mask_len > 0 ? buttons_return.mask[0] & 1<<3 : 0;
            x11->mouse_4[mouse_port] = buttons_return.mask_len > 1 ? buttons_return.mask[1] & 1<<0 : 0;
            x11->mouse_5[mouse_port] = buttons_return.mask_len > 1 ? buttons_return.mask[1] & 1<<1 : 0;
            /* XIQueryPointer() allocates the button mask for the caller */
            XFree(buttons_return.mask);
         }
      }
#else
      if (!x_query_core_pointer(x11, &win_x, &win_y))
         return;
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
         int centre_x, centre_y;
         int win_w, win_h;
         int warp_x            = win_x;
         int warp_y            = win_y;
         bool do_warp          = false;
         /* The size the event pump recorded for the frontend's
          * window; the server is asked only for a window it has not
          * seen. */
         unsigned size         = (x11->win == g_x11_win)
            ? (unsigned)retro_atomic_load_relaxed_int(&g_x11_size) : 0;

         if (VIDEO_SCALE_W(size) && VIDEO_SCALE_H(size))
         {
            win_w              = (int)VIDEO_SCALE_W(size);
            win_h              = (int)VIDEO_SCALE_H(size);
         }
         else
         {
            XWindowAttributes win_attr;
            if (!XGetWindowAttributes(x11->display, x11->win, &win_attr))
            {
               x11->mouse_delta_x[mouse_port] = 0;
               x11->mouse_delta_y[mouse_port] = 0;
               return;
            }
            win_w              = win_attr.width;
            win_h              = win_attr.height;
         }

         centre_x              = win_w >> 1;
         centre_y              = win_h >> 1;

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
         if (x11->mouse_x[mouse_port] >= win_w)
            x11->mouse_x[mouse_port] = (win_w - 1);

         /* Clamp Y */
         if (x11->mouse_y[mouse_port] < 0)
            x11->mouse_y[mouse_port] = 0;
         if (x11->mouse_y[mouse_port] >= win_h)
            x11->mouse_y[mouse_port] = (win_h - 1);

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
#ifdef HAVE_XI2
            /* On the connection the pointer events come on: the
             * position is the warp's until one newer than it. */
            if (x11->ptr_events)
            {
               x11->ptr_warp_serial[mouse_dev_idx]  = NextRequest(x11->event_display);
               x11->ptr_warp_pending[mouse_dev_idx] = true;
               x11->ptr_x[mouse_dev_idx]            = warp_x;
               x11->ptr_y[mouse_dev_idx]            = warp_y;
               XWarpPointer(x11->event_display, None,
                     x11->win, 0, 0, 0, 0,
                     warp_x, warp_y);
               XFlush(x11->event_display);
            }
            else
#endif
            {
               /* Sent now, not waited for: the next poll's pointer
                * query follows it on the connection, so it reads the
                * warped position. */
               XWarpPointer(x11->display, None,
                     x11->win, 0, 0, 0, 0,
                     warp_x, warp_y);
               XFlush(x11->display);
            }
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
