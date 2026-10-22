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
#include "../common/hid_report_desc.h"
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
   bool usb;
   bool bluetooth;
   char devnode[64];
   char name[NAME_MAX_LENGTH];
   uint8_t data[2][HIDRAW_REPORT_MAX];
   /* a pad with no handler, read from its report descriptor */
   bool generic;
   hid_desc_layout_t layout;
   hid_desc_state_t state;
   input_pad_view_t view;  /* the state, as it is lent */
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

/* Every pad with a handler: hidraw hands a report over as the device
 * sent it, its ID first where it has one, which is how the handlers
 * read them. A pad with none is read from its report descriptor. */
static bool hidraw_hid_handled(uint16_t vid, uint16_t pid, const char *name)
{
   return find_connection_entry(vid, pid, name) != NULL;
}

/* The ids and name the node's HID device reports through sysfs, read
 * before the node is opened. */
static bool hidraw_hid_uevent(const char *node, unsigned *bus,
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
      unsigned v, p;
      if (     !strncmp(line, "HID_ID=", STRLEN_CONST("HID_ID="))
            && sscanf(line + STRLEN_CONST("HID_ID="), "%x:%x:%x",
               bus, &v, &p) == 3)
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

/* The node's report descriptor, from sysfs - readable without the
 * node's own permissions - made into @layout. False for no pad. */
static bool hidraw_hid_descriptor(const char *node, hid_desc_layout_t *layout)
{
   char path[384];
   uint8_t desc[4096];
   ssize_t len;
   int fd;

   snprintf(path, sizeof(path), "/sys/class/hidraw/%s/device/report_descriptor",
         node);
   if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
      return false;
   len = read(fd, desc, sizeof(desc));
   close(fd);
   return len > 0 && hid_desc_parse(layout, desc, (size_t)len);
}

/* A pad with no handler, as one: its report decoded at the poll into
 * the device's state, which the getters read. */
static void *hidraw_generic_init(void *data, uint32_t slot,
      hid_driver_t *driver)
{
   return data;
}

static void hidraw_generic_deinit(void *data) { }

static void hidraw_generic_packet(void *data, uint8_t *packet, uint16_t size)
{
   hidraw_device_t *dev = (hidraw_device_t*)data;
   unsigned i;
   if (!dev)
      return;
   hid_desc_decode(&dev->layout, packet, size, &dev->state);
   pad_view_set_buttons(&dev->view, dev->state.buttons, false);
   for (i = 0; i < HID_DESC_AXES; i++)
      dev->view.axes[i] = dev->state.axes[i];
   dev->view.hats = (uint16_t)(
           (dev->state.hat_y < 0 ? 1 : 0) | (dev->state.hat_y > 0 ? 2 : 0)
         | (dev->state.hat_x < 0 ? 4 : 0) | (dev->state.hat_x > 0 ? 8 : 0));
}

static void hidraw_generic_rumble(void *data,
      enum retro_rumble_effect effect, uint16_t strength) { }

static void hidraw_generic_get_buttons(void *data, input_bits_t *state)
{
   hidraw_device_t *dev = (hidraw_device_t*)data;
   if (dev)
      *state = dev->view.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t hidraw_generic_axis(void *data, unsigned axis)
{
   hidraw_device_t *dev = (hidraw_device_t*)data;
   return (dev && axis < HID_DESC_AXES) ? dev->view.axes[axis] : 0;
}

static const char *hidraw_generic_name(void *data)
{
   hidraw_device_t *dev = (hidraw_device_t*)data;
   return dev ? dev->name : NULL;
}

static int32_t hidraw_generic_button(void *data, uint16_t joykey)
{
   hidraw_device_t *dev = (hidraw_device_t*)data;
   return (dev && joykey < 32)
      ? (int32_t)((dev->state.buttons >> joykey) & 1) : 0;
}

static const input_pad_view_t *hidraw_generic_view(void *data)
{
   return &((hidraw_device_t*)data)->view;
}

static pad_connection_interface_t hidraw_generic_iface = {
   hidraw_generic_init,
   hidraw_generic_deinit,
   hidraw_generic_packet,
   hidraw_generic_rumble,
   hidraw_generic_get_buttons,
   hidraw_generic_axis,
   hidraw_generic_name,
   hidraw_generic_button,
   false, /* multi_pad */
   0,     /* max_pad */
   NULL,  /* pad_init */
   NULL,  /* pad_deinit */
   NULL,  /* status */
   NULL,  /* joypad */
   NULL,  /* set_player */
   hidraw_generic_view
};

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
   unsigned bus;
   char name[NAME_MAX_LENGTH];
   const char *node = strrchr(devnode, '/');
   hidraw_device_t *dev;

   node = node ? node + 1 : devnode;
   if (     strncmp(node, "hidraw", STRLEN_CONST("hidraw"))
         || hidraw_hid_find(hid, devnode)
         || !hidraw_hid_uevent(node, &bus, &vid, &pid, name, sizeof(name)))
      return;

   if (!(dev = (hidraw_device_t*)calloc(1, sizeof(*dev))))
      return;
   if (!hidraw_hid_handled(vid, pid, name))
   {
      if (!hidraw_hid_descriptor(node, &dev->layout))
      {
         free(dev);
         return;
      }
      dev->generic = true;
   }
   dev->slot = -1;
   dev->vid  = vid;
   dev->pid  = pid;
   dev->usb       = (bus == 0x03); /* BUS_USB */
   dev->bluetooth = (bus == 0x05); /* BUS_BLUETOOTH */
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
   if (dev->generic)
   {
      joypad_connection_entry_t gen_entry;
      gen_entry.name  = dev->name;
      gen_entry.vid   = vid;
      gen_entry.pid   = pid;
      gen_entry.iface = &hidraw_generic_iface;
      dev->slot       = pad_connection_pad_init_entry(hid->slots, &gen_entry,
            dev, &hidraw_hid);
   }
   else
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

/* A pad let go: @gone when it left, and is said to have; not when the
 * driver is only stopping, as every joypad driver does at a restart -
 * the pad is still there, and is reported again when one starts. */
static void hidraw_hid_remove(hidraw_hid_t *hid, hidraw_device_t *dev,
      bool gone)
{
   hidraw_device_t **link;

   for (link = &hid->devices; *link; link = &(*link)->next)
   {
      if (*link != dev)
         continue;
      *link = dev->next;
      if (gone)
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
               hidraw_hid_remove(hid, dev, true);
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
      hidraw_hid_remove(hid, hid->devices, false);
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
      else
         pad_connection_idle(&hid->slots[dev->slot]);
      if (len < 0 && errno != EAGAIN && errno != EINTR)
      {
         RARCH_LOG("[hidraw] \"%s\" at %s is gone (%s).\n",
               dev->name, dev->devnode, strerror(errno));
         hidraw_hid_remove(hid, dev, true);
      }
   }
}

/* Lent: the packets reach the handlers in the poll, and nowhere else. */
static const input_pad_view_t *hidraw_hid_joypad_view(void *data,
      unsigned pad)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return NULL;
   return pad_connection_view(&hid->slots[pad]);
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
   unsigned hat_dir;

   if (port >= MAX_USERS)
      return 0;
   /* the hat of a pad read from its descriptor; handlers give theirs
    * as the RetroPad's D-pad */
   if ((hat_dir = GET_HAT_DIR(joykey)))
   {
      hidraw_hid_t *hid = (hidraw_hid_t*)data;
      const input_pad_view_t *view;
      if (     !hid || GET_HAT(joykey) != 0
            || !(view = pad_connection_view(&hid->slots[port])))
         return 0;
      switch (hat_dir)
      {
         case HAT_UP_MASK:    return (view->hats & 1) != 0;
         case HAT_DOWN_MASK:  return (view->hats & 2) != 0;
         case HAT_LEFT_MASK:  return (view->hats & 4) != 0;
         case HAT_RIGHT_MASK: return (view->hats & 8) != 0;
      }
      return 0;
   }
   /* every one get_buttons() says: a home button's menu toggle is
    * past the first 32 */
   if (joykey >= 256)
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
   /* the sticks, then what a handler has past them: a DualShock 3's
    * pressures */
   if (AXIS_NEG_GET(joyaxis) < 16)
   {
      int16_t val = pad_connection_get_axis(&hid->slots[port],
            port, AXIS_NEG_GET(joyaxis));
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < 16)
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

static void hidraw_hid_set_player(void *data, unsigned pad, unsigned player)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (!hid || pad >= MAX_USERS)
      return;
   pad_connection_set_player(&hid->slots[pad], player);
}

/* The handle is the pad's device. Reports go with their ID first. */
static void hidraw_hid_send_control(void *handle, uint8_t *s, size_t len)
{
   hidraw_device_t *dev = (hidraw_device_t*)handle;
   if (dev && write(dev->fd, s, len) < 0)
      RARCH_DBG("[hidraw] Write to %s failed (%s).\n",
            dev->devnode, strerror(errno));
}

static bool hidraw_hid_is_bluetooth(void *handle)
{
   hidraw_device_t *dev = (hidraw_device_t*)handle;
   return dev && dev->bluetooth;
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

/* A DualShock 3 on USB keeps the address it connects to over
 * Bluetooth in feature report 0xF5, from its third byte. */
static hidraw_device_t *hidraw_hid_usb_pad(void *data, unsigned pad)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   hidraw_device_t *dev;
   if (!hid)
      return NULL;
   for (dev = hid->devices; dev; dev = dev->next)
      if (dev->slot == (int32_t)pad)
         return dev->usb ? dev : NULL;
   return NULL;
}

static bool hidraw_hid_get_bt_host(void *data, unsigned pad, uint8_t *addr)
{
   uint8_t buf[8];
   hidraw_device_t *dev = hidraw_hid_usb_pad(data, pad);

   if (!dev)
      return false;
   memset(buf, 0, sizeof(buf));
   buf[0] = 0xf5;
   if (ioctl(dev->fd, HIDIOCGFEATURE(sizeof(buf)), buf) < (int)sizeof(buf))
      return false;
   memcpy(addr, buf + 2, 6);
   return true;
}

static bool hidraw_hid_set_bt_host(void *data, unsigned pad,
      const uint8_t *addr)
{
   uint8_t buf[8];
   hidraw_device_t *dev = hidraw_hid_usb_pad(data, pad);

   if (!dev)
      return false;
   buf[0] = 0xf5;
   buf[1] = 0x00;
   memcpy(buf + 2, addr, 6);
   return ioctl(dev->fd, HIDIOCSFEATURE(sizeof(buf)), buf) >= 0;
}

/* A pad's motion sensors, where its handler reads them. */
static bool hidraw_hid_set_sensor_state(void *data, unsigned pad,
      enum retro_sensor_action action, unsigned rate)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   (void)rate;
   if (!hid || pad >= MAX_USERS)
      return false;
   return pad_connection_set_sensor(&hid->slots[pad], action);
}

static bool hidraw_hid_get_sensor_input(void *data, unsigned pad,
      unsigned id, float *value)
{
   hidraw_hid_t *hid = (hidraw_hid_t*)data;
   if (!hid || pad >= MAX_USERS || !value)
      return false;
   return pad_connection_sensor(&hid->slots[pad], id, value);
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
   NULL, /* read */
   hidraw_hid_get_bt_host,
   hidraw_hid_set_bt_host,
   hidraw_hid_is_bluetooth,
   hidraw_hid_set_player,
   hidraw_hid_joypad_view,
   hidraw_hid_set_sensor_state,
   hidraw_hid_get_sensor_input
};
