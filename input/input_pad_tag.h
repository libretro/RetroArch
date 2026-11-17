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

#ifndef _INPUT_PAD_TAG_H
#define _INPUT_PAD_TAG_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <boolean.h>
#include <retro_inline.h>

/* A controller's tag tells it from connected ones of its name: the
 * last 6 letters and digits of its own serial or Bluetooth address
 * (input_pad_tag_uniq()), else 6 hex digits of its identity hash. A
 * controller keeps its tag across runs and versions only while the
 * hash and these rules stay as they are: do not change them without
 * reason. */
#define INPUT_PAD_TAG_LEN 6

/* FNV-1a, 32 bits, of @len bytes at @p, from @h (0x811c9dc5 to start) */
static INLINE uint32_t input_pad_tag_fnv1a(uint32_t h,
      const unsigned char *p, size_t len)
{
   size_t i;
   for (i = 0; i < len; i++)
   {
      h ^= p[i];
      h *= 0x01000193UL;
   }
   return h;
}

/* @uniq's last INPUT_PAD_TAG_LEN letters and digits, lowercased, into
 * @s (INPUT_PAD_TAG_LEN + 1 bytes): a Bluetooth address's own half.
 * False with fewer, or all one character (a placeholder such as
 * 00:00:00:00:00:00): such a serial tells nothing apart. */
static INLINE bool input_pad_tag_uniq(const char *uniq, char *s)
{
   size_t i;
   size_t n = INPUT_PAD_TAG_LEN;
   if (!uniq)
      return false;
   s[n] = '\0';
   for (i = strlen(uniq); i > 0 && n > 0; i--)
   {
      char c = uniq[i - 1];
      if (c >= 'A' && c <= 'Z')
         s[--n] = (char)(c - 'A' + 'a');
      else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
         s[--n] = c;
   }
   if (n)
      return false;
   for (i = 1; i < INPUT_PAD_TAG_LEN; i++)
      if (s[i] != s[0])
         return true;
   return false;
}

/* A controller's identity: its vendor and product ids, then its own
 * serial or Bluetooth address (@uniq) when input_pad_tag_uniq() takes
 * it, else where it is plugged in (@phys). 0 for none: @uniq NULL (the
 * driver cannot tell controllers of one model apart) or neither given.
 * Never 0 else. */
static INLINE uint32_t input_pad_tag_ident(unsigned vid, unsigned pid,
      const char *uniq, const char *phys)
{
   uint32_t h;
   unsigned char ids[4];
   char end[INPUT_PAD_TAG_LEN + 1];
   const char *id = input_pad_tag_uniq(uniq, end) ? uniq : phys;
   if (!uniq || !id || !*id)
      return 0;
   ids[0] = (unsigned char)(vid        & 0xff);
   ids[1] = (unsigned char)((vid >> 8) & 0xff);
   ids[2] = (unsigned char)(pid        & 0xff);
   ids[3] = (unsigned char)((pid >> 8) & 0xff);
   h      = input_pad_tag_fnv1a(0x811c9dc5UL, ids, sizeof(ids));
   h      = input_pad_tag_fnv1a(h, (const unsigned char*)id, strlen(id));
   return h ? h : 1;
}

/* The tag of the controller input_pad_tag_ident() gave @ident from
 * @uniq, into @s (INPUT_PAD_TAG_LEN + 1 bytes); "" for no identity.
 * Returns @s. */
static INLINE char *input_pad_tag_text(uint32_t ident, const char *uniq,
      char *s)
{
   unsigned i;
   uint32_t h = ident;
   if (!ident)
      *s = '\0';
   else if (!input_pad_tag_uniq(uniq, s))
   {
      /* murmur3's fmix32: ports a last character apart differ too */
      h ^= h >> 16;
      h *= 0x85ebca6bUL;
      h ^= h >> 13;
      h *= 0xc2b2ae35UL;
      h ^= h >> 16;
      for (i = 0; i < INPUT_PAD_TAG_LEN; i++)
      {
         unsigned d = (unsigned)(h >> (20 - 4 * i)) & 0xf;
         s[i] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
      }
      s[INPUT_PAD_TAG_LEN] = '\0';
   }
   return s;
}

#endif
