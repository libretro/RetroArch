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

/* TODO/FIXME - set this once the kqueue codepath is implemented and working properly,
 * also remove libepoll-shim from the Makefile when that happens. */
#if 1
#define HAVE_EPOLL
#else
#ifdef __linux__
#define HAVE_EPOLL 1
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined (__NetBSD__)
#define HAVE_KQUEUE 1
#endif
#endif

#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined(HAVE_EPOLL)
#include <sys/epoll.h>
#elif defined(HAVE_KQUEUE)
#include <sys/event.h>
#endif
#include <poll.h>

#include <libudev.h>
#if defined(__linux__)
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kd.h>
#elif defined(__FreeBSD__)
#include <dev/evdev/input.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#include <X11/Xlib.h>
#endif

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../input_keymaps.h"

#include "../common/linux_common.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#if defined(HAVE_XKBCOMMON) && defined(HAVE_KMS)
#define UDEV_XKB_HANDLING
#endif

/* Force UDEV_XKB_HANDLING for Lakka */
#ifdef HAVE_LAKKA
#ifndef UDEV_XKB_HANDLING
#define UDEV_XKB_HANDLING
#endif
#endif

#define UDEV_MAX_KEYS (KEY_MAX + 7) / 8

typedef struct udev_input udev_input_t;

typedef struct udev_input_device udev_input_device_t;

enum udev_input_dev_type
{
   UDEV_INPUT_KEYBOARD = 0,
   UDEV_INPUT_MOUSE,
   UDEV_INPUT_TOUCHPAD
};

/* NOTE: must be in sync with enum udev_input_dev_type */
static const char *g_dev_type_str[] =
{
   "ID_INPUT_KEY",
   "ID_INPUT_MOUSE",
   "ID_INPUT_TOUCHPAD"
};

typedef struct
{
   /* If device is "absolute" coords will be in device specific units
      and axis min value will be less than max, otherwise coords will be
      relative to full viewport and min and max values will be zero. */
   int32_t x_abs, y_abs;
   int32_t x_min, y_min;
   int32_t x_max, y_max;
   int32_t x_rel, y_rel;
   bool l, r, m, b4, b5;
   bool wu, wd, whu, whd;
   bool pp;
   int32_t abs;
} udev_input_mouse_t;

struct udev_input_device
{
   void (*handle_cb)(void *data,
         const struct input_event *event, udev_input_device_t *dev);
   int fd;
   dev_t dev;
   udev_input_mouse_t mouse;
   enum udev_input_dev_type type;
   char devnode[NAME_MAX_LENGTH];
   char ident[255]; /* could be mouse or keyboards store here */
};

typedef void (*device_handle_cb)(void *data,
      const struct input_event *event, udev_input_device_t *dev);

struct udev_input
{
   struct udev *udev;
   struct udev_monitor *monitor;
   udev_input_device_t **devices;

   int fd;
   /* OS pointer coords (zeros if we don't have X11) */
   int pointer_x;
   int pointer_y;

   unsigned num_devices;

   uint8_t state[UDEV_MAX_KEYS];

#ifdef UDEV_XKB_HANDLING
   bool xkb_handling;
#endif
};

#ifdef UDEV_XKB_HANDLING
int init_xkb(int fd, size_t size);
void free_xkb(void);
int handle_xkb(int code, int value);
#endif

static unsigned input_unify_ev_key_code(unsigned code)
{
   /* input_keymaps_translate_keysym_to_rk does not support the case
    * where multiple keysyms translate to the same RETROK_* code,
    * so unify remote control keysyms to keyboard keysyms here.
    *
    * Addendum: The rarch_keysym_lut lookup table also becomes
    * unusable if more than one keysym translates to the same
    * RETROK_* code, so certain keys must be left unmapped in
    * rarch_key_map_linux and instead be handled here */
   switch (code)
   {
      case KEY_OK:
      case KEY_SELECT:
         return KEY_ENTER;
      case KEY_BACK:
         return KEY_BACKSPACE;
      case KEY_EXIT:
         return KEY_CLEAR;
      default:
         break;
   }

   return code;
}

