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
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <libudev.h>
#ifdef __linux__
#include <linux/types.h>
#endif
#include <linux/input.h>

#include <retro_inline.h>
#include <compat/strl.h>
#include <string/stdstring.h>

#include "../input_driver.h"

#include "../../configuration.h"
#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

#include "../../verbosity.h"

/* Udev/evdev Linux joypad driver.
 * More complex and extremely low level,
 * but only Linux driver which can support joypad rumble.
 *
 * Uses udev for device detection + hotplug.
 *
 * Code adapted from SDL 2.0's implementation.
 */

#define UDEV_NUM_BUTTONS 64
#define NUM_AXES 32

#ifndef NUM_HATS
#define NUM_HATS 4
#endif

#define test_bit(nr, addr) \
   (((1UL << ((nr) % (sizeof(long) * CHAR_BIT))) & ((addr)[(nr) / (sizeof(long) * CHAR_BIT)])) != 0)
#define NBITS(x) ((((x) - 1) / (sizeof(long) * CHAR_BIT)) + 1)

#ifndef INPUT_PROP_ACCELEROMETER
#define INPUT_PROP_ACCELEROMETER 0x06
#endif

#define SENSOR_AXES 6 /* ABS_X..ABS_RZ on sensor node, maps 1:1 to RETRO_SENSOR_* 0-5 */
#define DEG_TO_RAD_F 0.017453293f

struct udev_joypad
{
   dev_t device;  /* TODO/FIXME - unsure of alignment */
   struct input_absinfo absinfo[NUM_AXES]; /* TODO/FIXME - unsure of alignment */

   uint64_t buttons;

   char *path;

   int fd;
   int num_effects;
   int effects[2]; /* [0] - strong, [1] - weak  */
   int32_t vid;
   int32_t pid;
   int16_t axes[NUM_AXES];
   int8_t hats[NUM_HATS][2];
   /* Maps keycodes -> button/axes */
   uint8_t button_bind[KEY_MAX];
   uint8_t axes_bind[ABS_MAX];
   uint16_t strength[2];
   uint16_t configured_strength[2];
   unsigned rumble_gain;

   char ident[NAME_MAX_LENGTH];
   char phys[NAME_MAX_LENGTH];
   bool has_set_ff[2];
   /* Deal with analog triggers that report -32767 to 32767 */
   bool neg_trigger[NUM_AXES];

   /* Sensor (IMU) support: sibling evdev node for accelerometer/gyroscope */
   int sensor_fd;
   char *sensor_path;
   struct input_absinfo sensor_absinfo[SENSOR_AXES];
   float sensor_data[SENSOR_AXES]; /* indexed by RETRO_SENSOR_* ID */
   bool sensor_accel_enabled;
   bool sensor_gyro_enabled;
   bool sensor_has_accel; /* sensor node has ABS_X/Y/Z or ABS_RX/RY/RZ */
   bool sensor_has_gyro;  /* sensor node has ABS_RX/RY/RZ */
   bool sensor_accel_on_rxyz_codes; /* wiimote: accel is reported on ABS_RX/RY/RZ instead of ABS_X/Y/Z. */
};

struct joypad_udev_entry
{
   const char *devnode;
   struct udev_list_entry *item;
};

/* TODO/FIXME - static globals */
static struct udev *udev_joypad_fd             = NULL;
static struct udev_monitor *udev_joypad_mon    = NULL;
static struct udev_joypad udev_pads[MAX_USERS];

static INLINE int16_t udev_compute_axis(const struct input_absinfo *info, int value)
{
   int range = info->maximum - info->minimum;
   int axis  = (value - info->minimum) * 0xffffll / range - 0x7fffll;

   if (axis > 0x7fff)
      return 0x7fff;
   else if (axis < -0x7fff)
      return -0x7fff;
   return axis;
}

static int udev_find_vacant_pad(void)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
      if (udev_pads[i].fd < 0)
         return i;
   return -1;
}

