/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2026 - libretro team
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

#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../content.h"
#include "../../core_info.h"
#include "../../retroarch_types.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../tasks/task_content.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "companion_core.h"

#define COMPANION_NO_SELECTION ((size_t)-1)

struct companion_core
{
   companion_callbacks_t cb;
   void *ud;

   /* Playlist files (*.lpl) in the playlist directory. */
   struct string_list *playlist_files;
   /* Display names, parallel to playlist_files. */
   char **playlist_names;

   /* Selected playlist. */
   playlist_t *playlist;
   playlist_parse_t *pending_parse;
   size_t selected;

   /* Budget for the parse step currently running. */
   retro_time_t budget_end;
};

/* --- Helpers --------------------------------------------------------- */

static void companion_core_free_playlist_names(companion_core_t *core)
{
   size_t i;
   if (!core->playlist_names || !core->playlist_files)
      return;
   for (i = 0; i < core->playlist_files->size; i++)
      free(core->playlist_names[i]);
   free(core->playlist_names);
   core->playlist_names = NULL;
}

static void companion_core_clear_playlist(companion_core_t *core)
{
   if (core->pending_parse)
   {
      playlist_parse_abort(core->pending_parse);
      core->pending_parse = NULL;
   }
   if (core->playlist)
   {
      playlist_free(core->playlist);
      core->playlist = NULL;
   }
}

static bool companion_core_budget_cb(void *ud)
{
   companion_core_t *core = (companion_core_t*)ud;
   return cpu_features_get_time_usec() < core->budget_end;
}

static void companion_core_playlist_config_init(playlist_config_t *cfg,
      const char *path)
{
   settings_t *settings      = config_get_ptr();

   memset(cfg, 0, sizeof(*cfg));
   cfg->capacity             = COLLECTION_SIZE;
   cfg->old_format           = settings->bools.playlist_use_old_format;
   cfg->compress             = settings->bools.playlist_compression;
   cfg->fuzzy_archive_match  = settings->bools.playlist_fuzzy_archive_match;
   cfg->autofix_paths        = false;

   playlist_config_set_base_content_directory(cfg,
         settings->bools.playlist_portable_paths
         ? settings->paths.directory_menu_content
         : NULL);
   playlist_config_set_path(cfg, path);
}

/* --- Lifecycle ------------------------------------------------------- */

companion_core_t *companion_core_new(const companion_callbacks_t *cb,
      void *ud)
{
   companion_core_t *core = (companion_core_t*)calloc(1, sizeof(*core));
   if (!core)
      return NULL;
   if (cb)
      core->cb    = *cb;
   core->ud       = ud;
   core->selected = COMPANION_NO_SELECTION;
   return core;
}

void companion_core_free(companion_core_t *core)
{
   if (!core)
      return;
   companion_core_clear_playlist(core);
   companion_core_free_playlist_names(core);
   if (core->playlist_files)
      string_list_free(core->playlist_files);
   free(core);
}

void companion_core_iterate(companion_core_t *core, unsigned budget_us)
{
   int ret;

   if (!core || !core->pending_parse)
      return;

   core->budget_end = cpu_features_get_time_usec() + (retro_time_t)budget_us;
   ret              = playlist_parse_step(core->pending_parse,
         companion_core_budget_cb, core);

   if (ret == 0)
      return; /* Budget exhausted; resume next iterate. */

   /* Finished (1) or failed (-1): playlist_parse_end() frees the
    * handle in either case and yields NULL on failure. */
   core->playlist      = playlist_parse_end(core->pending_parse);
   core->pending_parse = NULL;

   if (core->cb.on_playlist_changed)
      core->cb.on_playlist_changed(core->ud);
}

/* --- Playlist files -------------------------------------------------- */

void companion_core_refresh_playlists(companion_core_t *core)
{
   size_t i;
   settings_t *settings = config_get_ptr();
   const char *dir      = settings->paths.directory_playlist;

   if (!core)
      return;

   companion_core_free_playlist_names(core);
   if (core->playlist_files)
   {
      string_list_free(core->playlist_files);
      core->playlist_files = NULL;
   }

   if (!string_is_empty(dir))
      core->playlist_files = dir_list_new(dir,
            "lpl", false, true, false, false);

   if (core->playlist_files && core->playlist_files->size > 0)
   {
      dir_list_sort(core->playlist_files, false);
      core->playlist_names = (char**)calloc(
            core->playlist_files->size, sizeof(char*));
      if (core->playlist_names)
      {
         for (i = 0; i < core->playlist_files->size; i++)
         {
            const char *base = path_basename(
                  core->playlist_files->elems[i].data);
            core->playlist_names[i] = strdup(base ? base : "");
            if (core->playlist_names[i])
               path_remove_extension(core->playlist_names[i]);
         }
      }
   }

   /* The previously selected playlist may have moved or vanished;
    * keep its contents but drop the index so the UI reselects. */
   core->selected = COMPANION_NO_SELECTION;

   if (core->cb.on_playlists_changed)
      core->cb.on_playlists_changed(core->ud);
}

