/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2026 - Daniel De Matteis
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

/* Pads reached through Linux's hidraw nodes, beside the kernel's own
 * driver for them: nothing is detached. Reports are read without
 * blocking at each poll, on the thread that polls, so the pads' state
 * is written and read on one thread; there is no thread here and no
 * lock. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hidraw.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_UDEV
#include <poll.h>
#include <libudev.h>
#endif

#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../connect/joypad_connection.h"
#include "../input_defines.h"
#include "../../tasks/tasks_internal.h"
#include "../input_driver.h"
#include "../../verbosity.h"

#define HIDRAW_REPORT_MAX 256
/* reports taken from a pad in one poll at most */
#define HIDRAW_READS_MAX  64

typedef struct hidraw_device
{
   struct hidraw_device *next;
   int fd;
   int32_t slot;
   uint16_t vid;
   uint16_t pid;
   char devnode[64];
   char name[NAME_MAX_LENGTH];
   uint8_t data[2][HIDRAW_REPORT_MAX];
} hidraw_device_t;

typedef struct hidraw_hid
{
   joypad_connection_t *slots;
   hidraw_device_t *devices;
#ifdef HAVE_UDEV
   struct udev *udev;
   struct udev_monitor *monitor;
#endif
} hidraw_hid_t;

extern hid_driver_t hidraw_hid;

/* The pads whose handlers frame their reports as hidraw does: the
 * report ID first. */
static bool hidraw_hid_handled(uint16_t vid, uint16_t pid, const char *name)
{
   joypad_connection_entry_t *entry = find_connection_entry(vid, pid, name);
   return entry && entry->iface == &pad_connection_ps3;
}

/* The ids and name the node's HID device reports through sysfs, read
 * before the node is opened. */
static bool hidraw_hid_uevent(const char *node,
      uint16_t *vid, uint16_t *pid, char *name, size_t name_len)
{
   char path[384];
   char buf[1024];
   char *line, *save = NULL;
   ssize_t len;
   int fd;
   bool found = false;

   snprintf(path, sizeof(path), "/sys/class/hidraw/%s/device/uevent", node);
   if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
      return false;
   len = read(fd, buf, sizeof(buf) - 1);
   close(fd);
   if (len <= 0)
      return false;
   buf[len] = '\0';
   *name    = '\0';

   for (line = strtok_r(buf, "\n", &save); line;
         line = strtok_r(NULL, "\n", &save))
   {
      unsigned bus, v, p;
      if (     !strncmp(line, "HID_ID=", STRLEN_CONST("HID_ID="))
            && sscanf(line + STRLEN_CONST("HID_ID="), "%x:%x:%x",
               &bus, &v, &p) == 3)
      {
         *vid  = (uint16_t)v;
         *pid  = (uint16_t)p;
         found = true;
      }
      else if (!strncmp(line, "HID_NAME=", STRLEN_CONST("HID_NAME=")))
         strlcpy(name, line + STRLEN_CONST("HID_NAME="), name_len);
   }
   return found;
}

static hidraw_device_t *hidraw_hid_find(hidraw_hid_t *hid,
      const char *devnode)
{
   hidraw_device_t *dev;
   for (dev = hid->devices; dev; dev = dev->next)
      if (string_is_equal(dev->devnode, devnode))
         return dev;
   return NULL;
}

static void hidraw_hid_add(hidraw_hid_t *hid, const char *devnode)
{
   uint16_t vid, pid;
   char name[NAME_MAX_LENGTH];
   const char *node = strrchr(devnode, '/');
   hidraw_device_t *dev;

   node = node ? node + 1 : devnode;
   if (     strncmp(node, "hidraw", STRLEN_CONST("hidraw"))
         || hidraw_hid_find(hid, devnode)
         || !hidraw_hid_uevent(node, &vid, &pid, name, sizeof(name))
         || !hidraw_hid_handled(vid, pid, name))
      return;

   if (!(dev = (hidraw_device_t*)calloc(1, sizeof(*dev))))
      return;
   dev->slot = -1;
   dev->vid  = vid;
   dev->pid  = pid;
   strlcpy(dev->devnode, devnode, sizeof(dev->devnode));
   strlcpy(dev->name, name, sizeof(dev->name));

   if ((dev->fd = open(devnode, O_RDWR | O_NONBLOCK | O_CLOEXEC)) < 0)
   {
      RARCH_WARN("[hidraw] \"%s\" at %s cannot be opened (%s).\n",
            name, devnode, strerror(errno));
      free(dev);
      return;
   }

   /* the pad's handler starts it here, through set_report */
   dev->slot = pad_connection_pad_init(hid->slots, name, vid, pid,
         dev, &hidraw_hid);
   if (dev->slot < 0 || !hid->slots[dev->slot].connection)
   {
      RARCH_ERR("[hidraw] \"%s\" at %s could not be started.\n",
            name, devnode);
      if (dev->slot >= 0)
         pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      close(dev->fd);
      free(dev);
      return;
   }

   dev->next    = hid->devices;
   hid->devices = dev;
   RARCH_LOG("[hidraw] \"%s\" at %s on port %d.\n",
         name, devnode, dev->slot + 1);
   input_autoconfigure_connect(name, NULL, NULL, "hid",
         dev->slot, vid, pid);
}

