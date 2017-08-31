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
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/epoll.h>
#include <sys/poll.h>

#include <libudev.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kd.h>

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

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "../../gfx/video_driver.h"
#include "../common/linux_common.h"
#include "../../configuration.h"

#include "../../verbosity.h"

#if defined(HAVE_XKBCOMMON) && defined(HAVE_KMS)
#define UDEV_XKB_HANDLING
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
   "ID_INPUT_KEYBOARD",
   "ID_INPUT_MOUSE",
   "ID_INPUT_TOUCHPAD"
};

typedef struct
{
   int16_t x, y, dlt_x, dlt_y;
   bool l, r, m;
   bool wu, wd, whu, whd;
} udev_input_mouse_t;

typedef struct
{
   struct input_absinfo info_x;
   struct input_absinfo info_y;
   udev_input_mouse_t mouse; /* touch-pad will be presented to RA/core as mouse */
} udev_input_touchpad_t;

struct udev_input_device
{
   int fd;
   dev_t dev;
   void (*handle_cb)(void *data,
         const struct input_event *event, udev_input_device_t *dev);
   char devnode[PATH_MAX_LENGTH];
   enum udev_input_dev_type type;

   union
   {
      udev_input_mouse_t mouse;
      udev_input_touchpad_t touchpad;
   } state;
};

typedef void (*device_handle_cb)(void *data,
      const struct input_event *event, udev_input_device_t *dev);

struct udev_input
{
   bool blocked;
   struct udev *udev;
   struct udev_monitor *monitor;


   const input_device_driver_t *joypad;

   int epfd;
   udev_input_device_t **devices;
   unsigned num_devices;

#ifdef UDEV_XKB_HANDLING
   bool xkb_handling;
#endif
};

#ifdef UDEV_XKB_HANDLING
int init_xkb(int fd, size_t size);
void free_xkb(void);
int handle_xkb(int code, int value);
#endif

static uint8_t udev_key_state[UDEV_MAX_KEYS];

static unsigned input_unify_ev_key_code(unsigned code)
{
   /* input_keymaps_translate_keysym_to_rk does not support the case
      where multiple keysyms translate to the same RETROK_* code,
      so unify remote control keysyms to keyboard keysyms here.  */
   switch (code)
   {
      case KEY_OK:
         return KEY_ENTER;
      case KEY_BACK:
         return KEY_BACKSPACE;
      default:
         return code;
   }
}

static void udev_handle_keyboard(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
#ifdef UDEV_XKB_HANDLING
   udev_input_t *udev = (udev_input_t*)data;
#endif
   unsigned keysym;

   switch (event->type)
   {
      case EV_KEY:
         keysym = input_unify_ev_key_code(event->code);
         if (event->value)
            BIT_SET(udev_key_state, keysym);
         else
            BIT_CLEAR(udev_key_state, keysym);

#ifdef UDEV_XKB_HANDLING
         if (udev->xkb_handling && handle_xkb(keysym, event->value) == 0)
            return;
#endif

         input_keyboard_event(event->value,
               input_keymaps_translate_keysym_to_rk(keysym),
               0, 0, RETRO_DEVICE_KEYBOARD);
         break;

      default:
         break;
   }
}

static void udev_input_kb_free(void)
{
   unsigned i;

   for (i = 0; i < UDEV_MAX_KEYS; i++)
      udev_key_state[i] = 0;

#ifdef UDEV_XKB_HANDLING
   free_xkb();
#endif
}

static udev_input_mouse_t *udev_get_mouse(struct udev_input *udev, unsigned port)
{
   unsigned i;
   unsigned mouse_index      = 0;
   settings_t *settings      = config_get_ptr();
   udev_input_mouse_t *mouse = NULL;

   if (port >= MAX_USERS)
      return NULL;

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type == UDEV_INPUT_KEYBOARD)
         continue;

      if (mouse_index == settings->uints.input_mouse_index[port])
      {
         if (udev->devices[i]->type == UDEV_INPUT_MOUSE)
            mouse = &udev->devices[i]->state.mouse;
         else
            mouse = &udev->devices[i]->state.touchpad.mouse;
         break;
      }

      ++mouse_index;
   }

   return mouse;
}

