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
#include "../menu_display.h"
#include "../menu_setting.h"
#include "../menu_entry.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"
#include "../menu_hash.h"

#include "../../general.h"
#include "../../retroarch.h"
#include "../../runloop_data.h"
#include "../../input/input_remapping.h"

/* FIXME - Global variables, refactor */
char detect_content_path[PATH_MAX_LENGTH];
unsigned rdb_entry_start_game_selection_ptr;
size_t hack_shader_pass = 0;
#ifdef HAVE_NETWORKING
char core_updater_path[PATH_MAX_LENGTH];
#endif

static int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path)
{
   char s[PATH_MAX_LENGTH] = {0};
   fill_pathname_join(s, dir, path, sizeof(s));
   setting_set_with_string_representation(setting, s);
   return menu_setting_generic(setting, false);
}

static int rarch_defer_core_wrapper(menu_displaylist_info_t *info,
      size_t idx, size_t entry_idx, const char *path, uint32_t hash_label,
      bool is_carchive)
{
   const char *menu_path    = NULL;
   const char *menu_label    = NULL;
   int ret                  = 0;
   menu_handle_t *menu      = menu_driver_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   if (!menu)
      return -1;

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   ret = rarch_defer_core(global->core_info,
         menu_path, path, menu_label, menu->deferred_path,
         sizeof(menu->deferred_path));

   if (!is_carchive)
      fill_pathname_join(detect_content_path, menu_path, path,
            sizeof(detect_content_path));

   switch (ret)
   {
      case -1:
         switch (hash_label)
         {
            case MENU_LABEL_COLLECTION:
               info->list          = menu_list->menu_stack;
               info->type          = 0;
               info->directory_ptr = idx;
               rdb_entry_start_game_selection_ptr = idx;
               strlcpy(info->path, settings->libretro_directory, sizeof(info->path));
               strlcpy(info->label, "deferred_core_list_set", sizeof(info->label));
               ret = menu_displaylist_push_list(info, DISPLAYLIST_GENERIC);
               break;
            default:
               event_command(EVENT_CMD_LOAD_CORE);
               menu_common_load_content(false);
               ret = -1;
               break;
         }
         break;
      case 0:
         info->list          = menu_list->menu_stack;
         info->type          = 0;
         info->directory_ptr = idx;
         strlcpy(info->path, settings->libretro_directory, sizeof(info->path));

         switch (hash_label)
         {
            case MENU_LABEL_COLLECTION:
               rdb_entry_start_game_selection_ptr = idx;
               strlcpy(info->label, "deferred_core_list_set", sizeof(info->label));
               break;
            default:
               strlcpy(info->label, "deferred_core_list", sizeof(info->label));
               break;
         }

         ret = menu_displaylist_push_list(info, DISPLAYLIST_GENERIC);
         break;
   }

   return ret;
}

static int action_ok_file_load_with_detect_core_carchive(
      const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   uint32_t hash_label      = menu_hash_calculate(label);

   strlcat(detect_content_path, "#", sizeof(detect_content_path));
   strlcat(detect_content_path, path, sizeof(detect_content_path));

   return rarch_defer_core_wrapper(&info, idx, entry_idx, path, hash_label, true);
}

static int action_ok_file_load_with_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   uint32_t hash_label      = menu_hash_calculate(label);

   return rarch_defer_core_wrapper(&info, idx, entry_idx, path, hash_label, false);
}

static int action_ok_file_load_detect_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   strlcpy(global->fullpath, detect_content_path, sizeof(global->fullpath));
   strlcpy(settings->libretro, path, sizeof(settings->libretro));
   event_command(EVENT_CMD_LOAD_CORE);
   menu_common_load_content(false);

   return -1;
}

static int action_ok_playlist_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   uint32_t core_name_hash, core_path_hash;
   const char *entry_path       = NULL;
   const char *entry_label      = NULL;
   const char *core_path        = NULL;
   const char *core_name        = NULL;
   size_t selection_ptr         = 0;
   content_playlist_t *playlist = g_defaults.history;
   menu_handle_t *menu          = menu_driver_get_ptr();
   uint32_t hash_label          = menu_hash_calculate(label);

   if (!menu)
      return -1;

   switch (hash_label)
   {
      case MENU_LABEL_COLLECTION:
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         if (!menu->playlist)
         {
            menu->playlist = content_playlist_init(menu->db_playlist_file, 1000);

            if (!menu->playlist)
               return -1;
         }

         playlist = menu->playlist;
         break;
   }

   selection_ptr = entry_idx;

   switch (hash_label)
   {
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         selection_ptr = rdb_entry_start_game_selection_ptr;
         break;
   }

   content_playlist_get_index(playlist, selection_ptr,
         &entry_path, &entry_label, &core_path, &core_name, NULL, NULL); 

#if 0
   RARCH_LOG("path: %s, label: %s, core path: %s, core name: %s, idx: %d\n", entry_path, entry_label,
         core_path, core_name, selection_ptr);
