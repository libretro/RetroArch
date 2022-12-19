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

#include <queues/task_queue.h>
#include <gfx/scaler/scaler.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_NETWORKING)
#include "../core_updater_list.h"
#endif

#include "../playlist.h"

/* Required for task_push_core_backup() */
#include "../core_backup.h"

#if defined(HAVE_OVERLAY)
#include "../input/input_overlay.h"
#endif

RETRO_BEGIN_DECLS

typedef struct nbio_buf
{
   void *buf;
   char *path;
   unsigned bufsize;
} nbio_buf_t;

#ifdef HAVE_NETWORKING
typedef struct
{
   char *data;
   size_t len;
   int status;
} http_transfer_data_t;

void *task_push_http_transfer(const char *url, bool mute, const char *type,
      retro_task_callback_t cb, void *userdata);

void *task_push_http_transfer_with_user_agent(const char *url, bool mute, const char *type,
      const char *user_agent, retro_task_callback_t cb, void *userdata);

void *task_push_http_transfer_with_headers(const char *url, bool mute, const char *type,
   const char *headers, retro_task_callback_t cb, void *user_data);

void *task_push_http_post_transfer(const char *url, const char *post_data, bool mute, const char *type,
      retro_task_callback_t cb, void *userdata);

void *task_push_http_post_transfer_with_user_agent(const char *url, const char *post_data, bool mute,
   const char *type, const char *user_agent, retro_task_callback_t cb, void *user_data);

void *task_push_http_post_transfer_with_headers(const char *url, const char *post_data, bool mute,
   const char *type, const char *headers, retro_task_callback_t cb, void *user_data);

task_retriever_info_t *http_task_get_transfer_list(void);

bool task_push_bluetooth_scan(retro_task_callback_t cb);

bool task_push_wifi_scan(retro_task_callback_t cb);
bool task_push_wifi_enable(retro_task_callback_t cb);
bool task_push_wifi_disable(retro_task_callback_t cb);
bool task_push_wifi_disconnect(retro_task_callback_t cb);
bool task_push_wifi_connect(retro_task_callback_t cb, void*);

bool task_push_netplay_lan_scan(void (*cb)(const void*), unsigned timeout);

bool task_push_netplay_crc_scan(uint32_t crc, const char *content,
      const char *subsystem, const char *core, const char *hostname);
bool task_push_netplay_content_reload(const char *hostname);

bool task_push_netplay_nat_traversal(void *data, uint16_t port);
bool task_push_netplay_nat_close(void *data);

/* Core updater tasks */

void *task_push_get_core_updater_list(
      core_updater_list_t* core_list, bool mute, bool refresh_menu);

/* NOTE: If CRC is set to 0, CRC of local core file
 * will be calculated automatically */
void *task_push_core_updater_download(
      core_updater_list_t* core_list,
      const char *filename, uint32_t crc, bool mute,
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets);
void task_push_update_installed_cores(
      bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro,
      const char *path_dir_core_assets);
bool task_push_update_single_core(
      const char *path_core, bool auto_backup, size_t auto_backup_history_size,
      const char *path_dir_libretro, const char *path_dir_core_assets);
#if defined(ANDROID)
void *task_push_play_feature_delivery_core_install(
      core_updater_list_t* core_list,
      const char *filename,
      bool mute);
void task_push_play_feature_delivery_switch_installed_cores(
      const char *path_dir_libretro,
      const char *path_libretro_info);
#endif

bool task_push_pl_entry_thumbnail_download(
      const char *system,
      playlist_t *playlist,
      unsigned idx,
      bool overwrite,
      bool mute);

#ifdef HAVE_MENU
bool task_push_pl_thumbnail_download(
      const char *system,
      const playlist_config_t *playlist_config,
      const char *dir_thumbnails);
#endif

#endif

/* Core backup/restore tasks */

/* NOTE 1: If CRC is set to 0, CRC of core_path file will
 * be calculated automatically
 * NOTE 2: If core_display_name is set to NULL, display
 * name will be determined automatically
 * > core_display_name *must* be set to a non-empty
 *   string if task_push_core_backup() is *not* called
 *   on the main thread */
