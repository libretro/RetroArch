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
#include <file/archive_file.h>
#include <file/file_path.h>
#include <formats/image.h>
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
#include "../../runloop.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "companion_core.h"

#define COMPANION_NO_SELECTION ((size_t)-1)

#ifdef HAVE_LIBRETRODB
/* task_push_dbscan() carries no user data, so the requesting core is
 * kept here. Only one desktop companion is active at a time. */
static companion_core_t *companion_core_scan_owner = NULL;
#endif

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

   /* File the selected playlist was loaded from (select_playlist_path
    * may name a file outside the playlist directory). */
   char selected_path[PATH_MAX_LENGTH];

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
#ifdef HAVE_LIBRETRODB
   if (companion_core_scan_owner == core)
      companion_core_scan_owner = NULL;
#endif
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

static bool companion_core_begin_playlist(companion_core_t *core,
      const char *path, size_t index)
{
   playlist_config_t cfg;

   companion_core_clear_playlist(core);
   core->selected = index;
   strlcpy(core->selected_path, path, sizeof(core->selected_path));

   companion_core_playlist_config_init(&cfg, path);
   core->pending_parse = playlist_parse_begin(&cfg);

   /* The parse advances from companion_core_iterate(); a NULL handle
    * here is an allocation failure, reported as an empty playlist. */
   if (!core->pending_parse && core->cb.on_playlist_changed)
      core->cb.on_playlist_changed(core->ud);

   return true;
}

bool companion_core_select_playlist(companion_core_t *core, size_t i)
{
   const char *path = companion_core_playlist_path(core, i);
   if (!path)
      return false;
   return companion_core_begin_playlist(core, path, i);
}

bool companion_core_select_playlist_path(companion_core_t *core,
      const char *path)
{
   size_t i, n;

   if (!core || string_is_empty(path))
      return false;

   n = companion_core_playlist_count(core);
   for (i = 0; i < n; i++)
   {
      if (string_is_equal(path, core->playlist_files->elems[i].data))
         return companion_core_begin_playlist(core, path, i);
   }
   return companion_core_begin_playlist(core, path, COMPANION_NO_SELECTION);
}

size_t companion_core_selected_playlist(companion_core_t *core)
{
   if (!core)
      return COMPANION_NO_SELECTION;
   return core->selected;
}

const char *companion_core_selected_playlist_path(companion_core_t *core)
{
   if (!core || (!core->playlist && !core->pending_parse))
      return "";
   return core->selected_path;
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

/* --- Scan -------------------------------------------------------------- */

#ifdef HAVE_LIBRETRODB
static void companion_core_scan_finished(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   companion_core_t *core = companion_core_scan_owner;

   (void)task;
   (void)task_data;
   (void)user_data;
   (void)err;

#ifdef HAVE_MENU
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      if (menu_st->driver_ctx && menu_st->driver_ctx->environ_cb)
         menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
               NULL, menu_st->userdata);
   }
#endif

   if (core && core->cb.on_scan_finished)
      core->cb.on_scan_finished(core->ud);
}
#endif

bool companion_core_request_scan(companion_core_t *core, const char *path,
      bool directory, bool show_hidden_files)
{
#ifdef HAVE_LIBRETRODB
   settings_t *settings = config_get_ptr();

   if (!core || string_is_empty(path))
      return false;

   companion_core_scan_owner = core;
   return task_push_dbscan(
         settings->paths.directory_playlist,
         settings->paths.path_content_database,
         path, directory, show_hidden_files,
         companion_core_scan_finished);
#else
   (void)core;
   (void)path;
   (void)directory;
   (void)show_hidden_files;
   return false;
#endif
}

/* --- Thumbnails -------------------------------------------------------- */

size_t companion_core_thumbnail_dir(companion_core_t *core,
      const char *db_name, const char *subdir, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!core || !db_name || !subdir)
      return 0;

   fill_pathname_join_special(s,
         settings->paths.directory_thumbnails, db_name, len);
   return fill_pathname_join_special(s, s, subdir, len);
}

size_t companion_core_thumbnail_path(companion_core_t *core,
      const char *db_name, const char *subdir, const char *label,
      const char *content_path, char *s, size_t len)
{
   /* Extensions probed, in order; the first is also the default. */
   static const char *exts[] = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
   char name[PATH_MAX_LENGTH];
   size_t i, _len, name_len;

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!core || !label)
      return 0;

   /* Image content is its own thumbnail. */
   if (     !string_is_empty(content_path)
         && image_texture_get_type(content_path) != IMAGE_TYPE_NONE)
      return strlcpy(s, content_path, len);

   /* Characters the thumbnail repository replaces with '_':
    * & * / : ` < > ? \ | */
   name_len = strlcpy(name, label, sizeof(name));
   for (i = 0; i < name_len; i++)
   {
      switch (name[i])
      {
         case '&': case '*': case '/': case ':': case '`':
         case '<': case '>': case '?': case '\\': case '|':
            name[i] = '_';
            break;
         default:
            break;
      }
   }

   companion_core_thumbnail_dir(core, db_name, subdir, s, len);
   _len = fill_pathname_join_special(s, s, name, len);

   for (i = 0; i < sizeof(exts) / sizeof(exts[0]); i++)
   {
      strlcpy(s + _len, exts[i], len - _len);
      if (path_is_valid(s))
         return strlen(s);
   }
   strlcpy(s + _len, exts[0], len - _len);
   return strlen(s);
}

/* --- Running core ------------------------------------------------------ */

const char *companion_core_current_core_name(companion_core_t *core)
{
   const char *s = core ? runloop_state_get_ptr()->system.info.library_name : NULL;
   return s ? s : "";
}

