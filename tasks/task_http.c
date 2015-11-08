/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <retro_miscellaneous.h>
#include <net/net_http.h>
#include <queues/message_queue.h>
#include <string/string_list.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <file/file_extract.h>
#include <net/net_compat.h>
#include <retro_file.h>
#include <retro_stat.h>

#include "../file_ops.h"
#include "../general.h"
#include "../msg_hash.h"
#include "tasks.h"

#define CB_CORE_UPDATER_DOWNLOAD       0x7412da7dU
#define CB_CORE_UPDATER_LIST           0x32fd4f01U
#define CB_UPDATE_ASSETS               0xbf85795eU
#define CB_UPDATE_CORE_INFO_FILES      0xe6084091U
#define CB_UPDATE_AUTOCONFIG_PROFILES  0x28ada67dU
#define CB_UPDATE_CHEATS               0xc360fec3U
#define CB_UPDATE_OVERLAYS             0x699009a0U
#define CB_UPDATE_DATABASES            0x931eb8d3U
#define CB_UPDATE_SHADERS_GLSL         0x0121a186U
#define CB_UPDATE_SHADERS_CG           0xc93a53feU
#define CB_CORE_CONTENT_LIST           0xebc51227U
#define CB_CORE_CONTENT_DOWNLOAD       0x03b3c0a3U

extern char core_updater_path[PATH_MAX_LENGTH];

enum http_status_enum
{
   HTTP_STATUS_POLL = 0,
   HTTP_STATUS_CONNECTION_TRANSFER,
   HTTP_STATUS_CONNECTION_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER,
   HTTP_STATUS_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER_PARSE_FREE
};

typedef struct http_handle
{
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
      char elem1[PATH_MAX_LENGTH];
   } connection;
   msg_queue_t *msg_queue;
   struct http_t *handle;
   transfer_cb_t  cb;
   unsigned status;
} http_handle_t;

int cb_core_updater_list(void *data_, size_t len);
int cb_core_content_list(void *data_, size_t len);

static http_handle_t *http_ptr;

#ifdef HAVE_ZLIB
#ifdef HAVE_MENU
static int zlib_extract_core_callback(const char *name, const char *valid_exts,
      const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   char path[PATH_MAX_LENGTH];

   /* Make directory */
   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));
   path_basedir(path);

   if (!path_mkdir(path))
      goto error;

   /* Ignore directories. */
   if (name[strlen(name) - 1] == '/' || name[strlen(name) - 1] == '\\')
      return 1;

   fill_pathname_join(path, (const char*)userdata, name, sizeof(path));

   RARCH_LOG("path is: %s, CRC32: 0x%x\n", path, crc32);

   if (!zlib_perform_mode(path, valid_exts,
            cdata, cmode, csize, size, crc32, userdata))
      goto error;

   return 1;

error:
   RARCH_ERR("Failed to deflate to: %s.\n", path);
   return 0;
}
#endif
#endif

#ifdef HAVE_MENU
static int cb_generic_download(void *data, size_t len,
      const char *dir_path)
{
   char msg[PATH_MAX_LENGTH];
   char output_path[PATH_MAX_LENGTH];
   const char             *file_ext      = NULL;
   settings_t              *settings     = config_get_ptr();

   if (!data)
      return -1;

   fill_pathname_join(output_path, dir_path,
         core_updater_path, sizeof(output_path));

   if (!retro_write_file(output_path, data, len))
      return -1;

   snprintf(msg, sizeof(msg), "%s: %s.",
         msg_hash_to_str(MSG_DOWNLOAD_COMPLETE),
         core_updater_path);

   rarch_main_msg_queue_push(msg, 1, 90, true);

#ifdef HAVE_ZLIB
   file_ext = path_get_extension(output_path);

   if (!settings->network.buildbot_auto_extract_archive)
      return 0;

   if (!strcasecmp(file_ext,"zip"))
   {
      if (!zlib_parse_file(output_path, NULL, zlib_extract_core_callback,
               (void*)dir_path))
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_COULD_NOT_PROCESS_ZIP_FILE));

      if (path_file_exists(output_path))
         remove(output_path);
   }
