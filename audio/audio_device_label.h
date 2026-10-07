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

#ifndef __AUDIO_DEVICE_LABEL_H
#define __AUDIO_DEVICE_LABEL_H

#include <stddef.h>
#include <string.h>
#include <retro_inline.h>
#include <compat/strl.h>

/* A device list entry can be a path with the hardware's name after
 * it in brackets - "/dev/dsp0 (HDA Intel)" - which is how oss and
 * audioio list their nodes so the user can tell them apart. The menu
 * stores the entry as shown, and it comes back to init as the device
 * string, so a driver that opens the string as a path takes the path
 * off the front first: everything before the first " (". A string
 * without one is a path already and is copied as it is. Returns the
 * length of the path. */
static INLINE size_t audio_device_label_path(char *dst, size_t len,
      const char *entry)
{
   const char *cut = strstr(entry, " (");
   size_t n        = cut ? (size_t)(cut - entry) : strlen(entry);
   if (n >= len)
      n = len - 1;
   memcpy(dst, entry, n);
   dst[n] = '\0';
   return n;
}

#endif
