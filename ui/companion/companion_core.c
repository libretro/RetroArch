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
#include <streams/file_stream.h>
#include <string/stdstring.h>
#ifdef HAVE_NETWORKING
#include <net/net_http.h>
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../content.h"
#include "../../core_info.h"
#include "../../gfx/video_driver.h"
#include "../../input/input_driver.h"
#include "../../msg_hash.h"
#include "../../retroarch_types.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../runloop.h"
#include "../../verbosity.h"
#include "../../version.h"
#include "../../tasks/task_content.h"
#include "../../tasks/tasks_internal.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "companion_core.h"

#define COMPANION_NO_SELECTION ((size_t)-1)

#ifdef HAVE_NETWORKING
static void companion_core_download_orphan(companion_core_t *core);
#endif

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

   /* File-system browser listing. */
   struct string_list *browse;
   char browse_dir[PATH_MAX_LENGTH];

   /* Budget for the parse step currently running. */
   retro_time_t budget_end;

#ifdef HAVE_NETWORKING
   /* The one download in flight (see companion_download_t). */
   struct companion_download *download;
#endif
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

/* One-time migration of the Qt companion's former private settings
 * file. Older builds kept the desktop companion's presentation settings
 * in retroarch_qt.cfg (QSettings INI: "[General]" then key=value lines)
 * beside retroarch.cfg. Those now live in retroarch.cfg. On the first
 * companion start that still finds the old file, the values that have
 * a home are imported into settings_t and the file is deleted, so it
 * does not linger as dead configuration. Qt-only blobs (geometry,
 * dock_positions, table headers, options-dialog geometry) have no home
 * and are dropped with it. Runs once per process at most. */
static void companion_core_migrate_qt_cfg(void)
{
   static bool done = false;
   char path[PATH_MAX_LENGTH];
   const char *cfg      = path_get(RARCH_PATH_CONFIG);
   settings_t *settings = config_get_ptr();
   int64_t len          = 0;
   char *buf            = NULL;
   char *line, *next;

   if (done)
      return;
   done = true;

   if (string_is_empty(cfg))
      return;
   fill_pathname_basedir(path, cfg, sizeof(path));
   fill_pathname_join_special(path, path, "retroarch_qt.cfg", sizeof(path));
   if (!path_is_valid(path))
      return;
   if (!filestream_read_file(path, (void**)&buf, &len) || !buf)
      return;

   RARCH_LOG("[Companion] Importing %s into retroarch.cfg and removing it.\n",
         path);

   for (line = buf; line && *line; line = next)
   {
      char *eq, *end;
      next = strchr(line, '\n');
      if (next)
         *next++ = '\0';
      while (*line == ' ' || *line == '\t' || *line == '\r')
         line++;
      end = line + strlen(line);
      while (end > line && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
         *--end = '\0';
      if (*line == '\0' || *line == '[' || *line == ';' || *line == '#')
         continue;
      if (!(eq = strchr(line, '=')))
         continue;
      *eq = '\0';
      end = eq;
      while (end > line && (end[-1] == ' ' || end[-1] == '\t'))
         *--end = '\0';
      eq++;
      while (*eq == ' ' || *eq == '\t')
         eq++;

#define QT_BOOL(v) (string_is_equal((v), "true") || string_is_equal((v), "1"))
      if (string_is_equal(line, "view_type"))
         settings->uints.desktop_menu_view_type = string_is_equal(eq, "icons") ? 1 : 0;
      else if (string_is_equal(line, "icon_view_thumbnail_type"))
         settings->uints.desktop_menu_thumbnail_type =
              string_is_equal(eq, "screenshot") ? 1
            : string_is_equal(eq, "title")      ? 2
            : string_is_equal(eq, "logo")       ? 3 : 0;
      else if (string_is_equal(line, "initial_playlist"))
         strlcpy(settings->paths.desktop_menu_initial_playlist, eq,
               sizeof(settings->paths.desktop_menu_initial_playlist));
      else if (string_is_equal(line, "suggest_loaded_core_first"))
         settings->bools.desktop_menu_suggest_loaded_core_first = QT_BOOL(eq);
      else if (string_is_equal(line, "show_hidden_files"))
         settings->bools.show_hidden_files = QT_BOOL(eq);
      else if (string_is_equal(line, "save_last_tab"))
         settings->bools.desktop_menu_save_last_tab = QT_BOOL(eq);
      else if (string_is_equal(line, "last_tab"))
         settings->uints.desktop_menu_last_tab = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "save_geometry"))
         settings->bools.desktop_menu_save_geometry = QT_BOOL(eq);
      else if (string_is_equal(line, "show_welcome_screen"))
         settings->bools.desktop_menu_show_welcome_screen = QT_BOOL(eq);
      else if (string_is_equal(line, "scan_finish_confirm"))
         settings->bools.desktop_menu_scan_finish_confirm = QT_BOOL(eq);
      else if (string_is_equal(line, "thumbnail_cache_limit"))
         settings->uints.desktop_menu_thumbnail_cache_limit = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "thumbnail_max_size"))
         settings->uints.desktop_menu_thumbnail_max_size = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "thumbnail_quality"))
         settings->uints.desktop_menu_thumbnail_quality = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "icon_view_zoom"))
         settings->uints.desktop_menu_icon_view_zoom = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "all_playlists_list_max_count"))
         settings->uints.desktop_menu_all_playlists_list_max_count = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "all_playlists_grid_max_count"))
         settings->uints.desktop_menu_all_playlists_grid_max_count = (unsigned)strtoul(eq, NULL, 10);
      else if (string_is_equal(line, "theme"))
         settings->uints.desktop_menu_theme =
              string_is_equal(eq, "dark")   ? 1
            : string_is_equal(eq, "custom") ? 2 : 0;
      else if (string_is_equal(line, "custom_theme"))
         strlcpy(settings->paths.desktop_menu_custom_theme, eq,
               sizeof(settings->paths.desktop_menu_custom_theme));
      else if (string_is_equal(line, "highlight_color"))
         strlcpy(settings->arrays.desktop_menu_highlight_color, eq,
               sizeof(settings->arrays.desktop_menu_highlight_color));
      else if (string_is_equal(line, "hidden_playlists"))
      {
         /* QSettings writes a QStringList as "a, b, c". */
         char *p;
         strlcpy(settings->arrays.desktop_menu_hidden_playlists, eq,
               sizeof(settings->arrays.desktop_menu_hidden_playlists));
         for (p = settings->arrays.desktop_menu_hidden_playlists; *p; )
         {
            if (*p == ' ' && p > settings->arrays.desktop_menu_hidden_playlists
                  && p[-1] == ',')
               memmove(p, p + 1, strlen(p + 1) + 1); /* drop the space */
            else
               p++;
         }
      }
      /* geometry, dock_positions, options_dialog_geometry,
       * file_browser_table_headers: Qt blobs, dropped. */
