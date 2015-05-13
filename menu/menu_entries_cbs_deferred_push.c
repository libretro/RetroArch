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
#include "menu.h"
#include "menu_displaylist.h"
#include "menu_entries_cbs.h"
#include "menu_setting.h"

#include "../file_ext.h"
#include "../settings.h"

#include "../gfx/video_shader_driver.h"

static int deferred_push_core_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORE_INFO);
}

static int deferred_push_system_information(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORE_INFO);
}

static int deferred_push_rdb_entry_detail(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int ret;
   menu_displaylist_info_t info = {0};
   struct string_list *str_list  = string_split(label, "|"); 

   if (!str_list)
      return -1;

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.path_b,   str_list->elems[1].data, sizeof(info.path_b));
   strlcpy(info.label,    str_list->elems[0].data, sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_ENTRY);

   string_list_free(str_list);

   return ret;
}

static int deferred_push_core_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_ALL);
}

static int deferred_push_database_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return -1;

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   path, sizeof(info.path));
   strlcpy(info.path_b,    path, sizeof(info.path_b));
   strlcpy(info.label, label, sizeof(info.label));
   info.path_c[0] = '\0';

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);
}

static int deferred_push_cursor_manager_list_deferred(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   char *query = NULL, *rdb = NULL;
   char rdb_path[PATH_MAX_LENGTH];
   settings_t *settings   = config_get_ptr();
   config_file_t *conf = config_file_new(path);

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

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   rdb_path, sizeof(info.path));
   strlcpy(info.path_b,    path, sizeof(info.path_b));
   strlcpy(info.path_c,    query, sizeof(info.path_c));
   strlcpy(info.label, label, sizeof(info.label));

   menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);

   config_file_free(conf);
   return 0;
}

static int deferred_push_cursor_manager_list_deferred_query_subsearch(
      void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   int ret;
   menu_displaylist_info_t info = {0};
   char query[PATH_MAX_LENGTH];
   struct string_list *str_list  = string_split(path, "|"); 

   menu_database_build_query(query, sizeof(query), label, str_list->elems[0].data);

   if (query[0] == '\0')
   {
      string_list_free(str_list);
      return -1;
   }

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,   str_list->elems[1].data, sizeof(info.path));
   strlcpy(info.path_b,    str_list->elems[0].data, sizeof(info.path_b));
   strlcpy(info.path_c,    query, sizeof(info.path_c));
   strlcpy(info.label, label, sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_QUERY);

   string_list_free(str_list);

   return ret;
}

static int deferred_push_performance_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTER_SELECTION);
}

static int deferred_push_video_shader_preset_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PARAMETERS_PRESET);
}

static int deferred_push_video_shader_parameters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PARAMETERS);
}

static int deferred_push_settings(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS_ALL);

}

static int deferred_push_settings_subgroup(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list  = (file_list_t*)data;
   info.type  = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS_SUBGROUP);
}

static int deferred_push_category(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return -1;

   info.list  = (file_list_t*)data;
   info.type  = type;
   info.flags = SL_FLAG_ALL_SETTINGS;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SETTINGS);
}

static int deferred_push_video_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_VIDEO);
}

static int deferred_push_shader_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_SHADERS);
}

static int deferred_push_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      = type;
   strlcpy(info.path,  path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS);
}

static int deferred_push_management_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_MANAGEMENT);
}

static int deferred_push_core_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTERS_CORE);
}

static int deferred_push_frontend_counters(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_PERFCOUNTERS_FRONTEND);
}

static int deferred_push_core_cheat_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_CHEATS);
}


static int deferred_push_core_input_remapping_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_REMAPPINGS);
}

static int deferred_push_core_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORE_OPTIONS);
}

static int deferred_push_disk_options(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list      = (file_list_t*)data;
   info.menu_list = (file_list_t*)userdata;
   info.type      =  type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OPTIONS_DISK);
}

#ifdef HAVE_NETWORKING
/* HACK - we have to find some way to pass state inbetween
 * function pointer callback functions that don't necessarily
 * call each other. */
char *core_buf;
size_t core_len;

int cb_core_updater_list(void *data_, size_t len)
{
   char *data = (char*)data_;
   menu_handle_t *menu    = menu_driver_get_ptr();
   if (!menu)
      return -1;

   if (!data)
      return -1;

   if (core_buf)
      free(core_buf);

   core_buf = (char*)malloc(len * sizeof(char));

   if (!core_buf)
      return -1;

   memcpy(core_buf, data, len * sizeof(char));
   core_len = len;

   menu->nonblocking_refresh = false;

   return 0;
}