static int udev_open_joystick(const char *path)
{
   unsigned long evbit[NBITS(EV_MAX)]   = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};
   int fd = open(path, O_RDWR | O_NONBLOCK);

   if (fd < 0)
      return fd;

   if (  (ioctl(fd, EVIOCGBIT(0,      sizeof(evbit)),  evbit)  < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
         (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      goto error;

   /* Has to at least support EV_KEY interface. */
   if (!test_bit(EV_KEY, evbit))
      goto error;

   return fd;

error:
   close(fd);
   return -1;
}

#ifndef HAVE_LAKKA_SWITCH
static bool udev_set_rumble_gain(unsigned i, unsigned gain)
{
   struct input_event ie;
   struct udev_joypad *pad = (struct udev_joypad*)&udev_pads[i];

   /* Does not support > 100 gains */
   if ((pad->fd < 0) ||
       (gain > 100))
      return false;

   if (pad->rumble_gain == gain)
      return true;

   memset(&ie, 0, sizeof(ie));
   ie.type = EV_FF;
   ie.code = FF_GAIN;
   ie.value = 0xFFFF * (gain/100.0);

   if (write(pad->fd, &ie, sizeof(ie)) < (ssize_t)sizeof(ie))
   {
      RARCH_ERR("[udev] Failed to set rumble gain on pad #%u.\n", i);
      return false;
   }

   pad->rumble_gain = gain;

   return true;
}
#endif

static void udev_open_sensor_node(struct udev_joypad *pad,
      const char *devnode, unsigned p)
{
   unsigned long propbit[NBITS(INPUT_PROP_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)]         = {0};
   int fd = open(devnode, O_RDONLY | O_NONBLOCK);

   if (fd < 0)
      return;

   ioctl(fd, EVIOCGPROP(sizeof(propbit)), propbit);

   if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0)
   {
      int a;
      bool has_xyz  = test_bit(ABS_X,  absbit)
                   && test_bit(ABS_Y,  absbit)
                   && test_bit(ABS_Z,  absbit);
      bool has_rxyz = test_bit(ABS_RX, absbit)
                   && test_bit(ABS_RY, absbit)
                   && test_bit(ABS_RZ, absbit);

      if (!has_xyz && !has_rxyz)
      {
         /* Node has no usable sensor axes at all — skip it. */
         close(fd);
         return;
      }

      /* Standard layout: ABS_X/Y/Z = accel, ABS_RX/Y/Z = gyro.
       * Wiimote layout:  ABS_RX/Y/Z = accel only (no gyro, no ABS_X/Y/Z). */
      if (!has_xyz && has_rxyz)
      {
         /* Rotational-axis accelerometer (Wiimote).
          * Read absinfo from ABS_RX/Y/Z into sensor_absinfo 0/1/2 */
         pad->sensor_accel_on_rxyz_codes = true;
         pad->sensor_has_accel           = true;
         pad->sensor_has_gyro            = false;

         if (ioctl(fd, EVIOCGABS(ABS_RX), &pad->sensor_absinfo[0]) < 0)
            memset(&pad->sensor_absinfo[0], 0, sizeof(pad->sensor_absinfo[0]));
         if (ioctl(fd, EVIOCGABS(ABS_RY), &pad->sensor_absinfo[1]) < 0)
            memset(&pad->sensor_absinfo[1], 0, sizeof(pad->sensor_absinfo[1]));
         if (ioctl(fd, EVIOCGABS(ABS_RZ), &pad->sensor_absinfo[2]) < 0)
            memset(&pad->sensor_absinfo[2], 0, sizeof(pad->sensor_absinfo[2]));
      }
      else
      {
         /* Standard layout. */
         pad->sensor_accel_on_rxyz_codes = false;
         pad->sensor_has_accel           = has_xyz;
         pad->sensor_has_gyro            = has_rxyz;

         for (a = 0; a < SENSOR_AXES; a++)
         {
            if (test_bit(a, absbit))
               ioctl(fd, EVIOCGABS(a), &pad->sensor_absinfo[a]);
         }
      }
   }
   else
   {
      /* Could not read ABS capabilities — not a sensor. */
      close(fd);
      return;
   }

   pad->sensor_fd   = fd;
   pad->sensor_path = strdup(devnode);

   RARCH_LOG("[udev] Pad #%u: found sensor at %s "
         "(accel=%s%s, gyro=%s).\n",
         p, devnode,
         pad->sensor_has_accel ? "yes" : "no",
         pad->sensor_accel_on_rxyz_codes ? "[RX/RY/RZ]" : "",
         pad->sensor_has_gyro  ? "yes" : "no");
}

static void udev_find_sensor_sibling(struct udev_device *gamepad_dev,
      unsigned p)
{
   struct udev_enumerate *enumerate  = NULL;
   struct udev_list_entry *devs      = NULL;
   struct udev_list_entry *item      = NULL;
   struct udev_device *hid_parent    = NULL;
   const char *parent_syspath        = NULL;
   struct udev_joypad *pad           = &udev_pads[p];

   hid_parent = udev_device_get_parent_with_subsystem_devtype(
         gamepad_dev, "hid", NULL);
   if (!hid_parent)
   {
      RARCH_DBG("[udev] Pad #%u: no HID parent found for sensor search.\n", p);
      return;
   }

   parent_syspath = udev_device_get_syspath(hid_parent);
   if (!parent_syspath)
      return;

   RARCH_DBG("[udev] Pad #%u: searching for sensor sibling under %s\n",
         p, parent_syspath);

   enumerate = udev_enumerate_new(udev_joypad_fd);
   if (!enumerate)
      return;

   udev_enumerate_add_match_property(enumerate,
         "ID_INPUT_ACCELEROMETER", "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   if (!devs)
      RARCH_DBG("[udev] Pad #%u: no ID_INPUT_ACCELEROMETER devices found.\n", p);

   udev_list_entry_foreach(item, devs)
   {
      const char *name = udev_list_entry_get_name(item);
      struct udev_device *dev = udev_device_new_from_syspath(
            udev_joypad_fd, name);
      const char *devnode;
      struct udev_device *candidate_parent;
      const char *candidate_syspath;

      if (!dev)
         continue;

      devnode = udev_device_get_devnode(dev);
      if (!devnode)
      {
         udev_device_unref(dev);
         continue;
      }

      candidate_parent = udev_device_get_parent_with_subsystem_devtype(
            dev, "hid", NULL);
      if (!candidate_parent)
      {
         udev_device_unref(dev);
         continue;
      }

      candidate_syspath = udev_device_get_syspath(candidate_parent);
      if (  !candidate_syspath
         || !string_is_equal(parent_syspath, candidate_syspath))
      {
         udev_device_unref(dev);
         continue;
      }

      /* Found sibling sensor node */
      udev_open_sensor_node(pad, devnode, p);
      udev_device_unref(dev);
      break;
   }

   udev_enumerate_unref(enumerate);

   /* Fallback for devices like hid-wiimote whose accelerometer input node
    * is not tagged with ID_INPUT_ACCELEROMETER=1 by udev */
   if (pad->sensor_fd >= 0)
      return; /* primary scan already found it */

   RARCH_DBG("[udev] Pad #%u: no ID_INPUT_ACCELEROMETER device found, "
         "trying fallback scan under %s\n", p, parent_syspath);

   enumerate = udev_enumerate_new(udev_joypad_fd);
   if (!enumerate)
      return;

   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   udev_list_entry_foreach(item, devs)
   {
      const char *name         = udev_list_entry_get_name(item);
      struct udev_device *dev;
      const char *devnode;
      struct udev_device *candidate_parent;
      const char *candidate_syspath;

      /* Only look at eventNN nodes, not jsNN or the input parent itself */
      if (!strstr(name, "/event"))
         continue;

      dev = udev_device_new_from_syspath(udev_joypad_fd, name);
      if (!dev)
         continue;

      devnode = udev_device_get_devnode(dev);
      if (!devnode)
      {
         udev_device_unref(dev);
         continue;
      }

      /* Must share the same HID parent as the joypad */
      candidate_parent = udev_device_get_parent_with_subsystem_devtype(
            dev, "hid", NULL);
      if (!candidate_parent)
      {
         udev_device_unref(dev);
         continue;
      }

      candidate_syspath = udev_device_get_syspath(candidate_parent);
      if (  !candidate_syspath
         || !string_is_equal(parent_syspath, candidate_syspath))
      {
         udev_device_unref(dev);
         continue;
      }

      /* Skip the joypad's own event node */
      {
         struct stat st;
         if (stat(devnode, &st) == 0 && st.st_rdev == pad->device)
         {
            udev_device_unref(dev);
            continue;
         }
      }

      /* udev_open_sensor_node rejects nodes with no usable ABS axes */
      udev_open_sensor_node(pad, devnode, p);
      udev_device_unref(dev);

      if (pad->sensor_fd >= 0)
         break;
   }

   udev_enumerate_unref(enumerate);
}

static int udev_add_pad(struct udev_device *dev, unsigned p, int fd, const char *path)
{
   int i;
   struct stat st;
   int ret                              = 0;
   const char *buf                      = NULL;
   unsigned buttons                     = 0;
   unsigned axes                        = 0;
   struct udev_device *parent           = NULL;
   struct udev_joypad *pad              = (struct udev_joypad*)&udev_pads[p];
   struct input_id inputid              = {0};
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};
   unsigned long ffbit[NBITS(FF_MAX)]   = {0};
   const char *device_name              = input_config_get_device_name(p);
   size_t physlen                       = 0;

   if (string_is_empty(device_name))
      pad->ident[0] = '\0';
   else
      strlcpy(pad->ident, device_name, sizeof(pad->ident));

   /* Failed to get pad name */
   if (ioctl(fd, EVIOCGNAME(sizeof(pad->ident)), pad->ident) < 0)
      return -1;

   pad->vid = pad->pid = 0;

   if (ioctl(fd, EVIOCGID, &inputid) >= 0)
   {
      pad->vid = inputid.vendor;
      pad->pid = inputid.product;
   }
   if (ioctl(fd, EVIOCGPHYS(sizeof(pad->phys)), pad->phys) < 0)
      pad->phys[0] = '\0';  /* Clear if unavailable */
   else
      physlen = strlen(pad->phys);

   if (ioctl(fd, EVIOCGUNIQ(sizeof(pad->phys)-physlen), pad->phys+physlen) < 0)
       pad->phys[physlen] = '\0';  /* Clear if unavailable */

   if (fstat(fd, &st) < 0)
      return -1;

   if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
            (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0))
      return -1;

   /* Go through all possible keycodes, check if they are used,
    * and map them to button/axes/hat indices.
    */
   for (i = KEY_UP; i <= KEY_DOWN && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = BTN_MISC; i < KEY_MAX && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   /* The following two ranges are scanned and added after the above
    * ranges to maintain compatibility with existing key maps.
    */
   for (i = 0; i < KEY_UP && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = KEY_DOWN + 1; i < BTN_MISC && buttons < UDEV_NUM_BUTTONS; i++)
      if (test_bit(i, keybit))
         pad->button_bind[i] = buttons++;
   for (i = 0; i < ABS_MISC && axes < NUM_AXES; i++)
   {
      /* Skip hats for now. */
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
            pad->axes[axes]   = udev_compute_axis(abs, abs->value);
            /* Deal with analog triggers that report -32767 to 32767
               by testing if the axis initial value is negative, allowing for
               for some slop (1300 =~ 4%) in an axis centred around 0.
               The actual work is done in udev_joypad_axis.
               All bets are off if you're sitting on it. Reinitialise it by unpluging
               and plugging back in. */
            if (udev_compute_axis(abs, abs->value) < -1300)
              pad->neg_trigger[i] = true;
            pad->axes_bind[i] = axes++;
         }
      }
   }

   pad->device = st.st_rdev;
   pad->fd     = fd;
   pad->path   = strdup(path);

   /* Look for a sibling sensor (IMU) evdev node under the same HID parent */
   udev_find_sensor_sibling(dev, p);

   if (!string_is_empty(pad->ident))
   {
      input_autoconfigure_connect(
               pad->ident,
               NULL,
               pad->phys,
               udev_joypad.ident,
               p,
               pad->vid,
               pad->pid);

      ret = 1;
   }

   /* Check for rumble features. */
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0)
   {
      if (test_bit(FF_RUMBLE, ffbit))
         RARCH_LOG("[udev] Pad #%u (%s) supports force feedback.\n",
               p, path);

      if (ioctl(fd, EVIOCGEFFECTS, &pad->num_effects) >= 0)
         RARCH_LOG(
               "[udev] Pad #%u (%s) supports %d force feedback effects.\n",
               p, path, pad->num_effects);
   }