void *task_push_core_backup(
      const char *core_path, const char *core_display_name,
      uint32_t crc, enum core_backup_mode backup_mode,
      size_t auto_backup_history_size,
      const char *dir_core_assets, bool mute);

/* NOTE: If 'core_loaded' is true, menu stack should be
 * flushed if task_push_core_restore() returns true */
bool task_push_core_restore(const char *backup_path,
      const char *dir_libretro,
      bool *core_loaded);

bool task_push_pl_manager_reset_cores(const playlist_config_t *playlist_config);
bool task_push_pl_manager_clean_playlist(const playlist_config_t *playlist_config);

bool task_push_image_load(const char *fullpath,
      bool supports_rgba, unsigned upscale_threshold,
      retro_task_callback_t cb, void *userdata);

#ifdef HAVE_LIBRETRODB
bool task_push_dbscan(
      const char *playlist_directory,
      const char *content_database,
      const char *fullpath,
      bool directory, bool show_hidden_files,
      retro_task_callback_t cb);
#endif

bool task_push_manual_content_scan(
      const playlist_config_t *playlist_config,
      const char *playlist_directory);

#ifdef HAVE_OVERLAY
bool task_push_overlay_load_default(
      retro_task_callback_t cb,
      const char *overlay_path,
      bool overlay_hide_in_menu,
      bool overlay_hide_when_gamepad_connected,
      bool input_overlay_enable,
      float input_overlay_opacity,
      overlay_layout_desc_t *layout_desc,
      void *user_data);
#endif

bool patch_content(
      bool is_ips_pref,
      bool is_bps_pref,
      bool is_ups_pref,
      const char *name_ips,
      const char *name_bps,
      const char *name_ups,
      uint8_t **buf,
      void *data);

bool task_check_decompress(const char *source_file);

void *task_push_decompress(
      const char *source_file,
      const char *target_dir,
      const char *target_file,
      const char *subdir,
      const char *valid_ext,
      retro_task_callback_t cb,
      void *user_data,
      void *frontend_userdata,
      bool mute);

void task_file_load_handler(retro_task_t *task);

typedef struct screenshot_task_state screenshot_task_state_t;

enum screenshot_task_flags
{
   SS_TASK_FLAG_BGR24               = (1 << 0),
   SS_TASK_FLAG_SILENCE             = (1 << 1),
   SS_TASK_FLAG_IS_IDLE             = (1 << 2),
   SS_TASK_FLAG_IS_PAUSED           = (1 << 3),
   SS_TASK_FLAG_HISTORY_LIST_ENABLE = (1 << 4),
   SS_TASK_FLAG_WIDGETS_READY       = (1 << 5)
};

struct screenshot_task_state
{
   struct scaler_ctx scaler;
   uint8_t *out_buffer;
   const void *frame;
   void *userbuf;

   int pitch;
   unsigned width;
   unsigned height;
   unsigned pixel_format_type;

   uint8_t flags;

   char filename[PATH_MAX_LENGTH];
   char shotname[NAME_MAX_LENGTH];
};

bool take_screenshot(
      const char *screenshot_dir,
      const char *path, bool silence,
      bool has_valid_framebuffer, bool fullpath, bool use_thread);

bool event_load_save_files(bool is_sram_load_disabled);

bool event_save_files(bool sram_used);

void path_init_savefile_rtc(const char *savefile_path);

void *savefile_ptr_get(void);

void path_init_savefile_new(void);

/* Autoconfigure tasks */
extern const char* const input_builtin_autoconfs[];
void input_autoconfigure_blissbox_override_handler(
      int vid, int pid, char *device_name, size_t len);
bool input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned port,
      unsigned vid,
      unsigned pid);
bool input_autoconfigure_disconnect(
      unsigned port, const char *name);

void set_save_state_in_background(bool state);

#ifdef HAVE_CDROM
void task_push_cdrom_dump(const char *drive);
#endif

/* Menu explore tasks */
#if defined(HAVE_MENU) && defined(HAVE_LIBRETRODB)
bool task_push_menu_explore_init(const char *directory_playlist,
      const char *directory_database);
bool menu_explore_init_in_progress(void *data);
void menu_explore_wait_for_init_task(void);
#endif

RETRO_END_DECLS

#endif
