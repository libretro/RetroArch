/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <boolean.h>
#include <compat/msvc.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../record_driver.h"

static void record_null_free(void *data)
{
}

static void *record_null_new(const struct ffemu_params *params)
{
   return NULL;
}

static bool record_null_push_video(void *data,
      const struct ffemu_video_data *video_data)
{
   return false;
}

static bool record_null_push_audio(void *data,
      const struct ffemu_audio_data *audio_data)
{
   return false;
}

static bool record_null_finalize(void *data)
{
   return false;
}

const record_driver_t ffemu_null = {
   record_null_new,
   record_null_free,
   record_null_push_video,
   record_null_push_audio,
   record_null_finalize,
   "null",
};
