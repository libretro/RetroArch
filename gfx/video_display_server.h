/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __VIDEO_DISPLAY_SERVER__H
#define __VIDEO_DISPLAY_SERVER__H

#include <retro_common_api.h>
#include <boolean.h>

#include "video_defines.h"

RETRO_BEGIN_DECLS

enum display_server_flags
{
   DISPSERV_CTX_FLAGS_NONE = 0,
   DISPSERV_CTX_CRT_SWITCHRES
};

typedef struct video_display_config
{
   unsigned width;
   unsigned height;
   unsigned bpp;
   unsigned refreshrate;
   unsigned idx;
   bool current;
} video_display_config_t;

typedef struct video_display_server
{
   void *(*init)(void);
   void (*destroy)(void *data);
   bool (*set_window_opacity)(void *data, unsigned opacity);
   bool (*set_window_progress)(void *data, int progress, bool finished);
   bool (*set_window_decorations)(void *data, bool on);
   bool (*set_resolution)(void *data, unsigned width,
         unsigned height, int int_hz, float hz, int center, int monitor_index, int xoffset);
   void *(*get_resolution_list)(void *data,
         unsigned *size);
   const char *(*get_output_options)(void *data);
   void (*set_screen_orientation)(enum rotation rotation);
   enum rotation (*get_screen_orientation)(void);
   uint32_t (*get_flags)(void *data);
   const char *ident;
} video_display_server_t;

void* video_display_server_init(void);

void video_display_server_destroy(void);

bool video_display_server_get_flags(gfx_ctx_flags_t *flags);

bool video_display_server_set_window_opacity(unsigned opacity);

bool video_display_server_set_window_progress(int progress, bool finished);

bool video_display_server_set_window_decorations(bool on);

bool video_display_server_set_resolution(
      unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset);

void *video_display_server_get_resolution_list(unsigned *size);

const char *video_display_server_get_output_options(void);

const char *video_display_server_get_ident(void);

void video_display_server_set_screen_orientation(enum rotation rotation);

bool video_display_server_can_set_screen_orientation(void);

bool video_display_server_has_resolution_list(void);

enum rotation video_display_server_get_screen_orientation(void);

extern const video_display_server_t dispserv_win32;
extern const video_display_server_t dispserv_x11;
extern const video_display_server_t dispserv_android;

RETRO_END_DECLS

#endif