#undef QT_BOOL
   }
   free(buf);

   filestream_delete(path);
}

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

   companion_core_migrate_qt_cfg();
   return core;
}

void companion_core_free(companion_core_t *core)
{
   if (!core)
      return;
#ifdef HAVE_NETWORKING
   companion_core_download_orphan(core);
#endif
#ifdef HAVE_LIBRETRODB
   if (companion_core_scan_owner == core)
      companion_core_scan_owner = NULL;
#endif
   companion_core_clear_playlist(core);
   companion_core_free_playlist_names(core);
   if (core->playlist_files)
      string_list_free(core->playlist_files);
   if (core->browse)
      string_list_free(core->browse);
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

/* The special playlists that lead the list, in the Qt companion's order:
 * Favorites, History, Images, Music, Videos. Each is a real .lpl at a
 * configured path (unlike Qt's synthetic "All Playlists", which is not
 * a file and is left to the presentation). A path that is empty in the
 * config is skipped, so the count is not always five. */
static const char *companion_core_special_path(companion_core_t *core,
      size_t i)
{
   settings_t *settings = config_get_ptr();
   const char *paths[5];
   paths[0] = settings->paths.path_content_favorites;
   paths[1] = settings->paths.path_content_history;
   paths[2] = settings->paths.path_content_image_history;
   paths[3] = settings->paths.path_content_music_history;
   paths[4] = settings->paths.path_content_video_history;
   (void)core;
   if (i >= 5)
      return NULL;
   return string_is_empty(paths[i]) ? NULL : paths[i];
}

static enum msg_hash_enums companion_core_special_label(size_t i)
{
   switch (i)
   {
      case 0: return MENU_ENUM_LABEL_VALUE_FAVORITES_TAB;
      case 1: return MENU_ENUM_LABEL_VALUE_HISTORY_TAB;
      case 2: return MENU_ENUM_LABEL_VALUE_IMAGES_TAB;
      case 3: return MENU_ENUM_LABEL_VALUE_MUSIC_TAB;
      case 4: return MENU_ENUM_LABEL_VALUE_VIDEO_TAB;
      default: break;
   }
   return MSG_UNKNOWN;
}

/* How many of the five special playlists are configured (lead the list). */
static size_t companion_core_special_count(companion_core_t *core)
{
   size_t i, n = 0;
   for (i = 0; i < 5; i++)
      if (companion_core_special_path(core, i))
         n++;
   return n;
}

/* Map a public playlist index to (special i) or (file index), returning
 * whether it is special via *is_special. */
static bool companion_core_playlist_map(companion_core_t *core, size_t idx,
      size_t *out, bool *is_special)
{
   size_t i, seen = 0;
   for (i = 0; i < 5; i++)
   {
      if (!companion_core_special_path(core, i))
         continue;
      if (seen == idx)
      {
         *is_special = true;
         *out        = i;
         return true;
      }
      seen++;
   }
   idx -= seen;
   if (core->playlist_files && idx < core->playlist_files->size)
   {
      *is_special = false;
      *out        = idx;
      return true;
   }
   return false;
}

size_t companion_core_playlist_count(companion_core_t *core)
{
   size_t n;
   if (!core)
      return 0;
   n = companion_core_special_count(core);
   if (core->playlist_files)
      n += core->playlist_files->size;
   return n;
}

const char *companion_core_playlist_name(companion_core_t *core, size_t i)
{
   size_t r;
   bool special;
   if (!core || !companion_core_playlist_map(core, i, &r, &special))
      return NULL;
   if (special)
      return msg_hash_to_str(companion_core_special_label(r));
   return core->playlist_names ? core->playlist_names[r] : NULL;
}

const char *companion_core_playlist_path(companion_core_t *core, size_t i)
{
   size_t r;
   bool special;
   if (!core || !companion_core_playlist_map(core, i, &r, &special))
      return NULL;
   if (special)
      return companion_core_special_path(core, r);
   return core->playlist_files->elems[r].data;
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
      const char *p_i = companion_core_playlist_path(core, i);
      if (p_i && string_is_equal(path, p_i))
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

bool companion_core_entry_needs_core(companion_core_t *core, size_t i,
      char *s, size_t len)
{
   const char *core_path;
   const struct playlist_entry *entry = companion_core_entry(core, i);

   if (s && len)
      s[0] = '\0';
   if (!entry || string_is_empty(entry->path))
      return false;
   if (s && len)
      strlcpy(s, entry->path, len);

   core_path = entry->core_path;
   if (string_is_empty(core_path) || string_is_equal(core_path, "DETECT"))
      core_path = path_get(RARCH_PATH_CORE);
   return string_is_empty(core_path);
}

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

bool companion_core_start_core(companion_core_t *core)
{
   content_ctx_info_t content_info;
   if (!core)
      return false;
   memset(&content_info, 0, sizeof(content_info));
   path_clear(RARCH_PATH_BASENAME);
   return task_push_start_current_core(&content_info);
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

/* --- Companion settings (retroarch.cfg) -------------------------------- */

bool companion_core_pref_icon_view(companion_core_t *core)
{
   return core && config_get_ptr()->uints.desktop_menu_view_type == 1;
}

void companion_core_pref_set_icon_view(companion_core_t *core, bool icons)
{
   if (core)
      config_get_ptr()->uints.desktop_menu_view_type = icons ? 1 : 0;
}

const char *companion_core_pref_thumbnail_subdir(companion_core_t *core)
{
   if (!core)
      return COMPANION_THUMB_BOXART;
   switch (config_get_ptr()->uints.desktop_menu_thumbnail_type)
   {
      case 1:  return COMPANION_THUMB_SCREENSHOT;
      case 2:  return COMPANION_THUMB_TITLE;
      case 3:  return COMPANION_THUMB_LOGO;
      default: break;
   }
   return COMPANION_THUMB_BOXART;
}

const char *companion_core_pref_initial_playlist(companion_core_t *core)
{
   const char *p = core ? config_get_ptr()->paths.desktop_menu_initial_playlist : NULL;
   return p ? p : "";
}

bool companion_core_pref_suggest_loaded_core_first(companion_core_t *core)
{
   return core && config_get_ptr()->bools.desktop_menu_suggest_loaded_core_first;
}

bool companion_core_pref_show_hidden_files(companion_core_t *core)
{
   return core && config_get_ptr()->bools.show_hidden_files;
}

int companion_core_pref_last_tab(companion_core_t *core)
{
   settings_t *settings = config_get_ptr();
   if (!core || !settings->bools.desktop_menu_save_last_tab)
      return -1;
   return settings->uints.desktop_menu_last_tab == 1 ? 1 : 0;
}

void companion_core_pref_set_last_tab(companion_core_t *core, int tab)
{
   settings_t *settings = config_get_ptr();
   if (core && settings->bools.desktop_menu_save_last_tab)
      settings->uints.desktop_menu_last_tab = (tab == 1) ? 1 : 0;
}

unsigned companion_core_pref_icon_view_zoom(companion_core_t *core)
{
   unsigned z = core ? config_get_ptr()->uints.desktop_menu_icon_view_zoom : 50;
   return z > 100 ? 100 : z;
}

void companion_core_pref_set_icon_view_zoom(companion_core_t *core, unsigned z)
{
   if (core)
      config_get_ptr()->uints.desktop_menu_icon_view_zoom = z > 100 ? 100 : z;
}

unsigned companion_core_pref_thumbnail_type(companion_core_t *core)
{
   unsigned t = core ? config_get_ptr()->uints.desktop_menu_thumbnail_type : 0;
   return t > 3 ? 0 : t;
}

void companion_core_pref_set_thumbnail_type(companion_core_t *core, unsigned t)
{
   if (core)
      config_get_ptr()->uints.desktop_menu_thumbnail_type = t > 3 ? 0 : t;
}

/* --- Playlist icons ---------------------------------------------------- */

#define COMPANION_ICON_DIR "xmb/dot-art/png"

size_t companion_core_playlist_icon_path(companion_core_t *core, size_t i,
      char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   const char *name;
   size_t _len;

   if (!s || !len)
      return 0;
   s[0] = '\0';
   if (!core || string_is_empty(settings->paths.directory_assets))
      return 0;

   fill_pathname_join_special(s, settings->paths.directory_assets,
         COMPANION_ICON_DIR, len);

   /* <name>.png for a system playlist; the specials have no asset of
    * their own and take the folder. */
   name = companion_core_playlist_name(core, i);
   if (name && i >= companion_core_special_count(core))
   {
      char tmp[PATH_MAX_LENGTH];
      _len = fill_pathname_join_special(tmp, s, name, sizeof(tmp));
      strlcpy(tmp + _len, ".png", sizeof(tmp) - _len);
      if (path_is_valid(tmp))
         return strlcpy(s, tmp, len);
   }

   _len = fill_pathname_join_special(s, s, "folder.png", len);
   return path_is_valid(s) ? _len : 0;
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

/* --- Core information panel -------------------------------------------- */

/* Append a single (key, value) row.  status applies to value->attr.i. */
static void cc_info_append_row(
      struct string_list *keys,
      struct string_list *values,
      const char *key, const char *value,
      enum companion_core_info_row_status status)
{
   union string_list_elem_attr attr;
   attr.i = 0;
   string_list_append(keys, key ? key : "", attr);
   attr.i = (int)status;
   string_list_append(values, value ? value : "", attr);
}

/* Append a row whose key is "<msg_hash>:" and whose value is plain. */
static void cc_info_append_kv(
      struct string_list *keys,
      struct string_list *values,
      enum msg_hash_enums label_enum, const char *value)
{
   char key[256];
   size_t _len = strlcpy(key, msg_hash_to_str(label_enum), sizeof(key));
   strlcpy_lit(key + _len, ":", sizeof(key) - _len);
   cc_info_append_row(keys, values, key, value,
         COMPANION_CORE_INFO_ROW_NORMAL);
}

/* Append a row built from a list of strings joined by ", ". */
static void cc_info_append_joined(
      struct string_list *keys,
      struct string_list *values,
      enum msg_hash_enums label_enum,
      const struct string_list *src)
{
   char buf[NAME_MAX_LENGTH * 4];
   size_t i, _len = 0;
   buf[0] = '\0';
   for (i = 0; i < src->size; i++)
   {
      _len += strlcpy(buf + _len, src->elems[i].data, sizeof(buf) - _len);
      if (i < src->size - 1)
         _len += strlcpy_lit(buf + _len, ", ", sizeof(buf) - _len);
   }
   cc_info_append_kv(keys, values, label_enum, buf);
}

/* Collect a list of human-readable rows describing the currently-selected
 * core. Output: two parallel string_lists. values->elems[i].attr.i holds
 * a qt_core_info_row_status for that row.
 *
 * Returns false if no core is selected or its info isn't loaded; in that
 * case a single row with the "no info available" message is appended. */
bool companion_core_core_info_rows(
      const char *current_core_path,
      struct string_list *keys,
      struct string_list *values)
{
   size_t i;
   core_info_t *core_info = NULL;

   core_info_find(current_core_path, &core_info);

   if (    !current_core_path
        || !*current_core_path
        || !core_info
        || !(core_info->flags & CORE_INFO_FLAG_HAS_INFO))
   {
      cc_info_append_row(keys, values,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE),
            "", COMPANION_CORE_INFO_ROW_NORMAL);
      return false;
   }

   if (core_info->core_name)
      cc_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME, core_info->core_name);
   if (core_info->display_name)
      cc_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL, core_info->display_name);
   if (core_info->systemname)
      cc_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME, core_info->systemname);
   if (core_info->system_manufacturer)
      cc_info_append_kv(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
            core_info->system_manufacturer);

   if (core_info->categories_list)
      cc_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
            core_info->categories_list);
   if (core_info->authors_list)
      cc_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
            core_info->authors_list);
   if (core_info->permissions_list)
      cc_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
            core_info->permissions_list);
   if (core_info->licenses_list)
      cc_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
            core_info->licenses_list);
   if (core_info->supported_extensions_list)
      cc_info_append_joined(keys, values,
            MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
            core_info->supported_extensions_list);

   if (core_info->firmware_count > 0)
   {
      char tmp_path[PATH_MAX_LENGTH];
      core_info_ctx_firmware_t firmware_info;
      bool update_missing_firmware    = false;
      settings_t *settings            = config_get_ptr();
      uint8_t flags                   = content_get_flags();
      bool systemfiles_in_content_dir = settings->bools.systemfiles_in_content_dir;
      bool content_is_inited          = flags & CONTENT_ST_FLAG_IS_INITED;

      firmware_info.path              = core_info->path;

      if (systemfiles_in_content_dir && content_is_inited)
      {
         fill_pathname_basedir(tmp_path,
               path_get(RARCH_PATH_CONTENT),
               sizeof(tmp_path));
         if (!*tmp_path)
            firmware_info.directory.system = settings->paths.directory_system;
         else
         {
            size_t _len = strlen(tmp_path);
            if (     string_count_occurrences_single_character(tmp_path, PATH_DEFAULT_SLASH_C()) > 1
                  && tmp_path[_len - 1] == PATH_DEFAULT_SLASH_C())
                     tmp_path[_len - 1] = '\0';
            firmware_info.directory.system = tmp_path;
         }
      }
      else
         firmware_info.directory.system = settings->paths.directory_system;

      update_missing_firmware = core_info_list_update_missing_firmware(&firmware_info);

      if (update_missing_firmware)
      {
         char firmware_label[256];
         char tmp[PATH_MAX_LENGTH];
         size_t _len = strlcpy(firmware_label,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE),
               sizeof(firmware_label));
         strlcpy_lit(firmware_label + _len, ":",
               sizeof(firmware_label) - _len);

         cc_info_append_row(keys, values,
               firmware_label, "", COMPANION_CORE_INFO_ROW_FIRMWARE_NOTE);

         if (systemfiles_in_content_dir)
            cc_info_append_row(keys, values,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY),
                  "", COMPANION_CORE_INFO_ROW_FIRMWARE_NOTE);

         snprintf(tmp, sizeof(tmp),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH),
               firmware_info.directory.system);
         cc_info_append_row(keys, values,
               tmp, "", COMPANION_CORE_INFO_ROW_FIRMWARE_NOTE);

         for (i = 0; i < core_info->firmware_count; i++)
         {
            char lbl_txt[256];
            const char *val_txt          = NULL;
            bool missing                 = false;
            enum companion_core_info_row_status status;
            size_t lbl_len               = 0;

            if (!core_info->firmware[i].desc)
               continue;

            lbl_len = strlcpy_lit(lbl_txt, "(!) ", sizeof(lbl_txt));

            if (core_info->firmware[i].missing)
            {
               missing = true;
               if (core_info->firmware[i].optional)
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL),
                        sizeof(lbl_txt) - lbl_len);
               else
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED),
                        sizeof(lbl_txt) - lbl_len);
            }
            else
            {
               if (core_info->firmware[i].optional)
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL),
                        sizeof(lbl_txt) - lbl_len);
               else
                  strlcpy(lbl_txt + lbl_len,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED),
                        sizeof(lbl_txt) - lbl_len);
            }

            val_txt = core_info->firmware[i].desc
                  ? core_info->firmware[i].desc
                  : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME);
            status  = missing
                  ? COMPANION_CORE_INFO_ROW_FIRMWARE_MISSING
                  : COMPANION_CORE_INFO_ROW_FIRMWARE_PRESENT;
            cc_info_append_row(keys, values,
                  lbl_txt, val_txt, status);
         }
      }
   }

   if (core_info->notes && core_info->note_list)
   {
      for (i = 0; i < core_info->note_list->size; i++)
         cc_info_append_row(keys, values,
               "", core_info->note_list->elems[i].data,
               COMPANION_CORE_INFO_ROW_NOTE_NO_KEY);
   }

   return true;
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
   if (!string_is_empty(info->display_name))
      return info->display_name;
   if (!string_is_empty(info->core_name))
      return info->core_name;
   return path_basename(info->path);
}