static void udev_handle_keyboard(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   unsigned keysym;
   udev_input_t *udev = (udev_input_t*)data;

   switch (event->type)
   {
      case EV_KEY:
         keysym = input_unify_ev_key_code(event->code);
         if (event->value && video_driver_has_focus())
            BIT_SET(udev->state, keysym);
         else
            BIT_CLEAR(udev->state, keysym);

         /* TODO/FIXME: The udev driver is incomplete.
          * When calling input_keyboard_event() the
          * following parameters are omitted:
          * - character: the localised Unicode/UTF-8
          *   value of the pressed key
          * - mod: the current keyboard modifier
          *   bitmask
          * Without these values, input_keyboard_event()
          * does not function correctly (e.g. it is
          * impossible to use text entry in the menu).
          * I cannot find any usable reference for
          * converting a udev-returned key code into a
          * localised Unicode/UTF-8 value, so for the
          * time being we must rely on other sources:
          * - If we are using an X11-based context driver,
          *   input_keyboard_event() is handled correctly
          *   in x11_common:x11_check_window()
          * - If we are using KMS, input_keyboard_event()
          *   is handled correctly in
          *   keyboard_event_xkb:handle_xkb()
          * If neither are available, then just call
          * input_keyboard_event() without character and
          * mod, and hope for the best... */

         if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
         {
#ifdef UDEV_XKB_HANDLING
            if (udev->xkb_handling && handle_xkb(keysym, event->value) == 0)
               return;
#endif
            input_keyboard_event(event->value,
                  input_keymaps_translate_keysym_to_rk(keysym),
                  0, 0, RETRO_DEVICE_KEYBOARD);
         }

         break;

      default:
         break;
   }
}

static void udev_input_kb_free(struct udev_input *udev)
{
   unsigned i;

   for (i = 0; i < UDEV_MAX_KEYS; i++)
      udev->state[i] = 0;

#ifdef UDEV_XKB_HANDLING
   free_xkb();
#endif
}

static udev_input_mouse_t *udev_get_mouse(
      struct udev_input *udev, unsigned port)
{
   unsigned i;
   unsigned mouse_index      = 0;
   settings_t *settings      = config_get_ptr();
   udev_input_mouse_t *mouse = NULL;

   if (port >= MAX_USERS || !video_driver_has_focus())
      return NULL;

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type == UDEV_INPUT_KEYBOARD)
         continue;

      if (mouse_index == settings->uints.input_mouse_index[port])
      {
         mouse = &udev->devices[i]->mouse;
         break;
      }

      ++mouse_index;
   }

   return mouse;
}

static void udev_mouse_set_x(udev_input_mouse_t *mouse, int32_t x, bool abs)
{
    video_viewport_t vp;

   if (abs)
   {
      mouse->x_rel += x - mouse->x_abs;
      mouse->x_abs = x;
   }
   else
   {
      mouse->x_rel += x;
      if (video_driver_get_viewport_info(&vp))
      {
         mouse->x_abs += x;

         if (mouse->x_abs < vp.x)
            mouse->x_abs = vp.x;
         else if (mouse->x_abs >= vp.x + vp.full_width)
            mouse->x_abs = vp.x + vp.full_width - 1;
      }
   }
}

static int16_t udev_mouse_get_x(const udev_input_mouse_t *mouse)
{
   video_viewport_t vp;
   double src_width;
   double x;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
      src_width = mouse->x_max - mouse->x_min + 1;
   else
      src_width = vp.full_width;

   x = (double)vp.width / src_width * mouse->x_rel;

   return x + (x < 0 ? -0.5 : 0.5);
}

static void udev_mouse_set_y(udev_input_mouse_t *mouse, int32_t y, bool abs)
{
   video_viewport_t vp;

   if (abs)
   {
      mouse->y_rel += y - mouse->y_abs;
      mouse->y_abs = y;
   }
   else
   {
      mouse->y_rel += y;
      if (video_driver_get_viewport_info(&vp))
      {
         mouse->y_abs += y;

         if (mouse->y_abs < vp.y)
            mouse->y_abs = vp.y;
         else if (mouse->y_abs >= vp.y + vp.full_height)
            mouse->y_abs = vp.y + vp.full_height - 1;
      }
   }
}

static int16_t udev_mouse_get_y(const udev_input_mouse_t *mouse)
{
   video_viewport_t vp;
   double src_height;
   double y;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
      src_height = mouse->y_max - mouse->y_min + 1;
   else
      src_height = vp.full_height;

   y = (double)vp.height / src_height * mouse->y_rel;

   return y + (y < 0 ? -0.5 : 0.5);
}

static int16_t udev_mouse_get_pointer_x(const udev_input_mouse_t *mouse, bool screen)
{
   video_viewport_t vp;
   double src_min;
   double src_width;
   int32_t x;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
   {
      /* mouse coordinates are relative to the screen; convert them
       * to be relative to the viewport */
      double scaled_x;
      src_min = mouse->x_min;
      src_width = mouse->x_max - mouse->x_min + 1;
      scaled_x = vp.full_width * (mouse->x_abs - src_min) / src_width;
      x = -32767.0 + 65535.0 / vp.width * (scaled_x - vp.x);
   }
   else /* mouse coords are viewport relative */
   {
      if (screen) 
      {
         src_min = 0.0;
         src_width = vp.full_width;
      }
      else 
      {
         src_min = vp.x;
         src_width = vp.width;
      }
      x  = -32767.0 + 65535.0 / src_width * (mouse->x_abs - src_min);
   }

   x += (x < 0 ? -0.5 : 0.5);

   if (x < -0x7fff)
      return -0x7fff;
   else if(x > 0x7fff)
      return 0x7fff;

   return x;
}