#endif

   core_path_hash = core_path ? menu_hash_calculate(core_path) : 0;
   core_name_hash = core_name ? menu_hash_calculate(core_name) : 0;

   if (
         (core_path_hash == MENU_VALUE_DETECT) &&
         (core_name_hash == MENU_VALUE_DETECT)
      )
      return action_ok_file_load_with_detect_core(entry_path, label, type, selection_ptr, entry_idx);

   rarch_playlist_load_content(playlist, selection_ptr);

   return -1;
}



static int action_ok_cheat_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;

   if (!cheat)
      return -1;

   cheat_manager_apply_cheats(cheat);

   return 0;
}


static int action_ok_shader_pass_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char    *menu_path = NULL;
   menu_handle_t      *menu = menu_driver_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   if (!menu || !menu_list)
      return -1;

   (void)menu_path;
   (void)menu_list;

#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(menu->shader->pass[hack_shader_pass].source.path,
         menu_path, path,
         sizeof(menu->shader->pass[hack_shader_pass].source.path));

   /* This will reset any changed parameters. */
   video_shader_resolve_parameters(NULL, menu->shader);
   menu_list_flush_stack(menu_list, "shader_options", 0);
   return 0;
#else
   return -1;
#endif
}

#ifdef HAVE_SHADER_MANAGER
extern size_t hack_shader_pass;
#endif

