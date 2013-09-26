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

// Udev/evdev Linux joypad driver.
// More complex and extremely low level,
// but only Linux driver which can support joypad rumble.
// Uses udev for device detection + hotplug.
//
// Code adapted from SDL 2.0's implementation.

#define NUM_BUTTONS 32
#define NUM_AXES 32
#define NUM_HATS 4

struct udev_joypad
{
   int fd;
   dev_t device;

   // Input state polled
   bool buttons[NUM_BUTTONS];
   int16_t axes[NUM_AXES];
   int8_t hats[NUM_HATS][2];

   // Maps keycodes -> button/axes
   uint8_t button_bind[KEY_MAX];
   uint8_t axes_bind[ABS_MAX];
   struct input_absinfo absinfo[NUM_AXES];

   int num_effects;
   int effects[2]; // [0] - strong, [1] - weak 
   bool has_set_ff[2];
   uint16_t strength[2];

   char *ident;
   char *path;
};

static struct udev *g_udev;
static struct udev_monitor *g_udev_mon;
static struct udev_joypad g_pads[MAX_PLAYERS];

static inline int16_t compute_axis(const struct input_absinfo *info, int value)
{
   int range = info->maximum - info->minimum;
   int axis = (value - info->minimum) * 0xffffll / range - 0x7fffll;
   if (axis > 0x7fff)
      return 0x7fff;
   else if (axis < -0x7fff)
      return -0x7fff;
   else
      return axis;
}

static void poll_pad(unsigned i)
{
   struct udev_joypad *pad = &g_pads[i];
   if (pad->fd < 0)
      return;

   int len;
   struct input_event events[32];

   while ((len = read(pad->fd, events, sizeof(events))) > 0)
   {
      len /= sizeof(*events);
      for (int i = 0; i < len; i++)
      {
         int code = events[i].code;
         switch (events[i].type)
         {
            case EV_KEY:
               if (code >= BTN_MISC)
                  pad->buttons[pad->button_bind[code]] = events[i].value;
               break;

            case EV_ABS:
               if (code >= ABS_MISC)
                  break;

               switch (code)
               {
                  case ABS_HAT0X:
                  case ABS_HAT0Y:
                  case ABS_HAT1X:
                  case ABS_HAT1Y:
                  case ABS_HAT2X:
                  case ABS_HAT2Y:
                  case ABS_HAT3X:
                  case ABS_HAT3Y:
                  {
                     code -= ABS_HAT0X;
                     pad->hats[code >> 1][code & 1] = events[i].value;
                     break;
                  }

                  default:
                  {
                     unsigned axis = pad->axes_bind[code];
                     pad->axes[axis] = compute_axis(&pad->absinfo[axis], events[i].value);
                     break;
                  }
               }
               break;

            default:
               break;
         }
      }
   }
}

