/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015 - Andre Leiradella
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

#ifndef __RARCH_CHEEVOS_H
#define __RARCH_CHEEVOS_H

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
   int cheats_are_enabled;
   int cheats_were_enabled;
} cheevos_globals_t;

extern cheevos_globals_t cheevos_globals;

int cheevos_load(const void *data, size_t size);

void cheevos_test(void);

void cheevos_unload(void);

#endif /* __RARCH_CHEEVOS_H */
