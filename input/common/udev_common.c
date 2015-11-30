#include <stdio.h>

#include <sys/poll.h>

#include "udev_common.h"

static bool udev_mon_inited;
struct udev_monitor *g_udev_mon;
struct udev *g_udev;

bool udev_mon_new(void)
{
   if (udev_mon_inited)
      return true;

   g_udev = udev_new();
   if (!g_udev)
      return false;

   g_udev_mon = udev_monitor_new_from_netlink(g_udev, "udev");
   if (g_udev_mon)
   {
      udev_monitor_filter_add_match_subsystem_devtype(g_udev_mon, "input", NULL);
      udev_monitor_enable_receiving(g_udev_mon);
   }

   udev_mon_inited = true;

   return true;
}

void udev_mon_free(bool is_joypad)
{
   if (!udev_mon_inited || is_joypad)
      return;

   if (g_udev_mon)
      udev_monitor_unref(g_udev_mon);
   if (g_udev)
      udev_unref(g_udev);

   g_udev_mon      = NULL;
   g_udev          = NULL;
   udev_mon_inited = false;
}

bool udev_mon_hotplug_available(void)
{
   struct pollfd fds = {0};

   if (!g_udev_mon)
      return false;

   fds.fd     = udev_monitor_get_fd(g_udev_mon);
   fds.events = POLLIN;

   return (poll(&fds, 1, 0) == 1) && (fds.revents & POLLIN);
}
