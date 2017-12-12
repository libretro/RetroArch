/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2017 - Hans-Kristian Arntzen
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

#ifndef SLANG_PREPROCESS_H
#define SLANG_PREPROCESS_H

#include <boolean.h>
#include <retro_common_api.h>
#include "../video_driver.h"

RETRO_BEGIN_DECLS

/* Utility function to implement the same parameter reflection
 * which happens in the slang backend.
 * This does preprocess over the input file to handle #includes and so on. */
bool slang_preprocess_parse_parameters(const char *shader_path,
      struct video_shader *shader);

RETRO_END_DECLS

#endif

