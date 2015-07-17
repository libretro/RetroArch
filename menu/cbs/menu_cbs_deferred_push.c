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

#include <file/file_path.h>

#include "../menu.h"
#include "../menu_cbs.h"
#include "../menu_hash.h"
#include "../menu_displaylist.h"

#ifdef HAVE_LIBRETRODB
#include "../../database_info.h"
#endif

#include "../../cores/internal_cores.h"

#include "../../general.h"
#include "../../file_ext.h"
#include "../../gfx/video_shader_driver.h"

static int deferred_push_core_information(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_INFO);
}

static int deferred_push_system_information(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SYSTEM_INFO);
}

static int deferred_push_rdb_collection(menu_displaylist_info_t *info)
{
   /* TODO/FIXME - add path? */
   return menu_displaylist_push_list(info, DISPLAYLIST_PLAYLIST_COLLECTION);
}

static int deferred_push_help(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_HELP_SCREEN_LIST);
}

static int deferred_push_rdb_entry_detail(menu_displaylist_info_t *info)
{
   int ret;
   struct string_list *str_list  = string_split(info->label, "|"); 

   if (!str_list)
      return -1;

   strlcpy(info->path_b,   str_list->elems[1].data, sizeof(info->path_b));
   strlcpy(info->label,    str_list->elems[0].data, sizeof(info->label));

   ret = menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_ENTRY);

   string_list_free(str_list);

   return ret;
}

static int deferred_push_core_list_deferred(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_SUPPORTED);
}

static int deferred_push_core_collection_list_deferred(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_COLLECTION_SUPPORTED);
}

static int deferred_push_database_manager_list_deferred(menu_displaylist_info_t *info)
{
   strlcpy(info->path_b,    info->path, sizeof(info->path_b));
   info->path_c[0] = '\0';

   return menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_QUERY);
}

static int deferred_push_cursor_manager_list_deferred(menu_displaylist_info_t *info)
{
   char *query                    = NULL;
   char *rdb                      = NULL;
   char rdb_path[PATH_MAX_LENGTH] = {0};
   settings_t *settings           = config_get_ptr();
   config_file_t *conf            = config_file_new(info->path);

   if (!conf || !settings)
      return -1;

   if (!config_get_string(conf, "query", &query))
   {
      config_file_free(conf);
      return -1;
   }

   if (!config_get_string(conf, "rdb", &rdb))
   {
      config_file_free(conf);
      return -1;
   }

   fill_pathname_join(rdb_path, settings->content_database,
         rdb, sizeof(rdb_path));

   strlcpy(info->path_b, info->path, sizeof(info->path_b));
   strlcpy(info->path,   rdb_path,   sizeof(info->path));
   strlcpy(info->path_c,    query,   sizeof(info->path_c));

   menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_QUERY);

   config_file_free(conf);
   return 0;
}

static int deferred_push_cursor_manager_list_deferred_query_subsearch(menu_displaylist_info_t *info)
{
#ifdef HAVE_LIBRETRODB
   int ret;
   char query[PATH_MAX_LENGTH]   = {0};
   struct string_list *str_list  = string_split(info->path, "|"); 

   database_info_build_query(query, sizeof(query), info->label, str_list->elems[0].data);

   if (query[0] == '\0')
   {
      string_list_free(str_list);
      return -1;
   }

   strlcpy(info->path,   str_list->elems[1].data, sizeof(info->path));
   strlcpy(info->path_b,    str_list->elems[0].data, sizeof(info->path_b));
   strlcpy(info->path_c,    query, sizeof(info->path_c));

   ret = menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_QUERY);

   string_list_free(str_list);

   return ret;
#else
   return 0;
#endif
}

static int deferred_push_video_shader_preset_parameters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PARAMETERS_PRESET);
}

static int deferred_push_video_shader_parameters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PARAMETERS);
}

static int deferred_push_settings(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SETTINGS_ALL);
}

static int deferred_push_settings_subgroup(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_SETTINGS_SUBGROUP);
}

static int deferred_push_category(menu_displaylist_info_t *info)
{
   info->flags = SL_FLAG_ALL_SETTINGS;

   return menu_displaylist_push_list(info, DISPLAYLIST_SETTINGS);
}

static int deferred_push_shader_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_SHADERS);
}

static int deferred_push_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS);
}

static int deferred_push_content_settings(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CONTENT_SETTINGS);
}

static int deferred_push_add_content_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_ADD_CONTENT_LIST);
}

