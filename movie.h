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

#ifndef __RARCH_MOVIE_H
#define __RARCH_MOVIE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

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

enum bsv_ctl_state
{
   BSV_MOVIE_CTL_NONE = 0,
   BSV_MOVIE_CTL_IS_INITED,
   BSV_MOVIE_CTL_PLAYBACK_ON,
   BSV_MOVIE_CTL_PLAYBACK_OFF,
   /* Playback. */
   BSV_MOVIE_CTL_GET_INPUT,
   /* Recording. */
   BSV_MOVIE_CTL_SET_INPUT,
   BSV_MOVIE_CTL_SET_START_RECORDING,
   BSV_MOVIE_CTL_UNSET_START_RECORDING,
   BSV_MOVIE_CTL_START_RECORDING,
   BSV_MOVIE_CTL_SET_START_PLAYBACK,
   BSV_MOVIE_CTL_UNSET_START_PLAYBACK,
   BSV_MOVIE_CTL_START_PLAYBACK,
   BSV_MOVIE_CTL_UNSET_PLAYBACK,
   BSV_MOVIE_CTL_SET_FRAME_START,
   BSV_MOVIE_CTL_SET_FRAME_END,
   BSV_MOVIE_CTL_FRAME_REWIND,
   BSV_MOVIE_CTL_DEINIT,
   BSV_MOVIE_CTL_INIT,
   BSV_MOVIE_CTL_END_EOF,
   BSV_MOVIE_CTL_SET_END_EOF,
   BSV_MOVIE_CTL_END,
   BSV_MOVIE_CTL_SET_END,
   BSV_MOVIE_CTL_UNSET_END
};

const char *bsv_movie_get_path(void);

void bsv_movie_set_path(const char *path);

void bsv_movie_set_start_path(const char *path);

void bsv_movie_free(bsv_movie_t *handle);

bool bsv_movie_ctl(enum bsv_ctl_state state, void *data);

bool bsv_movie_init_handle(const char *path, enum rarch_movie_type type);

#ifdef __cplusplus
}
#endif

#endif