#ifndef HAVE_LAKKA_SWITCH
   /* Set rumble gain here, if supported */
   if (test_bit(FF_RUMBLE, ffbit))
   {
      settings_t *settings = config_get_ptr();
      unsigned rumble_gain = settings ? settings->uints.input_rumble_gain
                                      : DEFAULT_RUMBLE_GAIN;
      udev_set_rumble_gain(p, rumble_gain);
   }
#endif

   return ret;
}

static void udev_check_device(struct udev_device *dev, const char *path)
{
   int i;
   int ret;
   int pad, fd;
   struct stat st;

   if (stat(path, &st) < 0)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (st.st_rdev == udev_pads[i].device)
      {
         RARCH_LOG(
               "[udev] Device ID %u is already plugged.\n",
               (unsigned)st.st_rdev);
         return;
      }
   }

   if ((pad = udev_find_vacant_pad()) < 0)
      return;

   if ((fd = udev_open_joystick(path)) < 0)
      return;

   if (udev_add_pad(dev, pad, fd, path) == -1)
   {
      RARCH_ERR("[udev] Failed to add pad: %s.\n", path);
      close(fd);
   }
}

static void udev_free_pad(unsigned pad)
{
   if (udev_pads[pad].fd >= 0)
      close(udev_pads[pad].fd);
   if (udev_pads[pad].sensor_fd >= 0)
      close(udev_pads[pad].sensor_fd);

   if (udev_pads[pad].path)
      free(udev_pads[pad].path);
   if (udev_pads[pad].sensor_path)
      free(udev_pads[pad].sensor_path);
   udev_pads[pad].path = NULL;
   if (!string_is_empty(udev_pads[pad].ident))
      udev_pads[pad].ident[0] = '\0';

   memset(&udev_pads[pad], 0, sizeof(udev_pads[pad]));

   udev_pads[pad].fd        = -1;
   udev_pads[pad].sensor_fd = -1;
}

