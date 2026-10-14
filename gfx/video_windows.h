/*  RetroArch - A frontend for libretro.
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

#ifndef __VIDEO_WINDOWS_H
#define __VIDEO_WINDOWS_H

#include "video_views.h"

RETRO_BEGIN_DECLS

enum video_window_mode
{
   VIDEO_WINDOW_ATTACHED = 0,
   VIDEO_WINDOW_SEPARATE,
   VIDEO_WINDOW_HIDDEN
};

typedef struct video_window_settings
{
   unsigned pos;
   unsigned dims;
   unsigned monitor;
   unsigned mode;
   bool fullscreen;
} video_window_settings_t;

typedef struct video_windows_plan
{
   unsigned main_mask;
   unsigned separate_mask;
   unsigned hidden_mask;
} video_windows_plan_t;

void video_windows_plan(const video_views_map_t *map,
      const video_window_settings_t *settings, bool enabled,
      bool supported, unsigned forced_attached,
      video_windows_plan_t *out);

RETRO_END_DECLS

#endif
