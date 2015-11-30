#include <stdio.h>
#include "udev_common.h"

struct udev_monitor *g_udev_mon;
struct udev *g_udev;

bool udev_mon_new(void)
{
   g_udev = udev_new();
   if (!g_udev)
      return false;

   g_udev_mon = udev_monitor_new_from_netlink(g_udev, "udev");
   if (g_udev_mon)
   {
      udev_monitor_filter_add_match_subsystem_devtype(g_udev_mon, "input", NULL);
      udev_monitor_enable_receiving(g_udev_mon);
   }

   return true;
}

void udev_mon_free(void)
{
   if (g_udev_mon)
      udev_monitor_unref(g_udev_mon);
   g_udev_mon = NULL;
   if (g_udev)
      udev_unref(g_udev);
   g_udev = NULL;
}
