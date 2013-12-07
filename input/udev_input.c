/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "input_common.h"
#include "../general.h"
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/types.h>
#include <linux/input.h>

typedef struct udev_input udev_input_t;
typedef void (*device_poll_cb)(udev_input_t *udev, int fd);

struct input_device
{
   int fd;
   dev_t dev;
   device_poll_cb poll_cb;
   char devnode[PATH_MAX];
};

struct udev_input
{
   struct udev *udev;
   struct udev_monitor *monitor;

   const rarch_joypad_driver_t *joypad;
   uint8_t key_state[(KEY_MAX + 7) / 8];

   int epfd;
   struct input_device **devices;
   unsigned num_devices;

   int16_t mouse_x;
   int16_t mouse_y;
   bool mouse_l, mouse_r, mouse_m;
};

static inline bool get_bit(const uint8_t *buf, unsigned bit)
{
   return buf[bit >> 3] & (1 << (bit & 7));
}

static inline void clear_bit(uint8_t *buf, unsigned bit)
{
   buf[bit >> 3] &= ~(1 << (bit & 7));
}

static inline void set_bit(uint8_t *buf, unsigned bit)
{
   buf[bit >> 3] |= 1 << (bit & 7);
}

static void udev_poll_keyboard(udev_input_t *udev, int fd)
{
   int i, len;
   struct input_event events[32];
   while ((len = read(fd, events, sizeof(events))) > 0)
   {
      len /= sizeof(*events);
      for (i = 0; i < len; i++)
      {
         switch (events[i].type)
         {
            case EV_KEY:
               if (events[i].value)
                  set_bit(udev->key_state, events[i].code);
               else
                  clear_bit(udev->key_state, events[i].code);
               break;

            default:
               break;
         }
      }
   }
}

static void udev_poll_mouse(udev_input_t *udev, int fd)
{
   int i, len;
   struct input_event events[32];
   while ((len = read(fd, events, sizeof(events))) > 0)
   {
      len /= sizeof(*events);
      for (i = 0; i < len; i++)
      {
         switch (events[i].type)
         {
            case EV_KEY:
               switch (events[i].code)
               {
                  case BTN_LEFT:
                     udev->mouse_l = events[i].value;
                     break;

                  case BTN_RIGHT:
                     udev->mouse_r = events[i].value;
                     break;

                  case BTN_MIDDLE:
                     udev->mouse_m = events[i].value;
                     break;

                  default:
                     break;
               }
               break;

            case EV_REL:
               switch (events[i].code)
               {
                  case REL_X:
                     udev->mouse_x += events[i].value;
                     break;

                  case REL_Y:
                     udev->mouse_y += events[i].value;
                     break;

                  default:
                     break;
               }
               break;

            default:
               break;
         }
      }
   }
}

