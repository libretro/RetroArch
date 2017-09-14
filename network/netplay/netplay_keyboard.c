/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2017 - Gregor Richards
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

#include "netplay_private.h"

/* The mapping of keys from netplay (network) to libretro (host) */
const uint16_t netplay_key_ntoh_mapping[] = {
   (uint16_t) RETROK_UNKNOWN,
#define K(k) (uint16_t) RETROK_ ## k,
#define KL(k,l) (uint16_t) l,
#include "netplay_keys.h"
#undef KL
#undef K
   0
};

static bool mapping_defined = false;
static uint16_t mapping[RETROK_LAST];

/* The mapping of keys from libretro (host) to netplay (network) */
uint32_t netplay_key_hton(unsigned key)
{
   if (key >= RETROK_LAST)
      return NETPLAY_KEY_UNKNOWN;
   return mapping[key];
}

/* Because the hton keymapping has to be generated, call this before using
 * netplay_key_hton */
void netplay_key_hton_init(void)
{
   if (!mapping_defined)
   {
      uint16_t i;
      for (i = 0; i < NETPLAY_KEY_LAST; i++)
         mapping[netplay_key_ntoh(i)] = i;
      mapping_defined = true;
   }
}