static int16_t udev_mouse_get_pointer_y(const udev_input_mouse_t *mouse, bool screen)
{
   video_viewport_t vp;
   double src_min;
   double src_height;
   int32_t y;

   if (!video_driver_get_viewport_info(&vp))
      return 0;

   if (mouse->abs) /* mouse coords are absolute */
   {
      double scaled_y;
      /* mouse coordinates are relative to the screen; convert them
       * to be relative to the viewport */
      src_min = mouse->y_min;
      src_height = mouse->y_max - mouse->y_min + 1;
      scaled_y = vp.full_height * (mouse->y_abs - src_min) / src_height;
      y = -32767.0 + 65535.0 / vp.height * (scaled_y - vp.y);
   }
   else /* mouse coords are viewport relative */
   {
      if (screen)
      {
         src_min = 0.0;
         src_height = vp.full_height;
      }
      else
      {
         src_min = vp.y;
         src_height = vp.height;
      }
      y = -32767.0 + 65535.0 / src_height * (mouse->y_abs - src_min);
   }

   y += (y < 0 ? -0.5 : 0.5);

   if (y < -0x7fff)
      return -0x7fff;
   else if(y > 0x7fff)
      return 0x7fff;

   return y;
}

static void udev_handle_mouse(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   udev_input_mouse_t *mouse = &dev->mouse;

   switch (event->type)
   {
      case EV_KEY:
         switch (event->code)
         {
            case BTN_LEFT:
               mouse->l = event->value;
               break;

            case BTN_RIGHT:
               mouse->r = event->value;
               break;

            case BTN_MIDDLE:
               mouse->m = event->value;
               break;
            case BTN_TOUCH:
               mouse->pp = event->value;
               break;
            /*case BTN_??:
               mouse->b4 = event->value;
               break;*/

            /*case BTN_??:
               mouse->b5 = event->value;
               break;*/

            default:
               break;
         }
         break;

      case EV_REL:
         switch (event->code)
         {
            case REL_X:
               udev_mouse_set_x(mouse, event->value, false);
               break;
            case REL_Y:
               udev_mouse_set_y(mouse, event->value, false);
               break;
            case REL_WHEEL:
               if (event->value == 1)
                  mouse->wu = 1;
               else if (event->value == -1)
                  mouse->wd = 1;
               break;
            case REL_HWHEEL:
               if (event->value == 1)
                  mouse->whu = 1;
               else if (event->value == -1)
                  mouse->whd = 1;
               break;
         }
         break;

      case EV_ABS:
         switch (event->code)
         {
            case ABS_X:
               udev_mouse_set_x(mouse, event->value, true);
               break;
            case ABS_Y:
               udev_mouse_set_y(mouse, event->value, true);
               break;
         }
         break;
   }
}
#define test_bit(array, bit)    (array[bit/8] & (1<<(bit%8)))

static int udev_input_add_device(udev_input_t *udev,
      enum udev_input_dev_type type, const char *devnode, device_handle_cb cb)
{
   unsigned char keycaps[(KEY_MAX / 8) + 1] = {'\0'};
   unsigned char abscaps[(ABS_MAX / 8) + 1] = {'\0'};
   unsigned char relcaps[(REL_MAX / 8) + 1] = {'\0'};
   udev_input_device_t **tmp                = NULL;
   udev_input_device_t *device              = NULL;
   struct input_absinfo absinfo;
   int fd                                   = -1;
   int ret                                  = 0;
   struct stat st;
#if defined(HAVE_EPOLL)
   struct epoll_event event;
#elif defined(HAVE_KQUEUE)
   struct kevent event;
#endif

   st.st_dev = 0;

   if (stat(devnode, &st) < 0)
      goto end;

   fd = open(devnode, O_RDONLY | O_NONBLOCK);
   if (fd < 0)
      goto end;

   device = (udev_input_device_t*)calloc(1, sizeof(*device));
   if (!device)
      goto end;

   device->fd        = fd;
   device->dev       = st.st_dev;
   device->handle_cb = cb;
   device->type      = type;

   strlcpy(device->devnode, devnode, sizeof(device->devnode));

   if (ioctl(fd, EVIOCGNAME(sizeof(device->ident)), device->ident) < 0)
      device->ident[0] = '\0';

