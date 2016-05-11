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

#include <queues/message_queue.h>
#include <queues/task_queue.h>

#include "../core_type.h"
#include "../runloop.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CB_MENU_WALLPAPER     0xb476e505U
#define CB_MENU_THUMBNAIL     0x82f93a21U

enum nbio_status_enum
{
   NBIO_STATUS_POLL = 0,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_PARSE_FREE
};

enum image_type_enum
{
   IMAGE_TYPE_PNG = 0,
   IMAGE_TYPE_JPEG
};

#ifdef HAVE_NETWORKING
typedef struct
{
    char *data;
    size_t len;
} http_transfer_data_t;

typedef struct http_transfer_info
{
   char url[PATH_MAX_LENGTH];
   int progress;
} http_transfer_info_t;
#endif

typedef struct nbio_image_handle
{
   struct texture_image ti;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   transfer_cb_t  cb;
   void *handle;
   unsigned processing_pos_increment;
   unsigned pos_increment;
   uint64_t frame_count;
   int processing_final_state;
   unsigned status;
} nbio_image_handle_t;

typedef struct nbio_handle
{
   enum image_type_enum image_type;
   nbio_image_handle_t image;
   bool is_finished;
   transfer_cb_t  cb;
   struct nbio_t *handle;
   unsigned pos_increment;
   uint64_t frame_count;
   msg_queue_t *msg_queue;
   unsigned status;
} nbio_handle_t;

#ifdef HAVE_NETWORKING
void *rarch_task_push_http_transfer(const char *url, const char *type,
      retro_task_callback_t cb, void *userdata);

task_retriever_info_t *http_task_get_transfer_list(void);
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

bool rarch_task_check_decompress(const char *source_file);

bool rarch_task_image_load_handler(retro_task_t *task);

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

void rarch_task_image_load_free(retro_task_t *task);

void rarch_task_file_load_handler(retro_task_t *task);

#ifdef __cplusplus
}
#endif

#endif
