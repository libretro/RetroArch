/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/inotify.h>
#include <linux/joystick.h>

#include <fcntl.h>
#include <sys/epoll.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../input_driver.h"

#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#define NUM_BUTTONS 32
#define NUM_AXES 32

struct ohos_joypad
{
   int fd;
   uint32_t buttons;
   int16_t axes[NUM_AXES];

   char *ident;
};

/* TODO/FIXME - static globals */
static struct ohos_joypad ohos_pads[MAX_USERS];
static int ohos_epoll                              = 0;
static int ohos_inotify                            = 0;
static bool ohos_hotplug                           = false;

static void ohos_poll_pad(struct ohos_joypad *pad)
{
   struct js_event event;

   while (read(pad->fd, &event, sizeof(event)) == (ssize_t)sizeof(event))
   {
      unsigned type = event.type & ~JS_EVENT_INIT;

      switch (type)
      {
         case JS_EVENT_BUTTON:
            if (event.number < NUM_BUTTONS)
            {
               if (event.value)
                  BIT32_SET(pad->buttons, event.number);
               else
                  BIT32_CLEAR(pad->buttons, event.number);
            }
            break;

         case JS_EVENT_AXIS:
            if (event.number < NUM_AXES)
               pad->axes[event.number] = event.value;
            break;
      }
   }
}

static bool ohos_joypad_init_pad(const char *path,
      struct ohos_joypad *pad)
{
   /* Device can have just been created, but not made accessible (yet).
      IN_ATTRIB will signal when permissions change. */
   if (access(path, R_OK) < 0)
      return false;
   if (pad->fd >= 0)
      return false;

   pad->fd     = open(path, O_RDONLY | O_NONBLOCK);
   *pad->ident = '\0';

   if (pad->fd >= 0)
   {
      struct epoll_event event;

      ioctl(pad->fd,
               JSIOCGNAME(input_config_get_device_name_size(0)), pad->ident);

      event.events             = EPOLLIN;
      event.data.ptr           = pad;
      RARCH_LOG("[LinuxRaw] Device name is \"%s\".\n",pad->ident);
      if (epoll_ctl(ohos_epoll, EPOLL_CTL_ADD, pad->fd, &event) >= 0)
         return true;
   }

   return false;
}

static const char *ohos_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;
   return ohos_pads[pad].ident;
}

static void ohos_joypad_poll(void)
{
   int i, ret;
   struct epoll_event events[MAX_USERS + 1];

retry:
   ret = epoll_wait(ohos_epoll, events, MAX_USERS + 1, 0);
   if (ret < 0 && errno == EINTR)
      goto retry;

   for (i = 0; i < ret; i++)
   {
      struct ohos_joypad *ptr = (struct ohos_joypad*)
         events[i].data.ptr;

      if (ptr)
         ohos_poll_pad(ptr);
      else
      {
         /* handle plugged pad */
         int j, rc;
         size_t event_size  = sizeof(struct inotify_event) + NAME_MAX + 1;
         uint8_t *event_buf = (uint8_t*)calloc(1, event_size);

         while ((rc = read(ohos_inotify, event_buf, event_size)) >= 0)
         {
            struct inotify_event *event = (struct inotify_event*)&event_buf[0];

            event_buf[rc-1] = '\0';

            /* Can read multiple events in one read() call. */

            for (j = 0; j < rc; j += event->len + sizeof(struct inotify_event))
            {
               unsigned idx;

               event = (struct inotify_event*)&event_buf[j];

               if (strstr(event->name, "js") != event->name)
                  continue;

               idx = strtoul(event->name + 2, NULL, 0);
               if (idx >= MAX_USERS)
                  continue;

               if (event->mask & IN_DELETE)
               {
                  if (ohos_pads[idx].fd >= 0)
                  {
                     if (ohos_hotplug)
                     {
                        input_autoconfigure_disconnect(idx,
                              ohos_pads[idx].ident);
                        RARCH_LOG("[LinuxRaw] Disconnected \"%s\".\n",
                              ohos_pads[idx].ident);
                     }

                     close(ohos_pads[idx].fd);
                     ohos_pads[idx].buttons = 0;
                     memset(ohos_pads[idx].axes, 0,
                           sizeof(ohos_pads[idx].axes));
                     ohos_pads[idx].fd = -1;
                     *ohos_pads[idx].ident = '\0';

                     input_autoconfigure_connect(
                           NULL,
                           NULL, NULL,
                           "ohos",
                           idx,
                           0,
                           0);
                  }
               }
               /* Sometimes, device will be created before
                * access to it is established. */
               else if (event->mask & (IN_CREATE | IN_ATTRIB))
               {
                  char path[256];
                  size_t _len = strlcpy(path, "/dev/input/", sizeof(path));
                  strlcpy(path + _len, event->name, sizeof(path) - _len);
                  RARCH_DBG("[LinuxRaw] Reconnecting \"%s\".\n",path);

                  if (     (!ohos_pads[idx].ident || !*ohos_pads[idx].ident)
                        && ohos_joypad_init_pad(path, &ohos_pads[idx]))
                  {
                     input_autoconfigure_connect(
                           ohos_pads[idx].ident,
                           NULL, NULL,
                           "ohos",
                           idx,
                           0,
                           0);
                     RARCH_LOG("[LinuxRaw] Reconnected \"%s\".\n",ohos_pads[idx].ident);
                  }
               }
            }
         }

         free(event_buf);
      }
   }
}

