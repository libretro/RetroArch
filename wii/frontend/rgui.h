/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef RGUI_H__
#define RGUI_H__

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#else
extern "C" {
#endif

typedef enum
{
   RGUI_FILE_PLAIN,
   RGUI_FILE_DIRECTORY,
   RGUI_FILE_DEVICE,

   // settings options are done here too
   //RGUI_SETTINGS_VIDEO_STRETCH,
   RGUI_SETTINGS_VIDEO_FILTER
} rgui_file_type_t;

typedef enum
{
   RGUI_ACTION_UP,
   RGUI_ACTION_DOWN,
   RGUI_ACTION_LEFT,
   RGUI_ACTION_RIGHT,
   RGUI_ACTION_OK,
   RGUI_ACTION_CANCEL,
   RGUI_ACTION_REFRESH,
   RGUI_ACTION_SETTINGS,
   RGUI_ACTION_NOOP
} rgui_action_t;

typedef struct rgui_handle rgui_handle_t;

typedef enum
{
   RGUI_FILEBROWSER = 0,
   RGUI_SETTINGS
} rgui_mode_t;

typedef void (*rgui_file_enum_cb_t)(void *ctx,
      const char *path, rgui_file_type_t file_type, size_t directory_ptr);
typedef bool (*rgui_folder_enum_cb_t)(const char *directory,
      rgui_file_enum_cb_t file_cb, void *userdata, void *ctx);

#define RGUI_WIDTH 320
#define RGUI_HEIGHT 240

rgui_handle_t *rgui_init(const char *base_path,
      uint16_t *framebuf, size_t framebuf_pitch,
      const uint8_t *font_buf,
      rgui_folder_enum_cb_t folder_cb, void *userdata);

const char *rgui_iterate(rgui_handle_t *rgui, rgui_action_t action);

void rgui_free(rgui_handle_t *rgui);

#ifdef __cplusplus
}
#endif
#endif