static bool hotplug_available(udev_input_t *udev)
{
   if (!udev->monitor)
      return false;

   struct pollfd fds = {0};
   fds.fd = udev_monitor_get_fd(udev->monitor);
   fds.events = POLLIN;
   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static bool add_device(udev_input_t *udev, const char *devnode, device_poll_cb cb)
{
   struct stat st;
   if (stat(devnode, &st) < 0)
      return false;

#if 0
   // If we have added this device already, skip it (could have aliases in /dev/input).
   unsigned i;
   for (i = 0; i < udev->num_devices; i++)
      if (st.st_dev == udev->devices[i]->dev)
         return true;
#endif

   int fd = open(devnode, O_RDONLY | O_NONBLOCK);
   if (fd < 0)
      return false;

   struct input_device *device = (struct input_device*)calloc(1, sizeof(*device));
   if (!device)
   {
      close(fd);
      return false;
   }

   device->fd = fd;
   device->dev = st.st_dev;
   device->poll_cb = cb;

   strlcpy(device->devnode, devnode, sizeof(device->devnode));

   struct input_device **tmp = (struct input_device**)realloc(udev->devices,
         (udev->num_devices + 1) * sizeof(*udev->devices));

   if (!tmp)
   {
      close(fd);
      free(device);
      return false;
   }

   tmp[udev->num_devices++] = device;
   udev->devices = tmp;

   struct epoll_event event = {0};
   event.events = EPOLLIN;
   event.data.ptr = device;
   if (epoll_ctl(udev->epfd, EPOLL_CTL_ADD, fd, &event) < 0) // Shouldn't happen, but just check it.
      RARCH_ERR("Failed to add FD (%d) to epoll list (%s).\n", fd, strerror(errno));

   return true;
}

static void remove_device(udev_input_t *udev, const char *devnode)
{
   unsigned i;
   for (i = 0; i < udev->num_devices; i++)
   {
      if (!strcmp(devnode, udev->devices[i]->devnode))
      {
         close(udev->devices[i]->fd);
         free(udev->devices[i]);
         memmove(udev->devices + i, udev->devices + i + 1,
               (udev->num_devices - (i + 1)) * sizeof(*udev->devices));
         udev->num_devices--;
      }
   }
}

static void handle_hotplug(udev_input_t *udev)
{
   struct udev_device *dev = udev_monitor_receive_device(udev->monitor);
   if (!dev)
      return;

   const char *val_keyboard = udev_device_get_property_value(dev, "ID_INPUT_KEYBOARD");
   const char *val_mouse = udev_device_get_property_value(dev, "ID_INPUT_MOUSE");
   const char *action = udev_device_get_action(dev);
   const char *devnode = udev_device_get_devnode(dev);

   bool is_keyboard = val_keyboard && !strcmp(val_keyboard, "1") && devnode;
   bool is_mouse = val_mouse && !strcmp(val_mouse, "1") && devnode;

   if (!is_keyboard && !is_mouse)
      goto end;

   if (!strcmp(action, "add"))
   {
      RARCH_LOG("[udev]: Hotplug add %s: %s.\n", is_keyboard ? "keyboard" : "mouse", devnode);
      device_poll_cb cb = is_keyboard ? udev_poll_keyboard : udev_poll_mouse;
      add_device(udev, devnode, cb);
   }
   else if (!strcmp(action, "remove"))
   {
      RARCH_LOG("[udev]: Hotplug remove %s: %s.\n", is_keyboard ? "keyboard" : "mouse", devnode);
      remove_device(udev, devnode);
   }

end:
   udev_device_unref(dev);
}

static void udev_input_poll(void *data)
{
   udev_input_t *udev = (udev_input_t*)data;
   udev->mouse_x = udev->mouse_y = 0;

   while (hotplug_available(udev))
      handle_hotplug(udev);

   int i;
   struct epoll_event events[32];
   int ret = epoll_wait(udev->epfd, events, ARRAY_SIZE(events), 0);

   for (i = 0; i < ret; i++)
   {
      if (events[i].events & EPOLLIN)
      {
         struct input_device *device = (struct input_device*)events[i].data.ptr;
         device->poll_cb(udev, device->fd);
      }
   }

   input_joypad_poll(udev->joypad);
}

static int16_t udev_mouse_state(udev_input_t *udev, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return udev->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return udev->mouse_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return udev->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return udev->mouse_r;
      default:
         return 0;
   }
}

static int16_t udev_lightgun_state(udev_input_t *udev, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return udev->mouse_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return udev->mouse_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return udev->mouse_l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return udev->mouse_m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return udev->mouse_r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return udev->mouse_m && udev->mouse_r; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return udev->mouse_m && udev->mouse_l; 
      default:
         return 0;
   }
}

static bool udev_is_pressed(udev_input_t *udev, const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && get_bit(udev->key_state, input_translate_rk_to_keysym(binds[id].key));
   }
   else
      return false;
}

static int16_t udev_analog_pressed(udev_input_t *udev,
      const struct retro_keybind *binds, unsigned index, unsigned id)
{
   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   int16_t pressed_minus = udev_is_pressed(udev,
         binds, id_minus) ? -0x7fff : 0;
   int16_t pressed_plus = udev_is_pressed(udev,
         binds, id_plus) ? 0x7fff : 0;
   return pressed_plus + pressed_minus;
}