static void udev_joypad_remove_device(const char *path)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (     !string_is_empty(udev_pads[i].path)
            &&  string_is_equal(udev_pads[i].path, path))
      {
         input_autoconfigure_disconnect(i, udev_pads[i].ident);
         udev_free_pad(i);
         break;
      }
   }
}

static void udev_hotplug_sensor_add(struct udev_device *sensor_dev,
      const char *sensor_path)
{
   int i;
   struct udev_device *sensor_parent;
   const char *sensor_parent_syspath;

   sensor_parent = udev_device_get_parent_with_subsystem_devtype(
         sensor_dev, "hid", NULL);
   if (!sensor_parent)
      return;
   sensor_parent_syspath = udev_device_get_syspath(sensor_parent);
   if (!sensor_parent_syspath)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      struct udev_device *pad_dev, *pad_parent;
      const char *pad_parent_syspath;
      struct udev_joypad *pad = &udev_pads[i];

      if (pad->fd < 0 || pad->sensor_fd >= 0)
         continue;

      pad_dev = udev_device_new_from_devnum(
            udev_joypad_fd, 'c', pad->device);
      if (!pad_dev)
         continue;

      pad_parent = udev_device_get_parent_with_subsystem_devtype(
            pad_dev, "hid", NULL);
      if (pad_parent)
      {
         pad_parent_syspath = udev_device_get_syspath(pad_parent);
         if (  pad_parent_syspath
            && string_is_equal(sensor_parent_syspath, pad_parent_syspath))
         {
            udev_device_unref(pad_dev);
            udev_open_sensor_node(pad, sensor_path, i);
            return;
         }
      }
      udev_device_unref(pad_dev);
   }
}

