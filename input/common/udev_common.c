/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdio.h>
#include <sys/poll.h>

#include "udev_common.h"

static bool udev_mon_inited;
static bool udev_mon_first_inited_is_joypad;
static struct udev_monitor *g_udev_mon;
static struct udev *g_udev;

void udev_mon_free(bool is_joypad)
{
   if (!udev_mon_inited || (is_joypad && !udev_mon_first_inited_is_joypad))
      return;

   if (g_udev_mon)
      udev_monitor_unref(g_udev_mon);
   if (g_udev)
      udev_unref(g_udev);

   g_udev_mon                      = NULL;
   g_udev                          = NULL;
   udev_mon_inited                 = false;
   udev_mon_first_inited_is_joypad = false;
}

static bool udev_mon_hotplug_available(void)
{
   struct pollfd fds = {0};

   if (!g_udev_mon)
      return false;

   fds.fd     = udev_monitor_get_fd(g_udev_mon);
   fds.events = POLLIN;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}

struct udev_device *udev_mon_receive_device(void)
{
   return udev_monitor_receive_device(g_udev_mon);
}

struct udev_enumerate *udev_mon_enumerate(void)
{
   return  udev_enumerate_new(g_udev);
}

/* Get the filename of the /sys entry for the device
 * and create a udev_device object (dev) representing it. */
struct udev_device *udev_mon_device_new(const char *name)
{
   return udev_device_new_from_syspath(g_udev, name);
}

bool udev_ctl(enum udev_ctl_state state, void *data)
{
   switch (state)
   {
      case UDEV_CTL_MONITOR_HOTPLUG_AVAIL:
         return udev_mon_hotplug_available();
      case UDEV_CTL_NONE:
      default:
         return false;
   }

   return true;
}
