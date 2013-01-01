/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

typedef struct autosave autosave_t;

autosave_t *autosave_new(const char *path, const void *data, size_t size, unsigned interval);
void autosave_lock(autosave_t *handle);
void autosave_unlock(autosave_t *handle);
void autosave_free(autosave_t *handle);

void lock_autosave(void);
void unlock_autosave(void);

#endif
