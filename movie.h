/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_MOVIE_H
#define __SSNES_MOVIE_H

#include <stdint.h>
#include <stddef.h>
#include "boolean.h"

typedef struct bsv_movie bsv_movie_t;

enum ssnes_movie_type
{
   SSNES_MOVIE_PLAYBACK,
   SSNES_MOVIE_RECORD
};

uint32_t *bsv_header_generate(size_t *size, uint32_t magic);
bool bsv_parse_header(const uint32_t *header, uint32_t magic);

bsv_movie_t *bsv_movie_init(const char *path, enum ssnes_movie_type type);

// Playback
bool bsv_movie_get_input(bsv_movie_t *handle, int16_t *input);

// Recording
void bsv_movie_set_input(bsv_movie_t *handle, int16_t input);

// Used for rewinding while playback/record.
void bsv_movie_set_frame_start(bsv_movie_t *handle); // Debugging purposes.
void bsv_movie_set_frame_end(bsv_movie_t *handle);
void bsv_movie_frame_rewind(bsv_movie_t *handle);

void bsv_movie_free(bsv_movie_t *handle);

#endif

