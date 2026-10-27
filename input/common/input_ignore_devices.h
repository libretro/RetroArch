/*  RetroArch - A frontend for libretro.
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

#ifndef _INPUT_IGNORE_DEVICES_H
#define _INPUT_IGNORE_DEVICES_H

#include <stdint.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>

/* Controllers another program stands in for. Steam Input, starting a
 * game with a controller it translates, makes a virtual pad of it and
 * puts the real one's USB ids in SDL_GAMECONTROLLER_IGNORE_DEVICES, so
 * that the game reads the virtual pad and not both: "0x054c/0x09cc,
 * 0x057e/0x2009". SDL's games honour it; so do the joypad drivers
 * here that know a controller's ids, at the moment it connects. A
 * user may set it too, for a controller another program mirrors.
 *
 * Read where a controller connects, never on a frame. */

/* Whether @list, as the variable writes it, names @vid / @pid */
static INLINE bool input_ignore_devices_names(const char *list,
      uint16_t vid, uint16_t pid)
{
   const char *s = list;
   while (s && *s)
   {
      char *end;
      unsigned long v, p;
      while (*s == ' ' || *s == ',')
         s++;
      if (!*s)
         break;
      v = strtoul(s, &end, 16);
      if (end != s && *end == '/')
      {
         const char *ps = end + 1;
         p = strtoul(ps, &end, 16);
         if (end != ps && v == vid && p == pid)
            return true;
      }
      /* on to the next entry, read or not */
      while (*s && *s != ',')
         s++;
   }
   return false;
}

static INLINE bool input_device_ignored(uint16_t vid, uint16_t pid)
{
#if defined(__WINRT__) || defined(_XBOX)
   (void)vid; (void)pid;
   return false;
#else
   return (vid || pid) && input_ignore_devices_names(
         getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES"), vid, pid);
#endif
}

#endif