const char *companion_core_current_core_version(companion_core_t *core)
{
   const char *s = core ? runloop_state_get_ptr()->system.info.library_version : NULL;
   return s ? s : "";
}

bool companion_core_current_core_supports_no_content(companion_core_t *core)
{
   return core && runloop_state_get_ptr()->system.load_no_content;
}

/* --- "Launch with" candidates ------------------------------------------ */

static bool companion_core_launch_option_present(
      const companion_launch_option_t *opts, size_t n,
      const char *path, const char *name, const char *display_name,
      const char *file_id)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      if (!string_is_empty(path) && string_is_equal(opts[i].path, path))
         return true;
      if (!string_is_empty(name) && string_is_equal(opts[i].name, name))
         return true;
      if (!string_is_empty(display_name)
            && string_is_equal(opts[i].name, display_name))
         return true;
      /* Same core file under another directory / suffix. */
      if (!string_is_empty(file_id)
            && string_starts_with(path_basename(opts[i].path), file_id))
         return true;
   }
   return false;
}

static void companion_core_launch_option_set(companion_launch_option_t *o,
      const char *name, const char *path,
      enum companion_launch_selection sel)
{
   strlcpy(o->name, name ? name : "", sizeof(o->name));
   strlcpy(o->path, path ? path : "", sizeof(o->path));
   o->selection = sel;
}

size_t companion_core_launch_options(companion_core_t *core,
      const char *entry_core_path, const char *entry_core_name,
      const char *playlist_name, bool suggest_loaded_first,
      companion_launch_option_t *out, size_t max)
{
   size_t n = 0;
   char default_core[PATH_MAX_LENGTH];

   if (!core || !out || !max)
      return 0;

   /* The running core first, when asked for and one is loaded. */
   if (suggest_loaded_first)
   {
      const char *cur = companion_core_current_core_name(core);
      if (!string_is_empty(cur) && n < max)
         companion_core_launch_option_set(&out[n++], cur,
               path_get(RARCH_PATH_CORE), COMPANION_LAUNCH_CURRENT);
   }

   /* The entry's own core. */
   if (     !string_is_empty(entry_core_name)
         && !string_is_equal(entry_core_name, "DETECT")
         && n < max
         && !companion_core_launch_option_present(out, n,
               entry_core_path, entry_core_name, NULL, NULL))
      companion_core_launch_option_set(&out[n++], entry_core_name,
            entry_core_path, COMPANION_LAUNCH_PLAYLIST_SAVED);

   /* The playlist's default core. */
   if (     !string_is_empty(playlist_name)
         && companion_core_playlist_default_core(core, playlist_name,
               default_core, sizeof(default_core)))
   {
      core_info_t *info = NULL;
      if (     core_info_find(default_core, &info) && info
            && n < max
            && !companion_core_launch_option_present(out, n,
                  info->path, info->core_name, info->display_name,
                  info->core_file_id.str))
         companion_core_launch_option_set(&out[n++], info->core_name,
               info->path, COMPANION_LAUNCH_PLAYLIST_DEFAULT);
   }

   return n;
}

/* --- Installed cores --------------------------------------------------- */

static const core_info_t *companion_core_installed_core(size_t i)
{
   core_info_list_t *list = NULL;
   if (!core_info_get_list(&list) || !list || i >= list->count)
      return NULL;
   return &list->list[i];
}

size_t companion_core_installed_core_count(companion_core_t *core)
{
   core_info_list_t *list = NULL;
   if (!core || !core_info_get_list(&list) || !list)
      return 0;
   return list->count;
}

const char *companion_core_installed_core_path(companion_core_t *core,
      size_t i)
{
   const core_info_t *info = core ? companion_core_installed_core(i) : NULL;
   return info ? info->path : NULL;
}

const char *companion_core_installed_core_name(companion_core_t *core,
      size_t i)
{
   const core_info_t *info = core ? companion_core_installed_core(i) : NULL;
   if (!info)
      return NULL;
   return !string_is_empty(info->display_name)
      ? info->display_name : info->core_name;
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

playlist_t *companion_core_playlist_open_private(companion_core_t *core,
      const char *path)
{
   playlist_config_t cfg;
   if (!core || string_is_empty(path))
      return NULL;
   companion_core_playlist_config_init(&cfg, path);
   return playlist_init(&cfg);
}

size_t companion_core_resolve_content_path(companion_core_t *core,
      const char *path, char *s, size_t len)
{
   size_t _len;

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!core || string_is_empty(path))
      return 0;

   _len = strlcpy(s, path, len);

   if (path_is_compressed_file(path))
   {
      struct string_list *list = file_archive_get_file_list(path, NULL);
      if (list)
      {
         if (list->size == 1 && _len + 1 < len)
         {
            s[_len++] = '#';
            _len     += strlcpy(s + _len, list->elems[0].data, len - _len);
         }
         string_list_free(list);
      }
   }
   return _len;
}

bool companion_core_playlist_push(companion_core_t *core,
      playlist_t *playlist, const char *content_path, const char *label,
      const char *core_path, const char *core_name, const char *db_name)
{
   struct playlist_entry entry;

   if (!core || !playlist || string_is_empty(content_path))
      return false;

   memset(&entry, 0, sizeof(entry));
   /* playlist_push() reads the entry as const; the casts are safe. */
   entry.path      = (char*)content_path;
   entry.label     = (char*)label;
   entry.core_path = (char*)(string_is_empty(core_path) ? "DETECT" : core_path);
   entry.core_name = (char*)(string_is_empty(core_name) ? "DETECT" : core_name);
   entry.crc32     = (char*)"00000000|crc";
   entry.db_name   = (char*)db_name;

   return playlist_push(playlist, &entry);
}
