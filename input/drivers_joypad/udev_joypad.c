/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <linux/types.h>
#include <linux/input.h>

#include <retro_inline.h>

#include "../input_autodetect.h"
#include "../common/udev_common.h"
#include "../../general.h"
#include "../../verbosity.h"

/* Udev/evdev Linux joypad driver.
 * More complex and extremely low level,
 * but only Linux driver which can support joypad rumble.
 *
 * Uses udev for device detection + hotplug.
 *
 * Code adapted from SDL 2.0's implementation.
 */

#define UDEV_NUM_BUTTONS 32
#define NUM_AXES 32
#define NUM_HATS 4

#define test_bit(nr, addr) \
   (((1UL << ((nr) % (sizeof(long) * CHAR_BIT))) & ((addr)[(nr) / (sizeof(long) * CHAR_BIT)])) != 0)
#define NBITS(x) ((((x) - 1) / (sizeof(long) * CHAR_BIT)) + 1)

struct udev_joypad
{
   int fd;
   dev_t device;

   /* Input state polled. */
   uint64_t buttons;
   int16_t axes[NUM_AXES];
   int8_t hats[NUM_HATS][2];

   /* Maps keycodes -> button/axes */
   uint8_t button_bind[KEY_MAX];
   uint8_t axes_bind[ABS_MAX];
   struct input_absinfo absinfo[NUM_AXES];

   int num_effects;
   int effects[2]; /* [0] - strong, [1] - weak  */
   bool has_set_ff[2];
   uint16_t strength[2];
   uint16_t configured_strength[2];

   char ident[PATH_MAX_LENGTH];
   char *path;
   int32_t vid;
   int32_t pid;
};

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

