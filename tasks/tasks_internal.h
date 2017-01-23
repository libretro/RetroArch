/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Higor Euripedes
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
#ifndef TASKS_HANDLER_INTERNAL_H
#define TASKS_HANDLER_INTERNAL_H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <formats/image.h>

#include "../content.h"
#include "../core_type.h"
#include "../msg_hash.h"
#include "../frontend/frontend_driver.h"

RETRO_BEGIN_DECLS

typedef int (*transfer_cb_t)(void *data, size_t len);

enum content_mode_load
{
   CONTENT_MODE_LOAD_NONE = 0,
   CONTENT_MODE_LOAD_NOTHING_WITH_DUMMY_CORE,
   CONTENT_MODE_LOAD_FROM_CLI,
   CONTENT_MODE_LOAD_NOTHING_WITH_CURRENT_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_NOTHING_WITH_NEW_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_NOTHING_WITH_NET_RETROPAD_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_NOTHING_WITH_VIDEO_PROCESSOR_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_COMPANION_UI,
   CONTENT_MODE_LOAD_CONTENT_WITH_NEW_CORE_FROM_COMPANION_UI,
   CONTENT_MODE_LOAD_CONTENT_FROM_PLAYLIST_FROM_MENU
};

enum nbio_status_enum
{
   NBIO_STATUS_INIT = 0,
   NBIO_STATUS_POLL,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_PARSE_FREE
};

enum nbio_status_flags
{
   NBIO_FLAG_NONE = 0,
   NBIO_FLAG_IMAGE_SUPPORTS_RGBA
};

typedef struct nbio_handle
{
   enum image_type_enum image_type;
   void *data;
   bool is_finished;
   transfer_cb_t  cb;
   struct nbio_t *handle;
   unsigned pos_increment;
   msg_queue_t *msg_queue;
   unsigned status;
   uint32_t status_flags;
   char path[4096];
} nbio_handle_t;

#ifdef HAVE_NETWORKING
typedef struct
{
    char *data;
    size_t len;
} http_transfer_data_t;

void *task_push_http_transfer(const char *url, bool mute, const char *type,
      retro_task_callback_t cb, void *userdata);

task_retriever_info_t *http_task_get_transfer_list(void);

bool task_push_wifi_scan(void);

bool task_push_netplay_lan_scan(void);

bool task_push_netplay_crc_scan(uint32_t crc, char* name,
      const char *hostname, const char *corename);

#endif

bool task_push_image_load(const char *fullpath,
      retro_task_callback_t cb, void *userdata);

#ifdef HAVE_LIBRETRODB
bool task_push_dbscan(
      const char *playlist_directory,
      const char *content_database,
      const char *fullpath,
      bool directory, retro_task_callback_t cb);
#endif

#ifdef HAVE_OVERLAY
bool task_push_overlay_load_default(
        retro_task_callback_t cb, void *user_data);
#endif
    
int find_first_data_track(const char* cue_path,
      int32_t* offset, char* track_path, size_t max_len);

int detect_system(const char* track_path, int32_t offset,
        const char** system_name);

int detect_ps1_game(const char *track_path, char *game_id);

int detect_psp_game(const char *track_path, char *game_id);

bool task_check_decompress(const char *source_file);

bool task_image_load_handler(retro_task_t *task);

bool task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_content_load_default(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      enum content_mode_load mode,
      retro_task_callback_t cb,
      void *user_data);

void task_image_load_free(retro_task_t *task);

void task_file_load_handler(retro_task_t *task);

bool take_screenshot(const char *path, bool silence);

bool event_load_save_files(void);

bool event_save_files(void);

void path_init_savefile_rtc(void);

void *savefile_ptr_get(void);

void path_init_savefile_new(void);

bool input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned idx,
      unsigned vid,
      unsigned pid);

bool input_autoconfigure_disconnect(unsigned i, const char *ident);

void task_push_get_powerstate(void);

enum frontend_powerstate get_last_powerstate(int *percent);

extern const char* const input_builtin_autoconfs[];

RETRO_END_DECLS

#endif