static void udev_handle_touchpad(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   int16_t pos;
   unsigned width                  = 0;
   unsigned height                 = 0;
   udev_input_touchpad_t *touchpad = &dev->state.touchpad;
   udev_input_mouse_t *mouse       = &dev->state.touchpad.mouse;

   switch (event->type)
   {
      case EV_ABS:
         video_driver_get_size(&width, &height);
         switch (event->code)
         {
            case ABS_X:
               pos = (float)(event->value - touchpad->info_x.minimum) /
                     (touchpad->info_x.maximum - touchpad->info_x.minimum) * width;
               mouse->dlt_x += pos - mouse->x;
               mouse->x = pos;
               break;
            case ABS_Y:
               pos = (float)(event->value - touchpad->info_y.minimum) /
                     (touchpad->info_y.maximum - touchpad->info_y.minimum) * height;
               mouse->dlt_y += pos - mouse->y;
               mouse->y = pos;
         }
         break;

      case EV_KEY:
         switch (event->code)
         {
            case BTN_LEFT:
               mouse->l = event->value;
               break;
            case BTN_MIDDLE:
               mouse->m = event->value;
               break;
            case BTN_RIGHT:
               mouse->r = event->value;
         }
   }
}

static void udev_handle_mouse(void *data,
      const struct input_event *event, udev_input_device_t *dev)
{
   udev_input_mouse_t *mouse = &dev->state.mouse;

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
            default:
               break;
         }
         break;

      case EV_REL:
         switch (event->code)
         {
            case REL_X:
               mouse->dlt_x += event->value;
               break;
            case REL_Y:
               mouse->dlt_y += event->value;
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
   }
}

static bool udev_input_add_device(udev_input_t *udev,
      enum udev_input_dev_type type, const char *devnode, device_handle_cb cb)
{
   int fd;
   struct stat st;
   struct epoll_event event;
   udev_input_device_t **tmp;
   udev_input_device_t *device = NULL;

   st.st_dev                   = 0;

   if (stat(devnode, &st) < 0)
      return false;

   fd = open(devnode, O_RDONLY | O_NONBLOCK);
   if (fd < 0)
      return false;

   device = (udev_input_device_t*)calloc(1, sizeof(*device));
   if (!device)
      goto error;

   device->fd        = fd;
   device->dev       = st.st_dev;
   device->handle_cb = cb;
   device->type      = type;

   strlcpy(device->devnode, devnode, sizeof(device->devnode));

   /* Touchpads report in absolute coords. */
   if (type == UDEV_INPUT_TOUCHPAD &&
         (ioctl(fd, EVIOCGABS(ABS_X), &device->state.touchpad.info_x) < 0 ||
          ioctl(fd, EVIOCGABS(ABS_Y), &device->state.touchpad.info_y) < 0))
      goto error;

   tmp = ( udev_input_device_t**)realloc(udev->devices,
         (udev->num_devices + 1) * sizeof(*udev->devices));

   if (!tmp)
      goto error;

   tmp[udev->num_devices++] = device;
   udev->devices            = tmp;

   event.events             = EPOLLIN;
   event.data.ptr           = device;

   /* Shouldn't happen, but just check it. */
   if (epoll_ctl(udev->epfd, EPOLL_CTL_ADD, fd, &event) < 0)
   {
      RARCH_ERR("Failed to add FD (%d) to epoll list (%s).\n",
            fd, strerror(errno));
   }

   return true;

error:
   close(fd);
   if (device)
      free(device);

   return false;
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
   struct udev_device *dev           = udev_monitor_receive_device(
         udev->monitor);

   if (!dev)
      return;

