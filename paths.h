/*  RetroArch - A frontend for libretro.
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

#ifndef __PATHS_H
#define __PATHS_H

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

void path_set_redirect(void);

void path_set_basename(const char *path);

const char *path_get_current_savefile_dir(void);

RETRO_END_DECLS

#endif
