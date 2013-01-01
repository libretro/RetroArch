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

#ifndef __RARCH_MOVIE_H
#define __RARCH_MOVIE_H

#include <stdint.h>
#include <stddef.h>
#include "boolean.h"

#define BSV_MAGIC 0x42535631

#define MAGIC_INDEX 0
#define SERIALIZER_INDEX 1
#define CRC_INDEX 2
#define STATE_SIZE_INDEX 3

typedef struct bsv_movie bsv_movie_t;

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK,
   RARCH_MOVIE_RECORD
};

bsv_movie_t *bsv_movie_init(const char *path, enum rarch_movie_type type);

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