size_t companion_core_playlist_count(companion_core_t *core)
{
   if (!core || !core->playlist_files)
      return 0;
   return core->playlist_files->size;
}

const char *companion_core_playlist_name(companion_core_t *core, size_t i)
{
   if (!core || !core->playlist_files || !core->playlist_names
         || i >= core->playlist_files->size)
      return NULL;
   return core->playlist_names[i];
}

const char *companion_core_playlist_path(companion_core_t *core, size_t i)
{
   if (!core || !core->playlist_files || i >= core->playlist_files->size)
      return NULL;
   return core->playlist_files->elems[i].data;
}

/* --- Selected playlist ----------------------------------------------- */

bool companion_core_select_playlist(companion_core_t *core, size_t i)
{
   playlist_config_t cfg;
   const char *path = companion_core_playlist_path(core, i);

   if (!path)
      return false;

   companion_core_clear_playlist(core);
   core->selected = i;

   companion_core_playlist_config_init(&cfg, path);
   core->pending_parse = playlist_parse_begin(&cfg);

   /* The parse advances from companion_core_iterate(); a NULL handle
    * here is an allocation failure, reported as an empty playlist. */
   if (!core->pending_parse && core->cb.on_playlist_changed)
      core->cb.on_playlist_changed(core->ud);

   return true;
}

size_t companion_core_selected_playlist(companion_core_t *core)
{
   if (!core)
      return COMPANION_NO_SELECTION;
   return core->selected;
}

bool companion_core_playlist_loading(companion_core_t *core)
{
   return core && core->pending_parse;
}

size_t companion_core_entry_count(companion_core_t *core)
{
   if (!core || !core->playlist)
      return 0;
   return playlist_size(core->playlist);
}

const struct playlist_entry *companion_core_entry(companion_core_t *core,
      size_t i)
{
   const struct playlist_entry *entry = NULL;
   if (!core || !core->playlist || i >= playlist_size(core->playlist))
      return NULL;
   playlist_get_index(core->playlist, i, &entry);
   return entry;
}

/* --- Commands -------------------------------------------------------- */

bool companion_core_request_load_entry(companion_core_t *core, size_t i)
{
   const char *core_path;
   const struct playlist_entry *entry = companion_core_entry(core, i);

   if (!entry || string_is_empty(entry->path))
      return false;

   /* "DETECT" (or no core) in a playlist means: use whatever core is
    * currently loaded, exactly as the menu does. */
   core_path = entry->core_path;
   if (string_is_empty(core_path) || string_is_equal(core_path, "DETECT"))
      core_path = path_get(RARCH_PATH_CORE);
   if (string_is_empty(core_path))
      return false;

   return companion_core_request_load_content(core, core_path,
         entry->path, entry->label, entry->db_name, entry->crc32);
}

bool companion_core_request_load_content(companion_core_t *core,
      const char *core_path, const char *content_path,
      const char *label, const char *db_name, const char *crc32)
{
   content_ctx_info_t content_info;
   char core_path_cached[PATH_MAX_LENGTH];
   char db_name_full[PATH_MAX_LENGTH];
   core_info_t *info = NULL;

   if (!core || string_is_empty(core_path) || string_is_empty(content_path))
      return false;

   /* Search for the specified core - ensures the path is sanitised. */
   if (core_info_find(core_path, &info) && !string_is_empty(info->path))
      core_path = info->path;

   /* CMD_EVENT_UNLOAD_CORE below frees the global core_info list, and
    * with it the string core_path may point into; keep a copy. */
   strlcpy(core_path_cached, core_path, sizeof(core_path_cached));

   db_name_full[0] = '\0';
   if (!string_is_empty(db_name))
      fill_pathname(db_name_full, db_name, ".lpl", sizeof(db_name_full));

   memset(&content_info, 0, sizeof(content_info));

   companion_core_unload_core(core);

   if (!task_push_load_content_with_new_core_from_companion_ui(
         core_path_cached, content_path, label,
         db_name_full[0] ? db_name_full : NULL, crc32,
         &content_info, NULL, NULL))
      return false;

#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_PENDING_QUICK_MENU, NULL);
#endif
   return true;
}

bool companion_core_load_core(companion_core_t *core, const char *path)
{
#ifdef HAVE_DYNAMIC
   if (!core || string_is_empty(path))
      return false;

   path_set(RARCH_PATH_CORE, path);

   command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

   core_info_init_current_core();

   return command_event(CMD_EVENT_LOAD_CORE, NULL);
#else
   (void)core;
   (void)path;
   return false;
#endif
}

bool companion_core_unload_core(companion_core_t *core)
{
   if (!core)
      return false;
#ifdef HAVE_MENU
   menu_state_get_ptr()->selection_ptr = 0;
#endif
   return command_event(CMD_EVENT_UNLOAD_CORE, NULL);
}

const char *companion_core_current_core_path(companion_core_t *core)
{
   const char *p = core ? path_get(RARCH_PATH_CORE) : NULL;
   return p ? p : "";
}