   val_key       = udev_device_get_property_value(dev, "ID_INPUT_KEY");
   val_mouse     = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
   val_touchpad  = udev_device_get_property_value(dev, "ID_INPUT_TOUCHPAD");
   action        = udev_device_get_action(dev);
   devnode       = udev_device_get_devnode(dev);

   if (val_key && string_is_equal_fast(val_key, "1", 1) && devnode)
   {
      /* EV_KEY device, can be a keyboard or a remote control device.  */
      dev_type   = UDEV_INPUT_KEYBOARD;
      cb         = udev_handle_keyboard;
   }
   else if (val_mouse && string_is_equal_fast(val_mouse, "1", 1) && devnode)
   {
      dev_type   = UDEV_INPUT_MOUSE;
      cb         = udev_handle_mouse;
   }
   else if (val_touchpad && string_is_equal_fast(val_touchpad, "1", 1) && devnode)
   {
      dev_type   = UDEV_INPUT_TOUCHPAD;
      cb         = udev_handle_touchpad;
   }
   else
      goto end;

   if (string_is_equal_fast(action, "add", 3))
   {
      RARCH_LOG("[udev]: Hotplug add %s: %s.\n",
            g_dev_type_str[dev_type], devnode);
      udev_input_add_device(udev, dev_type, devnode, cb);
   }
   else if (string_is_equal_fast(action, "remove", 6))
   {
      RARCH_LOG("[udev]: Hotplug remove %s: %s.\n",
            g_dev_type_str[dev_type], devnode);
      udev_input_remove_device(udev, devnode);
   }

end:
   udev_device_unref(dev);
}

#ifdef HAVE_X11
static void udev_input_get_pointer_position(int *x, int *y)
{
   Window w;
   int p;
   unsigned m;
   Display *display = (Display*)video_driver_display_get();
   Window window    = (Window)video_driver_window_get();

   XQueryPointer(display, window, &w, &w, &p, &p, x, y, &m);
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
   struct epoll_event events[32];
   udev_input_mouse_t *mouse = NULL;
   int x                     = 0;
   int y                     = 0;
   udev_input_t *udev        = (udev_input_t*)data;

#ifdef HAVE_X11
   if (video_driver_display_type_get() == RARCH_DISPLAY_X11)
      udev_input_get_pointer_position(&x, &y);
#endif

   for (i = 0; i < udev->num_devices; ++i)
   {
      if (udev->devices[i]->type == UDEV_INPUT_MOUSE)
      {
         mouse    = &udev->devices[i]->state.mouse;
         mouse->x = x;
         mouse->y = y;
      }
      else if (udev->devices[i]->type == UDEV_INPUT_TOUCHPAD)
         mouse = &udev->devices[i]->state.touchpad.mouse;
      else
         continue;

      mouse->dlt_x = 0;
      mouse->dlt_y = 0;
      mouse->wu    = false;
      mouse->wd    = false;
      mouse->whu   = false;
      mouse->whd   = false;
   }

   while (udev->monitor && udev_input_poll_hotplug_available(udev->monitor))
      udev_input_handle_hotplug(udev);

   ret = epoll_wait(udev->epfd, events, ARRAY_SIZE(events), 0);

   for (i = 0; i < ret; i++)
   {
      if (events[i].events & EPOLLIN)
      {
         int j, len;
         struct input_event input_events[32];
         udev_input_device_t *device = (udev_input_device_t*)events[i].data.ptr;

         while ((len = read(device->fd,
                     input_events, sizeof(input_events))) > 0)
         {
            len /= sizeof(*input_events);
            for (j = 0; j < len; j++)
               device->handle_cb(udev, &input_events[j], device);
         }
      }
   }

   if (udev->joypad)
      udev->joypad->poll();
}

static bool udev_pointer_is_off_window(udev_input_mouse_t *mouse)
{
   struct video_viewport view;
   bool r = video_driver_get_viewport_info(&view);

   if (r)
      r = mouse->x < view.x ||
          mouse->x >= view.x + view.width ||
          mouse->y < view.y ||
          mouse->y >= view.y + view.height;

   return r;
}