static bool hotplug_available(void)
{
   if (!g_udev_mon)
      return false;
   struct pollfd fds = {0};
   fds.fd = udev_monitor_get_fd(g_udev_mon);
   fds.events = POLLIN;
   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static void check_device(const char *path, bool hotplugged);
static void remove_device(const char *path);

static void handle_hotplug(void)
{
   struct udev_device *dev = udev_monitor_receive_device(g_udev_mon);
   if (!dev)
      return;

   const char *val = udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
   const char *action = udev_device_get_action(dev);
   const char *devnode = udev_device_get_devnode(dev);

   if (!val || strcmp(val, "1") || !devnode)
      goto end;

   if (!strcmp(action, "add"))
   {
      RARCH_LOG("[udev]: Hotplug add: %s.\n", devnode);
      check_device(devnode, true);
   }
   else if (!strcmp(action, "remove"))
   {
      RARCH_LOG("[udev]: Hotplug remove: %s.\n", devnode);
      remove_device(devnode);
   }

end:
   udev_device_unref(dev);
}

static bool udev_set_rumble(unsigned i, enum retro_rumble_effect effect, uint16_t strength)
{
   struct udev_joypad *pad = &g_pads[i];

   if (pad->fd < 0)
      return false;
   if (pad->num_effects < 2)
      return false;

   if (pad->strength[effect] == strength)
      return true;

   if (pad->has_set_ff[effect] && strength != pad->strength[effect])
   {
      struct input_event play;
      memset(&play, 0, sizeof(play));
      play.type = EV_FF;
      play.code = pad->effects[effect];
      play.value = 0;
      if (write(pad->fd, &play, sizeof(play)) < (ssize_t)sizeof(play))
      {
         RARCH_ERR("[udev]: Failed to set rumble effect %u on pad %u.\n",
               effect, i);
         return false;
      }

      if (ioctl(pad->fd, EVIOCRMFF, (void*)(uintptr_t)pad->effects[effect]) < 0)
         RARCH_WARN("[udev]: Failed to remove effect.\n");

      pad->has_set_ff[effect] = false;
      pad->effects[effect] = -1;
   }

   // Have to defer the force feedback settings to here.
   // For some reason, effects are getting dropped when they're set at init.
   // Setting at init seems to work for pads which are hotplugged ...
   //
   // This approach might be cleaner in the end if we end up supporting configurable force feedback.
   if (!pad->has_set_ff[effect] && strength)
   {
      struct ff_effect e;
      memset(&e, 0, sizeof(e));
      e.type = FF_RUMBLE;
      e.id = -1;
      switch (effect)
      {
         case RETRO_RUMBLE_STRONG: e.u.rumble.strong_magnitude = strength; break;
         case RETRO_RUMBLE_WEAK: e.u.rumble.weak_magnitude = strength; break;
         default: return false;
      }

      if (ioctl(pad->fd, EVIOCSFF, &e) < 0)
      {
         RARCH_ERR("Failed to set rumble effect on pad #%u.\n", i);
         return false;
      }

      pad->has_set_ff[effect] = true;
      pad->effects[effect] = e.id;
   }
   pad->strength[effect] = strength;

   if (strength)
   {
      struct input_event play;
      memset(&play, 0, sizeof(play));
      play.type = EV_FF;
      play.code = pad->effects[effect];
      play.value = 1;
      if (write(pad->fd, &play, sizeof(play)) < (ssize_t)sizeof(play))
      {
         RARCH_ERR("[udev]: Failed to set rumble effect %u on pad %u.\n",
               effect, i);
         return false;
      }
   }

   return true;
}

static void udev_joypad_poll(void)
{
   while (hotplug_available())
      handle_hotplug();

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      poll_pad(i);
}

#define test_bit(nr, addr) \
   (((1UL << ((nr) % (sizeof(long) * CHAR_BIT))) & ((addr)[(nr) / (sizeof(long) * CHAR_BIT)])) != 0)
#define NBITS(x) ((((x) - 1) / (sizeof(long) * CHAR_BIT)) + 1)

#if 0
static void test_initial_rumble(int fd, const char *path)
{
   // Check for rumble features.
   unsigned long ffbit[NBITS(FF_MAX)] = {0};
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0)
   {
      if (test_bit(FF_RUMBLE, ffbit))
         RARCH_LOG("[udev]: Pad (%s) supports force feedback.\n",
               path);

      int effects;
      if (ioctl(fd, EVIOCGEFFECTS, &effects) >= 0)
         RARCH_LOG("[udev]: Pad (%s) supports %d force feedback effects.\n", path, effects);

      if (effects >= 2)
      {
         struct ff_effect effect;
         bool support_ff[2];
         int effects[2] = {-1, -1};

         // Strong rumble.
         memset(&effect, 0, sizeof(effect));
         effect.type = FF_RUMBLE;
         effect.id = -1;
         effect.u.rumble.strong_magnitude = 0x8000;
         effect.u.rumble.weak_magnitude = 0;
         support_ff[0] = ioctl(fd, EVIOCSFF, &effect) == 0;
         if (support_ff[0])
         {
            RARCH_LOG("[udev]: (%s) supports \"strong\" rumble effect (id %d).\n",
                  path, effect.id);
            effects[RETRO_RUMBLE_STRONG] = effect.id; // Gets updated by ioctl().
         }

         // Weak rumble.
         memset(&effect, 0, sizeof(effect));
         effect.type = FF_RUMBLE;
         effect.id = -1;
         effect.u.rumble.strong_magnitude = 0;
         effect.u.rumble.weak_magnitude = 0xc000;
         support_ff[1] = ioctl(fd, EVIOCSFF, &effect) == 0;
         if (support_ff[1])
         {
            RARCH_LOG("[udev]: Pad (%s) supports \"weak\" rumble effect (id %d).\n",
                  path, effect.id);
            effects[RETRO_RUMBLE_WEAK] = effect.id;
         }

         struct input_event play;
         memset(&play, 0, sizeof(play));
         play.type = EV_FF;
         play.code = effects[0];
         play.value = true;
         write(fd, &play, sizeof(play));
         rarch_sleep(500);
         play.value = false;
         write(fd, &play, sizeof(play));
         rarch_sleep(500);
         play.code = effects[1];
         play.value = true;
         write(fd, &play, sizeof(play));
         rarch_sleep(500);
         play.value = false;
         write(fd, &play, sizeof(play));
      }
   }

}
#endif