static int action_ok_shader_pass(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   hack_shader_pass             = type - MENU_SETTINGS_SHADER_PASS_0;
   menu_handle_t *menu          = menu_driver_get_ptr();
   menu_list_t *menu_list       = menu_list_get_ptr();
   settings_t *settings         = config_get_ptr();
   if (!menu || !menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->video.shader_dir, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_shader_parameters(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu          = menu_driver_get_ptr();
   menu_list_t *menu_list       = menu_list_get_ptr();

   if (!menu || !menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = MENU_SETTING_ACTION;
   info.directory_ptr = idx;
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_push_generic_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t          *menu = menu_driver_get_ptr();
   menu_list_t       *menu_list = menu_list_get_ptr();

   if (!menu || !menu_list)
      return -1;

   if (path)
      strlcpy(menu->deferred_path, path,
            sizeof(menu->deferred_path));

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_push_default(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_list_t       *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, label, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_shader_preset(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_handle_t *menu          = menu_driver_get_ptr();
   settings_t *settings         = config_get_ptr();
   menu_list_t       *menu_list = menu_list_get_ptr();

   if (!menu)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->video.shader_dir, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_push_content_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();
   menu_list_t       *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = MENU_FILE_DIRECTORY;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->menu_content_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_disk_image_append_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();
   menu_list_t       *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->menu_content_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_configurations_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();
   const char *dir              = settings->menu_config_directory;
   menu_list_t       *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   if (dir)
      strlcpy(info.path, dir, sizeof(info.path));
   else
      strlcpy(info.path, label, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_cheat_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_list_t       *menu_list = menu_list_get_ptr();
   settings_t *settings         = config_get_ptr();
   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->cheat_database, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_audio_dsp_plugin(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_list_t       *menu_list = menu_list_get_ptr();
   settings_t *settings         = config_get_ptr();
   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->audio.filter_dir, sizeof(info.path));
   strlcpy(info.label, "audio_dsp_plugin", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_video_filter(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   settings_t *settings         = config_get_ptr();
   menu_list_t     *menu_list   = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->video.filter_dir, sizeof(info.path));
   strlcpy(info.label, "deferred_video_filter", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_core_updater_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char url_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t info   = {0};
   driver_t *driver               = driver_get_ptr();
   menu_list_t       *menu_list   = menu_list_get_ptr();
   settings_t *settings           = config_get_ptr();
   if (!menu_list)
      return -1;

   (void)url_path;

   menu_entries_set_nonblocking_refresh();

   if (settings->network.buildbot_url[0] == '\0')
      return -1;

#ifdef HAVE_NETWORKING
   event_command(EVENT_CMD_NETWORK_INIT);

   fill_pathname_join(url_path, settings->network.buildbot_url,
         ".index", sizeof(url_path));

   rarch_main_data_msg_queue_push(DATA_TYPE_HTTP, url_path, "cb_core_updater_list", 0, 1,
         true);
#endif

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, "deferred_core_updater_list", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
} 
static int action_ok_remap_file(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info   = {0};
   menu_list_t       *menu_list   = menu_list_get_ptr();
   settings_t *settings           = config_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->input_remapping_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_record_configfile(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info   = {0};
   menu_list_t       *menu_list   = menu_list_get_ptr();
   global_t   *global             = global_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, global->record.config_dir, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_playlist_collection(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info   = {0};
   menu_list_t       *menu_list   = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_content_collection_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t   info = {0};
   menu_list_t       *menu_list   = menu_list_get_ptr();
   settings_t       *settings     = config_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->playlist_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_core_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info   = {0};
   menu_list_t       *menu_list   = menu_list_get_ptr();
   settings_t *settings           = config_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, settings->libretro_directory, sizeof(info.path));
   strlcpy(info.label, label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_record_configfile_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char          *menu_path = NULL;
   global_t               *global = global_get_ptr();
   menu_list_t       *menu_list   = menu_list_get_ptr();

   if (!global || !menu_list)
      return -1;

   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(global->record.config, menu_path, path, sizeof(global->record.config));

   menu_list_flush_stack(menu_list, menu_hash_to_str(MENU_VALUE_RECORDING_SETTINGS), 0);
   return 0;
}

static int action_ok_remap_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char            *menu_path = NULL;
   char remap_path[PATH_MAX_LENGTH] = {0};
   menu_list_t       *menu_list     = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   (void)remap_path;
   (void)menu_path;

   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(remap_path, menu_path, path, sizeof(remap_path));
   input_remapping_load_file(remap_path);

   menu_list_flush_stack(menu_list, "core_input_remapping_options", 0);

   return 0;
}

static int action_ok_video_filter_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char             *menu_path = NULL;
   char filter_path[PATH_MAX_LENGTH] = {0};
   menu_list_t       *menu_list      = menu_list_get_ptr();
   settings_t              *settings = config_get_ptr();

   (void)filter_path;
   (void)menu_path;

   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(filter_path, menu_path, path, sizeof(filter_path));

   strlcpy(settings->video.softfilter_plugin, filter_path,
         sizeof(settings->video.softfilter_plugin));

   event_command(EVENT_CMD_REINIT);

   menu_list_flush_stack(menu_list, "video_options", 0);

   return 0;
}

static int action_ok_cheat_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char            *menu_path = NULL;
   char cheat_path[PATH_MAX_LENGTH] = {0};
   menu_list_t       *menu_list     = menu_list_get_ptr();
   global_t                 *global = global_get_ptr();

   (void)cheat_path;
   (void)menu_path;
   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(cheat_path, menu_path, path, sizeof(cheat_path));

   if (global->cheat)
      cheat_manager_free(global->cheat);

   global->cheat = cheat_manager_load(cheat_path);

   if (!global->cheat)
      return -1;

   menu_list_flush_stack(menu_list, "core_cheat_options", 0);

   return 0;
}

static int action_ok_menu_wallpaper_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char wallpaper_path[PATH_MAX_LENGTH] = {0};
   const char *menu_label               = NULL;
   const char *menu_path                = NULL;
   rarch_setting_t *setting             = NULL;
   menu_list_t       *menu_list         = menu_list_get_ptr();
   settings_t *settings                 = config_get_ptr();

   if (!menu_list)
      return -1;

   menu_list_get_last_stack(menu_list, &menu_path, &menu_label,
         NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   fill_pathname_join(wallpaper_path, menu_path, path, sizeof(wallpaper_path));

   if (path_file_exists(wallpaper_path))
   {
      strlcpy(settings->menu.wallpaper, wallpaper_path, sizeof(settings->menu.wallpaper));

      rarch_main_data_msg_queue_push(DATA_TYPE_IMAGE, wallpaper_path, "cb_menu_wallpaper", 0, 1,
            true);
   }

   menu_list_pop_stack_by_needle(menu_list, setting->name);

   return 0;
}

static int action_ok_shader_preset_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char             *menu_path = NULL;
   char shader_path[PATH_MAX_LENGTH] = {0};
   menu_handle_t               *menu = menu_driver_get_ptr();
   menu_list_t       *menu_list      = menu_list_get_ptr();
   if (!menu || !menu_list)
      return -1;

   (void)shader_path;
   (void)menu_path;
   (void)menu_list;

#ifdef HAVE_SHADER_MANAGER
   menu_list_get_last_stack(menu_list, &menu_path, NULL,
         NULL, NULL);

   fill_pathname_join(shader_path, menu_path, path, sizeof(shader_path));
   menu_shader_manager_set_preset(menu->shader,
         video_shader_parse_type(shader_path, RARCH_SHADER_NONE),
         shader_path);
   menu_list_flush_stack(menu_list, "shader_options", 0);
   return 0;
#else
   return -1;
#endif
}

static int action_ok_cheat(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_key_start_line("Input Cheat",
         label, type, idx, menu_input_st_cheat_callback);
   return 0;
}

static int action_ok_shader_preset_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_key_start_line("Preset Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_cheat_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_key_start_line("Cheat Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_remap_file_save_as(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_input_key_start_line("Remapping Filename",
         label, type, idx, menu_input_st_string_callback);
   return 0;
}

static int action_ok_remap_file_save_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char directory[PATH_MAX_LENGTH]   = {0};
   char file[PATH_MAX_LENGTH]        = {0};
   const char *core_name             = NULL;
   global_t *global                  = global_get_ptr();
   settings_t *settings              = config_get_ptr();

   core_name = global->system.info.library_name;

   fill_pathname_join(directory,settings->input_remapping_directory,core_name,PATH_MAX_LENGTH);
   fill_pathname_join(file,core_name,core_name,PATH_MAX_LENGTH);

   if(!path_file_exists(directory))
       path_mkdir(directory);

   if(input_remapping_save_file(file))
      rarch_main_msg_queue_push("Remap file saved successfully", 1, 100, true);
   else
      rarch_main_msg_queue_push("Error saving remap file", 1, 100, true);

   return 0;
}

static int action_ok_remap_file_save_game(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *core_name           = NULL;
   const char *game_name           = NULL;
   char directory[PATH_MAX_LENGTH] = {0};
   char file[PATH_MAX_LENGTH]      = {0};
   global_t *global                = global_get_ptr();
   settings_t *settings            = config_get_ptr();

   core_name = global->system.info.library_name;
   game_name = path_basename(global->basename);

   fill_pathname_join(directory,settings->input_remapping_directory,core_name,PATH_MAX_LENGTH);
   fill_pathname_join(file,core_name,game_name,PATH_MAX_LENGTH);

   if(!path_file_exists(directory))
       path_mkdir(directory);

   if(input_remapping_save_file(file))
      rarch_main_msg_queue_push("Remap file saved successfully", 1, 100, true);
   else
      rarch_main_msg_queue_push("Error saving remap file", 1, 100, true);

   return 0;
}

static int action_ok_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_entry_pathdir_set_value(0, NULL);
}

static int action_ok_core_deferred_set(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char core_display_name[PATH_MAX_LENGTH] = {0};
   menu_handle_t                     *menu = menu_driver_get_ptr();
   menu_list_t                  *menu_list = menu_list_get_ptr();
   if (!menu || !menu_list)
      return -1;

   rarch_assert(menu->playlist != NULL);

   core_info_get_name(path, core_display_name, sizeof(core_display_name));

   idx = rdb_entry_start_game_selection_ptr;

   content_playlist_update(menu->playlist, idx,
         menu->playlist->entries[idx].path, menu->playlist->entries[idx].label,
         path , core_display_name,
         menu->playlist->entries[idx].crc32,
         menu->playlist->entries[idx].db_name);

   content_playlist_write_file(menu->playlist);

   menu_list_pop_stack(menu_list);

   return -1;
}

static int action_ok_core_load_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu      = menu_driver_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   if (!menu)
      return -1;

   if (path)
      strlcpy(settings->libretro, path, sizeof(settings->libretro));
   strlcpy(global->fullpath, menu->deferred_path,
         sizeof(global->fullpath));

   menu_common_load_content(false);

   return -1;
}

static int action_ok_database_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

static int action_ok_rdb_entry(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char tmp[PATH_MAX_LENGTH]               = {0};
   menu_displaylist_info_t            info = {0};
   menu_list_t                  *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   strlcpy(tmp, "deferred_rdb_entry_detail|", sizeof(tmp));
   strlcat(tmp, path, sizeof(tmp));

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, label, sizeof(info.path));
   strlcpy(info.label, tmp, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_cursor_manager_list_deferred(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

static int action_ok_core_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path    = NULL;
   menu_handle_t *menu      = menu_driver_get_ptr();
   menu_list_t   *menu_list = menu_list_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   if (!menu || !menu_list)
      return -1;

   (void)global;

   menu_list_get_last_stack(menu_list,
         &menu_path, NULL, NULL, NULL);

   fill_pathname_join(settings->libretro, menu_path, path,
         sizeof(settings->libretro));
   event_command(EVENT_CMD_LOAD_CORE);
   menu_list_pop_stack(menu_list);
#if defined(HAVE_DYNAMIC)
   /* No content needed for this core, load core immediately. */

   if (menu->load_no_content && settings->core.set_supports_no_game_enable)
   {
      *global->fullpath = '\0';

      menu_common_load_content(false);
      return -1;
   }

   return 0;
   /* Core selection on non-console just updates directory listing.
    * Will take effect on new content load. */
#elif defined(RARCH_CONSOLE)
   event_command(EVENT_CMD_RESTART_RETROARCH);
   return -1;
#endif
}

static int action_ok_core_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return 0;
}

static int action_ok_compressed_archive_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_list_t *menu_list = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.directory_ptr = idx;
   strlcpy(info.path, path, sizeof(info.path));
   strlcpy(info.label, "load_open_zip", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
}

static int action_ok_directory_push(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info   = {0};
   const char *menu_path          = NULL;
   const char *menu_label         = NULL;
   char cat_path[PATH_MAX_LENGTH] = {0};
   menu_list_t         *menu_list = menu_list_get_ptr();
   if (!menu_list || !path)
      return -1;

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   fill_pathname_join(cat_path, menu_path, path, sizeof(cat_path));

   info.list          = menu_list->menu_stack;
   info.type          = type;
   info.directory_ptr = idx;
   strlcpy(info.path, cat_path, sizeof(info.path));
   strlcpy(info.label, menu_label, sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_database_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char rdb_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t info   = {0};
   menu_list_t         *menu_list = menu_list_get_ptr();
   settings_t          *settings  = config_get_ptr();

   if (!menu_list || !path || !label)
      return -1;

   fill_pathname_join(rdb_path, settings->content_database,
         path, sizeof(rdb_path));

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, rdb_path, sizeof(info.path));
   strlcpy(info.label, "deferred_database_manager_list", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_cursor_manager_list(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char cursor_path[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t      info = {0};
   settings_t              *settings = config_get_ptr();
   menu_list_t            *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   fill_pathname_join(cursor_path, settings->cursor_directory,
         path, sizeof(cursor_path));

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, cursor_path, sizeof(info.path));
   strlcpy(info.label, "deferred_cursor_manager_list", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);
}

static int action_ok_config_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path         = NULL;
   char config[PATH_MAX_LENGTH]  = {0};
   menu_navigation_t        *nav = menu_navigation_get_ptr();
   menu_handle_t           *menu = menu_driver_get_ptr();
   menu_display_t          *disp = menu_display_get_ptr();
   menu_list_t        *menu_list = menu_list_get_ptr();

   if (!menu || !menu_list)
      return -1;

   menu_list_get_last_stack(menu_list, &menu_path, NULL, NULL, NULL);

   fill_pathname_join(config, menu_path, path, sizeof(config));
   menu_list_flush_stack(menu_list, NULL, MENU_SETTINGS);

   disp->msg_force = true;

   if (rarch_replace_config(config))
   {
      menu_navigation_clear(nav, false);
      return -1;
   }

   return 0;
}

static int action_ok_disk_image_append(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   char image[PATH_MAX_LENGTH] = {0};
   const char *menu_path       = NULL;
   menu_list_t      *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   menu_list_get_last_stack(menu_list, &menu_path, NULL, NULL, NULL);

   fill_pathname_join(image, menu_path, path, sizeof(image));
   event_disk_control_append_image(image);

   event_command(EVENT_CMD_RESUME);

   menu_list_flush_stack(menu_list, NULL, MENU_SETTINGS);
   return -1;
}


static int action_ok_file_load(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_label   = NULL;
   const char *menu_path    = NULL;
   rarch_setting_t *setting = NULL;
   global_t *global         = global_get_ptr();
   menu_list_t   *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   menu_list_get_last(menu_list->menu_stack,
         &menu_path, &menu_label, NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (setting && setting->type == ST_PATH)
   {
      menu_action_setting_set_current_string_path(setting, menu_path, path);
      menu_list_pop_stack_by_needle(menu_list, setting->name);
   }
   else
   {
      if (type == MENU_FILE_IN_CARCHIVE)
         fill_pathname_join_delim(global->fullpath, menu_path, path,
               '#',sizeof(global->fullpath));
      else
         fill_pathname_join(global->fullpath, menu_path, path,
               sizeof(global->fullpath));

      menu_common_load_content(true);

      return -1;
   }

   return 0;
}

static int action_ok_set_path(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   const char *menu_path    = NULL;
   const char *menu_label   = NULL;
   rarch_setting_t *setting = NULL;
   menu_list_t   *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   menu_list_get_last_stack(menu_list,
         &menu_path, &menu_label, NULL, NULL);

   setting = menu_setting_find(menu_label);

   if (!setting)
      return -1;

   menu_action_setting_set_current_string_path(setting, menu_path, path);
   menu_list_pop_stack_by_needle(menu_list, setting->name);

   return 0;
}

static int action_ok_custom_viewport(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   int ret                  = 0;
   video_viewport_t *custom = video_viewport_get_custom();
   settings_t *settings     = config_get_ptr();
   menu_list_t   *menu_list = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   info.list          = menu_list->menu_stack;
   info.type          = MENU_SETTINGS_CUSTOM_VIEWPORT;
   info.directory_ptr = idx;
   strlcpy(info.label, "custom_viewport_1", sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);

   video_driver_viewport_info(custom);

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   settings->video.aspect_ratio_idx = ASPECT_RATIO_CUSTOM;

   event_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);
   return ret;
}


static int generic_action_ok_command(enum event_command cmd)
{
   if (!event_command(cmd))
      return -1;
   return 0;
}

static int action_ok_load_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(EVENT_CMD_LOAD_STATE) == -1)
      return -1;
   return generic_action_ok_command(EVENT_CMD_RESUME);
}


static int action_ok_save_state(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   if (generic_action_ok_command(EVENT_CMD_SAVE_STATE) == -1)
      return -1;
   return generic_action_ok_command(EVENT_CMD_RESUME);
}

static int action_ok_core_updater_download(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
#ifdef HAVE_NETWORKING
   char core_path[PATH_MAX_LENGTH] = {0};
   char msg[PATH_MAX_LENGTH]       = {0};
   settings_t *settings            = config_get_ptr();

   fill_pathname_join(core_path, settings->network.buildbot_url,
         path, sizeof(core_path));

   strlcpy(core_updater_path, path, sizeof(core_updater_path));
   snprintf(msg, sizeof(msg), "Starting download: %s.", path);

   rarch_main_msg_queue_push(msg, 1, 90, true);

   rarch_main_data_msg_queue_push(DATA_TYPE_HTTP, core_path,
         "cb_core_updater_download", 0, 1, true);
#endif
   return 0;
}

static int action_ok_disk_cycle_tray_status(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_DISK_EJECT_TOGGLE);
}

static int action_ok_unload_core(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_UNLOAD_CORE);
}

static int action_ok_quit(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_QUIT);
}