static void *ohos_joypad_init(void *data)
{
   size_t i, _len;
   char path[PATH_MAX_LENGTH];
   int fd      = epoll_create(32);
   bool init_ok;

   if (fd < 0)
      return NULL;

   ohos_epoll = fd;
   _len           = strlcpy(path, "/dev/input/js", sizeof(path));

   for (i = 0; i < MAX_USERS; i++)
   {
      struct ohos_joypad *pad = (struct ohos_joypad*)&ohos_pads[i];

      pad->fd    = -1;
      pad->ident = input_config_get_device_name_ptr(i);

      snprintf(path + _len, sizeof(path) - _len, "%u", (uint32_t)i);

      init_ok = ohos_joypad_init_pad(path, pad);

      RARCH_DBG("[LinuxRaw] Scanning path \"%s\", ident \"%s\".\n", path, pad->ident);
      input_autoconfigure_connect(pad->ident, NULL, NULL, "ohos",
            i, 0, 0);

      if (init_ok)
         ohos_poll_pad(pad);
   }

   ohos_inotify = inotify_init();

   if (ohos_inotify >= 0)
   {
      struct epoll_event event;

      fcntl(ohos_inotify, F_SETFL, fcntl(ohos_inotify, F_GETFL) | O_NONBLOCK);
      inotify_add_watch(ohos_inotify, "/dev/input", IN_DELETE | IN_CREATE | IN_ATTRIB);

      event.events             = EPOLLIN;
      event.data.ptr           = NULL;

      /* Shouldn't happen, but just check it. */
      if (epoll_ctl(ohos_epoll, EPOLL_CTL_ADD, ohos_inotify, &event) < 0)
      {
         RARCH_ERR("[LinuxRaw] Failed to add FD (%d) to epoll list (%s).\n",
               ohos_inotify, strerror(errno));
      }
   }

   ohos_hotplug = true;

   return (void*)-1;
}

static void ohos_joypad_destroy(void)
{
   int i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (ohos_pads[i].fd >= 0)
         close(ohos_pads[i].fd);
   }

   memset(ohos_pads, 0, sizeof(ohos_pads));

   for (i = 0; i < MAX_USERS; i++)
      ohos_pads[i].fd = -1;

   if (ohos_inotify >= 0)
      close(ohos_inotify);
   ohos_inotify = -1;

   if (ohos_epoll >= 0)
      close(ohos_epoll);
   ohos_epoll = -1;

   ohos_hotplug = false;
}

static int32_t ohos_joypad_button(unsigned port, uint16_t joykey)
{
   const struct ohos_joypad    *pad = (const struct ohos_joypad*)
      &ohos_pads[port];
   if (port >= MAX_USERS)
      return 0;
   if (joykey < NUM_BUTTONS)
      return (BIT32_GET(pad->buttons, joykey));
   return 0;
}

static void ohos_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	const struct ohos_joypad *pad = (const struct ohos_joypad*)
      &ohos_pads[port];

	if (pad)
   {
		BITS_COPY16_PTR(state, pad->buttons);
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ohos_joypad_axis_state(
      const struct ohos_joypad *pad,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < NUM_AXES)
   {
      /* Kernel returns values in range [-0x7fff, 0x7fff]. */
      int16_t val = pad->axes[AXIS_NEG_GET(joyaxis)];
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < NUM_AXES)
   {
      int16_t val = pad->axes[AXIS_POS_GET(joyaxis)];
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t ohos_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const struct ohos_joypad *pad = (const struct ohos_joypad*)
      &ohos_pads[port];
   return ohos_joypad_axis_state(pad, port, joyaxis);
}

static int16_t ohos_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   const struct ohos_joypad    *pad = (const struct ohos_joypad*)
      &ohos_pads[port_idx];

   if (port_idx >= MAX_USERS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN &&
            (joykey < NUM_BUTTONS)   &&
            (BIT32_GET(pad->buttons, joykey)))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ohos_joypad_axis_state(pad, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool ohos_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && ohos_pads[pad].fd >= 0;
}

input_device_driver_t ohos_joypad = {
   ohos_joypad_init,
   ohos_joypad_query_pad,
   ohos_joypad_destroy,
   ohos_joypad_button,
   ohos_joypad_state,
   ohos_joypad_get_buttons,
   ohos_joypad_axis,
   ohos_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   ohos_joypad_name,
   "ohos",
};