static int16_t udev_mouse_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return 0;

   if (id != RETRO_DEVICE_ID_MOUSE_X && id != RETRO_DEVICE_ID_MOUSE_Y &&
         udev_pointer_is_off_window(mouse))
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return screen ? mouse->x : mouse->dlt_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return screen ? mouse->y : mouse->dlt_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return mouse->l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return mouse->r;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return mouse->m;
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

static int16_t udev_lightgun_state(udev_input_t *udev,
      unsigned port, unsigned id)
{
   udev_input_mouse_t *mouse = udev_get_mouse(udev, port);

   if (!mouse)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return mouse->dlt_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return mouse->dlt_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return mouse->l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return mouse->m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return mouse->r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return mouse->m && mouse->r;
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return mouse->m && mouse->l;
   }

   return 0;
}

static int16_t udev_analog_pressed(const struct retro_keybind *binds,
      unsigned idx, unsigned id)
{
   unsigned id_minus     = 0;
   unsigned id_plus      = 0;
   int16_t pressed_minus = 0;
   int16_t pressed_plus  = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   if (     binds[id_minus].valid 
         && BIT_GET(udev_key_state,
            rarch_keysym_lut[binds[id_minus].key]))
      pressed_minus = -0x7fff;
   if (     binds[id_plus].valid 
         && BIT_GET(udev_key_state,
         rarch_keysym_lut[binds[id_plus].key]))
      pressed_plus = 0x7fff;

   return pressed_plus + pressed_minus;
}

