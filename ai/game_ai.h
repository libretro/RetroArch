/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2021      - David G.F.
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

#ifndef RARCH_GAME_AI_H__
#define RARCH_GAME_AI_H__

#include <boolean.h>
#include <retro_common_api.h>

#include <libretro.h>

RETRO_BEGIN_DECLS

signed short int game_ai_input(unsigned int port, unsigned int device,
      unsigned int idx, unsigned int id, signed short int result);

void game_ai_init(void);

void game_ai_shutdown(void);

void game_ai_load(const char * name, void * ram_ptr,
      int ram_size, retro_log_printf_t log);

void game_ai_think(bool override_p1, bool override_p2, bool show_debug,
      const void *frame_data, unsigned int frame_w, unsigned int frame_h,
      unsigned int frame_pitch, unsigned int pixel_format);

RETRO_END_DECLS

#endif