static int action_ok_save_new_config(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_MENU_SAVE_CONFIG);
}

static int action_ok_resume_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_RESUME);
}

static int action_ok_restart_content(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_RESET);
}

static int action_ok_screenshot(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_TAKE_SCREENSHOT);
}

static int action_ok_file_load_or_resume(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_handle_t *menu   = menu_driver_get_ptr();
   global_t      *global = global_get_ptr();

   if (!menu)
      return -1;

   if (!strcmp(menu->deferred_path, global->fullpath))
      return generic_action_ok_command(EVENT_CMD_RESUME);
   else
   {
      strlcpy(global->fullpath,
            menu->deferred_path, sizeof(global->fullpath));
      event_command(EVENT_CMD_LOAD_CORE);
      rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
      return -1;
   }
}

static int action_ok_shader_apply_changes(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return generic_action_ok_command(EVENT_CMD_SHADERS_APPLY_CHANGES);
}

static int action_ok_lookup_setting(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   return menu_setting_set(type, label, MENU_ACTION_OK, false);
}

static int action_ok_rdb_entry_submenu(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   int ret;
   union string_list_elem_attr attr;
   char new_label[PATH_MAX_LENGTH] = {0};
   menu_displaylist_info_t info    = {0};
   char *rdb                       = NULL;
   int len                         = 0;
   struct string_list *str_list    = NULL;
   struct string_list *str_list2   = NULL;
   menu_list_t          *menu_list = menu_list_get_ptr();

   if (!menu_list || !label)
      return -1;

   str_list = string_split(label, "|");

   if (!str_list)
      return -1;

   str_list2 = string_list_new();
   if (!str_list2)
   {
      string_list_free(str_list);
      return -1;
   }

   /* element 0 : label
    * element 1 : value
    * element 2 : database path
    */

   attr.i = 0;

   len += strlen(str_list->elems[1].data) + 1;
   string_list_append(str_list2, str_list->elems[1].data, attr);

   len += strlen(str_list->elems[2].data) + 1;
   string_list_append(str_list2, str_list->elems[2].data, attr);

   rdb = (char*)calloc(len, sizeof(char));

   if (!rdb)
   {
      string_list_free(str_list);
      string_list_free(str_list2);
      return -1;
   }

   string_list_join_concat(rdb, len, str_list2, "|");

   strlcpy(new_label, "deferred_cursor_manager_list_", sizeof(new_label));
   strlcat(new_label, str_list->elems[0].data, sizeof(new_label));

   info.list          = menu_list->menu_stack;
   info.type          = 0;
   info.directory_ptr = idx;
   strlcpy(info.path, rdb, sizeof(info.path));
   strlcpy(info.label, new_label, sizeof(info.label));

   ret = menu_displaylist_push_list(&info, DISPLAYLIST_GENERIC);

   string_list_free(str_list);
   string_list_free(str_list2);

   return ret;
}

