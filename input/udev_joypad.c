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
#include <fcntl.h>
#include <libudev.h>
#include <linux/types.h>
#include <linux/joystick.h>

// Udev/Evdev Linux joypad driver.
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
   uint16_t hats[NUM_HATS];

   // Maps keycodes -> button/axes
   uint8_t button_bind[KEY_MAX];
   uint8_t axes_bind[ABS_MAX];
   struct input_absinfo absinfo[NUM_AXES];

   char ident[256];
};

static struct udev *g_udev;
static struct udev_monitor *g_udev_mon;
static struct udev_joypad g_pads[MAX_PLAYERS];

/*
static bool udev_joypad_init_pad(const char *path, struct udev_joypad *pad)
{
   if (pad->fd >= 0)
      return false;

   // Device can have just been created, but not made accessible (yet).
   // IN_ATTRIB will signal when permissions change.
   if (access(path, R_OK) < 0)
      return false;

   pad->fd = open(path, O_RDONLY | O_NONBLOCK);

   *pad->ident = '\0';
   if (pad->fd >= 0)
   {
      if (ioctl(pad->fd, JSIOCGNAME(sizeof(g_settings.input.device_names[0])), pad->ident) >= 0)
      {
         RARCH_LOG("[Joypad]: Found pad: %s on %s.\n", pad->ident, path);

         if (g_hotplug)
         {
            char msg[512];
            snprintf(msg, sizeof(msg), "Joypad #%u (%s) connected.", (unsigned)(pad - g_pads), pad->ident);
#ifndef IS_JOYCONFIG
            msg_queue_push(g_extern.msg_queue, msg, 0, 60);
#endif
         }
      }

      else
         RARCH_ERR("[Joypad]: Didn't find ident of %s.\n", path);

      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.ptr = pad;
      epoll_ctl(g_epoll, EPOLL_CTL_ADD, pad->fd, &event);
      return true;
   }
   else
   {
      RARCH_ERR("[Joypad]: Failed to open pad %s (error: %s).\n", path, strerror(errno));
      return false;
   }
}

static void handle_plugged_pad(void)
{
   size_t event_size = sizeof(struct inotify_event) + NAME_MAX + 1;
   uint8_t *event_buf = (uint8_t*)calloc(1, event_size);
   if (!event_buf)
      return;

   int rc;
   while ((rc = read(g_notify, event_buf, event_size)) >= 0)
   {
      struct inotify_event *event = NULL;
      // Can read multiple events in one read() call.
      for (int i = 0; i < rc; i += event->len + sizeof(struct inotify_event))
      {
         event = (struct inotify_event*)&event_buf[i];

         if (strstr(event->name, "js") != event->name)
            continue;

         unsigned index = strtoul(event->name + 2, NULL, 0);
         if (index >= MAX_PLAYERS)
            continue;

         if (event->mask & IN_DELETE)
         {
            if (g_pads[index].fd >= 0)
            {
               if (g_hotplug)
               {
                  char msg[512];
                  snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.", index, g_pads[index].ident);
#ifndef IS_JOYCONFIG
                  msg_queue_push(g_extern.msg_queue, msg, 0, 60);
#endif
               }

               RARCH_LOG("[Joypad]: Joypad %s disconnected.\n", g_pads[index].ident);
               close(g_pads[index].fd);
               memset(g_pads[index].buttons, 0, sizeof(g_pads[index].buttons));
               memset(g_pads[index].axes, 0, sizeof(g_pads[index].axes));
               g_pads[index].fd = -1;
               *g_pads[index].ident = '\0';

               input_config_autoconfigure_joypad(index, NULL, NULL);
            }
         }
         // Sometimes, device will be created before acess to it is established.
         else if (event->mask & (IN_CREATE | IN_ATTRIB))
         {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), "/dev/input/%s", event->name);
            bool ret = udev_joypad_init_pad(path, &g_pads[index]);

            if (*g_pads[index].ident && ret)
               input_config_autoconfigure_joypad(index, g_pads[index].ident, "udev");
         }
      }
   }

   free(event_buf);
}
*/

static void handle_hat(struct udev_joypad *pad,
      unsigned hat, unsigned axis, int value)
{
}

static int16_t compute_axis(const struct input_absinfo *info, int value)
{
   return 0;
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
                     handle_hat(pad, code / 2, code % 2, events[i].value);
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

static void udev_joypad_poll(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      poll_pad(i);
}

#define test_bit(nr, addr) \
   (((1UL << ((nr) % (sizeof(long) * CHAR_BIT))) & ((addr)[(nr) / (sizeof(long) * CHAR_BIT)])) != 0)
#define NBITS(x) ((((x) - 1) / (sizeof(long) * CHAR_BIT)) + 1)

static int open_joystick(const char *path)
{
   int fd = open(path, O_RDONLY | O_NONBLOCK);
   if (fd < 0)
      return fd;

   unsigned long evbit[NBITS(EV_MAX)] = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};

   if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      return false;

   // Has to at least support EV_KEY interface.
   if (!test_bit(EV_KEY, evbit))
      return false;

   return fd;
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
   memset(&g_pads[pad], 0, sizeof(g_pads[pad]));
   g_pads[pad].fd = -1;
}

static bool add_pad(unsigned i, int fd)
{
   struct udev_joypad *pad = &g_pads[i];
   if (ioctl(fd, EVIOCGNAME(sizeof(pad->ident)), pad->ident) < 0)
      return false;

   RARCH_LOG("[udev]: Found pad: %s on.\n", pad->ident);

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
         pad->axes_bind[i] = axes++;
      }
   }

   pad->device = st.st_rdev;
   pad->fd = fd;

   return true;
}

static void check_device(const char *path)
{
   struct stat st;
   if (stat(path, &st) < 0)
      return;

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      if (st.st_rdev == g_pads[i].device)
         return;

   int fd = open_joystick(path);
   if (fd < 0)
      return;

   int pad = find_vacant_pad();
   if (pad < 0)
   {
      close(fd);
      return;
   }

   if (add_pad(pad, fd))
   {
      // Autodetect.
   }
   else
      close(fd);
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
      free_pad(i);
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
      check_device(udev_device_get_devnode(dev));
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

static bool udev_joypad_button(unsigned port, uint16_t joykey)
{
   const struct udev_joypad *pad = &g_pads[port];

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
      // Kernel returns values in range [-0x7fff, 0x7fff].
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
   udev_joypad_name,
   "udev",
};