static void udev_hotplug_sensor_remove(const char *path)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
   {
      struct udev_joypad *pad = &udev_pads[i];
      if (  pad->sensor_fd >= 0
         && pad->sensor_path
         && string_is_equal(pad->sensor_path, path))
      {
         close(pad->sensor_fd);
         pad->sensor_fd = -1;
         free(pad->sensor_path);
         pad->sensor_path          = NULL;
         pad->sensor_accel_enabled = false;
         pad->sensor_gyro_enabled  = false;
         pad->sensor_has_accel     = false;
         pad->sensor_has_gyro      = false;
         pad->sensor_accel_on_rxyz_codes = false;
         RARCH_LOG("[udev] Pad #%u: sensor removed.\n", i);
         break;
      }
   }
}

static void udev_joypad_destroy(void)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
      udev_free_pad(i);

   if (udev_joypad_mon)
      udev_monitor_unref(udev_joypad_mon);

   if (udev_joypad_fd)
      udev_unref(udev_joypad_fd);

   udev_joypad_mon = NULL;
   udev_joypad_fd  = NULL;
}

static bool udev_set_rumble(unsigned i,
      enum retro_rumble_effect effect, uint16_t strength)
{
   uint16_t old_strength;
   struct udev_joypad *pad = (struct udev_joypad*)&udev_pads[i];

   if (pad->fd < 0)
      return false;
   if (pad->num_effects < 2)
      return false;

