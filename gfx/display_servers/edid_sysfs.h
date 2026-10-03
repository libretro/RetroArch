/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* The EDID the kernel already read for a DRM connector, from
 * /sys/class/drm/<card>-<connector>/edid. Readable without root and
 * present whatever runs on top of the kernel driver, so it is the
 * fallback for the X11 server (XRandR only relays the property when
 * the driver publishes it) and the only route on Wayland, which has
 * no protocol for it. Header-only: each server that wants it includes
 * this and gets a private copy of one small static function. */

#ifndef __EDID_SYSFS_H
#define __EDID_SYSFS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __linux__
#include <dirent.h>
#endif

#include <compat/strl.h>

#ifdef __linux__
static int edid_sysfs_read_node(const char *node, uint8_t *out, size_t max)
{
   char path[512];
   size_t n = 0;
   FILE *f;
   size_t _len = strlcpy(path, "/sys/class/drm/", sizeof(path));
   _len       += strlcpy(path + _len, node, sizeof(path) - _len);
   strlcpy(path + _len, "/edid", sizeof(path) - _len);
   f = fopen(path, "rb");
   if (!f)
      return -1;
   n = fread(out, 1, max, f);
   fclose(f);
   /* whole blocks only; a connector with nothing attached reads 0 */
   n -= n % 128;
   return n ? (int)n : -1;
}

static bool edid_sysfs_node_enabled(const char *node)
{
   char path[512];
   char buf[16];
   FILE *f;
   size_t _len = strlcpy(path, "/sys/class/drm/", sizeof(path));
   _len       += strlcpy(path + _len, node, sizeof(path) - _len);
   strlcpy(path + _len, "/enabled", sizeof(path) - _len);
   f = fopen(path, "rb");
   if (!f)
      return false;
   buf[0] = '\0';
   if (!fgets(buf, sizeof(buf), f))
      buf[0] = '\0';
   fclose(f);
   return !strncmp(buf, "enabled", 7);
}
#endif

/* Read the EDID of one connector. connector is a DRM name ("HDMI-A-1",
 * "DP-2") matched against the tail of the sysfs node, or NULL for the
 * first enabled connector that has one. A disabled connector is never
 * used for NULL: the EDID shown must be the display in use, not a
 * second monitor that happens to be plugged in. Returns the bytes
 * copied, or -1. */
static int edid_sysfs_read(const char *connector, uint8_t *out, size_t max)
{
#ifdef __linux__
   DIR *dir;
   struct dirent *ent;
   int n = -1;

   if (!out || max < 128)
      return -1;
   if (!(dir = opendir("/sys/class/drm")))
      return -1;
   while ((ent = readdir(dir)))
   {
      const char *name = ent->d_name;
      const char *dash;
      if (strncmp(name, "card", 4) || !(dash = strchr(name, '-')))
         continue;
      /* "card1-HDMI-A-1": the connector is everything after the
       * first dash */
      if (connector ? (strcmp(dash + 1, connector) != 0)
                    : !edid_sysfs_node_enabled(name))
         continue;
      n = edid_sysfs_read_node(name, out, max);
      if (n > 0)
         break;
   }
   closedir(dir);
   return n;
#else
   (void)connector;
   (void)out;
   (void)max;
   return -1;
#endif
}

#endif
