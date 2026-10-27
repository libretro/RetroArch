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
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

/* Controllers another program stands in for. Steam Input, starting a
 * game with a controller it translates, makes a virtual pad of it and
 * puts the real one's USB ids in SDL_GAMECONTROLLER_IGNORE_DEVICES, so
 * that the game reads the virtual pad and not both: "0x054c/0x09cc,
 * 0x057e/0x2009". SDL's games honour it; so do the joypad drivers
 * here that know a controller's ids, at the moment it connects. A
 * user may set it too, for a controller another program mirrors - or
 * pick the controller in Input Information, which keeps it in the
 * input_ignore_devices setting, written the same way.
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

/* The @n-th entry of @list, the vendor and product ids it names;
 * false past the last. Entries that cannot be read are passed over. */
static INLINE bool input_ignore_devices_at(const char *list, unsigned n,
      uint16_t *vid, uint16_t *pid)
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
         if (end != ps && !n--)
         {
            *vid = (uint16_t)v;
            *pid = (uint16_t)p;
            return true;
         }
      }
      while (*s && *s != ',')
         s++;
   }
   return false;
}

/* @vid / @pid added to @list, a buffer of @size; false where there is
 * no room. One already there is not added again. */
static INLINE bool input_ignore_devices_add(char *list, size_t size,
      uint16_t vid, uint16_t pid)
{
   static const char hex[] = "0123456789abcdef";
   char entry[12];
   size_t len = strlen(list), at = 0;
   int i;

   if (input_ignore_devices_names(list, vid, pid))
      return true;
   if (len)
      entry[at++] = ',';
   for (i = 12; i >= 0; i -= 4)
      entry[at++] = hex[(vid >> i) & 15];
   entry[at++] = '/';
   for (i = 12; i >= 0; i -= 4)
      entry[at++] = hex[(pid >> i) & 15];
   entry[at] = '\0';
   if (len + at + 1 > size)
      return false;
   memcpy(list + len, entry, at + 1);
   return true;
}

/* Every entry of @list naming @vid / @pid taken out of it */
static INLINE void input_ignore_devices_remove(char *list,
      uint16_t vid, uint16_t pid)
{
   char *out      = list;
   const char *s  = list;
   while (*s)
   {
      const char *start;
      char *end;
      unsigned long v, p = 0;
      bool match = false;
      while (*s == ' ' || *s == ',')
         s++;
      if (!*s)
         break;
      start = s;
      v     = strtoul(s, &end, 16);
      if (end != s && *end == '/')
      {
         const char *ps = end + 1;
         p     = strtoul(ps, &end, 16);
         match = end != ps && v == vid && p == pid;
      }
      while (*s && *s != ',')
         s++;
      if (!match)
      {
         if (out != list)
            *out++ = ',';
         memmove(out, start, (size_t)(s - start));
         out += s - start;
      }
   }
   *out = '\0';
}

/* Whether @vid / @pid is a controller to leave alone: one Steam Input
 * (or any program, through SDL_GAMECONTROLLER_IGNORE_DEVICES) stands
 * in for, or one in @user, the list the user chose to ignore */
static INLINE bool input_ignore_devices_check(const char *user,
      uint16_t vid, uint16_t pid)
{
   if (!vid && !pid)
      return false;
#if !defined(__WINRT__) && !defined(_XBOX)
   if (input_ignore_devices_names(
            getenv("SDL_GAMECONTROLLER_IGNORE_DEVICES"), vid, pid))
      return true;
#endif
   return input_ignore_devices_names(user, vid, pid);
}

/* input_ignore_devices_check() with the user's list (input_driver.c) */
RETRO_BEGIN_DECLS
bool input_device_ignored(uint16_t vid, uint16_t pid);
RETRO_END_DECLS

#endif