static void udev_poll_pad(struct udev_joypad *pad, unsigned p)
{
   int i, len;
   struct input_event events[32];

   if (pad->fd < 0)
      return;

   while ((len = read(pad->fd, events, sizeof(events))) > 0)
   {
      len /= sizeof(*events);
      for (i = 0; i < len; i++)
      {
         int code = events[i].code;
         switch (events[i].type)
         {
            case EV_KEY:
               if (code >= BTN_MISC || (code >= KEY_UP && code <= KEY_DOWN))
               {
                  if (events[i].value)
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
                  {
                     code                           -= ABS_HAT0X;
                     pad->hats[code >> 1][code & 1]  = events[i].value;
                     break;
                  }

                  default:
                  {
                     unsigned axis   = pad->axes_bind[code];
                     pad->axes[axis] = udev_compute_axis(&pad->absinfo[axis], events[i].value);
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

static int udev_find_vacant_pad(void)
{
   unsigned i;

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

   if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
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
   unsigned long keybit[NBITS(KEY_MAX)] = {0};
   unsigned long absbit[NBITS(ABS_MAX)] = {0};
   unsigned long ffbit[NBITS(FF_MAX)]   = {0};
   autoconfig_params_t params           = {{0}};
   settings_t *settings                 = config_get_ptr();

   strlcpy(pad->ident, settings->input.device_names[p], sizeof(pad->ident));

   if (ioctl(fd, EVIOCGNAME(sizeof(pad->ident)), pad->ident) < 0)
   {
      RARCH_LOG("[udev]: Failed to get pad name: %s.\n", pad->ident);
      return -1;
   }

   /* Don't worry about unref'ing the parent. */
   parent = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");

   pad->vid = pad->pid = 0;

   if ((buf = udev_device_get_sysattr_value(parent, "idVendor")) != NULL)
      pad->vid = strtol(buf, NULL, 16);

   if ((buf = udev_device_get_sysattr_value(parent, "idProduct")) != NULL)
      pad->pid = strtol(buf, NULL, 16);

   RARCH_LOG("[udev]: Plugged pad: %s (%04x:%04x) on port #%u.\n",
             pad->ident, pad->vid, pad->pid, p);

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
            pad->axes_bind[i] = axes++;
         }
      }
   }

   pad->device = st.st_rdev;
   pad->fd     = fd;
   pad->path   = strdup(path);

   if (*pad->ident)
   {
      params.idx = p;
      strlcpy(params.name, pad->ident, sizeof(params.name));
      params.vid = pad->vid;
      params.pid = pad->pid;
      settings->input.pid[p] = params.pid;
      settings->input.vid[p] = params.vid;
      strlcpy(settings->input.device_names[p], params.name, sizeof(settings->input.device_names[p]));
      strlcpy(params.driver, udev_joypad.ident, sizeof(params.driver));
      input_config_autoconfigure_joypad(&params);

      ret = 1;
   }

   /* Check for rumble features. */
   if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0)
   {
      if (test_bit(FF_RUMBLE, ffbit))
         RARCH_LOG("[udev]: Pad #%u (%s) supports force feedback.\n",
               p, path);

      if (ioctl(fd, EVIOCGEFFECTS, &pad->num_effects) >= 0)
         RARCH_LOG("[udev]: Pad #%u (%s) supports %d force feedback effects.\n", p, path, pad->num_effects);
   }

   return ret;
}

static void udev_check_device(struct udev_device *dev, const char *path, bool hotplugged)
{
   int ret;
   int pad, fd;
   unsigned i;
   struct stat st;

   if (stat(path, &st) < 0)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (st.st_rdev == udev_pads[i].device)
      {
         RARCH_LOG("[udev]: Device ID %u is already plugged.\n", (unsigned)st.st_rdev);
         return;
      }
   }

   pad = udev_find_vacant_pad();
   if (pad < 0)
      return;

   fd = udev_open_joystick(path);
   if (fd < 0)
      return;

   ret = udev_add_pad(dev, pad, fd, path);

   switch (ret)
   {
      case -1:
         RARCH_ERR("[udev]: Failed to add pad: %s.\n", path);
         close(fd);
         break;
      case 1:
         /* Pad was autoconfigured. */
         break;
      case 0:
      default:
         if (hotplugged)
         {
            char msg[PATH_MAX_LENGTH] = {0};

            snprintf(msg, sizeof(msg), "Device #%u (%s) connected.", pad, path);
            rarch_main_msg_queue_push(msg, 0, 60, false);
            RARCH_LOG("[udev]: %s\n", msg);
         }
         break;
   }
}

static void udev_free_pad(unsigned pad)
{
   if (udev_pads[pad].fd >= 0)
      close(udev_pads[pad].fd);

   if (udev_pads[pad].path)
      free(udev_pads[pad].path);
   udev_pads[pad].path = NULL;
   if (udev_pads[pad].ident[0] != '\0')
      udev_pads[pad].ident[0] = '\0';

   memset(&udev_pads[pad], 0, sizeof(udev_pads[pad]));

   udev_pads[pad].fd    = -1;
}

static void udev_joypad_remove_device(const char *path)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (udev_pads[i].path && !strcmp(udev_pads[i].path, path))
      {
         input_config_autoconfigure_disconnect(i, udev_pads[i].ident);
         udev_free_pad(i);
         break;
      }
   }
}

static void udev_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
      udev_free_pad(i);

   udev_mon_free(true);
}

static void udev_joypad_handle_hotplug(void)
{
   const char *val;
   const char *action;
   const char *devnode;
   struct udev_device *dev = udev_mon_receive_device();
   if (!dev)
      return;

   val     = udev_device_get_property_value(dev, "ID_INPUT_JOYSTICK");
   action  = udev_device_get_action(dev);
   devnode = udev_device_get_devnode(dev);

   if (!val || strcmp(val, "1") || !devnode)
      goto end;

   if (!strcmp(action, "add"))
   {
      RARCH_LOG("[udev]: Hotplug add: %s.\n", devnode);
      udev_check_device(dev, devnode, true);
   }
   else if (!strcmp(action, "remove"))
   {
      RARCH_LOG("[udev]: Hotplug remove: %s.\n", devnode);
      udev_joypad_remove_device(devnode);
   }

end:
   udev_device_unref(dev);
}

