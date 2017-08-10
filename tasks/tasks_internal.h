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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../content.h"
#include "../core_type.h"
#include "../msg_hash.h"
#include "../frontend/frontend_driver.h"

RETRO_BEGIN_DECLS

typedef int (*transfer_cb_t)(void *data, size_t len);

typedef struct nbio_buf
{
   void *buf;
   unsigned bufsize;
} nbio_buf_t;

enum content_mode_load
{
   CONTENT_MODE_LOAD_NONE = 0,
   CONTENT_MODE_LOAD_CONTENT_WITH_CURRENT_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_FFMPEG_CORE_FROM_MENU,
   CONTENT_MODE_LOAD_CONTENT_WITH_IMAGEVIEWER_CORE_FROM_MENU
};

enum nbio_status_enum
{
   NBIO_STATUS_INIT = 0,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_FINISHED
};

enum nbio_status_flags
{
   NBIO_FLAG_NONE = 0,
   NBIO_FLAG_IMAGE_SUPPORTS_RGBA
};

enum nbio_type
{
   NBIO_TYPE_NONE = 0,
   NBIO_TYPE_JPEG,
   NBIO_TYPE_PNG,
   NBIO_TYPE_TGA,
   NBIO_TYPE_BMP,
   NBIO_TYPE_OGG,
   NBIO_TYPE_MOD,
   NBIO_TYPE_WAV
};

typedef struct nbio_handle
{
   enum nbio_type type;
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

void *task_push_http_post_transfer(const char *url, const char *post_data, bool mute, const char *type,
      retro_task_callback_t cb, void *userdata);

task_retriever_info_t *http_task_get_transfer_list(void);

bool task_push_wifi_scan(retro_task_callback_t cb);

bool task_push_netplay_lan_scan(retro_task_callback_t cb);

bool task_push_netplay_crc_scan(uint32_t crc, char* name,
      const char *hostname, const char *corename);

bool task_push_netplay_lan_scan_rooms(retro_task_callback_t cb);

bool task_push_netplay_nat_traversal(void *nat_traversal_state, uint16_t port);

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
    
bool task_check_decompress(const char *source_file);

bool task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_with_current_core_from_companion_ui(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_from_cli(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_new_core(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_start_builtin_core(content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_start_current_core(content_ctx_info_t *content_info);

bool task_push_start_dummy_core(content_ctx_info_t *content_info);

bool task_push_load_content_with_new_core_from_companion_ui(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data);
   
#ifdef HAVE_MENU
bool task_push_load_content_with_new_core_from_menu(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_from_playlist_from_menu(
      const char *core_path,
      const char *fullpath,
      content_ctx_info_t *content_info,
      retro_task_callback_t cb,
      void *user_data);

bool task_push_load_content_with_core_from_menu(
      const char *fullpath,
      content_ctx_info_t *content_info,
      enum rarch_core_type type,
      retro_task_callback_t cb,
      void *user_data);
#endif

void task_file_load_handler(retro_task_t *task);

bool task_audio_mixer_load_handler(retro_task_t *task);

bool take_screenshot(const char *path, bool silence, bool has_valid_framebuffer);

bool event_load_save_files(void);

bool event_save_files(void);

void path_init_savefile_rtc(const char *savefile_path);

void *savefile_ptr_get(void);

void path_init_savefile_new(void);

bool input_is_autoconfigured(unsigned i);

unsigned input_autoconfigure_get_device_name_index(unsigned i);

void input_autoconfigure_reset(void);

bool input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned idx,
      unsigned vid,
      unsigned pid);

bool input_autoconfigure_disconnect(unsigned i, const char *ident);

bool input_autoconfigure_get_swap_override(void);

void task_push_get_powerstate(void);

enum frontend_powerstate get_last_powerstate(int *percent);

bool task_push_audio_mixer_load(const char *fullpath, retro_task_callback_t cb, void *user_data);

extern const char* const input_builtin_autoconfs[];

RETRO_END_DECLS

#endif