static int16_t udev_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned index, unsigned id)
{
   udev_input_t *udev = (udev_input_t*)data;
   int16_t ret;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return udev_is_pressed(udev, binds[port], id) ||
            input_joypad_pressed(udev->joypad, port, binds[port], id);

      case RETRO_DEVICE_ANALOG:
         ret = udev_analog_pressed(udev, binds[port], index, id);
         if (!ret)
            ret = input_joypad_analog(udev->joypad, port, index, id, binds[port]);
         return ret;

      case RETRO_DEVICE_KEYBOARD:
         return id < RETROK_LAST && get_bit(udev->key_state, input_translate_rk_to_keysym(id));

      case RETRO_DEVICE_MOUSE:
         return udev_mouse_state(udev, id);

      case RETRO_DEVICE_LIGHTGUN:
         return udev_lightgun_state(udev, id);

      default:
         return 0;
   }
}

static bool udev_bind_button_pressed(void *data, int key)
{
   udev_input_t *udev = (udev_input_t*)data;
   return udev_is_pressed(udev, g_settings.input.binds[0], key) ||
      input_joypad_pressed(udev->joypad, 0, g_settings.input.binds[0], key);
}

static void udev_input_free(void *data)
{
   if (!data)
      return;

   unsigned i;
   udev_input_t *udev = (udev_input_t*)data;
   if (udev->joypad)
      udev->joypad->destroy();

   if (udev->epfd >= 0)
      close(udev->epfd);

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

   free(udev);
}

static bool open_devices(udev_input_t *udev, const char *type, device_poll_cb cb)
{
   struct udev_list_entry *devs;
   struct udev_list_entry *item;

   struct udev_enumerate *enumerate = udev_enumerate_new(udev->udev);
   if (!enumerate)
      return false;

   udev_enumerate_add_match_property(enumerate, type, "1");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);
   for (item = devs; item; item = udev_list_entry_get_next(item))
   {
      const char *name = udev_list_entry_get_name(item);
      struct udev_device *dev = udev_device_new_from_syspath(udev->udev, name);
      const char *devnode = udev_device_get_devnode(dev);

      int fd = devnode ? open(devnode, O_RDONLY | O_NONBLOCK) : -1;

      if (devnode)
      {
         RARCH_LOG("[udev] Adding device %s as type %s.\n", devnode, type);
         if (!add_device(udev, devnode, cb))
            RARCH_ERR("[udev] Failed to open device: %s (%s).\n", devnode, strerror(errno));
      }

      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);
   return true;
}

static void *udev_input_init(void)
{
   udev_input_t *udev = (udev_input_t*)calloc(1, sizeof(*udev));
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

   udev->epfd = epoll_create(32);
   if (udev->epfd < 0)
   {
      RARCH_ERR("Failed to create epoll FD.\n");
      goto error;
   }

   if (!open_devices(udev, "ID_INPUT_KEYBOARD", udev_poll_keyboard))
   {
      RARCH_ERR("Failed to open keyboard.\n");
      goto error;
   }

   if (!open_devices(udev, "ID_INPUT_MOUSE", udev_poll_mouse))
   {
      RARCH_ERR("Failed to open mouse.\n");
      goto error;
   }

   udev->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
   input_init_keyboard_lut(rarch_key_map_linux);
   return udev;

error:
   udev_input_free(udev);
   return NULL;
}

static uint64_t udev_input_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD) |
      (1 << RETRO_DEVICE_ANALOG) |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_MOUSE) |
      (1 << RETRO_DEVICE_LIGHTGUN);
}

static void udev_input_grab_mouse(void *data, bool state)
{
   // Dummy for now. Might be useful in the future.
   (void)data;
   (void)state;
}

static bool udev_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   udev_input_t *udev = (udev_input_t*)data;
   return input_joypad_set_rumble(udev->joypad, port, effect, strength);
}

static const rarch_joypad_driver_t *udev_input_get_joypad_driver(void *data)
{
   udev_input_t *udev = (udev_input_t*)data;
   return udev->joypad;
}

const input_driver_t input_udev = {
   udev_input_init,
   udev_input_poll,
   udev_input_state,
   udev_bind_button_pressed,
   udev_input_free,
   NULL,
   NULL,
   udev_input_get_capabilities,
   "udev",
   udev_input_grab_mouse,
   udev_input_set_rumble,
   udev_input_get_joypad_driver,
};