const char *companion_core_installed_core_version(companion_core_t *core,
      size_t i)
{
   const core_info_t *info = core ? companion_core_installed_core(i) : NULL;
   return (info && info->display_version) ? info->display_version : "";
}

size_t companion_core_installed_cores_supporting(companion_core_t *core,
      const char *content_path)
{
   core_info_list_t *list       = NULL;
   const core_info_t *supported = NULL;
   size_t n                     = 0;

   if (!core || !core_info_get_list(&list) || !list)
      return 0;
   if (string_is_empty(content_path))
      return list->count;

   core_info_list_get_supported_cores(list, content_path, &supported, &n);
   return n;
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

/* --- File-system browser ----------------------------------------------- */

bool companion_core_browse_open(companion_core_t *core, const char *path)
{
   settings_t *settings     = config_get_ptr();
   struct string_list *list;
   const char *dir          = path;

   if (!core)
      return false;

   if (string_is_empty(dir))
      dir = !string_is_empty(settings->paths.directory_menu_content)
         ? settings->paths.directory_menu_content : NULL;

#ifdef _WIN32
   /* NULL dir with no default: list nothing rather than a bogus root;
    * a drive picker is a platform concern for the backend. */
   if (!dir)
      return false;
#else
   if (!dir)
      dir = "/";
#endif

   /* Directories and files, no hidden, archives shown as files. */
   list = dir_list_new(dir, NULL, true, false, true, false);
   if (!list)
      return false;
   dir_list_sort(list, true); /* directories first */

   if (core->browse)
      string_list_free(core->browse);
   core->browse = list;
   strlcpy(core->browse_dir, dir, sizeof(core->browse_dir));
   return true;
}

const char *companion_core_browse_dir(companion_core_t *core)
{
   return (core && core->browse) ? core->browse_dir : "";
}

/* Index 0 is the ".." parent link (unless already at a filesystem
 * root); real entries follow, so the accessors offset by it. */
static bool companion_core_browse_has_parent(companion_core_t *core)
{
   const char *d = core->browse_dir;
   size_t len    = strlen(d);
   if (len == 0)
      return false;
#ifdef _WIN32
   /* "C:\" is a root. */
   if (len <= 3 && d[1] == ':')
      return false;
#else
   if (len == 1 && d[0] == '/')
      return false;
#endif
   return true;
}

size_t companion_core_browse_count(companion_core_t *core)
{
   size_t n;
   if (!core || !core->browse)
      return 0;
   n = core->browse->size;
   if (companion_core_browse_has_parent(core))
      n++;
   return n;
}

/* Maps a public index to a browse->elems index, or -1 for the parent. */
static long companion_core_browse_real(companion_core_t *core, size_t i)
{
   if (companion_core_browse_has_parent(core))
   {
      if (i == 0)
         return -1;
      return (long)(i - 1);
   }
   return (long)i;
}

const char *companion_core_browse_name(companion_core_t *core, size_t i)
{
   long r;
   if (!core || !core->browse)
      return NULL;
   r = companion_core_browse_real(core, i);
   if (r < 0)
      return "..";
   if ((size_t)r >= core->browse->size)
      return NULL;
   return path_basename(core->browse->elems[r].data);
}

const char *companion_core_browse_path(companion_core_t *core, size_t i)
{
   long r;
   if (!core || !core->browse)
      return NULL;
   r = companion_core_browse_real(core, i);
   if (r < 0)
      return core->browse_dir; /* parent handled in _activate */
   if ((size_t)r >= core->browse->size)
      return NULL;
   return core->browse->elems[r].data;
}

bool companion_core_browse_is_dir(companion_core_t *core, size_t i)
{
   long r;
   if (!core || !core->browse)
      return false;
   r = companion_core_browse_real(core, i);
   if (r < 0)
      return true; /* parent */
   if ((size_t)r >= core->browse->size)
      return false;
   return core->browse->elems[r].attr.i == RARCH_DIRECTORY;
}

int companion_core_browse_activate(companion_core_t *core, size_t i,
      const char *pick_core_path, bool *needs_core,
      char *content, size_t content_len)
{
   const char *core_path;
   long r;

   if (needs_core)
      *needs_core = false;
   if (content && content_len)
      content[0] = '\0';
   if (!core || !core->browse)
      return -1;

   r = companion_core_browse_real(core, i);

   /* Parent link. */
   if (r < 0)
   {
      char parent[PATH_MAX_LENGTH];
      strlcpy(parent, core->browse_dir, sizeof(parent));
      path_parent_dir(parent, strlen(parent));
      return companion_core_browse_open(core, parent) ? 0 : -1;
   }

   if ((size_t)r >= core->browse->size)
      return -1;

   if (core->browse->elems[r].attr.i == RARCH_DIRECTORY)
      return companion_core_browse_open(core,
            core->browse->elems[r].data) ? 0 : -1;

   /* A file: load it. */
   {
      const char *file = core->browse->elems[r].data;
      if (content && content_len)
         strlcpy(content, file, content_len);

      core_path = pick_core_path;
      if (string_is_empty(core_path))
         core_path = path_get(RARCH_PATH_CORE);
      if (string_is_empty(core_path))
      {
         if (needs_core)
            *needs_core = true;
         return -1;
      }
      return companion_core_request_load_content(core, core_path,
            file, NULL, NULL, NULL) ? 1 : -1;
   }
}

/* --- Window hand-off ---------------------------------------------------- */

void companion_core_prepare_show_window(companion_core_t *core)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!core)
      return;

   if (input_state_get_ptr()->flags & INP_FLAG_GRAB_MOUSE_STATE)
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   if (video_st && video_st->poke && video_st->poke->show_mouse)
      video_st->poke->show_mouse(video_st->data, true);
   if (settings->bools.video_fullscreen)
      command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);
}

