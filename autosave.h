/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __RARCH_AUTOSAVE_H
#define __RARCH_AUTOSAVE_H

#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/**
 * autosave_lock:
 *
 * Lock autosave.
 **/
void autosave_lock(void);

/**
 * autosave_unlock:
 *
 * Unlocks autosave.
 **/
void autosave_unlock(void);

bool autosave_init(void);

void autosave_deinit(void);

RETRO_END_DECLS

#endif