   old_strength = pad->strength[effect];
   if (old_strength != strength)
   {
      int old_effect = pad->has_set_ff[effect] ? pad->effects[effect] : -1;

      if (strength && strength != pad->configured_strength[effect])
      {
         /* Create new or update old playing state. */
         struct ff_effect e      = {0};
         /* This defines the length of the effect and
            the delay before playing it. This means there
            is a limit on the maximum vibration time, but
            it's hopefully sufficient for most cases. Maybe
            there's a better way? */
         struct ff_replay replay = {0xffff, 0};

         e.type   = FF_RUMBLE;
         e.id     = old_effect;
         e.replay = replay;

         switch (effect)
         {
            case RETRO_RUMBLE_STRONG:
               e.u.rumble.strong_magnitude = strength;
               break;
            case RETRO_RUMBLE_WEAK:
               e.u.rumble.weak_magnitude = strength;
               break;
            default:
               return false;
         }

         if (ioctl(pad->fd, EVIOCSFF, &e) < 0)
         {
            RARCH_ERR("[udev] Failed to set rumble effect on pad #%u.\n", i);
            return false;
         }

         pad->effects[effect]             = e.id;
         pad->has_set_ff[effect]          = true;
         pad->configured_strength[effect] = strength;
      }
      pad->strength[effect] = strength;

      /* It seems that we can update strength with EVIOCSFF atomically. */
      if ((!!strength) != (!!old_strength))
      {
         struct input_event play;

         play.type  = EV_FF;
         play.code  = pad->effects[effect];
         play.value = !!strength;

         if (write(pad->fd, &play, sizeof(play)) < (ssize_t)sizeof(play))
         {
            RARCH_ERR("[udev] Failed to play rumble effect #%u on pad #%u.\n",
                  effect, i);
            return false;
         }
      }
   }

   return true;
}

static bool udev_joypad_poll_hotplug_available(struct udev_monitor *dev)
{
   struct pollfd fds;

   fds.fd      = udev_monitor_get_fd(dev);
   fds.events  = POLLIN;
   fds.revents = 0;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

static void udev_joypad_poll(void)
{
   unsigned p;

   while (udev_joypad_mon && udev_joypad_poll_hotplug_available(udev_joypad_mon))
   {
      struct udev_device *dev = udev_monitor_receive_device(udev_joypad_mon);

      if (dev)
      {
         const char *val     = udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
         const char *action  = udev_device_get_action(dev);
         const char *devnode = udev_device_get_devnode(dev);

         if (val && string_is_equal(val, "1") && devnode)
         {
            /* Hotplug add */
            if (string_is_equal(action, "add"))
               udev_check_device(dev, devnode);
            /* Hotplug removal */
            else if (string_is_equal(action, "remove"))
               udev_joypad_remove_device(devnode);
            /* Device change */
            else if  (string_is_equal(action, "change"))
            {
               udev_joypad_remove_device(devnode);
               udev_check_device(dev, devnode);
            }
         }
         else if (devnode)
         {
            /* Check for sensor node hotplug */
            const char *accel_val = udev_device_get_property_value(
                  dev, "ID_INPUT_ACCELEROMETER");
            if (accel_val && string_is_equal(accel_val, "1"))
            {
               if (string_is_equal(action, "add"))
                  udev_hotplug_sensor_add(dev, devnode);
               else if (string_is_equal(action, "remove"))
                  udev_hotplug_sensor_remove(devnode);
            }
         }

         udev_device_unref(dev);
      }
   }

   for (p = 0; p < MAX_USERS; p++)
   {
      int i;
      ssize_t _len;
      struct input_event events[32];
      struct udev_joypad *pad = &udev_pads[p];

      if (pad->fd < 0)
         continue;

      while ((_len = read(pad->fd, events, sizeof(events))) > 0)
      {
         _len /= sizeof(*events);
         for (i = 0; i < _len; i++)
         {
            uint16_t type = events[i].type;
            uint16_t code = events[i].code;
            int32_t value = events[i].value;

            switch (type)
            {
               case EV_KEY:
                  if (code > 0 && code < KEY_MAX)
                  {
                     if (value)
                        BIT64_SET(pad->buttons, pad->button_bind[code]);
                     else
                        BIT64_CLEAR(pad->buttons, pad->button_bind[code]);
                  }
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
                        code                           -= ABS_HAT0X;
                        pad->hats[code >> 1][code & 1]  = value;
                        break;
                     default:
                        {
                           unsigned axis   = pad->axes_bind[code];
                           pad->axes[axis] = udev_compute_axis(
                                 &pad->absinfo[axis], value);
                           break;
                        }
                  }
                  break;

               default:
                  break;
            }
         }
      }

      /* Read sensor events from sibling IMU node */
      if (  pad->sensor_fd >= 0
         && (pad->sensor_accel_enabled || pad->sensor_gyro_enabled))
      {
         struct input_event sevents[32];
         ssize_t slen;
         while ((slen = read(pad->sensor_fd, sevents, sizeof(sevents))) > 0)
         {
            int si;
            slen /= sizeof(*sevents);
            for (si = 0; si < slen; si++)
            {
               uint16_t code;
               int res;

               if (sevents[si].type != EV_ABS)
                  continue;

               code = sevents[si].code;

               /* Wiimote / rotational-axis accel:
                * ABS_RX(3)->ACCEL_X(0), ABS_RY(4)->ACCEL_Y(1), ABS_RZ(5)->ACCEL_Z(2).
                * Remap so sensor_data is indexed by RETRO_SENSOR_* IDs. */
               if (pad->sensor_accel_on_rxyz_codes)
               {
                  if (code == ABS_RX)      code = 0;
                  else if (code == ABS_RY) code = 1;
                  else if (code == ABS_RZ) code = 2;
                  else continue; /* no other axes expected on this node */
               }
               else if (code >= SENSOR_AXES)
                  continue;

               /* Normalise raw value to SI units.
                * resolution field = units per g (accel) or units per deg/s (gyro).
                * When resolution == 0 (hid-wiimote doesn't set it), fall back to
                * using absinfo.maximum as the full-scale value so that
                * sensor_data ends up in g (± 1.0 at maximum deflection). */
               res = pad->sensor_absinfo[code].resolution;
               if (res > 0)
                  pad->sensor_data[code] =
                        (float)sevents[si].value / (float)res;
               else
               {
                  int maxval = pad->sensor_absinfo[code].maximum;
                  if (maxval > 0)
                     pad->sensor_data[code] =
                           (float)sevents[si].value / (float)maxval;
                  else
                     pad->sensor_data[sevents[si].code] =
                           (float)sevents[si].value;
               }
            }
         }
      }
   }
}