bool companion_core_video_started_fullscreen(companion_core_t *core)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   return core && video_st && (video_st->flags & VIDEO_FLAG_STARTED_FULLSCREEN);
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

/* --- Thumbnail downloads ----------------------------------------------- */

#ifdef HAVE_NETWORKING

#define COMPANION_DL_USER_AGENT   "RetroArch-WIMP/" PACKAGE_VERSION
#define COMPANION_DL_PARTIAL_EXT  ".partial"
#define COMPANION_DL_TEMP_EXT     ".tmp"
#define COMPANION_DL_PACK_URL     "http://thumbnailpacks.libretro.com/"
#define COMPANION_DL_PACK_EXT     ".zip"
#define COMPANION_DL_THUMB_URL    "https://thumbnails.libretro.com/"
#define COMPANION_DL_THUMB_EXT    ".png"

/* Task user data. Owned by the task chain; the core only points at it
 * while the transfer is in flight, and clears d->core if it goes away
 * first (companion_core_download_orphan), so a late callback finds no
 * one to notify and just frees it. */
typedef struct companion_download
{
   companion_core_t *core;
   retro_task_t *task;
   char db_name[NAME_MAX_LENGTH];
   char label[NAME_MAX_LENGTH];
   char subdir[64];
   char output_path[PATH_MAX_LENGTH];   /* .partial file */
   bool is_pack;
} companion_download_t;