static int deferred_push_load_content_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_LOAD_CONTENT_LIST);
}

static int deferred_push_information_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_INFORMATION_LIST);
}

static int deferred_push_management_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_MANAGEMENT);
}

static int deferred_push_core_counters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_PERFCOUNTERS_CORE);
}

static int deferred_push_frontend_counters(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_PERFCOUNTERS_FRONTEND);
}

static int deferred_push_core_cheat_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_CHEATS);
}

static int deferred_push_core_input_remapping_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_REMAPPINGS);
}

static int deferred_push_core_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_OPTIONS);
}

static int deferred_push_disk_options(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_OPTIONS_DISK);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

static int cb_net_generic(void *data_, size_t len)
{
   char             *data = (char*)data_;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return -1;

   if (!data)
      return -1;

   if (core_buf)
      free(core_buf);

   core_buf = (char*)malloc((len+1) * sizeof(char));

   if (!core_buf)
      return -1;

   memcpy(core_buf, data, len * sizeof(char));
   core_buf[len] = '\0';
   core_len = len;

   menu_entries_unset_nonblocking_refresh();

   return 0;
}

int cb_core_updater_list(void *data_, size_t len)
{
   return cb_net_generic(data_, len);
}

int cb_core_content_list(void *data_, size_t len)
{
   return cb_net_generic(data_, len);
}

static int deferred_push_core_updater_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_UPDATER);
}

static int deferred_push_core_content_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_CORE_CONTENT);
}
#endif

static int deferred_archive_action_detect_core(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_ARCHIVE_ACTION_DETECT_CORE);
}

static int deferred_archive_action(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_ARCHIVE_ACTION);
}

static int deferred_archive_open_detect_core(menu_displaylist_info_t *info)
{
   settings_t        *settings = config_get_ptr();
   global_t            *global = global_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();
   menu_handle_t *menu    = menu_driver_get_ptr();

   fill_pathname_join(info->path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(info->path));
   fill_pathname_join(info->label, menu->scratch2_buf,
         menu->scratch_buf, sizeof(info->label));

   info->type_default = MENU_FILE_PLAIN;
   info->setting      = menu_setting_find(info->label);


   if (global->core_info)
      strlcpy(info->exts, core_info_list_get_all_extensions(
         global->core_info), sizeof(info->exts));
   else if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         strlcpy(info->exts, global->menu.info.valid_extensions,
               sizeof(info->exts));
   }
   else
      strlcpy(info->exts, system->valid_extensions, sizeof(info->exts));

   (void)settings;

   if (settings->multimedia.builtin_mediaplayer_enable ||
         settings->multimedia.builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};

      (void)sysinfo;
#ifdef HAVE_FFMPEG
      if (settings->multimedia.builtin_mediaplayer_enable)
      {
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (settings->multimedia.builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
   }

   return menu_displaylist_push_list(info, DISPLAYLIST_DEFAULT);
}

static int deferred_archive_open(menu_displaylist_info_t *info)
{
   settings_t        *settings = config_get_ptr();
   global_t            *global = global_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();
   menu_handle_t *menu    = menu_driver_get_ptr();

   fill_pathname_join(info->path, menu->scratch2_buf,
         menu->scratch_buf, sizeof(info->path));
   fill_pathname_join(info->label, menu->scratch2_buf,
         menu->scratch_buf, sizeof(info->label));

   info->type_default = MENU_FILE_PLAIN;
   info->setting      = menu_setting_find(info->label);

   if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         strlcpy(info->exts, global->menu.info.valid_extensions,
               sizeof(info->exts));
   }
   else
      strlcpy(info->exts, system->valid_extensions, sizeof(info->exts));

   (void)settings;

   if (settings->multimedia.builtin_mediaplayer_enable ||
         settings->multimedia.builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};

      (void)sysinfo;
#ifdef HAVE_FFMPEG
      if (settings->multimedia.builtin_mediaplayer_enable)
      {
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (settings->multimedia.builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
   }
   return menu_displaylist_push_list(info, DISPLAYLIST_DEFAULT);
}

static int deferred_push_history_list(menu_displaylist_info_t *info)
{
   return menu_displaylist_push_list(info, DISPLAYLIST_HISTORY);
}

int deferred_push_content_list(void *data, void *userdata, const char *path,
      const char *label, unsigned type)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   return menu_displaylist_push((file_list_t*)data, menu_list->selection_buf);
}