   /* UDEV_INPUT_MOUSE may report in absolute coords too */
   if (type == UDEV_INPUT_MOUSE || type == UDEV_INPUT_TOUCHPAD )
   {
      bool mouse = 0;
      /* gotta have some buttons!  return -1 to skip error logging for this:)  */
      if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof (keycaps)), keycaps) == -1)
      {
         ret = -1;
         goto end;
      }

      if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof (relcaps)), relcaps) != -1)
      {
         if ( (test_bit(relcaps, REL_X)) && (test_bit(relcaps, REL_Y)) )
         {
            mouse = 1;

            if (!test_bit(keycaps, BTN_MOUSE))
               RARCH_LOG("[udev]: Waring REL pointer device (%s) has no mouse button\n",device->ident);
         }
      }

      if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof (abscaps)), abscaps) != -1)
      {
         if ( (test_bit(abscaps, ABS_X)) && (test_bit(abscaps, ABS_Y)) )
         {
            mouse =1;

            /* check for abs touch devices... */
            if (test_bit(keycaps, BTN_TOUCH))
               device->mouse.abs = 1;

            /* check for light gun or any other device that might not have a touch button */
            else
               device->mouse.abs = 2;

            if ( !test_bit(keycaps, BTN_TOUCH) && !test_bit(keycaps, BTN_MOUSE) )
               RARCH_LOG("[udev]: Warning ABS pointer device (%s) has no touch or mouse button\n",device->ident);
         }
      }

      device->mouse.x_min = 0;
      device->mouse.y_min = 0;
      device->mouse.x_max = 0;
      device->mouse.y_max = 0;

      if (device->mouse.abs)
      {

         if (ioctl(fd, EVIOCGABS(ABS_X), &absinfo) == -1)
         {
            RARCH_LOG("[udev]: ABS pointer device (%s) Failed to get ABS_X parameters \n",device->ident);
            goto end;
         }

         device->mouse.x_min = absinfo.minimum;
         device->mouse.x_max = absinfo.maximum;

         if (ioctl(fd, EVIOCGABS(ABS_Y), &absinfo) == -1)
         {
            RARCH_LOG("[udev]: ABS pointer device (%s) Failed to get ABS_Y parameters \n",device->ident);
            goto end;
         }
         device->mouse.y_min = absinfo.minimum;
         device->mouse.y_max = absinfo.maximum;
      }

      if (!mouse)
         goto end;

   }

   tmp = (udev_input_device_t**)realloc(udev->devices,
         (udev->num_devices + 1) * sizeof(*udev->devices));

   if (!tmp)
      goto end;

   tmp[udev->num_devices++] = device;
   udev->devices            = tmp;

#if defined(HAVE_EPOLL)
   event.events             = EPOLLIN;
   event.data.ptr           = device;

   /* Shouldn't happen, but just check it. */
   if (epoll_ctl(udev->fd, EPOLL_CTL_ADD, fd, &event) < 0)
   {
      RARCH_ERR("udev]: Failed to add FD (%d) to epoll list (%s).\n",
            fd, strerror(errno));
   }
#elif defined(HAVE_KQUEUE)
   EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, LISTENSOCKET);
   if (kevent(udev->fd, &event, 1, NULL, 0, NULL) == -1)
   {
      RARCH_ERR("udev]: Failed to add FD (%d) to kqueue list (%s).\n",
            fd, strerror(errno));
   }
#endif
   ret = 1;

end:
   /* Free resources in the event of
    * an error */
   if (ret != 1)
   {
      if (fd >= 0)
         close(fd);
      if (device)
         free(device);
   }

   return ret;
}

static void udev_input_remove_device(udev_input_t *udev, const char *devnode)
{
   unsigned i;

   for (i = 0; i < udev->num_devices; i++)
   {
      if (!string_is_equal(devnode, udev->devices[i]->devnode))
         continue;

      close(udev->devices[i]->fd);
      free(udev->devices[i]);
      memmove(udev->devices + i, udev->devices + i + 1,
            (udev->num_devices - (i + 1)) * sizeof(*udev->devices));
      udev->num_devices--;
   }
}