#endif

   return 0;
}

static int cb_core_updater_download(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   int ret = cb_generic_download(data, len, settings->libretro_directory);
   if (ret == 0)
      event_command(EVENT_CMD_CORE_INFO_INIT);
   return ret;
}

static int cb_core_content_download(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->core_assets_directory);
}

static int cb_update_core_info_files(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->libretro_info_path);
}

static int cb_update_assets(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->assets_directory);
}

static int cb_update_autoconfig_profiles(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->input.autoconfig_dir);
}

static int cb_update_shaders_cg(void *data, size_t len)
{
   char shaderdir[PATH_MAX_LENGTH];
   settings_t              *settings     = config_get_ptr();
   fill_pathname_join(shaderdir, settings->video.shader_dir, "shaders_cg",
         sizeof(shaderdir));
   if (!path_file_exists(shaderdir))
      if (!path_mkdir(shaderdir))
         return -1;

   return cb_generic_download(data, len, shaderdir);
}

static int cb_update_shaders_glsl(void *data, size_t len)
{
   char shaderdir[PATH_MAX_LENGTH];
   settings_t              *settings     = config_get_ptr();
   fill_pathname_join(shaderdir, settings->video.shader_dir, "shaders_glsl",
         sizeof(shaderdir));
   if (!path_file_exists(shaderdir))
      if (!path_mkdir(shaderdir))
         return -1;

   return cb_generic_download(data, len, shaderdir);
}

static int cb_update_databases(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->content_database);
}

static int cb_update_overlays(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->overlay_directory);
}

static int cb_update_cheats(void *data, size_t len)
{
   settings_t              *settings     = config_get_ptr();
   return cb_generic_download(data, len, settings->cheat_database);
}
#endif

static int rarch_main_data_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int rarch_main_data_http_conn_iterate_transfer_parse(http_handle_t *http)
{
   if (net_http_connection_done(http->connection.handle))
   {
      if (http->connection.handle && http->connection.cb)
         http->connection.cb(http, 0);
   }
   
   net_http_connection_free(http->connection.handle);

   http->connection.handle = NULL;

   return 0;
}

static int rarch_main_data_http_iterate_transfer_parse(http_handle_t *http)
{
   size_t len = 0;
   char *data = (char*)net_http_data(http->handle, &len, false);

   if (data && http->cb)
      http->cb(data, len);

   net_http_delete(http->handle);

   http->handle = NULL;
   msg_queue_clear(http->msg_queue);

   return 0;
}


static int cb_http_conn_default(void *data_, size_t len)
{
   http_handle_t *http = (http_handle_t*)data_;

   if (!http)
      return -1;

   if (!network_init())
      return -1;

   http->handle = net_http_new(http->connection.handle);

   if (!http->handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      return -1;
   }

   http->cb     = NULL;

   if (http->connection.elem1[0] != '\0')
   {
      uint32_t label_hash = msg_hash_calculate(http->connection.elem1);

      switch (label_hash)
      {
#ifdef HAVE_MENU
         case CB_CORE_UPDATER_DOWNLOAD:
            http->cb = &cb_core_updater_download;
            break;
         case CB_CORE_CONTENT_DOWNLOAD:
            http->cb = &cb_core_content_download;
            break;
         case CB_CORE_UPDATER_LIST:
            http->cb = &cb_core_updater_list;
            break;
         case CB_CORE_CONTENT_LIST:
            http->cb = &cb_core_content_list;
            break;
         case CB_UPDATE_ASSETS:
            http->cb = &cb_update_assets;
            break;
         case CB_UPDATE_CORE_INFO_FILES:
            http->cb = &cb_update_core_info_files;
            break;
         case CB_UPDATE_AUTOCONFIG_PROFILES:
            http->cb = &cb_update_autoconfig_profiles;
            break;
         case CB_UPDATE_CHEATS:
            http->cb = &cb_update_cheats;
            break;
         case CB_UPDATE_DATABASES:
            http->cb = &cb_update_databases;
            break;
         case CB_UPDATE_SHADERS_CG:
            http->cb = &cb_update_shaders_cg;
            break;
         case CB_UPDATE_SHADERS_GLSL:
            http->cb = &cb_update_shaders_glsl;
            break;
         case CB_UPDATE_OVERLAYS:
            http->cb = &cb_update_overlays;
            break;
#endif
         default:
            break;
      }
   }

   return 0;
}