static int deferred_push_core_updater_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_UPDATER);
}
#endif

static int deferred_push_history_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_HISTORY);
}

static int deferred_push_content_actions(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_HORIZONTAL_CONTENT_ACTIONS);
}

int deferred_push_content_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;
   return menu_displaylist_push((file_list_t*)data, menu_list->selection_buf);
}

static int deferred_push_database_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings   = config_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_RDB;
   strlcpy(info.exts, "rdb", sizeof(info.exts));
   strlcpy(info.path, settings->content_database, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASES);
}

static int deferred_push_cursor_manager_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings   = config_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CURSOR;
   strlcpy(info.exts, "dbc", sizeof(info.exts));
   strlcpy(info.path, settings->cursor_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DATABASE_CURSORS);
}

static int deferred_push_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.exts, EXT_EXECUTABLES, sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES);
}

static int deferred_push_configurations(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CONFIG;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CONFIG_FILES);
}

static int deferred_push_video_shader_preset(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_SHADER_PRESET;
   strlcpy(info.exts, "cgp|glslp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PRESET);
}

static int deferred_push_video_shader_pass(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_SHADER;
   strlcpy(info.exts, "cg|glsl", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_SHADER_PASS);
}

static int deferred_push_video_filter(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_VIDEOFILTER;
   strlcpy(info.exts, "filt", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_VIDEO_FILTERS);
}

static int deferred_push_images(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_IMAGE;
   strlcpy(info.exts, "png", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_IMAGES);
}

static int deferred_push_audio_dsp_plugin(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_AUDIOFILTER;
   strlcpy(info.exts, "dsp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_AUDIO_FILTERS);
}

static int deferred_push_cheat_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_CHEAT;
   strlcpy(info.exts, "cht", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CHEAT_FILES);
}

static int deferred_push_remap_file_load(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_REMAP;
   strlcpy(info.exts, "rmp", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_REMAP_FILES);
}

static int deferred_push_record_configfile(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_RECORD_CONFIG;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_RECORD_CONFIG_FILES);
}

static int deferred_push_input_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_OVERLAY;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_input_osk_overlay(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_OVERLAY;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_OVERLAYS);
}

static int deferred_push_video_font_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_FONT;
   strlcpy(info.exts, "ttf", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_FONTS);
}

static int deferred_push_content_history_path(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.exts, "cfg", sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_CONTENT_HISTORY);
}

static int deferred_push_detect_core_list(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   global_t *global = global_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   if (global->core_info)
      strlcpy(info.exts, core_info_list_get_all_extensions(
         global->core_info), sizeof(info.exts));
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));
   
   return menu_displaylist_push_list(&info, DISPLAYLIST_CORES_DETECTED);
}

static int deferred_push_default(void *data, void *userdata,
      const char *path, const char *label, unsigned type)
{
   menu_displaylist_info_t info = {0};
   global_t *global         = global_get_ptr();

   info.list         = (file_list_t*)data;
   info.menu_list    = (file_list_t*)userdata;
   info.type         = type;
   info.type_default = MENU_FILE_PLAIN;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));
   info.setting      = menu_setting_find(label);

   if (info.setting && info.setting->browser_selection_type == ST_DIR) {}
   else if (global->menu.info.valid_extensions)
   {
      if (*global->menu.info.valid_extensions)
         snprintf(info.exts, sizeof(info.exts), "%s",
               global->menu.info.valid_extensions);
   }
   else
      strlcpy(info.exts, global->system.valid_extensions, sizeof(info.exts));

   return menu_displaylist_push_list(&info, DISPLAYLIST_DEFAULT);
}

