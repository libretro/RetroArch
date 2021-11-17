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
#include <errno.h>

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
   bool has_set_ff[2];
   /* Deal with analog triggers that report -32767 to 32767 */
   bool neg_trigger[NUM_AXES];
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
      RARCH_ERR("[udev]: Failed to set rumble gain on pad #%u.\n", i);
      return false;
   }

   pad->rumble_gain = gain;

   return true;
}
#endif

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
               for some slop (1300 =~ 4%)in an axis centred around 0.
               The actual work is done in udev_joypad_axis.
               All bets are off if you're sitting on it. Reinitailise it by unpluging
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

   if (!string_is_empty(pad->ident))
   {
      input_autoconfigure_connect(
               pad->ident,
               NULL,
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
         RARCH_LOG("[udev]: Pad #%u (%s) supports force feedback.\n",
               p, path);

      if (ioctl(fd, EVIOCGEFFECTS, &pad->num_effects) >= 0)
         RARCH_LOG(
               "[udev]: Pad #%u (%s) supports %d force feedback effects.\n",
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
         RARCH_LOG(
               "[udev]: Device ID %u is already plugged.\n",
               (unsigned)st.st_rdev);
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
   if (!string_is_empty(udev_pads[pad].ident))
      udev_pads[pad].ident[0] = '\0';

   memset(&udev_pads[pad], 0, sizeof(udev_pads[pad]));

   udev_pads[pad].fd    = -1;
}

static void udev_joypad_remove_device(const char *path)
{
   unsigned i;

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

static void udev_joypad_destroy(void)
{
   unsigned i;

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
      /* This defines the length of the effect and
         the delay before playing it. This means there
         is a limit on the maximum vibration time, but
         it's hopefully sufficient for most cases. Maybe
         there's a better way? */
      struct ff_replay replay = {0xffff, 0};

      e.type = FF_RUMBLE;
      e.id   = old_effect;
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
         }

         udev_device_unref(dev);
      }
   }

   for (p = 0; p < MAX_USERS; p++)
   {
      int i, len;
      struct input_event events[32];
      struct udev_joypad *pad = &udev_pads[p];

      if (pad->fd < 0)
         continue;

      while ((len = read(pad->fd, events, sizeof(events))) > 0)
      {
         len /= sizeof(*events);
         for (i = 0; i < len; i++)
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
   }
}

static void *udev_joypad_init(void *data)
{
   unsigned i;
   unsigned sorted_count = 0;
   struct udev_list_entry *devs     = NULL;
   struct udev_list_entry *item     = NULL;
   struct udev_enumerate *enumerate = NULL;
   struct joypad_udev_entry sorted[MAX_USERS];

   for (i = 0; i < MAX_USERS; i++)
      udev_pads[i].fd = -1;

   udev_joypad_fd = udev_new();
   if (!udev_joypad_fd)
      return NULL;

   udev_joypad_mon = udev_monitor_new_from_netlink(udev_joypad_fd, "udev");
   if (udev_joypad_mon)
   {
      udev_monitor_filter_add_match_subsystem_devtype(
            udev_joypad_mon, "input", NULL);
      udev_monitor_enable_receiving(udev_joypad_mon);
   }

   enumerate = udev_enumerate_new(udev_joypad_fd);
   if (!enumerate)
      goto error;

   udev_enumerate_add_match_property(enumerate, "ID_INPUT_JOYSTICK", "1");
   udev_enumerate_add_match_subsystem(enumerate, "input");
   udev_enumerate_scan_devices(enumerate);
   devs = udev_enumerate_get_list_entry(enumerate);

   udev_list_entry_foreach(item, devs)
   {
      const char         *name = udev_list_entry_get_name(item);
      struct udev_device  *dev = udev_device_new_from_syspath(udev_joypad_fd, name);
      const char      *devnode = udev_device_get_devnode(dev);

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
   if (port >= DEFAULT_MAX_PADS)
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
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   const struct udev_joypad *pad        = (const struct udev_joypad*)
      &udev_pads[port_idx];

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

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
   NULL,
#endif
   udev_joypad_name,
   "udev",
};
