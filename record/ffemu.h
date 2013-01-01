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

#ifndef __FFEMU_H
#define __FFEMU_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ffemu_pix_format
{
   FFEMU_PIX_RGB565 = 0,
   FFEMU_PIX_BGR24,
   FFEMU_PIX_ARGB8888
};

// Parameters passed to ffemu_new()
struct ffemu_params
{
   // FPS of input video.
   double fps;
   // Sample rate of input audio.
   double samplerate;

   // Desired output resolution.
   unsigned out_width;
   unsigned out_height;

   // Total size of framebuffer used in input.
   unsigned fb_width;
   unsigned fb_height;

   // Aspect ratio of input video. Parameters are passed to the muxer,
   // the video itself is not scaled.
   float aspect_ratio;

   // Audio channels.
   unsigned channels;

   // Input pixel format.
   enum ffemu_pix_format pix_fmt;

   // Filename to dump to.
   const char *filename;

   // Path to config. Optional.
   const char *config;
};

struct ffemu_video_data
{
   const void *data;
   unsigned width;
   unsigned height;
   int pitch;
   bool is_dupe;
};

struct ffemu_audio_data
{
   const void *data;
   size_t frames;
};

typedef struct ffemu ffemu_t;

ffemu_t *ffemu_new(const struct ffemu_params *params);
void ffemu_free(ffemu_t* handle);

bool ffemu_push_video(ffemu_t *handle, const struct ffemu_video_data *data);
bool ffemu_push_audio(ffemu_t *handle, const struct ffemu_audio_data *data);
bool ffemu_finalize(ffemu_t *handle);

#ifdef __cplusplus
}
#endif

#endif