static int action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   menu_displaylist_info_t info = {0};
   menu_list_t *menu_list    = menu_list_get_ptr();
   if (!menu_list)
      return -1;

   info.list = menu_list->menu_stack;
   strlcpy(info.label, "help", sizeof(info.label));

   return menu_displaylist_push_list(&info, DISPLAYLIST_HELP);
}

static int action_ok_video_resolution(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx)
{
   unsigned width = 0, height = 0;
   global_t *global = global_get_ptr();

   (void)global;
   (void)width;
   (void)height;

#ifdef __CELLOS_LV2__
   if (global->console.screen.resolutions.list[
         global->console.screen.resolutions.current.idx] == 
         CELL_VIDEO_OUT_RESOLUTION_576)
   {
      if (global->console.screen.pal_enable)
         global->console.screen.pal60_enable = true;
   }
   else
   {
      global->console.screen.pal_enable = false;
      global->console.screen.pal60_enable = false;
   }

   event_command(EVENT_CMD_REINIT);
#else
   if (video_driver_get_video_output_size(&width, &height))
      video_driver_set_video_mode(width, height, true);
#endif

   return 0;
}

static int is_rdb_entry(uint32_t label_hash)
{
   switch (label_hash)
   {
      case MENU_LABEL_RDB_ENTRY_PUBLISHER:
      case MENU_LABEL_RDB_ENTRY_DEVELOPER:
      case MENU_LABEL_RDB_ENTRY_ORIGIN:
      case MENU_LABEL_RDB_ENTRY_FRANCHISE:
      case MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
      case MENU_LABEL_RDB_ENTRY_ESRB_RATING:
      case MENU_LABEL_RDB_ENTRY_BBFC_RATING:
      case MENU_LABEL_RDB_ENTRY_ELSPA_RATING:
      case MENU_LABEL_RDB_ENTRY_PEGI_RATING:
      case MENU_LABEL_RDB_ENTRY_CERO_RATING:
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
      case MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
      case MENU_LABEL_RDB_ENTRY_RELEASE_MONTH:
      case MENU_LABEL_RDB_ENTRY_RELEASE_YEAR:
      case MENU_LABEL_RDB_ENTRY_MAX_USERS:
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash, const char *elem0)
{
   rarch_setting_t *setting = menu_setting_find(label);
   uint32_t elem0_hash      = menu_hash_calculate(elem0);

   if (elem0[0] != '\0' && (is_rdb_entry(elem0_hash) == 0))
   {
      cbs->action_ok = action_ok_rdb_entry_submenu;
      return 0;
   }

   if (setting && setting->browser_selection_type == ST_DIR)
   {
      cbs->action_ok = action_ok_push_generic_list;
      return 0;
   }

   switch (hash)
   {
      case MENU_LABEL_CUSTOM_BIND_ALL:
         cbs->action_ok = action_ok_lookup_setting;
         break;
      case MENU_LABEL_SAVESTATE:
         cbs->action_ok = action_ok_save_state;
         break;
      case MENU_LABEL_LOADSTATE:
         cbs->action_ok = action_ok_load_state;
         break;
      case MENU_LABEL_RESUME_CONTENT:
         cbs->action_ok = action_ok_resume_content;
         break;
      case MENU_LABEL_RESTART_CONTENT:
         cbs->action_ok = action_ok_restart_content;
         break;
      case MENU_LABEL_TAKE_SCREENSHOT:
         cbs->action_ok = action_ok_screenshot;
         break;
      case MENU_LABEL_FILE_LOAD_OR_RESUME:
         cbs->action_ok = action_ok_file_load_or_resume;
         break;
      case MENU_LABEL_QUIT_RETROARCH:
         cbs->action_ok = action_ok_quit;
         break;
      case MENU_LABEL_UNLOAD_CORE:
         cbs->action_ok = action_ok_unload_core;
         break;
      case MENU_LABEL_SAVE_NEW_CONFIG:
         cbs->action_ok = action_ok_save_new_config;
         break;
      case MENU_LABEL_HELP:
         cbs->action_ok = action_ok_help;
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         cbs->action_ok = action_ok_shader_pass;
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         cbs->action_ok = action_ok_shader_preset;
         break;
      case MENU_LABEL_CHEAT_FILE_LOAD:
         cbs->action_ok = action_ok_cheat_file;
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         cbs->action_ok = action_ok_audio_dsp_plugin;
         break;
      case MENU_LABEL_VIDEO_FILTER:
         cbs->action_ok = action_ok_video_filter;
         break;
      case MENU_LABEL_REMAP_FILE_LOAD:
         cbs->action_ok = action_ok_remap_file;
         break;
      case MENU_LABEL_RECORD_CONFIG:
         cbs->action_ok = action_ok_record_configfile;
         break;
      case MENU_LABEL_CORE_UPDATER_LIST:
         cbs->action_ok = action_ok_core_updater_list;
         break;
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         cbs->action_ok = action_ok_shader_parameters;
         break;
      case MENU_LABEL_SHADER_OPTIONS:
      case MENU_LABEL_VIDEO_OPTIONS:
      case MENU_VALUE_INPUT_SETTINGS:
      case MENU_LABEL_CORE_OPTIONS:
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
      case MENU_LABEL_CORE_INFORMATION:
      case MENU_LABEL_SYSTEM_INFORMATION:
      case MENU_LABEL_DISK_OPTIONS:
      case MENU_LABEL_SETTINGS:
      case MENU_LABEL_PERFORMANCE_COUNTERS:
      case MENU_LABEL_FRONTEND_COUNTERS:
      case MENU_LABEL_CORE_COUNTERS:
      case MENU_LABEL_MANAGEMENT:
      case MENU_LABEL_OPTIONS:
         cbs->action_ok = action_ok_push_default;
         break;
      case MENU_LABEL_LOAD_CONTENT:
      case MENU_LABEL_DETECT_CORE_LIST:
         cbs->action_ok = action_ok_push_content_list;
         break;
      case MENU_LABEL_DETECT_CORE_LIST_OK:
         cbs->action_ok = action_ok_file_load_detect_core;
         break;
      case MENU_LABEL_HISTORY_LIST:
      case MENU_LABEL_CURSOR_MANAGER_LIST:
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         cbs->action_ok = action_ok_push_generic_list;
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         cbs->action_ok = action_ok_shader_apply_changes;
         break;
      case MENU_LABEL_CHEAT_APPLY_CHANGES:
         cbs->action_ok = action_ok_cheat_apply_changes;
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         cbs->action_ok = action_ok_shader_preset_save_as;
         break;
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         cbs->action_ok = action_ok_cheat_file_save_as;
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_AS:
         cbs->action_ok = action_ok_remap_file_save_as;
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         cbs->action_ok = action_ok_remap_file_save_core;
         break;
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         cbs->action_ok = action_ok_remap_file_save_game;
         break;
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         cbs->action_ok = action_ok_content_collection_list;
         break;
      case MENU_LABEL_CORE_LIST:
         cbs->action_ok = action_ok_core_list;
         break;
      case MENU_LABEL_DISK_IMAGE_APPEND:
         cbs->action_ok = action_ok_disk_image_append_list;
         break;
      case MENU_LABEL_CONFIGURATIONS:
         cbs->action_ok = action_ok_configurations_list;
         break;
      default:
         return -1;
   }

   return 0;
}

static int menu_cbs_init_bind_ok_compare_type(menu_file_list_cbs_t *cbs,
      uint32_t label_hash, uint32_t menu_label_hash, unsigned type)
{
   if (type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD ||
         type == MENU_SETTINGS_CUSTOM_BIND)
      cbs->action_ok = action_ok_lookup_setting;
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
      cbs->action_ok = NULL;
   else if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
      cbs->action_ok = action_ok_cheat;
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            cbs->action_ok = action_ok_video_resolution;
            break;
         case MENU_FILE_PLAYLIST_ENTRY:
            cbs->action_ok = action_ok_playlist_entry;
            break;
         case MENU_FILE_PLAYLIST_COLLECTION:
            cbs->action_ok = action_ok_playlist_collection;
            break;
         case MENU_FILE_CONTENTLIST_ENTRY:
            cbs->action_ok = action_ok_push_generic_list;
            break;
         case MENU_FILE_CHEAT:
            cbs->action_ok = action_ok_cheat_file_load;
            break;
         case MENU_FILE_RECORD_CONFIG:
            cbs->action_ok = action_ok_record_configfile_load;
            break;
         case MENU_FILE_REMAP:
            cbs->action_ok = action_ok_remap_file_load;
            break;
         case MENU_FILE_SHADER_PRESET:
            cbs->action_ok = action_ok_shader_preset_load;
            break;
         case MENU_FILE_SHADER:
            cbs->action_ok = action_ok_shader_pass_load;
            break;
         case MENU_FILE_IMAGE:
            cbs->action_ok = action_ok_menu_wallpaper_load;
            break;
         case MENU_FILE_USE_DIRECTORY:
            cbs->action_ok = action_ok_path_use_directory;
            break;
         case MENU_FILE_CONFIG:
            cbs->action_ok = action_ok_config_load;
            break;
         case MENU_FILE_DIRECTORY:
            cbs->action_ok = action_ok_directory_push;
            break;
         case MENU_FILE_CARCHIVE:
            cbs->action_ok = action_ok_compressed_archive_push;
            break;
         case MENU_FILE_CORE:
            if (label_hash == MENU_LABEL_DEFERRED_CORE_LIST)
               cbs->action_ok = action_ok_core_load_deferred;
            else
            {
               switch (menu_label_hash)
               {
                  case MENU_LABEL_DEFERRED_CORE_LIST_SET:
                     cbs->action_ok = action_ok_core_deferred_set;
                     break;
                  case MENU_LABEL_CORE_LIST:
                     cbs->action_ok = action_ok_core_load;
                     break;
                  case MENU_LABEL_CORE_UPDATER_LIST:
                     cbs->action_ok = action_ok_core_download;
                     break;
               }
            }
            break;
         case MENU_FILE_DOWNLOAD_CORE:
            cbs->action_ok = action_ok_core_updater_download;
            break;
         case MENU_FILE_DOWNLOAD_CORE_INFO:
            break;
         case MENU_FILE_RDB:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  cbs->action_ok = action_ok_database_manager_list_deferred;
                  break;
               case MENU_LABEL_DATABASE_MANAGER_LIST:
               case MENU_VALUE_HORIZONTAL_MENU:
                  cbs->action_ok = action_ok_database_manager_list;
                  break;
            }
            break;
         case MENU_FILE_RDB_ENTRY:
            cbs->action_ok = action_ok_rdb_entry;
            break;
         case MENU_FILE_CURSOR:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
                  cbs->action_ok = action_ok_cursor_manager_list_deferred;
                  break;
               case MENU_LABEL_CURSOR_MANAGER_LIST:
                  cbs->action_ok = action_ok_cursor_manager_list;
                  break;
            }
            break;
         case MENU_FILE_FONT:
         case MENU_FILE_OVERLAY:
         case MENU_FILE_AUDIOFILTER:
            cbs->action_ok = action_ok_set_path;
            break;
         case MENU_FILE_VIDEOFILTER:
            cbs->action_ok = action_ok_video_filter_file_load;
            break;
