/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_FIXUP_H
#define __RARCH_CHEEVOS_FIXUP_H

#include <stdint.h>
#include <boolean.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct
{
   unsigned address;
   const uint8_t* location;
} rcheevos_fixup_t;

typedef struct
{
   rcheevos_fixup_t* elements;
   unsigned capacity, count;
   bool dirty;
} rcheevos_fixups_t;

void rcheevos_fixup_init(rcheevos_fixups_t* fixups);
void rcheevos_fixup_destroy(rcheevos_fixups_t* fixups);

const uint8_t* rcheevos_fixup_find(rcheevos_fixups_t* fixups, unsigned address, int console);

const uint8_t* rcheevos_patch_address(unsigned address, int console);

RETRO_END_DECLS

#endif