static int open_joystick(const char *path)
{
   int fd = open(path, O_RDWR | O_NONBLOCK);
   if (fd < 0)
      return fd;

   unsigned long evbit[NBITS(EV_MAX)] = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};

   if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      goto error;

   // Has to at least support EV_KEY interface.
   if (!test_bit(EV_KEY, evbit))
      goto error;

   return fd;

error:
   close(fd);
   return -1;
}

static int find_vacant_pad(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      if (g_pads[i].fd < 0)
         return i;
   return -1;
}

static void free_pad(unsigned pad)
{
   if (g_pads[pad].fd >= 0)
      close(g_pads[pad].fd);

   free(g_pads[pad].path);
   if (g_pads[pad].ident)
      *g_pads[pad].ident = '\0';
   memset(&g_pads[pad], 0, sizeof(g_pads[pad]));

   g_pads[pad].fd = -1;
   g_pads[pad].ident = g_settings.input.device_names[pad];
   input_config_autoconfigure_joypad(pad, NULL, NULL);
}

static bool add_pad(unsigned i, int fd, const char *path)
{
   struct udev_joypad *pad = &g_pads[i];
   if (ioctl(fd, EVIOCGNAME(sizeof(g_settings.input.device_names[0])), pad->ident) < 0)
   {
      RARCH_LOG("[udev]: Failed to get pad name.\n");
      return false;
   }

   RARCH_LOG("[udev]: Plugged pad: %s on port #%u.\n", pad->ident, i);

   struct stat st;
   if (fstat(fd, &st) < 0)
      return false;

   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};

   if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
            (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      return false;

   // Go through all possible keycodes, check if they are used,
   // and map them to button/axes/hat indices.
   unsigned buttons = 0;
   unsigned axes = 0;
   for (int i = BTN_JOYSTICK; i < KEY_MAX && buttons < NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (int i = BTN_MISC; i < BTN_JOYSTICK; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (int i = 0; i < ABS_MISC && axes < NUM_AXES; i++)
   {
      // Skip hats for now.
      if (i == ABS_HAT0X)
      {
         i = ABS_HAT3Y;
         continue;
      }
      
      if (test_bit(i, absbit))
      {
         struct input_absinfo *abs = &pad->absinfo[axes];
         if (ioctl(fd, EVIOCGABS(i), abs) < 0)
            continue;
         if (abs->maximum > abs->minimum)
         {
            pad->axes[axes] = compute_axis(abs, abs->value);
            pad->axes_bind[i] = axes++;
         }
      }
   }

   pad->device = st.st_rdev;
   pad->fd = fd;
   pad->path = strdup(path);
   if (*pad->ident)
      input_config_autoconfigure_joypad(i, pad->ident, "udev");

   // Check for rumble features.
   unsigned long ffbit[NBITS(FF_MAX)] = {0};
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0)
   {
      if (test_bit(FF_RUMBLE, ffbit))
         RARCH_LOG("[udev]: Pad #%u (%s) supports force feedback.\n",
               i, path);

      if (ioctl(fd, EVIOCGEFFECTS, &pad->num_effects) >= 0)
         RARCH_LOG("[udev]: Pad #%u (%s) supports %d force feedback effects.\n", i, path, pad->num_effects);
   }

   return true;
}

static void check_device(const char *path, bool hotplugged)
{
   struct stat st;
   if (stat(path, &st) < 0)
      return;

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (st.st_rdev == g_pads[i].device)
      {
         RARCH_LOG("[udev]: Device ID %u is already plugged.\n", (unsigned)st.st_rdev);
         return;
      }
   }

   int pad = find_vacant_pad();
   if (pad < 0)
      return;

   int fd = open_joystick(path);
   if (fd < 0)
      return;

   if (add_pad(pad, fd, path))
   {
#ifndef IS_JOYCONFIG
      if (hotplugged)
      {
         char msg[512];
         snprintf(msg, sizeof(msg), "Joypad #%u (%s) connected.", pad, path);
         msg_queue_push(g_extern.msg_queue, msg, 0, 60);
         RARCH_LOG("[udev]: %s\n", msg);
      }
#else
      (void)hotplugged;
#endif

#if 0
      test_initial_rumble(fd, path);
#endif
   }
   else
   {
      RARCH_ERR("[udev]: Failed to add pad: %s.\n", path);
      close(fd);
   }
}