static void hidraw_hid_remove(hidraw_hid_t *hid, hidraw_device_t *dev)
{
   hidraw_device_t **link;

   for (link = &hid->devices; *link; link = &(*link)->next)
   {
      if (*link != dev)
         continue;
      *link = dev->next;
      input_autoconfigure_disconnect(dev->slot, dev->name);
      pad_connection_pad_deinit(&hid->slots[dev->slot], dev->slot);
      close(dev->fd);
      free(dev);
      return;
   }
}

static void hidraw_hid_scan(hidraw_hid_t *hid)
{
   struct dirent *entry;
   DIR *dir = opendir("/dev");

   if (!dir)
      return;
   while ((entry = readdir(dir)))
   {
      char devnode[sizeof(entry->d_name) + 5];
      if (strncmp(entry->d_name, "hidraw", STRLEN_CONST("hidraw")))
         continue;
      snprintf(devnode, sizeof(devnode), "/dev/%s", entry->d_name);
      hidraw_hid_add(hid, devnode);
   }
   closedir(dir);
}

#ifdef HAVE_UDEV
static void hidraw_hid_hotplug(hidraw_hid_t *hid)
{
   struct pollfd fds;

   fds.fd     = udev_monitor_get_fd(hid->monitor);
   fds.events = POLLIN;

   for (;;)
   {
      struct udev_device *udev_dev;
      const char *action, *devnode;

      fds.revents = 0;
      if (poll(&fds, 1, 0) != 1 || !(fds.revents & POLLIN))
         break;
      if (!(udev_dev = udev_monitor_receive_device(hid->monitor)))
         continue;
      action  = udev_device_get_action(udev_dev);
      devnode = udev_device_get_devnode(udev_dev);
      if (devnode && action)
      {
         hidraw_device_t *dev = hidraw_hid_find(hid, devnode);
         if (string_is_equal(action, "remove"))
         {
            if (dev)
               hidraw_hid_remove(hid, dev);
         }
         else if (!dev)
            hidraw_hid_add(hid, devnode);
      }
      udev_device_unref(udev_dev);
   }
}
#endif

static void hidraw_hid_free(const void *data)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;

   if (!hid)
      return;
   while (hid->devices)
      hidraw_hid_remove(hid, hid->devices);
#ifdef HAVE_UDEV
   if (hid->monitor)
      udev_monitor_unref(hid->monitor);
   if (hid->udev)
      udev_unref(hid->udev);
#endif
   if (hid->slots)
      pad_connection_destroy(hid->slots);
   free(hid);
}

static void *hidraw_hid_init(void)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)calloc(1, sizeof(*hid));

   if (!hid)
      return NULL;
   if (!(hid->slots = pad_connection_init(MAX_USERS)))
   {
      free(hid);
      return NULL;
   }
#ifdef HAVE_UDEV
   /* listening before the scan, so a pad plugged in during it is not
    * missed */
   if (     (hid->udev    = udev_new())
         && (hid->monitor = udev_monitor_new_from_netlink(hid->udev, "udev")))
   {
      udev_monitor_filter_add_match_subsystem_devtype(hid->monitor,
            "hidraw", NULL);
      udev_monitor_enable_receiving(hid->monitor);
   }
#endif
   hidraw_hid_scan(hid);
   return hid;
}

static void hidraw_hid_poll(void *data)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   hidraw_device_t *dev, *next;

   if (!hid)
      return;
#ifdef HAVE_UDEV
   if (hid->monitor)
      hidraw_hid_hotplug(hid);