static void udev_input_handle_hotplug(udev_input_t *udev)
{
   device_handle_cb cb;
   enum udev_input_dev_type dev_type = UDEV_INPUT_KEYBOARD;
   const char *val_key               = NULL;
   const char *val_mouse             = NULL;
   const char *val_touchpad          = NULL;
   const char *action                = NULL;
   const char *devnode               = NULL;
   int mouse = 0;
   int check = 0;
   struct udev_device *dev           = udev_monitor_receive_device(
         udev->monitor);

   if (!dev)
      return;

   val_key       = udev_device_get_property_value(dev, "ID_INPUT_KEY");
   val_mouse     = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
   val_touchpad  = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
   action        = udev_device_get_action(dev);
   devnode       = udev_device_get_devnode(dev);

   if (val_key && string_is_equal(val_key, "1") && devnode)
   {
      /* EV_KEY device, can be a keyboard or a remote control device.  */
      dev_type   = UDEV_INPUT_KEYBOARD;
      cb         = udev_handle_keyboard;
   }
   else if (val_mouse && string_is_equal(val_mouse, "1") && devnode)
   {
      dev_type   = UDEV_INPUT_MOUSE;
      cb         = udev_handle_mouse;
   }
   else if (val_touchpad && string_is_equal(val_touchpad, "1") && devnode)
   {
      dev_type   = UDEV_INPUT_TOUCHPAD;
      cb         = udev_handle_mouse;
   }
   else
      goto end;

   /* Hotplug add */
   if (string_is_equal(action, "add"))
      udev_input_add_device(udev, dev_type, devnode, cb);
   /* Hotplug remove */
   else if (string_is_equal(action, "remove"))
      udev_input_remove_device(udev, devnode);

   /* we need to re index the mouse friendly names when a mouse is hotplugged */
   if ( dev_type  != UDEV_INPUT_KEYBOARD)
   {
      /*first clear all */
      int i;
      for (i = 0; i < MAX_USERS; i++)
        input_config_set_mouse_display_name(i, "N/A");

     /* Add what devices we have now */
      for (i = 0; i < udev->num_devices; ++i)
      {
         if (udev->devices[i]->type != UDEV_INPUT_KEYBOARD)
         {
            input_config_set_mouse_display_name(mouse, udev->devices[i]->ident);
            mouse++;
         }
       }
   }

end:
   udev_device_unref(dev);
}

#ifdef HAVE_X11
static void udev_input_get_pointer_position(int *x, int *y)
{
   if (video_driver_display_type_get() == RARCH_DISPLAY_X11)
   {
      Window w;
      int p;
      unsigned m;
      Display *display = (Display*)video_driver_display_get();
      Window window    = (Window)video_driver_window_get();

      XQueryPointer(display, window, &w, &w, &p, &p, x, y, &m);
   }
}

static void udev_input_adopt_rel_pointer_position_from_mouse(
      int *x, int *y, udev_input_mouse_t *mouse)
{
   static int noX11DispX = 0;
   static int noX11DispY = 0;

   struct video_viewport view;
   bool r = video_driver_get_viewport_info(&view);
   int dx = udev_mouse_get_x(mouse);
   int dy = udev_mouse_get_y(mouse);
   if (r && (dx || dy) &&
         video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      int minX = view.x;
      int maxX = view.x + view.width;
      int minY = view.y;
      int maxY = view.y + view.height;

      /* Not running in a window. */
      noX11DispX = noX11DispX + dx;
      if (noX11DispX < minX)
         noX11DispX = minX;
      if (noX11DispX > maxX)
         noX11DispX = maxX;
      noX11DispY = noX11DispY + dy;
      if (noX11DispY < minY)
         noX11DispY = minY;
      if (noX11DispY > maxY)
         noX11DispY = maxY;
      *x = noX11DispX;
      *y = noX11DispY;
   }
   mouse->x_rel = 0;
   mouse->y_rel = 0;
}
#endif

static bool udev_input_poll_hotplug_available(struct udev_monitor *dev)
{
   struct pollfd fds;

   fds.fd      = udev_monitor_get_fd(dev);
   fds.events  = POLLIN;
   fds.revents = 0;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static void udev_input_poll(void *data)
{
   int i, ret;
#if defined(HAVE_EPOLL)
   struct epoll_event events[32];
#elif defined(HAVE_KQUEUE)
   struct kevent events[32];
#endif
   udev_input_mouse_t *mouse = NULL;
   udev_input_t *udev        = (udev_input_t*)data;

#ifdef HAVE_X11
   udev_input_get_pointer_position(&udev->pointer_x, &udev->pointer_y);
#endif

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type == UDEV_INPUT_KEYBOARD)
         continue;

      mouse = &udev->devices[i]->mouse;
#ifdef HAVE_X11
      udev_input_adopt_rel_pointer_position_from_mouse(
            &udev->pointer_x, &udev->pointer_y, mouse);
#else
      mouse->x_rel = 0;
      mouse->y_rel = 0;
#endif
      mouse->wu    = false;
      mouse->wd    = false;
      mouse->whu   = false;
      mouse->whd   = false;
   }

   while (udev->monitor && udev_input_poll_hotplug_available(udev->monitor))
      udev_input_handle_hotplug(udev);

#if defined(HAVE_EPOLL)
   ret = epoll_wait(udev->fd, events, ARRAY_SIZE(events), 0);
