/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
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

#ifndef __VIDEO_MODELINE_INI_H
#define __VIDEO_MODELINE_INI_H

#include <retro_common_api.h>

#include "modeline_core.h"

RETRO_BEGIN_DECLS

/* Load a "key value" ini into the generator. The file name is tried as
 * given, then in ./, ./ini/ and (outside Windows) /etc/. Lines
 * starting with # are comments. Returns true if a file was found;
 * every key goes through modeline_set_option(). Call
 * modeline_parse_options() afterwards to re-derive ranges. The
 * on-disk name stays switchres.ini and *.switchres.ini. */
bool modeline_ini_load(video_modeline_gen_t *gen, const char *file_name);

/* Parse one ini image already in memory (same grammar). */
void modeline_ini_parse_buffer(video_modeline_gen_t *gen,
      const char *buf, size_t len);

RETRO_END_DECLS

#endif