static void companion_core_download_orphan(companion_core_t *core)
{
   if (core->download)
   {
      core->download->core = NULL;
      core->download       = NULL;
   }
}

static companion_download_t *companion_core_download_new(
      companion_core_t *core, const char *db_name, const char *label,
      const char *subdir, bool is_pack)
{
   companion_download_t *d = (companion_download_t*)calloc(1, sizeof(*d));
   if (!d)
      return NULL;
   d->core    = core;
   d->is_pack = is_pack;
   strlcpy(d->db_name, db_name ? db_name : "", sizeof(d->db_name));
   strlcpy(d->label,   label   ? label   : "", sizeof(d->label));
   strlcpy(d->subdir,  subdir  ? subdir  : "", sizeof(d->subdir));
   return d;
}

/* Detach @d from its core (the transfer is over) and free it. */
static void companion_core_download_done(companion_download_t *d)
{
   if (d->core && d->core->download == d)
      d->core->download = NULL;
   free(d);
}

/* The label as the thumbnail server names files: the repository's
 * forbidden characters replaced by '_' (this set also covers the
 * double quote), then percent-encoded for the URL. */
static void companion_core_download_scrub(char *s)
{
   static const char chars[] = "&*/:`\"<>?\\|";
   size_t i;
   for (i = 0; i < sizeof(chars) - 1; i++)
      string_replace_all_chars(s, chars[i], '_');
}

