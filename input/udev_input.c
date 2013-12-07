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
#include <fcntl.h>
#include <libudev.h>
#include <linux/types.h>
#include <linux/input.h>

typedef struct udev_input
{
   const rarch_joypad_driver_t *joypad;
   uint8_t key_state[(KEY_MAX + 7) / 8];

   int keyboard_fd;
   int mouse_fd;

   int16_t mouse_x;
   int16_t mouse_y;
   bool mouse_l, mouse_r, mouse_m;
} udev_input_t;

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

static void udev_input_poll(void *data)
{
   udev_input_t *udev = (udev_input_t*)data;

   if (udev->keyboard_fd >= 0)
   {
      int i, len;
      struct input_event events[32];
      while ((len = read(udev->keyboard_fd, events, sizeof(events))) > 0)
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

   if (udev->mouse_fd >= 0)
   {
      udev->mouse_x = udev->mouse_y = 0;

      int i, len;
      struct input_event events[32];
      while ((len = read(udev->mouse_fd, events, sizeof(events))) > 0)
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
   udev_input_t *udev = (udev_input_t*)data;
   if (udev->joypad)
      udev->joypad->destroy();

   if (udev->keyboard_fd >= 0)
      close(udev->keyboard_fd);
   if (udev->mouse_fd >= 0)
      close(udev->mouse_fd);

   free(udev);
}

static int open_device(const char *type)
{
   int ret = -1;
   struct udev_list_entry *devs = NULL;
   struct udev_list_entry *item = NULL;

   struct udev *udev = udev_new();
   if (!udev)
      return ret;

   struct udev_enumerate *enumerate = udev_enumerate_new(udev);
   if (!enumerate)
   {
      udev_unref(udev);
      return ret;
   }

   udev_enumerate_add_match_property(enumerate, type, "1");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);
   for (item = devs; item; item = udev_list_entry_get_next(item))
   {
      const char *name = udev_list_entry_get_name(item);
      struct udev_device *dev = udev_device_new_from_syspath(udev, name);
      const char *devnode = udev_device_get_devnode(dev);

      int fd = devnode ? open(devnode, O_RDONLY | O_NONBLOCK) : -1;
      udev_device_unref(dev);

      if (fd >= 0)
      {
         ret = fd;
         break;
      }
   }

   udev_enumerate_unref(enumerate);
   udev_unref(udev);

   return ret;
}

static void *udev_input_init(void)
{
   udev_input_t *udev = (udev_input_t*)calloc(1, sizeof(*udev));
   if (!udev)
      return NULL;

   udev->keyboard_fd = open_device("ID_INPUT_KEYBOARD");
   udev->mouse_fd = open_device("ID_INPUT_MOUSE");

   if (udev->keyboard_fd < 0)
      RARCH_WARN("Failed to find a keyboard.\n");
   if (udev->mouse_fd < 0)
      RARCH_WARN("Failed to find a mouse.\n");

   udev->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
   input_init_keyboard_lut(rarch_key_map_linux);
   return udev;
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
   NULL,
   udev_input_set_rumble,
   udev_input_get_joypad_driver,
};