size_t companion_core_playlist_default_core(companion_core_t *core,
      const char *name, char *s, size_t len)
{
   size_t _len;
   char playlist_path[PATH_MAX_LENGTH];
   settings_t *settings  = config_get_ptr();
   playlist_t *playlist  = NULL;
   bool owned            = false;
   const char *def       = NULL;

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!core || string_is_empty(name))
      return 0;

   _len = fill_pathname_join_special(playlist_path,
         settings->paths.directory_playlist, name, sizeof(playlist_path));
   strlcpy_lit(playlist_path + _len, ".lpl", sizeof(playlist_path) - _len);

   playlist = companion_core_playlist_open(core, playlist_path, &owned);
   if (!playlist)
      return 0;

   def = playlist_get_default_core_path(playlist);
   if (!string_is_empty(def) && !string_is_equal(def, "DETECT"))
      strlcpy(s, def, len);

   companion_core_playlist_release(core, playlist, owned, false);
   return strlen(s);
}

/* --- Playlist editing -------------------------------------------------- */

playlist_t *companion_core_playlist_open(companion_core_t *core,
      const char *path, bool *owned)
{
   playlist_config_t cfg;
   playlist_t *cached = playlist_get_cached();

   if (owned)
      *owned = false;
   if (!core || string_is_empty(path))
      return NULL;

   /* Borrow the menu's cached playlist when it is the same file: an
    * edit then goes through the object the menu reads, and the write
    * keeps disk coherent with it. */
   if (cached && string_is_equal(path, playlist_get_conf_path(cached)))
      return cached;

   companion_core_playlist_config_init(&cfg, path);
   if (owned)
      *owned = true;
   return playlist_init(&cfg);
}

void companion_core_playlist_release(companion_core_t *core,
      playlist_t *playlist, bool owned, bool write)
{
   if (!core || !playlist)
      return;
   if (write)
      playlist_write_file(playlist);
   if (owned)
      playlist_free(playlist);
}

bool companion_core_playlist_update_entry(companion_core_t *core,
      const char *path, size_t index, const struct playlist_entry *entry)
{
   bool owned           = false;
   playlist_t *playlist = companion_core_playlist_open(core, path, &owned);

   if (!playlist || !entry)
      return false;
   if (index >= playlist_size(playlist))
   {
      companion_core_playlist_release(core, playlist, owned, false);
      return false;
   }

   playlist_update(playlist, index, entry);
   companion_core_playlist_release(core, playlist, owned, true);
   return true;
}

bool companion_core_playlist_delete_entry(companion_core_t *core,
      const char *path, size_t index)
{
   bool owned           = false;
   playlist_t *playlist = companion_core_playlist_open(core, path, &owned);

   if (!playlist)
      return false;
   if (index >= playlist_size(playlist))
   {
      companion_core_playlist_release(core, playlist, owned, false);
      return false;
   }

   playlist_delete_index(playlist, index);
   companion_core_playlist_release(core, playlist, owned, true);
   return true;
}

bool companion_core_playlist_set_default_core(companion_core_t *core,
      const char *path, const char *core_path)
{
   core_info_t *info    = NULL;
   bool owned           = false;
   playlist_t *playlist = companion_core_playlist_open(core, path, &owned);

   if (!playlist)
      return false;

   if (!string_is_empty(core_path) && core_info_find(core_path, &info))
   {
      playlist_set_default_core_path(playlist, info->path);
      playlist_set_default_core_name(playlist, info->display_name);
   }
   else
   {
      playlist_set_default_core_path(playlist, "DETECT");
      playlist_set_default_core_name(playlist, "DETECT");
   }

   companion_core_playlist_release(core, playlist, owned, true);
   return true;
}

bool companion_core_request_load(companion_core_t *core,
      const char *core_path, const char *content_path)
{
   content_ctx_info_t content_info;

   if (!core)
      return false;

   memset(&content_info, 0, sizeof(content_info));

   if (!string_is_empty(core_path))
      return task_push_load_content_with_new_core_from_companion_ui(
            core_path, content_path, NULL, NULL, NULL,
            &content_info, NULL, NULL);

   return task_push_load_content_with_current_core_from_companion_ui(
         content_path, &content_info, CORE_TYPE_PLAIN, NULL, NULL);
}

void companion_core_event_command(companion_core_t *core,
      enum event_command cmd)
{
   if (!core)
      return;
   command_event(cmd, NULL);
}

/* --- Inbound notifications ------------------------------------------- */

void companion_core_status_message(companion_core_t *core,
      const char *msg, unsigned prio, unsigned duration, bool flush)
{
   if (core && core->cb.on_status_message)
      core->cb.on_status_message(core->ud, msg, prio, duration, flush);
}

void companion_core_log_message(companion_core_t *core, const char *msg)
{
   if (core && core->cb.on_log_message)
      core->cb.on_log_message(core->ud, msg);
}

void companion_core_notify_refresh(companion_core_t *core)
{
   if (core && core->cb.on_notify_refresh)
      core->cb.on_notify_refresh(core->ud);
}