/* Write the transfer to .partial, then move it over the final name.
 * Returns the result code; @final receives the final path. */
static enum companion_download_result companion_core_download_commit(
      const companion_download_t *d, const http_transfer_data_t *data,
      const char *err, char *final, size_t len)
{
   char *ext;
   char dir[PATH_MAX_LENGTH];

   if (!data || !data->data || data->status != 200 || err)
   {
      RARCH_ERR("[Companion] Download failed (HTTP %d).\n",
            data ? data->status : 0);
      return COMPANION_DL_ERR_NETWORK;
   }

   strlcpy(dir, d->output_path, sizeof(dir));
   path_basedir_wrapper(dir);
   path_mkdir(dir);

   if (!filestream_write_file(d->output_path, data->data, data->len))
   {
      RARCH_ERR("[Companion] Could not write \"%s\".\n", d->output_path);
      return COMPANION_DL_ERR_WRITE;
   }

   strlcpy(final, d->output_path, len);
   if ((ext = strstr(final, COMPANION_DL_PARTIAL_EXT)))
      *ext = '\0';

   if (path_is_valid(final))
      filestream_delete(final);

   if (filestream_rename(d->output_path, final) != 0)
   {
      RARCH_ERR("[Companion] Could not rename \"%s\".\n", d->output_path);
      return COMPANION_DL_ERR_RENAME;
   }
   return COMPANION_DL_OK;
}