#elif defined(HAVE_KQUEUE)
   {
      struct timespec timeoutspec;
      timeoutspec.tv_sec  = timeout;
      timeoutspec.tv_nsec = 0;
      ret                 = kevent(udev->fd, NULL, 0, events,
            ARRAY_SIZE(events), &timeoutspec);
   }
#endif

   for (i = 0; i < ret; i++)
   {
      /* TODO/FIXME - add HAVE_EPOLL/HAVE_KQUEUE codepaths here */
      if (events[i].events & EPOLLIN)
      {
         int j, len;
         struct input_event input_events[32];
#if defined(HAVE_EPOLL)
         udev_input_device_t *device = (udev_input_device_t*)events[i].data.ptr;
#elif defined(HAVE_KQUEUE)
         udev_input_device_t *device = (udev_input_device_t*)events[i].udata;
#endif

         while ((len = read(device->fd,
                     input_events, sizeof(input_events))) > 0)
         {
            len /= sizeof(*input_events);
            for (j = 0; j < len; j++)
               device->handle_cb(udev, &input_events[j], device);
         }
      }
   }
}

static bool udev_pointer_is_off_window(const udev_input_t *udev)
{
#ifdef HAVE_X11
   struct video_viewport view;
   bool r = video_driver_get_viewport_info(&view);

   if (r)
      r = udev->pointer_x < 0 ||
          udev->pointer_x >= view.full_width ||
          udev->pointer_y < 0 ||
          udev->pointer_y >= view.full_height;
   return r;
#else
   return false;
#endif
}

static int16_t udev_lightgun_aiming_state(
      udev_input_t *udev, unsigned port, unsigned id )
{

   const int edge_detect       = 32700;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;

   udev_input_mouse_t *mouse   = udev_get_mouse(udev, port);

   if (!mouse)
      return 0;

   res_x = udev_mouse_get_pointer_x(mouse, false);
   res_y = udev_mouse_get_pointer_y(mouse, false);

   inside =    (res_x >= -edge_detect)
            && (res_y >= -edge_detect)
            && (res_x <= edge_detect)
            && (res_y <= edge_detect);

   switch ( id )
   {
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            return res_x;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            return res_y;
         break;
      case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
         return !inside;
      default:
         break;
   }

   return 0;
}

static int16_t udev_mouse_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return 0;

   if (id != RETRO_DEVICE_ID_MOUSE_X && id != RETRO_DEVICE_ID_MOUSE_Y &&
         udev_pointer_is_off_window(udev))
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return screen ? udev->pointer_x : udev_mouse_get_x(mouse);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return screen ? udev->pointer_y : udev_mouse_get_y(mouse);
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->m;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return mouse->b4;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return mouse->b5;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->wd;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return mouse->whu;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return mouse->whd;
   }

   return 0;
}

static bool udev_keyboard_pressed(udev_input_t *udev, unsigned key)
{
   int bit = rarch_keysym_lut[key];
   return BIT_GET(udev->state, bit);
}

static bool udev_mouse_button_pressed(
      udev_input_t *udev, unsigned port, unsigned key)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return false;
/* todo add multi touch button check pointer devices */
   switch ( key )
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->m;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
         return mouse->b4;
      case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
         return mouse->b5;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return mouse->wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return mouse->wd;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
         return mouse->whu;
      case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
         return mouse->whd;
   }

   return false;
}

static int16_t udev_pointer_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return udev_mouse_get_pointer_x(mouse, screen);
      case RETRO_DEVICE_ID_POINTER_Y:
         return udev_mouse_get_pointer_y(mouse, screen);
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         if(mouse->abs == 1)
           return mouse->pp;
         else
           return mouse->l;
   }
   return 0;
}

static unsigned udev_retro_id_to_rarch(unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:
         return RARCH_LIGHTGUN_DPAD_RIGHT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:
         return RARCH_LIGHTGUN_DPAD_LEFT;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:
         return RARCH_LIGHTGUN_DPAD_UP;
      case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:
         return RARCH_LIGHTGUN_DPAD_DOWN;
      case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
         return RARCH_LIGHTGUN_SELECT;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return RARCH_LIGHTGUN_START;
      case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
         return RARCH_LIGHTGUN_RELOAD;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return RARCH_LIGHTGUN_TRIGGER;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:
         return RARCH_LIGHTGUN_AUX_A;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:
         return RARCH_LIGHTGUN_AUX_B;
      case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:
         return RARCH_LIGHTGUN_AUX_C;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return RARCH_LIGHTGUN_START;
      default:
         break;
   }

   return 0;
}