static int16_t udev_pointer_state(udev_input_t *udev,
      unsigned port, unsigned id, bool screen)
{
   struct video_viewport vp;
   bool inside                 = false;
   int16_t res_x               = 0;
   int16_t res_y               = 0;
   int16_t res_screen_x        = 0;
   int16_t res_screen_y        = 0;
   udev_input_mouse_t *mouse   = udev_get_mouse(udev, port);

   vp.x                        = 0;
   vp.y                        = 0;
   vp.width                    = 0;
   vp.height                   = 0;
   vp.full_width               = 0;
   vp.full_height              = 0;

   if (!mouse)
      return 0;

   if (!(video_driver_translate_coord_viewport_wrap(&vp,
         mouse->x, mouse->y, &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return mouse->l;
   }

   return 0;
}

static int16_t udev_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   int16_t ret                = 0;
   udev_input_t *udev         = (udev_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         ret = BIT_GET(udev_key_state,
               rarch_keysym_lut[binds[port][id].key]);
         if (!ret)
            ret = input_joypad_pressed(udev->joypad,
                  joypad_info, port, binds[port], id);
         return ret;
      case RETRO_DEVICE_ANALOG:
         ret = udev_analog_pressed(binds[port], idx, id);
         if (!ret && binds[port])
            ret = input_joypad_analog(udev->joypad,
                  joypad_info, port, idx, id, binds[port]);
         return ret;
      case RETRO_DEVICE_KEYBOARD:
         if (video_driver_cb_has_focus())
            return id < RETROK_LAST && BIT_GET(udev_key_state,
                  rarch_keysym_lut[(enum retro_key)id]);
         break;
      case RETRO_DEVICE_MOUSE:
         return udev_mouse_state(udev, port, id, false);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return udev_mouse_state(udev, port, id, true);
      case RETRO_DEVICE_POINTER:
         return udev_pointer_state(udev, port, id, false);
      case RARCH_DEVICE_POINTER_SCREEN:
         return udev_pointer_state(udev, port, id, true);
      case RETRO_DEVICE_LIGHTGUN:
         return udev_lightgun_state(udev, port, id);
   }

   return 0;
}

static bool udev_input_meta_key_pressed(void *data, int key)
{
   return false;
}

static void udev_input_free(void *data)
{
   unsigned i;
   udev_input_t *udev = (udev_input_t*)data;

   if (!data || !udev)
      return;

   if (udev->joypad)
      udev->joypad->destroy();

   if (udev->epfd >= 0)
      close(udev->epfd);

   udev->epfd = -1;

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

   udev_input_kb_free();

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
            RARCH_LOG("[udev] Adding device %s as type %s.\n",
                  devnode, type_str);
            if (!udev_input_add_device(udev, type, devnode, cb))
               RARCH_ERR("[udev] Failed to open device: %s (%s).\n",
                     devnode, strerror(errno));
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
   int fd;
#ifdef UDEV_XKB_HANDLING
   gfx_ctx_ident_t ctx_ident;
#endif
   udev_input_t *udev   = (udev_input_t*)calloc(1, sizeof(*udev));

   if (!udev)
      return NULL;

   udev->udev = udev_new();
   if (!udev->udev)
   {
      RARCH_ERR("Failed to create udev handle.\n");
      goto error;
   }

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
   udev->xkb_handling = string_is_equal(ctx_ident.ident, "kms");
#endif

   fd = epoll_create(32);
   if (fd < 0)
   {
      RARCH_ERR("Failed to create epoll FD.\n");
      goto error;
   }

   udev->epfd  = fd;

   if (!open_devices(udev, UDEV_INPUT_KEYBOARD, udev_handle_keyboard))
   {
      RARCH_ERR("Failed to open keyboard.\n");
      goto error;
   }

   if (!open_devices(udev, UDEV_INPUT_MOUSE, udev_handle_mouse))
   {
      RARCH_ERR("Failed to open mouse.\n");
      goto error;
   }

   if (!open_devices(udev, UDEV_INPUT_TOUCHPAD, udev_handle_touchpad))
   {
      RARCH_ERR("Failed to open touchpads.\n");
      goto error;
   }

   /* If using KMS and we forgot this, 
    * we could lock ourselves out completely. */
   if (!udev->num_devices)
      RARCH_WARN("[udev]: Couldn't open any keyboard, mouse or touchpad. Are permissions set correctly for /dev/input/event*?\n");

   udev->joypad = input_joypad_init_driver(joypad_driver, udev);
   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

   linux_terminal_disable_input();

#ifndef HAVE_X11
   RARCH_WARN("[udev]: Full-screen pointer won't be available.\n");
#endif

   return udev;

error:
   udev_input_free(udev);
   return NULL;
}

static uint64_t udev_input_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_ANALOG)   |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_MOUSE)    |
      (1 << RETRO_DEVICE_LIGHTGUN);
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

static bool udev_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   udev_input_t *udev = (udev_input_t*)data;
   if (udev && udev->joypad)
      return input_joypad_set_rumble(udev->joypad,
            port, effect, strength);
   return false;
}

static const input_device_driver_t *udev_input_get_joypad_driver(void *data)
{
   udev_input_t *udev = (udev_input_t*)data;
   if (!udev)
      return NULL;
   return udev->joypad;
}

static bool udev_input_keyboard_mapping_is_blocked(void *data)
{
   udev_input_t *udev = (udev_input_t*)data;
   if (!udev)
      return false;
   return udev->blocked;
}

static void udev_input_keyboard_mapping_set_block(void *data, bool value)
{
   udev_input_t *udev = (udev_input_t*)data;
   if (!udev)
      return;
   udev->blocked = value;
}

input_driver_t input_udev = {
   udev_input_init,
   udev_input_poll,
   udev_input_state,
   udev_input_meta_key_pressed,
   udev_input_free,
   NULL,
   NULL,
   udev_input_get_capabilities,
   "udev",
   udev_input_grab_mouse,
   linux_terminal_grab_stdin,
   udev_input_set_rumble,
   udev_input_get_joypad_driver,
   NULL,
   udev_input_keyboard_mapping_is_blocked,
   udev_input_keyboard_mapping_set_block,
};