static void companion_core_download_http_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   char final[PATH_MAX_LENGTH];
   companion_download_t *d   = (companion_download_t*)user_data;
   companion_core_t *core;
   enum companion_download_result r;

   (void)task;
   if (!d)
      return;

   final[0] = '\0';
   r        = companion_core_download_commit(d,
         (const http_transfer_data_t*)task_data, err, final, sizeof(final));
   core     = d->core;

   if (core && core->cb.on_thumbnail_downloaded)
      core->cb.on_thumbnail_downloaded(core->ud, d->db_name, d->label,
            d->subdir, r == COMPANION_DL_OK ? final : NULL,
            r == COMPANION_DL_OK);
   companion_core_download_done(d);
}

/* Existing files the pack will overwrite are deleted first, or renamed
 * aside with .tmp when they cannot be (extraction does not replace
 * in place). */
static enum companion_download_result companion_core_pack_prepare(
      const char *archive)
{
   size_t i;
   struct string_list *list = file_archive_get_file_list(archive, NULL);
   enum companion_download_result r = COMPANION_DL_OK;

   if (!list || list->size == 0)
   {
      if (list)
         string_list_free(list);
      RARCH_ERR("[Companion] Downloaded archive is empty.\n");
      return COMPANION_DL_ERR_ARCHIVE_EMPTY;
   }

   for (i = 0; i < list->size && r == COMPANION_DL_OK; i++)
   {
      char tmp[PATH_MAX_LENGTH];
      size_t _len;
      const char *target = list->elems[i].data;

      if (!filestream_exists(target))
         continue;
      if (filestream_delete(target) == 0)
         continue;

      _len = strlcpy(tmp, target, sizeof(tmp));
      strlcpy(tmp + _len, COMPANION_DL_TEMP_EXT, sizeof(tmp) - _len);

      if (filestream_exists(tmp) && filestream_delete(tmp) != 0)
      {
         RARCH_ERR("[Companion] Could not delete \"%s\".\n", target);
         r = COMPANION_DL_ERR_DELETE;
      }
      else if (filestream_rename(target, tmp) != 0)
      {
         RARCH_ERR("[Companion] Could not rename \"%s\".\n", target);
         r = COMPANION_DL_ERR_RENAME;
      }
   }

   string_list_free(list);
   return r;
}

static void companion_core_pack_extract_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   decompress_task_data_t *dec = (decompress_task_data_t*)task_data;
   companion_download_t *d     = (companion_download_t*)user_data;
   companion_core_t *core;

   (void)task;
   if (err)
      RARCH_ERR("[Companion] %s", err);

   if (dec)
   {
      if (filestream_exists(dec->source_file))
         filestream_delete(dec->source_file);
      free(dec->source_file);
      free(dec);
   }

   if (!d)
      return;
   core = d->core;
   if (core && core->cb.on_thumbnail_pack_finished)
      core->cb.on_thumbnail_pack_finished(core->ud,
            (!err || !*err) ? COMPANION_DL_OK : COMPANION_DL_ERR_EXTRACT);
   companion_core_download_done(d);
}

static void companion_core_pack_http_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   char final[PATH_MAX_LENGTH];
   companion_download_t *d = (companion_download_t*)user_data;
   companion_core_t *core;
   enum companion_download_result r;

   (void)task;
   if (!d)
      return;

   final[0] = '\0';
   r        = companion_core_download_commit(d,
         (const http_transfer_data_t*)task_data, err, final, sizeof(final));
   core     = d->core;

   if (r == COMPANION_DL_OK)
      r = companion_core_pack_prepare(final);

   if (r == COMPANION_DL_OK && core)
   {
      RARCH_LOG("[Companion] Thumbnail pack downloaded, extracting.\n");
      d->task = (retro_task_t*)task_push_decompress(final,
            config_get_ptr()->paths.directory_thumbnails,
            NULL, NULL, NULL, companion_core_pack_extract_cb, d, NULL,
            false);
      if (d->task)
         return; /* d lives on into the extract callback */
      r = COMPANION_DL_ERR_EXTRACT;
   }

   if (core && core->cb.on_thumbnail_pack_finished)
      core->cb.on_thumbnail_pack_finished(core->ud, r);
   companion_core_download_done(d);
}