#endif

   for (dev = hid->devices; dev; dev = next)
   {
      ssize_t len     = 0;
      ssize_t got_len = 0;
      unsigned n;

      next = dev->next;
      /* the newest report is the pad's state */
      for (n = 0; n < HIDRAW_READS_MAX; n++)
      {
         if ((len = read(dev->fd, dev->data[n & 1], HIDRAW_REPORT_MAX)) <= 0)
            break;
         got_len = len;
      }
      if (got_len > 0)
         pad_connection_packet(&hid->slots[dev->slot], dev->slot,
               dev->data[(n - 1) & 1], (uint32_t)got_len);
      if (len < 0 && errno != EAGAIN && errno != EINTR)
      {
         RARCH_LOG("[hidraw] \"%s\" at %s is gone (%s).\n",
               dev->name, dev->devnode, strerror(errno));
         hidraw_hid_remove(hid, dev);
      }
   }
}

static bool hidraw_hid_joypad_query(void *data, unsigned pad)
{
   return pad < MAX_USERS;
}

static const char *hidraw_hid_joypad_name(void *data, unsigned pad)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return NULL;
   return pad_connection_get_name(&hid->slots[pad], pad);
}

static void hidraw_hid_joypad_get_buttons(void *data, unsigned port,
      input_bits_t *state)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (hid && port < MAX_USERS)
      pad_connection_get_buttons(&hid->slots[port], port, state);
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidraw_hid_joypad_button(void *data,
      unsigned port, uint16_t joykey)
{
   input_bits_t buttons;

   if (port >= MAX_USERS || GET_HAT_DIR(joykey) || joykey >= 32)
      return 0;
   hidraw_hid_joypad_get_buttons(data, port, &buttons);
   return BIT256_GET(buttons, joykey) != 0;
}

static int16_t hidraw_hid_joypad_axis(void *data,
      unsigned port, uint32_t joyaxis)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;

   if (!hid || port >= MAX_USERS)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_NEG_GET(joyaxis));
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_POS_GET(joyaxis));
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t hidraw_hid_joypad_state(
      void *data,
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   unsigned i;
   int16_t ret       = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      if (     joykeys[i] != NO_BTN
            && hidraw_hid_joypad_button(data, port_idx, joykeys[i]))
         ret |= (1 << i);
      else if (joyaxes[i] != AXIS_NONE
            && ((float)abs(hidraw_hid_joypad_axis(data, port_idx, joyaxes[i]))
               / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

static bool hidraw_hid_joypad_rumble(void *data, unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return false;
   return pad_connection_rumble(&hid->slots[pad], pad, effect, strength);
}

/* The handle is the pad's device. Reports go with their ID first. */
static void hidraw_hid_send_control(void *handle, uint8_t *s, size_t len)
{
   hidraw_device_t *dev = (hidraw_device_t*)handle;
   if (dev && write(dev->fd, s, len) < 0)
      RARCH_DBG("[hidraw] Write to %s failed (%s).\n",
            dev->devnode, strerror(errno));
}

static int32_t hidraw_hid_set_report(void *handle, uint8_t report_type,
      uint8_t report_id, uint8_t *s, size_t len)
{
   hidraw_device_t *dev = (hidraw_device_t*)handle;

   if (!dev || !len || s[0] != report_id)
      return -1;
   switch (report_type)
   {
      case HID_REPORT_FEATURE:
         return ioctl(dev->fd, HIDIOCSFEATURE(len), s);
      case HID_REPORT_OUTPUT:
         return (int32_t)write(dev->fd, s, len);
      default:
         break;
   }
   return -1;
}

static int32_t hidraw_hid_get_report(void *handle, uint8_t report_type,
      uint8_t report_id, uint8_t *s, size_t len)
{
   hidraw_device_t *dev = (hidraw_device_t*)handle;

   if (!dev || !len || report_type != HID_REPORT_FEATURE)
      return -1;
   s[0] = report_id;
   return ioctl(dev->fd, HIDIOCGFEATURE(len), s);
}

hid_driver_t hidraw_hid = {
   hidraw_hid_init,
   hidraw_hid_joypad_query,
   hidraw_hid_free,
   hidraw_hid_joypad_button,
   hidraw_hid_joypad_state,
   hidraw_hid_joypad_get_buttons,
   hidraw_hid_joypad_axis,
   hidraw_hid_poll,
   hidraw_hid_joypad_rumble,
   hidraw_hid_joypad_name,
   "hidraw",
   hidraw_hid_send_control,
   hidraw_hid_set_report,
   hidraw_hid_get_report,
   NULL, /* set_idle */
   NULL, /* set_protocol */
   NULL  /* read */
};