static int16_t udev_input_state(
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
   udev_input_t *udev         = (udev_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  if (udev_mouse_button_pressed(udev, port, binds[port][i].mbutton))
                     ret |= (1 << i);
               }
            }
            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if ((binds[port][i].key < RETROK_LAST) &&
                           udev_keyboard_pressed(udev, binds[port][i].key))
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
               if (
                     (binds[port][id].key < RETROK_LAST) &&
                     udev_keyboard_pressed(udev, binds[port][id].key)
                     && ((    id != RARCH_GAME_FOCUS_TOGGLE)
                        && !keyboard_mapping_blocked)
                     )
                  return 1;
               else if (
                     (binds[port][id].key < RETROK_LAST) &&
                     udev_keyboard_pressed(udev, binds[port][id].key)
                     && (    id == RARCH_GAME_FOCUS_TOGGLE)
                     )
                  return 1;
               else if (udev_mouse_button_pressed(udev, port,
                        binds[port][id].mbutton))
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
               if BIT_GET(udev->state, sym)
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
               if (BIT_GET(udev->state, sym))
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && udev_keyboard_pressed(udev, id);

      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         return udev_mouse_state(udev, port, id,
               device == RARCH_DEVICE_MOUSE_SCREEN);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         if (idx == 0) /* multi-touch unsupported (for now) */
            return udev_pointer_state(udev, port, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
         break;

      case RETRO_DEVICE_LIGHTGUN:
         switch ( id )
         {
            /*aiming*/
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               return udev_lightgun_aiming_state( udev, port, id );

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
                  unsigned new_id                = udev_retro_id_to_rarch(id);
                  const uint64_t bind_joykey     = input_config_binds[port][new_id].joykey;
                  const uint64_t bind_joyaxis    = input_config_binds[port][new_id].joyaxis;
                  const uint64_t autobind_joykey = input_autoconf_binds[port][new_id].joykey;
                  const uint64_t autobind_joyaxis= input_autoconf_binds[port][new_id].joyaxis;
                  uint16_t port                  = joypad_info->joy_idx;
                  float axis_threshold           = joypad_info->axis_threshold;
                  const uint64_t joykey          = (bind_joykey != NO_BTN)
                     ? bind_joykey  : autobind_joykey;
                  const uint32_t joyaxis         = (bind_joyaxis != AXIS_NONE)
                     ? bind_joyaxis : autobind_joyaxis;
                  if (!keyboard_mapping_blocked)
                     if ((binds[port][new_id].key < RETROK_LAST)
                           && udev_keyboard_pressed(udev, binds[port]
                              [new_id].key))
                        return 1;
                  if (binds[port][new_id].valid)
                  {
                     if ((uint16_t)joykey != NO_BTN && joypad->button(
                              port, (uint16_t)joykey))
                        return 1;
                     if (joyaxis != AXIS_NONE &&
                           ((float)abs(joypad->axis(port, joyaxis))
                            / 0x8000) > axis_threshold)
                        return 1;
                     if (udev_mouse_button_pressed(udev, port,
                              binds[port][new_id].mbutton))
                        return 1;
                  }
               }
               break;
               /*deprecated*/
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               {
                  udev_input_mouse_t *mouse = udev_get_mouse(udev, port);
                  if (mouse)
                     return udev_mouse_get_x(mouse);
               }
               break;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               {
                  udev_input_mouse_t *mouse = udev_get_mouse(udev, port);
                  if (mouse)
                     return udev_mouse_get_y(mouse);
               }
               break;
         }
         break;
   }

   return 0;
}

static void udev_input_free(void *data)
{
   unsigned i;
   udev_input_t *udev = (udev_input_t*)data;

   if (!data || !udev)
      return;

   if (udev->fd >= 0)
      close(udev->fd);

   udev->fd = -1;

   for (i = 0; i < udev->num_devices; i++)
   {
      close(udev->devices[i]->fd);
      free(udev->devices[i]);
   }
   free(udev->devices);

   if (udev->monitor)
      udev_monitor_unref(udev->monitor);
   if (udev->udev)
      udev_unref(udev->udev);

   udev_input_kb_free(udev);

   free(udev);
}

static bool open_devices(udev_input_t *udev,
      enum udev_input_dev_type type, device_handle_cb cb)
{
   const char             *type_str = g_dev_type_str[type];
   struct udev_list_entry     *devs = NULL;
   struct udev_list_entry     *item = NULL;
   struct udev_enumerate *enumerate = udev_enumerate_new(udev->udev);

   if (!enumerate)
      return false;