void menu_entries_cbs_init_bind_deferred_push(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1)
{
   settings_t *settings   = config_get_ptr();

   if (!cbs)
      return;

   cbs->action_deferred_push = deferred_push_default;

   if ((strlen(elem1) != 0) && !!strcmp(elem0, elem1))
   {
      if (menu_entries_common_is_settings_entry(elem0))
      {
         if (!settings->menu.collapse_subgroups_enable)
         {
            cbs->action_deferred_push = deferred_push_settings_subgroup;
            return;
         }
      }
   }

   if (strstr(label, "deferred_rdb_entry_detail"))
      cbs->action_deferred_push = deferred_push_rdb_entry_detail;
#ifdef HAVE_NETWORKING
   else if (!strcmp(label, "deferred_core_updater_list"))
      cbs->action_deferred_push = deferred_push_core_updater_list;
#endif
   else if (!strcmp(label, "history_list"))
      cbs->action_deferred_push = deferred_push_history_list;
   else if (!strcmp(label, "database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list;
   else if (!strcmp(label, "cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list;
   else if (!strcmp(label, "cheat_file_load"))
      cbs->action_deferred_push = deferred_push_cheat_file_load;
   else if (!strcmp(label, "remap_file_load"))
      cbs->action_deferred_push = deferred_push_remap_file_load;
   else if (!strcmp(label, "record_config"))
      cbs->action_deferred_push = deferred_push_record_configfile;
   else if (!strcmp(label, "content_actions"))
      cbs->action_deferred_push = deferred_push_content_actions;
   else if (!strcmp(label, "shader_options"))
      cbs->action_deferred_push = deferred_push_shader_options;
   else if (!strcmp(label, "video_options"))
      cbs->action_deferred_push = deferred_push_video_options;
   else if (!strcmp(label, "options"))
      cbs->action_deferred_push = deferred_push_options;
   else if (!strcmp(label, "management"))
      cbs->action_deferred_push = deferred_push_management_options;
   else if (type == MENU_SETTING_GROUP)
      cbs->action_deferred_push = deferred_push_category;
   else if (!strcmp(label, "deferred_core_list"))
      cbs->action_deferred_push = deferred_push_core_list_deferred;
   else if (!strcmp(label, "deferred_video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "deferred_database_manager_list"))
      cbs->action_deferred_push = deferred_push_database_manager_list_deferred;
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred;
   else if (
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_enhancement_hw") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_famitsu_magazine_rating") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth") ||
         !strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear")
         )
      cbs->action_deferred_push = deferred_push_cursor_manager_list_deferred_query_subsearch;
   else if (!strcmp(label, "core_information"))
      cbs->action_deferred_push = deferred_push_core_information;
   else if (!strcmp(label, "system_information"))
      cbs->action_deferred_push = deferred_push_system_information;
   else if (!strcmp(label, "performance_counters"))
      cbs->action_deferred_push = deferred_push_performance_counters;
   else if (!strcmp(label, "core_counters"))
      cbs->action_deferred_push = deferred_push_core_counters;
   else if (!strcmp(label, "video_shader_preset_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_preset_parameters;
   else if (!strcmp(label, "video_shader_parameters"))
      cbs->action_deferred_push = deferred_push_video_shader_parameters;
   else if (!strcmp(label, "settings"))
      cbs->action_deferred_push = deferred_push_settings;
   else if (!strcmp(label, "frontend_counters"))
      cbs->action_deferred_push = deferred_push_frontend_counters;
   else if (!strcmp(label, "core_options"))
      cbs->action_deferred_push = deferred_push_core_options;
   else if (!strcmp(label, "core_cheat_options"))
      cbs->action_deferred_push = deferred_push_core_cheat_options;
   else if (!strcmp(label, "core_input_remapping_options"))
      cbs->action_deferred_push = deferred_push_core_input_remapping_options;
   else if (!strcmp(label, "disk_options"))
      cbs->action_deferred_push = deferred_push_disk_options;
   else if (!strcmp(label, "core_list"))
      cbs->action_deferred_push = deferred_push_core_list;
   else if (!strcmp(label, "configurations"))
      cbs->action_deferred_push = deferred_push_configurations;
   else if (!strcmp(label, "video_shader_preset"))
      cbs->action_deferred_push = deferred_push_video_shader_preset;
   else if (!strcmp(label, "video_shader_pass"))
      cbs->action_deferred_push = deferred_push_video_shader_pass;
   else if (!strcmp(label, "video_filter"))
      cbs->action_deferred_push = deferred_push_video_filter;
   else if (!strcmp(label, "menu_wallpaper"))
      cbs->action_deferred_push = deferred_push_images;
   else if (!strcmp(label, "audio_dsp_plugin"))
      cbs->action_deferred_push = deferred_push_audio_dsp_plugin;
   else if (!strcmp(label, "input_overlay"))
      cbs->action_deferred_push = deferred_push_input_overlay;
   else if (!strcmp(label, "input_osk_overlay"))
      cbs->action_deferred_push = deferred_push_input_osk_overlay;
   else if (!strcmp(label, "video_font_path"))
      cbs->action_deferred_push = deferred_push_video_font_path;
   else if (!strcmp(label, "game_history_path"))
      cbs->action_deferred_push = deferred_push_content_history_path;
   else if (!strcmp(label, "detect_core_list"))
      cbs->action_deferred_push = deferred_push_detect_core_list;
}