/**
 * rarch_main_data_http_iterate_poll:
 *
 * Polls HTTP message queue to see if any new URLs 
 * are pending.
 *
 * If handle is freed, will set up a new http handle. 
 * The transfer will be started on the next frame.
 *
 * Returns: 0 when an URL has been pulled and we will
 * begin transferring on the next frame. Returns -1 if
 * no HTTP URL has been pulled. Do nothing in that case.
 **/
static int rarch_main_data_http_iterate_poll(http_handle_t *http)
{
   char elem0[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *url              = msg_queue_pull(http->msg_queue);

   if (!url)
      return -1;

   /* Can only deal with one HTTP transfer at a time for now */
   if (http->handle)
      return -1; 

   str_list                     = string_split(url, "|");

   if (!str_list || (str_list->size < 1))
      goto error;

   strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));

   http->connection.handle = net_http_connection_new(elem0);

   if (!http->connection.handle)
      return -1;

   http->connection.cb     = &cb_http_conn_default;


   if (str_list->size > 1)
   {
      strlcpy(http->connection.elem1,
            str_list->elems[1].data,
            sizeof(http->connection.elem1));
   }

   string_list_free(str_list);
   
   return 0;

error:
   if (str_list)
      string_list_free(str_list);
   return -1;
}

/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_data_http_iterate_transfer(void *data)
{
   http_handle_t *http  = (http_handle_t*)data;
   size_t pos  = 0, tot = 0;

   if (!net_http_update(http->handle, &pos, &tot))
   {
      int percent = 0;

      if(tot != 0)
         percent = (unsigned long long)pos * 100
            / (unsigned long long)tot;

      if (percent > 0)
      {
         char tmp[PATH_MAX_LENGTH];
         snprintf(tmp, sizeof(tmp), "%s: %d%%",
               msg_hash_to_str(MSG_DOWNLOAD_PROGRESS),
               percent);
         data_runloop_osd_msg(tmp, sizeof(tmp));
      }

      return -1;
   }

   return 0;
}

void rarch_main_data_http_iterate(bool is_thread)
{
   http_handle_t     *http = (http_handle_t*)http_ptr;
   if (!http)
      return;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         rarch_main_data_http_conn_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!rarch_main_data_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         rarch_main_data_http_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_POLL;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(http))
            http->status = HTTP_STATUS_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_POLL:
      default:
         if (rarch_main_data_http_iterate_poll(http) == 0)
            http->status = HTTP_STATUS_CONNECTION_TRANSFER;
         break;
   }
}


void rarch_main_data_http_init_msg_queue(void)
{
   http_handle_t     *http = (http_handle_t*)http_ptr;
   if (!http)
      return;

   if (!http->msg_queue)
      retro_assert(http->msg_queue       = msg_queue_new(8));
}


msg_queue_t *rarch_main_data_http_get_msg_queue_ptr(void)
{
   http_handle_t     *http = (http_handle_t*)http_ptr;
   if (!http)
      return NULL;
   return http->msg_queue;
}

void *rarch_main_data_http_get_handle(void)
{
   http_handle_t     *http = (http_handle_t*)http_ptr;
   if (!http)
      return NULL;
   if (http->handle == NULL)
      return NULL;
   return http->handle;
}

void *rarch_main_data_http_conn_get_handle(void)
{
   http_handle_t     *http = (http_handle_t*)http_ptr;
   if (!http)
      return NULL;
   if (http->connection.handle == NULL)
      return NULL;
   return http->connection.handle;
}

void rarch_main_data_http_uninit(void)
{
   if (http_ptr)
      free(http_ptr);
   http_ptr = NULL;
}

void rarch_main_data_http_init(void)
{
   http_ptr              = (http_handle_t*)calloc(1, sizeof(*http_ptr));
}
