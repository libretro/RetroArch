/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef RARCH_VIDEO_THREAD_H__
#define RARCH_VIDEO_THREAD_H__

#include <limits.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "font_driver.h"

RETRO_BEGIN_DECLS

typedef int (*custom_command_method_t)(void*);

typedef bool (*custom_font_command_method_t)(const void **font_driver,
      void **font_handle, void *video_data, const char *font_path,
      float font_size, enum font_driver_render_api api,
      bool is_threaded);

typedef struct thread_packet thread_packet_t;

typedef struct thread_video thread_video_t;

/**
 * video_init_thread:
 * @out_driver                : Output video driver
 * @out_data                  : Output video data
 * @input                     : Input input driver
 * @input_data                : Input input data
 * @driver                    : Input Video driver
 * @info                      : Video info handle.
 *
 * Creates, initializes and starts a video driver in a new thread.
 * Access to video driver will be mediated through this driver.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_init_thread(
      const video_driver_t **out_driver, void **out_data,
      input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t info);

/**
 * video_thread_get_ptr:
 * @drv                       : Found driver.
 *
 * Gets the underlying video driver associated with the
 * threaded video wrapper. Sets @drv to the found
 * video driver.
 *
 * Returns: Video driver data of the video driver associated
 * with the threaded wrapper (if successful). If not successful,
 * NULL.
 **/
void *video_thread_get_ptr(const video_driver_t **drv);

const char *video_thread_get_ident(void);

bool video_thread_font_init(
      const void **font_driver,
      void **font_handle,
      void *data,
      const char *font_path,
      float font_size,
      enum font_driver_render_api api,
      custom_font_command_method_t func,
      bool is_threaded);

unsigned video_thread_texture_load(void *data,
      custom_command_method_t func);

RETRO_END_DECLS

#endif