   udev_enumerate_add_match_property(enumerate, type_str, "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   for (item = devs; item; item = udev_list_entry_get_next(item))
   {
      const char *name        = udev_list_entry_get_name(item);

      /* Get the filename of the /sys entry for the device
       * and create a udev_device object (dev) representing it. */
      struct udev_device *dev = udev_device_new_from_syspath(udev->udev, name);
      const char *devnode     = udev_device_get_devnode(dev);

      if (devnode)
      {
         int fd = open(devnode, O_RDONLY | O_NONBLOCK);

         if (fd != -1)
         {
            int check = udev_input_add_device(udev, type, devnode, cb);
            if (check == 0)
               RARCH_LOG("[udev]: udev_input_add_device error : %s (%s).\n",
                     devnode, strerror(errno));

            (void)check;
            close(fd);
         }
      }
      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);

   return true;
}

static void *udev_input_init(const char *joypad_driver)
{
   int mouse = 0;
   int keyboard=0;
   int fd;
   int i;
#ifdef UDEV_XKB_HANDLING
   gfx_ctx_ident_t ctx_ident;
#endif
   udev_input_t *udev   = (udev_input_t*)calloc(1, sizeof(*udev));

   if (!udev)
      return NULL;

   udev->udev = udev_new();
   if (!udev->udev)
      goto error;

   udev->monitor = udev_monitor_new_from_netlink(udev->udev, "udev");
   if (udev->monitor)
   {
      udev_monitor_filter_add_match_subsystem_devtype(udev->monitor, "input", NULL);
      udev_monitor_enable_receiving(udev->monitor);
   }

#ifdef UDEV_XKB_HANDLING
   if (init_xkb(-1, 0) == -1)
      goto error;

   video_context_driver_get_ident(&ctx_ident);
#ifdef HAVE_LAKKA
   /* Force xkb_handling on Lakka */
   udev->xkb_handling = true;
#else
   udev->xkb_handling = string_is_equal(ctx_ident.ident, "kms");
#endif /* HAVE_LAKKA */
#endif

#if defined(HAVE_EPOLL)
   fd = epoll_create(32);
   if (fd < 0)
      goto error;
#elif defined(HAVE_KQUEUE)
   fd = kqueue();
   if (fd == -1)
      goto error;
#endif

   udev->fd  = fd;

   if (!open_devices(udev, UDEV_INPUT_KEYBOARD, udev_handle_keyboard))
      goto error;

   if (!open_devices(udev, UDEV_INPUT_MOUSE, udev_handle_mouse))
      goto error;

   if (!open_devices(udev, UDEV_INPUT_TOUCHPAD, udev_handle_mouse))
      goto error;

   /* If using KMS and we forgot this,
    * we could lock ourselves out completely. */
   if (!udev->num_devices)
      RARCH_WARN("[udev]: Couldn't open any keyboard, mouse or touchpad. Are permissions set correctly for /dev/input/event* and /run/udev/?\n");

   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

#ifdef __linux__
   linux_terminal_disable_input();
#endif

#ifndef HAVE_X11
   /* TODO/FIXME - this can't be hidden behind a compile-time ifdef */
   RARCH_WARN("[udev]: Full-screen pointer won't be available.\n");
#endif

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type != UDEV_INPUT_KEYBOARD)
      {
         RARCH_LOG("[udev]: Mouse #%u: \"%s\" (%s) %s\n",
            mouse,
            udev->devices[i]->ident,
            udev->devices[i]->mouse.abs ? "ABS" : "REL",
            udev->devices[i]->devnode);

         input_config_set_mouse_display_name(mouse, udev->devices[i]->ident);
         mouse++;
       }
       else
       {
          RARCH_LOG("[udev]: Keyboard #%u: \"%s\" (%s).\n",
             keyboard,
             udev->devices[i]->ident,
             udev->devices[i]->devnode);
             keyboard++;
       }
   }

   return udev;

error:
   udev_input_free(udev);
   return NULL;
}

static uint64_t udev_input_get_capabilities(void *data)
{
   return
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_ANALOG)
      | (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_MOUSE)
      | (1 << RETRO_DEVICE_LIGHTGUN);
}

static void udev_input_grab_mouse(void *data, bool state)
{
#ifdef HAVE_X11
   Window window;
   Display *display = NULL;

   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
   {
      RARCH_WARN("[udev]: Mouse grab/ungrab feature unavailable.\n");
      return;
   }

   display = (Display*)video_driver_display_get();
   window  = (Window)video_driver_window_get();

   if (state)
      XGrabPointer(display, window, False,
            ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            GrabModeAsync, GrabModeAsync, window, None, CurrentTime);
   else
      XUngrabPointer(display, CurrentTime);
#else
   RARCH_WARN("[udev]: Mouse grab/ungrab feature unavailable.\n");
#endif
}

input_driver_t input_udev = {
   udev_input_init,
   udev_input_poll,
   udev_input_state,
   udev_input_free,
   NULL,
   NULL,
   udev_input_get_capabilities,
   "udev",
   udev_input_grab_mouse,
#ifdef __linux__
   linux_terminal_grab_stdin,
#else
   NULL,
#endif
   NULL
};
