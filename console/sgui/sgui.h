/*  SSNES - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef SGUI_H__
#define SGUI_H__

#include <stdint.h>
#include <stddef.h>

#ifndef __cplusplus
#include <stdbool.h>
#else
extern "C" {
#endif

typedef enum
{
   SGUI_FILE_PLAIN,
   SGUI_FILE_DIRECTORY
} sgui_file_type_t;

typedef enum
{
   SGUI_ACTION_UP,
   SGUI_ACTION_DOWN,
   SGUI_ACTION_LEFT,
   SGUI_ACTION_RIGHT,
   SGUI_ACTION_OK,
   SGUI_ACTION_CANCEL,
   SGUI_ACTION_REFRESH,
   SGUI_ACTION_NOOP
} sgui_action_t;

typedef struct sgui_handle sgui_handle_t;

typedef void (*sgui_file_enum_cb_t)(void *ctx, const char *path,
      sgui_file_type_t file_type);
typedef bool (*sgui_folder_enum_cb_t)(const char *directory,
      sgui_file_enum_cb_t file_cb, void *userdata, void *ctx);

#define SGUI_WIDTH 320
#define SGUI_HEIGHT 240

sgui_handle_t *sgui_init(const char *base_path,
      uint16_t *framebuf, size_t framebuf_pitch,
      const uint8_t *font_buf,
      sgui_folder_enum_cb_t folder_cb, void *userdata);

const char *sgui_iterate(sgui_handle_t *sgui, sgui_action_t action);

void sgui_free(sgui_handle_t *sgui);

#ifdef __cplusplus
}
#endif
#endif