bool companion_core_thumbnail_download(companion_core_t *core,
      const char *db_name, const char *label, const char *subdir)
{
   char url[PATH_MAX_LENGTH];
   char name[NAME_MAX_LENGTH];
   char *encoded = NULL;
   size_t _len;
   companion_download_t *d;
   settings_t *settings = config_get_ptr();

   if (!core || core->download || string_is_empty(db_name)
         || string_is_empty(label) || string_is_empty(subdir))
      return false;

   strlcpy(name, label, sizeof(name));
   companion_core_download_scrub(name);

   d = companion_core_download_new(core, db_name, name, subdir, false);
   if (!d)
      return false;

   /* <thumbnails>/<db>/<subdir>/<name>.png.partial */
   companion_core_thumbnail_dir(core, db_name, subdir,
         d->output_path, sizeof(d->output_path));
   path_mkdir(d->output_path);
   _len = fill_pathname_join_special(d->output_path, d->output_path, name,
         sizeof(d->output_path));
   strlcpy(d->output_path + _len, COMPANION_DL_THUMB_EXT COMPANION_DL_PARTIAL_EXT,
         sizeof(d->output_path) - _len);

   /* https://thumbnails.libretro.com/<db>/<subdir>/<name>.png, each path
    * component percent-encoded. */
   _len  = strlcpy(url, COMPANION_DL_THUMB_URL, sizeof(url));
   net_http_urlencode(&encoded, db_name);
   _len += strlcpy(url + _len, encoded ? encoded : db_name, sizeof(url) - _len);
   free(encoded); encoded = NULL;
   _len += strlcpy(url + _len, "/", sizeof(url) - _len);
   _len += strlcpy(url + _len, subdir, sizeof(url) - _len);
   _len += strlcpy(url + _len, "/", sizeof(url) - _len);
   net_http_urlencode(&encoded, name);
   _len += strlcpy(url + _len, encoded ? encoded : name, sizeof(url) - _len);
   free(encoded);
   strlcpy(url + _len, COMPANION_DL_THUMB_EXT, sizeof(url) - _len);

   RARCH_LOG("[Companion] Downloading \"%s\".\n", url);

   d->task = (retro_task_t*)task_push_http_transfer_with_user_agent(url, true,
         NULL, COMPANION_DL_USER_AGENT, companion_core_download_http_cb, d);
   if (!d->task)
   {
      RARCH_ERR("[Companion] Failed to start thumbnail transfer.\n");
      free(d);
      return false;
   }
   core->download = d;
   (void)settings;
   return true;
}

bool companion_core_thumbnail_pack_download(companion_core_t *core,
      const char *db_name)
{
   char url[PATH_MAX_LENGTH];
   char *encoded = NULL;
   size_t _len;
   companion_download_t *d;
   settings_t *settings = config_get_ptr();

   if (!core || core->download || string_is_empty(db_name))
      return false;

   d = companion_core_download_new(core, db_name, NULL, NULL, true);
   if (!d)
      return false;

   /* <thumbnails>/<db>.zip.partial */
   path_mkdir(settings->paths.directory_thumbnails);
   _len = fill_pathname_join_special(d->output_path,
         settings->paths.directory_thumbnails, db_name, sizeof(d->output_path));
   strlcpy(d->output_path + _len, COMPANION_DL_PACK_EXT COMPANION_DL_PARTIAL_EXT,
         sizeof(d->output_path) - _len);

   _len  = strlcpy(url, COMPANION_DL_PACK_URL, sizeof(url));
   net_http_urlencode(&encoded, db_name);
   _len += strlcpy(url + _len, encoded ? encoded : db_name, sizeof(url) - _len);
   free(encoded);
   strlcpy(url + _len, COMPANION_DL_PACK_EXT, sizeof(url) - _len);

   RARCH_LOG("[Companion] Downloading \"%s\".\n", url);

   d->task = (retro_task_t*)task_push_http_transfer_with_user_agent(url, true,
         NULL, COMPANION_DL_USER_AGENT, companion_core_pack_http_cb, d);
   if (!d->task)
   {
      RARCH_ERR("[Companion] Failed to start thumbnail pack transfer.\n");
      free(d);
      return false;
   }
   core->download = d;
   return true;
}

void companion_core_download_cancel(companion_core_t *core)
{
   if (!core || !core->download)
      return;
   if (core->download->task)
      task_set_flags(core->download->task, RETRO_TASK_FLG_CANCELLED, true);
   /* The task's callback still runs and frees the user data; it must
    * not report into a UI that considers the transfer gone. */
   companion_core_download_orphan(core);
}

bool companion_core_download_active(companion_core_t *core)
{
   return core && core->download;
}

#else /* !HAVE_NETWORKING */

bool companion_core_thumbnail_download(companion_core_t *core,
      const char *db_name, const char *label, const char *subdir)
{
   (void)core; (void)db_name; (void)label; (void)subdir;
   return false;
}

bool companion_core_thumbnail_pack_download(companion_core_t *core,
      const char *db_name)
{
   (void)core; (void)db_name;
   return false;
}

void companion_core_download_cancel(companion_core_t *core) { (void)core; }
bool companion_core_download_active(companion_core_t *core)
{
   (void)core;
   return false;
}

#endif