static bool udev_set_rumble(unsigned i, enum retro_rumble_effect effect, uint16_t strength)
{
   int old_effect;
   uint16_t old_strength;
   struct udev_joypad *pad = (struct udev_joypad*)&udev_pads[i];

   if (pad->fd < 0)
      return false;
   if (pad->num_effects < 2)
      return false;

   old_strength = pad->strength[effect];
   if (old_strength == strength)
      return true;

   old_effect = pad->has_set_ff[effect] ? pad->effects[effect] : -1;

   if (strength && strength != pad->configured_strength[effect])
   {
      /* Create new or update old playing state. */
      struct ff_effect e = {0};

      e.type = FF_RUMBLE;
      e.id   = old_effect;

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
         RARCH_ERR("Failed to set rumble effect on pad #%u.\n", i);
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
      struct input_event play = {{0}};

      play.type  = EV_FF;
      play.code  = pad->effects[effect];
      play.value = !!strength;

      if (write(pad->fd, &play, sizeof(play)) < (ssize_t)sizeof(play))
      {
         RARCH_ERR("[udev]: Failed to play rumble effect #%u on pad #%u.\n",
               effect, i);
         return false;
      }
   }

   return true;
}

static void udev_joypad_poll(void)
{
   unsigned i;
   while (udev_mon_hotplug_available())
      udev_joypad_handle_hotplug();

   for (i = 0; i < MAX_USERS; i++)
      udev_poll_pad(&udev_pads[i], i);
}

static bool udev_joypad_init(void *data)
{
   unsigned i;
   struct udev_list_entry *devs = NULL;
   struct udev_list_entry *item = NULL;
   struct udev_enumerate *enumerate = NULL;
   settings_t *settings = config_get_ptr();

   (void)data;

   for (i = 0; i < MAX_USERS; i++)
      udev_pads[i].fd = -1;

   if (!udev_mon_new(true))
      goto error;

   enumerate = udev_mon_enumerate();
   if (!enumerate)
      goto error;

   udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   for (item = devs; item; item = udev_list_entry_get_next(item))
   {
      const char         *name = udev_list_entry_get_name(item);
      struct udev_device  *dev = udev_mon_device_new(name);
      const char      *devnode = udev_device_get_devnode(dev);

      if (devnode)
         udev_check_device(dev, devnode, false);
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
      case HAT_LEFT_MASK:
         return pad->hats[h][0] < 0;
      case HAT_RIGHT_MASK:
         return pad->hats[h][0] > 0;
      case HAT_UP_MASK:
         return pad->hats[h][1] < 0;
      case HAT_DOWN_MASK:
         return pad->hats[h][1] > 0;
   }

   return 0;
}

static bool udev_joypad_button(unsigned port, uint16_t joykey)
{
   const struct udev_joypad *pad = (const struct udev_joypad*)&udev_pads[port];

   if (GET_HAT_DIR(joykey))
      return udev_joypad_hat(pad, joykey);
   return joykey < UDEV_NUM_BUTTONS && BIT64_GET(pad->buttons, joykey);
}

static uint64_t udev_joypad_get_buttons(unsigned port)
{
   const struct udev_joypad *pad = (const struct udev_joypad*)&udev_pads[port];
   if (!pad)
      return 0;
   return pad->buttons;
}

static int16_t udev_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t val = 0;
   const struct udev_joypad *pad = (const struct udev_joypad*)
      &udev_pads[port];

   if (joyaxis == AXIS_NONE)
      return 0;

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
   return pad < MAX_USERS && udev_pads[pad].fd >= 0;
}

static const char *udev_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;

   return *udev_pads[pad].ident ? udev_pads[pad].ident : NULL;
}

input_device_driver_t udev_joypad = {
   udev_joypad_init,
   udev_joypad_query_pad,
   udev_joypad_destroy,
   udev_joypad_button,
   udev_joypad_get_buttons,
   udev_joypad_axis,
   udev_joypad_poll,
   udev_set_rumble,
   udev_joypad_name,
   "udev",
};