static void *udev_joypad_init(void *data)
{
   int i;
   unsigned sorted_count = 0;
   struct udev_list_entry *devs     = NULL;
   struct udev_list_entry *item     = NULL;
   struct udev_enumerate *enumerate = NULL;
   struct joypad_udev_entry sorted[MAX_USERS];

   for (i = 0; i < MAX_USERS; i++)
   {
      udev_pads[i].fd        = -1;
      udev_pads[i].sensor_fd = -1;
   }

   if (!(udev_joypad_fd = udev_new()))
      return NULL;

   if ((udev_joypad_mon = udev_monitor_new_from_netlink(udev_joypad_fd, "udev")))
   {
      udev_monitor_filter_add_match_subsystem_devtype(
            udev_joypad_mon, "input", NULL);
      udev_monitor_enable_receiving(udev_joypad_mon);
   }

   if (!(enumerate = udev_enumerate_new(udev_joypad_fd)))
      goto error;

   udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   if (!(devs = udev_enumerate_get_list_entry(enumerate)))
      RARCH_DBG("[udev] Couldn't open any joypads. Are permissions set correctly for /dev/input/event* and /run/udev/?\n");

   udev_list_entry_foreach(item, devs)
   {
      const char         *name = udev_list_entry_get_name(item);
      struct udev_device  *dev = udev_device_new_from_syspath(udev_joypad_fd, name);
      const char      *devnode = udev_device_get_devnode(dev);
#if defined(DEBUG)
      struct udev_list_entry *list_entry = NULL;
      RARCH_DBG("[udev] udev_joypad_init entry name=%s devnode=%s\n", name, devnode);
      udev_list_entry_foreach(list_entry, udev_device_get_properties_list_entry(dev))
         RARCH_DBG("[udev] udev_joypad_init property %s=%s\n",
                       udev_list_entry_get_name(list_entry),
                       udev_list_entry_get_value(list_entry));
#endif

      if (devnode)
         udev_check_device(dev, devnode);
      udev_device_unref(dev);
   }

   udev_enumerate_unref(enumerate);

   return (void*)-1;

error:
   udev_joypad_destroy();
   return NULL;
}

static int32_t udev_joypad_button_state(
      const struct udev_joypad *pad,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h < NUM_HATS)
      {
         switch (hat_dir)
         {
            case HAT_LEFT_MASK:
               return (pad->hats[h][0] < 0);
            case HAT_RIGHT_MASK:
               return (pad->hats[h][0] > 0);
            case HAT_UP_MASK:
               return (pad->hats[h][1] < 0);
            case HAT_DOWN_MASK:
               return (pad->hats[h][1] > 0);
            default:
               break;
         }
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < UDEV_NUM_BUTTONS)
      return (BIT64_GET(pad->buttons, joykey));
   return 0;
}

static int32_t udev_joypad_button(unsigned port, uint16_t joykey)
{
   const struct udev_joypad *pad        = (const struct udev_joypad*)
      &udev_pads[port];
   if (port >= MAX_USERS)
      return 0;
   return udev_joypad_button_state(pad, port, joykey);
}