static void remove_device(const char *path)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].path && !strcmp(g_pads[i].path, path))
      {
#ifndef IS_JOYCONFIG
         char msg[512];
         snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.", i, g_pads[i].ident);
         msg_queue_push(g_extern.msg_queue, msg, 0, 60);
         RARCH_LOG("[udev]: %s\n", msg);
#endif
         free_pad(i);
         break;
      }
   }
}

static void udev_joypad_destroy(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      free_pad(i);

   if (g_udev_mon)
      udev_monitor_unref(g_udev_mon);
   g_udev_mon = NULL;
   if (g_udev)
      udev_unref(g_udev);
   g_udev = NULL;
}

static bool udev_joypad_init(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      g_pads[i].fd = -1;
      g_pads[i].ident = g_settings.input.device_names[i];
   }

   struct udev_list_entry *devs = NULL;
   struct udev_list_entry *item = NULL;

   g_udev = udev_new();
   if (!g_udev)
      return false;

   g_udev_mon = udev_monitor_new_from_netlink(g_udev, "udev");
   if (g_udev_mon)
   {
      udev_monitor_filter_add_match_subsystem_devtype(g_udev_mon, "input", NULL);
      udev_monitor_enable_receiving(g_udev_mon);
   }

   struct udev_enumerate *enumerate = udev_enumerate_new(g_udev);
   if (!enumerate)
      goto error;

   udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);
   for (struct udev_list_entry *item = devs; item; item = udev_list_entry_get_next(item))
   {
      const char *name = udev_list_entry_get_name(item);
      struct udev_device *dev = udev_device_new_from_syspath(g_udev, name);
      const char *devnode = udev_device_get_devnode(dev);
      if (devnode)
         check_device(devnode, false);
      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);
   return true;

error:
   if (enumerate)
      udev_enumerate_unref(enumerate);
   udev_joypad_destroy();
   return false;
}

static bool udev_joypad_hat(const struct udev_joypad *pad, uint16_t hat)
{
   unsigned h = GET_HAT(hat);
   if (h >= NUM_HATS)
      return false;

   switch (GET_HAT_DIR(hat))
   {
      case HAT_LEFT_MASK: return pad->hats[h][0] < 0;
      case HAT_RIGHT_MASK: return pad->hats[h][0] > 0;
      case HAT_UP_MASK: return pad->hats[h][1] < 0;
      case HAT_DOWN_MASK: return pad->hats[h][1] > 0;
      default: return 0;
   }
}

static bool udev_joypad_button(unsigned port, uint16_t joykey)
{
   const struct udev_joypad *pad = &g_pads[port];

   if (GET_HAT_DIR(joykey))
      return udev_joypad_hat(pad, joykey);
   else
      return joykey < NUM_BUTTONS && pad->buttons[joykey];
}

static int16_t udev_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct udev_joypad *pad = &g_pads[port];

   int16_t val = 0;
   if (AXIS_NEG_GET(joyaxis) < NUM_AXES)
   {
      val = pad->axes[AXIS_NEG_GET(joyaxis)];
      if (val > 0)
         val = 0;
   }
   else if (AXIS_POS_GET(joyaxis) < NUM_AXES)
   {
      val = pad->axes[AXIS_POS_GET(joyaxis)];
      if (val < 0)
         val = 0;
   }

   return val;
}

static bool udev_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].fd >= 0;
}

static const char *udev_joypad_name(unsigned pad)
{
   if (pad >= MAX_PLAYERS)
      return NULL;

   return *g_pads[pad].ident ? g_pads[pad].ident : NULL;
}

const rarch_joypad_driver_t udev_joypad = {
   udev_joypad_init,
   udev_joypad_query_pad,
   udev_joypad_destroy,
   udev_joypad_button,
   udev_joypad_axis,
   udev_joypad_poll,
   udev_set_rumble,
   udev_joypad_name,
   "udev",
};

