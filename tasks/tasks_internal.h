/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Higor Euripedes
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
#ifndef TASKS_HANDLER_INTERNAL_H
#define TASKS_HANDLER_INTERNAL_H

#include <stdint.h>
#include <boolean.h>

#include <queues/task_queue.h>

#include "../core_type.h"
#include "../runloop.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_NETWORKING
typedef struct {
    char *data;
    size_t len;
} http_transfer_data_t;

bool rarch_task_push_http_transfer(const char *url, const char *type,
      retro_task_callback_t cb, void *userdata);
#endif

bool rarch_task_push_image_load(const char *fullpath, const char *type,
      retro_task_callback_t cb, void *userdata);

#ifdef HAVE_LIBRETRODB
bool rarch_task_push_dbscan(const char *fullpath,
      bool directory, retro_task_callback_t cb);
#endif

#ifdef HAVE_OVERLAY
bool rarch_task_push_overlay_load_default(
        retro_task_callback_t cb, void *user_data);
#endif
    
int find_first_data_track(const char* cue_path,
      int32_t* offset, char* track_path, size_t max_len);

int detect_system(const char* track_path, int32_t offset,
        const char** system_name);

int detect_ps1_game(const char *track_path, char *game_id);

int detect_psp_game(const char *track_path, char *game_id);

bool rarch_task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      retro_task_callback_t cb,
      void *user_data);

bool rarch_task_push_content_load_default(
      const char *core_path,
      const char *fullpath,
      bool persist,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

#ifdef __cplusplus
}
#endif

#endif
