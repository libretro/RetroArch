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
#include <sys/inotify.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <linux/joystick.h>

#define NUM_BUTTONS 32
#define NUM_AXES 32

struct linuxraw_joypad
{
   int fd;
   bool buttons[NUM_BUTTONS];
   int16_t axes[NUM_AXES];

   char *ident;
};

static struct linuxraw_joypad g_pads[MAX_PLAYERS];
static int g_notify;
static int g_epoll;
static bool g_hotplug;

static void poll_pad(struct linuxraw_joypad *pad)
{
   struct js_event event;
   while (read(pad->fd, &event, sizeof(event)) == (ssize_t)sizeof(event))
   {
      unsigned type = event.type & ~JS_EVENT_INIT;

      switch (type)
      {
         case JS_EVENT_BUTTON:
            if (event.number < NUM_BUTTONS)
               pad->buttons[event.number] = event.value;
            break;

         case JS_EVENT_AXIS:
            if (event.number < NUM_AXES)
               pad->axes[event.number] = event.value;
            break;
      }
   }
}

static bool linuxraw_joypad_init_pad(const char *path, struct linuxraw_joypad *pad)
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

#ifndef IS_JOYCONFIG
         if (g_hotplug)
         {
            char msg[512];
            snprintf(msg, sizeof(msg), "Joypad #%u (%s) connected.", (unsigned)(pad - g_pads), pad->ident);
            msg_queue_push(g_extern.msg_queue, msg, 0, 60);
         }
#endif
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
   int i, rc;
   size_t event_size = sizeof(struct inotify_event) + NAME_MAX + 1;
   uint8_t *event_buf = (uint8_t*)calloc(1, event_size);
   if (!event_buf)
      return;

   while ((rc = read(g_notify, event_buf, event_size)) >= 0)
   {
      struct inotify_event *event = NULL;
      // Can read multiple events in one read() call.
      for (i = 0; i < rc; i += event->len + sizeof(struct inotify_event))
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
#ifndef IS_JOYCONFIG
               if (g_hotplug)
               {
                  char msg[512];
                  snprintf(msg, sizeof(msg), "Joypad #%u (%s) disconnected.", index, g_pads[index].ident);
                  msg_queue_push(g_extern.msg_queue, msg, 0, 60);
               }
#endif

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
            bool ret = linuxraw_joypad_init_pad(path, &g_pads[index]);

            if (*g_pads[index].ident && ret)
               input_config_autoconfigure_joypad(index, g_pads[index].ident, "linuxraw");
         }
      }
   }

   free(event_buf);
}

static void linuxraw_joypad_poll(void)
{
   int i, ret;
   struct epoll_event events[MAX_PLAYERS + 1];

retry:
   ret = epoll_wait(g_epoll, events, MAX_PLAYERS + 1, 0);
   if (ret < 0 && errno == EINTR)
      goto retry;

   for (i = 0; i < ret; i++)
   {
      if (events[i].data.ptr)
         poll_pad((struct linuxraw_joypad*)events[i].data.ptr);
      else
         handle_plugged_pad();
   }
}

static void linuxraw_joypad_setup_notify(void)
{
   fcntl(g_notify, F_SETFL, fcntl(g_notify, F_GETFL) | O_NONBLOCK);
   inotify_add_watch(g_notify, "/dev/input", IN_DELETE | IN_CREATE | IN_ATTRIB);
}

static bool linuxraw_joypad_init(void)
{
   unsigned i;
   g_epoll = epoll_create(MAX_PLAYERS + 1);
   if (g_epoll < 0)
      return false;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      struct linuxraw_joypad *pad = &g_pads[i];
      pad->fd = -1;
      pad->ident = g_settings.input.device_names[i];
      
      char path[PATH_MAX];
      snprintf(path, sizeof(path), "/dev/input/js%u", i);

      if (linuxraw_joypad_init_pad(path, pad))
      {
         input_config_autoconfigure_joypad(i, pad->ident, "linuxraw");
         poll_pad(pad);
      }
      else
         input_config_autoconfigure_joypad(i, NULL, NULL);
   }

   g_notify = inotify_init();
   if (g_notify >= 0)
   {
      linuxraw_joypad_setup_notify();

      struct epoll_event event;
      event.events = EPOLLIN;
      event.data.ptr = NULL;
      epoll_ctl(g_epoll, EPOLL_CTL_ADD, g_notify, &event);
   }

   g_hotplug = true;

   return true;
}

static void linuxraw_joypad_destroy(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].fd >= 0)
         close(g_pads[i].fd);
   }

   memset(g_pads, 0, sizeof(g_pads));
   for (i = 0; i < MAX_PLAYERS; i++)
      g_pads[i].fd = -1;

   if (g_notify >= 0)
      close(g_notify);
   g_notify = -1;

   if (g_epoll >= 0)
      close(g_epoll);
   g_epoll = -1;

   g_hotplug = false;
}

static bool linuxraw_joypad_button(unsigned port, uint16_t joykey)
{
   const struct linuxraw_joypad *pad = &g_pads[port];

   return joykey < NUM_BUTTONS && pad->buttons[joykey];
}

static int16_t linuxraw_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct linuxraw_joypad *pad = &g_pads[port];

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

static bool linuxraw_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].fd >= 0;
}

static const char *linuxraw_joypad_name(unsigned pad)
{
   if (pad >= MAX_PLAYERS)
      return NULL;

   return *g_pads[pad].ident ? g_pads[pad].ident : NULL;
}

const rarch_joypad_driver_t linuxraw_joypad = {
   linuxraw_joypad_init,
   linuxraw_joypad_query_pad,
   linuxraw_joypad_destroy,
   linuxraw_joypad_button,
   linuxraw_joypad_axis,
   linuxraw_joypad_poll,
   NULL,
   linuxraw_joypad_name,
   "linuxraw",
};