static int deferred_push_database_manager_list(menu_displaylist_info_t *info)
{
   settings_t *settings   = config_get_ptr();

   info->type_default = MENU_FILE_RDB;
   strlcpy(info->exts, "rdb", sizeof(info->exts));
   strlcpy(info->path, settings->content_database, sizeof(info->path));

   return menu_displaylist_push_list(info, DISPLAYLIST_DATABASES);
}

static int deferred_push_cursor_manager_list(menu_displaylist_info_t *info)
{
   settings_t *settings   = config_get_ptr();

   info->type_default = MENU_FILE_CURSOR;
   strlcpy(info->exts, "dbc", sizeof(info->exts));
   strlcpy(info->path, settings->cursor_directory, sizeof(info->path));

   return menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_CURSORS);
}

static int deferred_push_content_collection_list(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_PLAIN;
   strlcpy(info->exts, "lpl", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_DATABASE_PLAYLISTS);
}

static int deferred_push_core_list(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_PLAIN;
   strlcpy(info->exts, EXT_EXECUTABLES, sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CORES);
}

static int deferred_push_configurations(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_CONFIG;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CONFIG_FILES);
}

static int deferred_push_video_shader_preset(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_SHADER_PRESET;
   strlcpy(info->exts, "cgp|glslp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PRESET);
}

static int deferred_push_video_shader_pass(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_SHADER;
   strlcpy(info->exts, "cg|glsl", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_SHADER_PASS);
}

static int deferred_push_video_filter(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_VIDEOFILTER;
   strlcpy(info->exts, "filt", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_VIDEO_FILTERS);
}

static int deferred_push_images(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_IMAGE;
   strlcpy(info->exts, "png", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_IMAGES);
}

static int deferred_push_audio_dsp_plugin(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_AUDIOFILTER;
   strlcpy(info->exts, "dsp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_AUDIO_FILTERS);
}

static int deferred_push_cheat_file_load(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_CHEAT;
   strlcpy(info->exts, "cht", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CHEAT_FILES);
}

static int deferred_push_remap_file_load(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_REMAP;
   strlcpy(info->exts, "rmp", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_REMAP_FILES);
}

static int deferred_push_record_configfile(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_RECORD_CONFIG;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_RECORD_CONFIG_FILES);
}

static int deferred_push_input_overlay(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_OVERLAY;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_input_osk_overlay(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_OVERLAY;
   strlcpy(info->exts, "cfg", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_video_font_path(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_FONT;
   strlcpy(info->exts, "ttf", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_FONTS);
}

static int deferred_push_content_history_path(menu_displaylist_info_t *info)
{
   info->type_default = MENU_FILE_PLAIN;
   strlcpy(info->exts, "lpl", sizeof(info->exts));

   return menu_displaylist_push_list(info, DISPLAYLIST_CONTENT_HISTORY);
}

static int deferred_push_detect_core_list(menu_displaylist_info_t *info)
{
   settings_t *settings   = config_get_ptr();
   global_t *global       = global_get_ptr();

   info->type_default = MENU_FILE_PLAIN;
   if (global->core_info)
      strlcpy(info->exts, core_info_list_get_all_extensions(
         global->core_info), sizeof(info->exts));

   (void)settings;

   if (settings->multimedia.builtin_mediaplayer_enable ||
         settings->multimedia.builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};
       
      (void)sysinfo;
#ifdef HAVE_FFMPEG
      if (settings->multimedia.builtin_mediaplayer_enable)
      {
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (settings->multimedia.builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
   }
   
   return menu_displaylist_push_list(info, DISPLAYLIST_CORES_DETECTED);
}

static int deferred_push_default(menu_displaylist_info_t *info)
{
   settings_t        *settings = config_get_ptr();
   global_t            *global = global_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   info->type_default = MENU_FILE_PLAIN;
   info->setting      = menu_setting_find(info->label);

   if (info->setting && info->setting->browser_selection_type == ST_DIR) {}
   else if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         strlcpy(info->exts, global->menu.info.valid_extensions,
               sizeof(info->exts));
   }
   else
      strlcpy(info->exts, system->valid_extensions, sizeof(info->exts));

   (void)settings;

   if (settings->multimedia.builtin_mediaplayer_enable ||
         settings->multimedia.builtin_imageviewer_enable)
   {
      struct retro_system_info sysinfo = {0};
       
      (void)sysinfo;
#ifdef HAVE_FFMPEG
      if (settings->multimedia.builtin_mediaplayer_enable)
      {
         libretro_ffmpeg_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
#ifdef HAVE_IMAGEVIEWER
      if (settings->multimedia.builtin_imageviewer_enable)
      {
         libretro_imageviewer_retro_get_system_info(&sysinfo);
         strlcat(info->exts, "|", sizeof(info->exts));
         strlcat(info->exts, sysinfo.valid_extensions, sizeof(info->exts));
      }
#endif
   }

   return menu_displaylist_push_list(info, DISPLAYLIST_DEFAULT);
}

static int menu_cbs_init_bind_deferred_push_compare_label(menu_file_list_cbs_t *cbs, 
      const char *label, uint32_t label_hash)
{
   if (strstr(label, menu_hash_to_str(MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL)))
      cbs->action_deferred_push = deferred_push_rdb_entry_detail;
   else
   {
      switch (label_hash)
      {
         case MENU_LABEL_DEFERRED_ARCHIVE_ACTION_DETECT_CORE:
            cbs->action_deferred_push = deferred_archive_action_detect_core;
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_ACTION:
            cbs->action_deferred_push = deferred_archive_action;
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_OPEN_DETECT_CORE:
            cbs->action_deferred_push = deferred_archive_open_detect_core;
            break;
         case MENU_LABEL_DEFERRED_ARCHIVE_OPEN:
            cbs->action_deferred_push = deferred_archive_open;
            break;
         case MENU_LABEL_DEFERRED_CORE_CONTENT_LIST:
#ifdef HAVE_NETWORKING
            cbs->action_deferred_push = deferred_push_core_content_list;
#endif
            break;
         case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
#ifdef HAVE_NETWORKING
            cbs->action_deferred_push = deferred_push_core_updater_list;
#endif
            break;
         case MENU_LABEL_LOAD_CONTENT_HISTORY:
            cbs->action_deferred_push = deferred_push_history_list;
            break;
         case MENU_LABEL_DATABASE_MANAGER_LIST:
            cbs->action_deferred_push = deferred_push_database_manager_list;
            break;
         case MENU_LABEL_CURSOR_MANAGER_LIST:
            cbs->action_deferred_push = deferred_push_cursor_manager_list;
            break;
         case MENU_LABEL_CHEAT_FILE_LOAD:
            cbs->action_deferred_push = deferred_push_cheat_file_load;
            break;
         case MENU_LABEL_REMAP_FILE_LOAD:
            cbs->action_deferred_push = deferred_push_remap_file_load;
            break;
         case MENU_LABEL_RECORD_CONFIG:
            cbs->action_deferred_push = deferred_push_record_configfile;
            break;
         case MENU_LABEL_SHADER_OPTIONS:
            cbs->action_deferred_push = deferred_push_shader_options;
            break;
         case MENU_LABEL_ONLINE_UPDATER:
            cbs->action_deferred_push = deferred_push_options;
            break;
         case MENU_LABEL_CONTENT_SETTINGS:
            cbs->action_deferred_push = deferred_push_content_settings;
            break;
         case MENU_LABEL_ADD_CONTENT_LIST:
            cbs->action_deferred_push = deferred_push_add_content_list;
            break;
         case MENU_LABEL_LOAD_CONTENT_LIST:
            cbs->action_deferred_push = deferred_push_load_content_list;
            break;
         case MENU_LABEL_INFORMATION_LIST:
            cbs->action_deferred_push = deferred_push_information_list;
            break;
         case MENU_LABEL_MANAGEMENT:
            cbs->action_deferred_push = deferred_push_management_options;
            break;
         case MENU_LABEL_HELP_LIST:
            cbs->action_deferred_push = deferred_push_help;
            break;
         case MENU_LABEL_DEFERRED_CORE_LIST:
            cbs->action_deferred_push = deferred_push_core_list_deferred;
            break;
         case MENU_LABEL_DEFERRED_CORE_LIST_SET:
            cbs->action_deferred_push = deferred_push_core_collection_list_deferred;
            break;
         case MENU_LABEL_DEFERRED_VIDEO_FILTER:
            cbs->action_deferred_push = deferred_push_video_filter;
            break;
         case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
            cbs->action_deferred_push = deferred_push_database_manager_list_deferred;
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
            cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred;
            break;
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PUBLISHER:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_DEVELOPER:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ORIGIN:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FRANCHISE:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ENHANCEMENT_HW:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ESRB_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_BBFC_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_ELSPA_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_PEGI_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_CERO_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_MAX_USERS:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEMONTH:
         case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST_RDB_ENTRY_RELEASEYEAR:
            cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred_query_subsearch;
            break;
         case MENU_LABEL_CORE_INFORMATION:
            cbs->action_deferred_push = deferred_push_core_information;
            break;
         case MENU_LABEL_SYSTEM_INFORMATION:
            cbs->action_deferred_push = deferred_push_system_information;
            break;
         case MENU_LABEL_CORE_COUNTERS:
            cbs->action_deferred_push = deferred_push_core_counters;
            break;
         case MENU_LABEL_FRONTEND_COUNTERS:
            cbs->action_deferred_push = deferred_push_frontend_counters;
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            cbs->action_deferred_push = deferred_push_video_shader_preset_parameters;
            break;
         case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
            cbs->action_deferred_push = deferred_push_video_shader_parameters;
            break;
         case MENU_LABEL_SETTINGS:
            cbs->action_deferred_push = deferred_push_settings;
            break;
         case MENU_LABEL_CORE_OPTIONS:
            cbs->action_deferred_push = deferred_push_core_options;
            break;
         case MENU_LABEL_CORE_CHEAT_OPTIONS:
            cbs->action_deferred_push = deferred_push_core_cheat_options;
            break;
         case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
            cbs->action_deferred_push = deferred_push_core_input_remapping_options;
            break;
         case MENU_LABEL_CORE_LIST:
            cbs->action_deferred_push = deferred_push_core_list;
            break;
         case MENU_LABEL_CONTENT_COLLECTION_LIST:
            cbs->action_deferred_push = deferred_push_content_collection_list;
            break;
         case MENU_LABEL_CONFIGURATIONS:
            cbs->action_deferred_push = deferred_push_configurations;
            break;
         case MENU_LABEL_VIDEO_SHADER_PRESET:
            cbs->action_deferred_push = deferred_push_video_shader_preset;
            break;
         case MENU_LABEL_VIDEO_SHADER_PASS:
            cbs->action_deferred_push = deferred_push_video_shader_pass;
            break;
         case MENU_LABEL_VIDEO_FILTER:
            cbs->action_deferred_push = deferred_push_video_filter;
            break;
         case MENU_LABEL_MENU_WALLPAPER:
            cbs->action_deferred_push = deferred_push_images;
            break;
         case MENU_LABEL_AUDIO_DSP_PLUGIN:
            cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
            break;
         case MENU_LABEL_INPUT_OVERLAY:
            cbs->action_deferred_push = deferred_push_input_overlay;
            break;
         case MENU_LABEL_INPUT_OSK_OVERLAY:
            cbs->action_deferred_push = deferred_push_input_osk_overlay;
            break;
         case MENU_LABEL_VIDEO_FONT_PATH:
            cbs->action_deferred_push = deferred_push_video_font_path;
            break;
         case MENU_LABEL_CONTENT_HISTORY_PATH:
            cbs->action_deferred_push = deferred_push_content_history_path;
            break;
         case MENU_LABEL_DOWNLOADED_FILE_DETECT_CORE_LIST:
         case MENU_LABEL_DETECT_CORE_LIST:
            cbs->action_deferred_push = deferred_push_detect_core_list;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_deferred_push_compare_type(menu_file_list_cbs_t *cbs, unsigned type,
      uint32_t label_hash)
{
   if (type == MENU_SETTING_GROUP)
      cbs->action_deferred_push = deferred_push_category;
   else if (type == MENU_FILE_PLAYLIST_COLLECTION)
      cbs->action_deferred_push = deferred_push_rdb_collection;
   else if (type == MENU_SETTING_ACTION_CORE_DISK_OPTIONS)
      cbs->action_deferred_push = deferred_push_disk_options;
   else
      return -1;

   return 0;
}

int menu_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   settings_t *settings   = config_get_ptr();
   rarch_setting_t *setting = menu_setting_find(elem0);

   if (!cbs)
      return -1;

   cbs->action_deferred_push = deferred_push_default;

   if (setting)
   {
      uint32_t parent_group_hash = menu_hash_calculate(setting->parent_group);

      if ((parent_group_hash == MENU_VALUE_MAIN_MENU) && setting->type == ST_GROUP)
      {
         if (!settings->menu.collapse_subgroups_enable)
         {
            cbs->action_deferred_push = deferred_push_settings_subgroup;
            return 0;
         }
      }
   }

   if (menu_cbs_init_bind_deferred_push_compare_label(cbs, label, label_hash) == 0)
      return 0;

   if (menu_cbs_init_bind_deferred_push_compare_type(cbs, type, label_hash) == 0)
      return 0;

   return -1;
}
