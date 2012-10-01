/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#if defined(__CELLOS_LV2__) || defined(GEKKO)
#ifndef _PPU_INTRINSICS_H
#include <ppu_intrinsics.h>
#endif
#endif

unsigned long long rarch_get_performance_counter(void)
{
#if defined(__CELLOS_LV2__) || defined(GEKKO)
   unsigned long long time = __mftb();
#endif
   return time;
}
