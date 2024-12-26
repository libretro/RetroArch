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

struct linuxraw_joypad
{
   int fd;
   uint32_t buttons;
   int16_t axes[NUM_AXES];

   char *ident;
};

/* TODO/FIXME - static globals */
static struct linuxraw_joypad linuxraw_pads[MAX_USERS];
static int linuxraw_epoll                              = 0;
static int linuxraw_inotify                            = 0;
static bool linuxraw_hotplug                           = false;

static void linuxraw_poll_pad(struct linuxraw_joypad *pad)
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

static bool linuxraw_joypad_init_pad(const char *path,
      struct linuxraw_joypad *pad)
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
      RARCH_LOG("[linuxraw]: device name is \"%s\".\n",pad->ident);
      if (epoll_ctl(linuxraw_epoll, EPOLL_CTL_ADD, pad->fd, &event) >= 0)
         return true;
   }

   return false;
}

static const char *linuxraw_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS || string_is_empty(linuxraw_pads[pad].ident))
      return NULL;

   return linuxraw_pads[pad].ident;
}

static void linuxraw_joypad_poll(void)
{
   int i, ret;
   struct epoll_event events[MAX_USERS + 1];

retry:
   ret = epoll_wait(linuxraw_epoll, events, MAX_USERS + 1, 0);
   if (ret < 0 && errno == EINTR)
      goto retry;

   for (i = 0; i < ret; i++)
   {
      struct linuxraw_joypad *ptr = (struct linuxraw_joypad*)
         events[i].data.ptr;

      if (ptr)
         linuxraw_poll_pad(ptr);
      else
      {
         /* handle plugged pad */
         int j, rc;
         size_t event_size  = sizeof(struct inotify_event) + NAME_MAX + 1;
         uint8_t *event_buf = (uint8_t*)calloc(1, event_size);

         while ((rc = read(linuxraw_inotify, event_buf, event_size)) >= 0)
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
                  if (linuxraw_pads[idx].fd >= 0)
                  {
                     if (linuxraw_hotplug)
                     {
                        input_autoconfigure_disconnect(idx,
                              linuxraw_pads[idx].ident);
                        RARCH_LOG("[linuxraw]: disconnected \"%s\".\n",
                              linuxraw_pads[idx].ident);
                     }

                     close(linuxraw_pads[idx].fd);
                     linuxraw_pads[idx].buttons = 0;
                     memset(linuxraw_pads[idx].axes, 0,
                           sizeof(linuxraw_pads[idx].axes));
                     linuxraw_pads[idx].fd = -1;
                     *linuxraw_pads[idx].ident = '\0';

                     input_autoconfigure_connect(
                           NULL,
                           NULL,
                           "linuxraw",
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
                  RARCH_DBG("[linuxraw]: reconnecting \"%s\".\n",path);

                  if (     string_is_empty(linuxraw_pads[idx].ident)
                        && linuxraw_joypad_init_pad(path, &linuxraw_pads[idx]))
                  {
                     input_autoconfigure_connect(
                           linuxraw_pads[idx].ident,
                           NULL,
                           "linuxraw",
                           idx,
                           0,
                           0);
                     RARCH_LOG("[linuxraw]: reconnected \"%s\".\n",linuxraw_pads[idx].ident);
                  }
               }
            }
         }

         free(event_buf);
      }
   }
}

static void *linuxraw_joypad_init(void *data)
{
   size_t i, _len;
   char path[PATH_MAX_LENGTH];
   int fd      = epoll_create(32);
   bool init_ok;

   if (fd < 0)
      return NULL;

   linuxraw_epoll = fd;
   _len           = strlcpy(path, "/dev/input/js", sizeof(path));

   for (i = 0; i < MAX_USERS; i++)
   {
      struct linuxraw_joypad *pad = (struct linuxraw_joypad*)&linuxraw_pads[i];

      pad->fd    = -1;
      pad->ident = input_config_get_device_name_ptr(i);

      snprintf(path + _len, sizeof(path) - _len, "%u", (uint32_t)i);

      init_ok = linuxraw_joypad_init_pad(path, pad);

      RARCH_DBG("[linuxraw]: scanning path \"%s\", ident \"%s\".\n",path,pad->ident);
      input_autoconfigure_connect(pad->ident, NULL, "linuxraw",
            i, 0, 0);

      if (init_ok)
         linuxraw_poll_pad(pad);
   }

   linuxraw_inotify = inotify_init();

   if (linuxraw_inotify >= 0)
   {
      struct epoll_event event;

      fcntl(linuxraw_inotify, F_SETFL, fcntl(linuxraw_inotify, F_GETFL) | O_NONBLOCK);
      inotify_add_watch(linuxraw_inotify, "/dev/input", IN_DELETE | IN_CREATE | IN_ATTRIB);

      event.events             = EPOLLIN;
      event.data.ptr           = NULL;

      /* Shouldn't happen, but just check it. */
      if (epoll_ctl(linuxraw_epoll, EPOLL_CTL_ADD, linuxraw_inotify, &event) < 0)
      {
         RARCH_ERR("Failed to add FD (%d) to epoll list (%s).\n",
               linuxraw_inotify, strerror(errno));
      }
   }

   linuxraw_hotplug = true;

   return (void*)-1;
}

static void linuxraw_joypad_destroy(void)
{
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (linuxraw_pads[i].fd >= 0)
         close(linuxraw_pads[i].fd);
   }

   memset(linuxraw_pads, 0, sizeof(linuxraw_pads));

   for (i = 0; i < MAX_USERS; i++)
      linuxraw_pads[i].fd = -1;

   if (linuxraw_inotify >= 0)
      close(linuxraw_inotify);
   linuxraw_inotify = -1;

   if (linuxraw_epoll >= 0)
      close(linuxraw_epoll);
   linuxraw_epoll = -1;

   linuxraw_hotplug = false;
}

static int32_t linuxraw_joypad_button(unsigned port, uint16_t joykey)
{
   const struct linuxraw_joypad    *pad = (const struct linuxraw_joypad*)
      &linuxraw_pads[port];
   if (port >= MAX_USERS)
      return 0;
   if (joykey < NUM_BUTTONS)
      return (BIT32_GET(pad->buttons, joykey));
   return 0;
}

static void linuxraw_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	const struct linuxraw_joypad *pad = (const struct linuxraw_joypad*)
      &linuxraw_pads[port];

	if (pad)
   {
		BITS_COPY16_PTR(state, pad->buttons);
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t linuxraw_joypad_axis_state(
      const struct linuxraw_joypad *pad,
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

static int16_t linuxraw_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const struct linuxraw_joypad *pad = (const struct linuxraw_joypad*)
      &linuxraw_pads[port];
   return linuxraw_joypad_axis_state(pad, port, joyaxis);
}

static int16_t linuxraw_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   const struct linuxraw_joypad    *pad = (const struct linuxraw_joypad*)
      &linuxraw_pads[port_idx];

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
            ((float)abs(linuxraw_joypad_axis_state(pad, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool linuxraw_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && linuxraw_pads[pad].fd >= 0;
}

input_device_driver_t linuxraw_joypad = {
   linuxraw_joypad_init,
   linuxraw_joypad_query_pad,
   linuxraw_joypad_destroy,
   linuxraw_joypad_button,
   linuxraw_joypad_state,
   linuxraw_joypad_get_buttons,
   linuxraw_joypad_axis,
   linuxraw_joypad_poll,
   NULL,
   NULL,
   linuxraw_joypad_name,
   "linuxraw",
};
