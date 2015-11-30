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
#ifndef __RECORD_DRIVER_H
#define __RECORD_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

enum ffemu_pix_format
{
   FFEMU_PIX_RGB565 = 0,
   FFEMU_PIX_BGR24,
   FFEMU_PIX_ARGB8888
};

/* Parameters passed to ffemu_new() */
struct ffemu_params
{
   /* Framerate per second of input video. */
   double fps;
   /* Sample rate of input audio. */
   double samplerate;

   /* Desired output resolution. */
   unsigned out_width;
   unsigned out_height;

   /* Total size of framebuffer used in input. */
   unsigned fb_width;
   unsigned fb_height;

   /* Aspect ratio of input video. Parameters are passed to the muxer,
    * the video itself is not scaled.
    */
   float aspect_ratio;

   /* Audio channels. */
   unsigned channels;

   /* Input pixel format. */
   enum ffemu_pix_format pix_fmt;

   /* Filename to dump to. */
   const char *filename;

   /* Path to config. Optional. */
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

typedef struct record_driver
{
   void *(*init)(const struct ffemu_params *params);
   void  (*free)(void *data);
   bool  (*push_video)(void *data,const struct ffemu_video_data *video_data);
   bool  (*push_audio)(void *data, const struct ffemu_audio_data *audio_data);
   bool  (*finalize)(void *data);
   const char *ident;
} record_driver_t;

extern const record_driver_t ffemu_ffmpeg;
extern const record_driver_t ffemu_null;

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void);

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
const record_driver_t *ffemu_find_backend(const char *ident);

/**
 * record_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to record driver at index. Can be NULL
 * if nothing found.
 **/
const void *record_driver_find_handle(int idx);

/**
 * record_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of record driver at index. Can be NULL
 * if nothing found.
 **/
const char *record_driver_find_ident(int idx);

/**
 * gfx_ctx_init_first:
 * @backend                 : Recording backend handle.
 * @data                    : Recording data handle.
 * @params                  : Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool record_driver_init_first(const record_driver_t **backend, void **data,
      const struct ffemu_params *params);

void recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch);

bool recording_deinit(void);

void find_record_driver(void);

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool recording_init(void);

bool *recording_is_enabled(void);

void recording_set_state(bool state);

void recording_push_audio(const int16_t *data, size_t samples);

#ifdef __cplusplus
}
#endif

#endif
