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
#include <sys/types.h>
#include <fcntl.h>
#include <linux/joystick.h>

#define NUM_BUTTONS 32
#define NUM_AXES 32

struct linuxraw_joypad
{
   int fd;
   bool buttons[NUM_BUTTONS];
   int16_t axes[NUM_AXES];
};

static struct linuxraw_joypad g_pads[MAX_PLAYERS];

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

static void linuxraw_joypad_poll(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      struct linuxraw_joypad *pad = &g_pads[i];
      if (pad->fd < 0)
         continue;

      poll_pad(pad);
   }
}

static bool linuxraw_joypad_init(void)
{
   bool has_pad = false;

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      struct linuxraw_joypad *pad = &g_pads[i];

      char path[PATH_MAX];
      snprintf(path, sizeof(path), "/dev/input/js%u", i);
      pad->fd = open(path, O_RDONLY | O_NONBLOCK);

      has_pad |= pad->fd >= 0;
   }

   // Get initial state.
   if (has_pad)
      linuxraw_joypad_poll();

   return has_pad;
}

static void linuxraw_joypad_destroy(void)
{
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].fd >= 0)
         close(g_pads[i].fd);
   }

   memset(g_pads, 0, sizeof(g_pads));
   for (unsigned i = 0; i < MAX_PLAYERS; i++)
      g_pads[i].fd = -1;
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

const rarch_joypad_driver_t linuxraw_joypad = {
   linuxraw_joypad_init,
   linuxraw_joypad_query_pad,
   linuxraw_joypad_destroy,
   linuxraw_joypad_button,
   linuxraw_joypad_axis,
   linuxraw_joypad_poll,
   "linuxraw",
};