#ifdef HAVE_COMPRESSION
         case MENU_FILE_IN_CARCHIVE:
#endif
         case MENU_FILE_PLAIN:
            switch (menu_label_hash)
            {
               case MENU_LABEL_DETECT_CORE_LIST:
#ifdef HAVE_COMPRESSION
                  if (type == MENU_FILE_IN_CARCHIVE)
                     cbs->action_ok = action_ok_file_load_with_detect_core_carchive;
                  else
#endif
                     cbs->action_ok = action_ok_file_load_with_detect_core;
                  break;
               case MENU_LABEL_DISK_IMAGE_APPEND:
                  cbs->action_ok = action_ok_disk_image_append;
                  break;
               default:
                  cbs->action_ok = action_ok_file_load;
                  break;
            }
            break;
         case MENU_SETTINGS_CUSTOM_VIEWPORT:
            cbs->action_ok = action_ok_custom_viewport;
            break;
         case MENU_SETTINGS:
         case MENU_SETTING_GROUP:
         case MENU_SETTING_SUBGROUP:
            cbs->action_ok = action_ok_push_default;
            break;
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_CYCLE_TRAY_STATUS:
            cbs->action_ok = action_ok_disk_cycle_tray_status;
            break;
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_ok(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   cbs->action_ok = action_ok_lookup_setting;

   if (menu_cbs_init_bind_ok_compare_label(cbs, label, label_hash, elem0) == 0)
      return 0;

   if (menu_cbs_init_bind_ok_compare_type(cbs, label_hash, menu_label_hash, type) == 0)
      return 0;

   return -1;
}