static void udev_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	const struct udev_joypad *pad = (const struct udev_joypad*)
      &udev_pads[port];

	if (pad)
   {
		BITS_COPY64_PTR( state, pad->buttons );
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t udev_joypad_axis_state(
      const struct udev_joypad *pad,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < NUM_AXES)
   {
      int16_t val = pad->axes[AXIS_NEG_GET(joyaxis)];
      /* Deal with analog triggers that report -32767 to 32767 */
      if ((
               (AXIS_NEG_GET(joyaxis) == ABS_Z) ||
               (AXIS_NEG_GET(joyaxis) == ABS_RZ))
            && (pad->neg_trigger[AXIS_NEG_GET(joyaxis)]))
         val = (val + 0x7fff) / 2;
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < NUM_AXES)
   {
      int16_t val = pad->axes[AXIS_POS_GET(joyaxis)];
      /* Deal with analog triggers that report -32767 to 32767 */
      if ((
               (AXIS_POS_GET(joyaxis) == ABS_Z) ||
               (AXIS_POS_GET(joyaxis) == ABS_RZ))
            && (pad->neg_trigger[AXIS_POS_GET(joyaxis)]))
         val = (val + 0x7fff) / 2;
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t udev_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const struct udev_joypad *pad = (const struct udev_joypad*)
      &udev_pads[port];
   return udev_joypad_axis_state(pad, port, joyaxis);
}

static int16_t udev_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx < MAX_USERS)
   {
      int i;
      const struct udev_joypad *pad     = (const struct udev_joypad*)
         &udev_pads[port_idx];
      for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
      {
         /* Auto-binds are per joypad, not per user. */
         const uint64_t joykey  = (binds[i].joykey != NO_BTN)
            ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
         const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
            ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
         if (
               (uint16_t)joykey != NO_BTN
               && udev_joypad_button_state(pad, port_idx, (uint16_t)joykey)
            )
            ret |= ( 1 << i);
         else if (joyaxis != AXIS_NONE &&
               ((float)abs(udev_joypad_axis_state(pad, port_idx, joyaxis))
                / 0x8000) > joypad_info->axis_threshold)
            ret |= (1 << i);
      }
   }

   return ret;
}

static bool udev_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && udev_pads[pad].fd >= 0;
}

static const char *udev_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS || string_is_empty(udev_pads[pad].ident))
      return NULL;

   return udev_pads[pad].ident;
}

static bool udev_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;

   pad = &udev_pads[port];

   if (pad->sensor_fd < 0)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         if (!pad->sensor_has_accel)
            return false;
         pad->sensor_accel_enabled = true;
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         pad->sensor_accel_enabled = false;
         return true;
      case RETRO_SENSOR_GYROSCOPE_ENABLE:
         if (!pad->sensor_has_gyro)
            return false;
         pad->sensor_gyro_enabled = true;
         return true;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
         pad->sensor_gyro_enabled = false;
         return true;
      default:
         break;
   }
   return false;
}

static bool udev_get_sensor_input(unsigned port,
      unsigned id, float *value)
{
   const struct udev_joypad *pad;

   if (port >= MAX_USERS)
      return false;

   pad = &udev_pads[port];

   if (pad->sensor_fd < 0)
      return false;

   if (id > RETRO_SENSOR_GYROSCOPE_Z)
      return false;

   /* Check if the requested sensor type is enabled */
   if (id <= RETRO_SENSOR_ACCELEROMETER_Z)
   {
      if (!pad->sensor_accel_enabled)
         return false;
      /* Accelerometer: resolution is units/g, so sensor_data is already in g */
      *value = pad->sensor_data[id];
   }
   else
   {
      if (!pad->sensor_gyro_enabled)
         return false;
      /* Gyroscope: resolution is units/(deg/s), convert to rad/s */
      *value = pad->sensor_data[id] * DEG_TO_RAD_F;
   }

   return true;
}

input_device_driver_t udev_joypad = {
   udev_joypad_init,
   udev_joypad_query_pad,
   udev_joypad_destroy,
   udev_joypad_button,
   udev_joypad_state,
   udev_joypad_get_buttons,
   udev_joypad_axis,
   udev_joypad_poll,
   udev_set_rumble,
#ifndef HAVE_LAKKA_SWITCH
   udev_set_rumble_gain,
#else
   NULL, /* set_rumble_gain */
#endif
   udev_set_sensor_state,
   udev_get_sensor_input,
   udev_joypad_name,
   "udev",
};
