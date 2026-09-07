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

/* Shared companion (desktop / WIMP) UI core.
 *
 * Toolkit-agnostic model and command layer shared by the Qt, Win32 and
 * Cocoa companion drivers. Contains no toolkit types and never blocks
 * the caller: anything that can take more than a couple of milliseconds
 * is either driven in budgeted steps from companion_core_iterate() or
 * posted to the RetroArch task queue, with completion reported through
 * the callback table supplied at creation time.
 *
 * Presentation backends own only windows, controls, layout and event
 * wiring. They read model state through the accessors below and issue
 * requests through the request functions; they never own the data. */

#ifndef __UI_COMPANION_CORE_H
#define __UI_COMPANION_CORE_H

#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "../../command.h"
#include "../../playlist.h"

RETRO_BEGIN_DECLS

typedef struct companion_core companion_core_t;

enum companion_download_result
{
   COMPANION_DL_OK = 0,
   COMPANION_DL_ERR_NETWORK,       /* HTTP failed / non-200 */
   COMPANION_DL_ERR_WRITE,         /* could not write the file */
   COMPANION_DL_ERR_RENAME,        /* could not rename .partial */
   COMPANION_DL_ERR_ARCHIVE_EMPTY, /* pack held no files */
   COMPANION_DL_ERR_DELETE,        /* could not replace an existing file */
   COMPANION_DL_ERR_EXTRACT        /* decompression failed */
};

/* All callbacks are invoked on the main (UI) thread, from within the
 * request function that caused them or from companion_core_iterate().
 * Every member may be NULL. */
typedef struct companion_callbacks
{
   /* The set of available playlist files changed
    * (companion_core_playlist_count / _name / _path). */
   void (*on_playlists_changed)(void *ud);
   /* The selected playlist finished (re)loading, or was cleared
    * (companion_core_entry_count / _entry). */
   void (*on_playlist_changed)(void *ud);
   /* A status-bar message from RetroArch's message queue. */
   void (*on_status_message)(void *ud, const char *msg,
         unsigned prio, unsigned duration, bool flush);
   /* A log line RetroArch wants shown in the companion's log view. */
   void (*on_log_message)(void *ud, const char *msg);
   /* RetroArch asked the companion to refresh whatever it shows. */
   void (*on_notify_refresh)(void *ud);
   /* A scan started with companion_core_request_scan() finished; the
    * playlist files may have changed. */
   void (*on_scan_finished)(void *ud);
   /* A single thumbnail download finished. @path is the file written
    * (NULL on failure). */
   void (*on_thumbnail_downloaded)(void *ud, const char *db_name,
         const char *label, const char *subdir, const char *path,
         bool success);
   /* A thumbnail pack download + extraction finished. */
   void (*on_thumbnail_pack_finished)(void *ud,
         enum companion_download_result result);
   /* A companion_core_browse_open() finished: the listing is in place.
    * Fired from companion_core_iterate() on the UI thread. (Last, so
    * existing positional tables stay valid; NULL if unused.) */
   void (*on_browse_changed)(void *ud);
} companion_callbacks_t;

/* Lifecycle */
companion_core_t *companion_core_new(const companion_callbacks_t *cb,
      void *ud);
/* Change the user pointer handed to the callbacks (a backend that
 * creates the core before the object its callbacks act on). */
void companion_core_set_ud(companion_core_t *core, void *ud);
void companion_core_free(companion_core_t *core);

/* Advance pending budgeted work (playlist parse, ...) for at most
 * @budget_us microseconds. Call from the backend's iterate / idle
 * path. Never blocks past the budget. */
void companion_core_iterate(companion_core_t *core, unsigned budget_us);

/* "All Playlists": the first playlist entry every companion lists. Not
 * a file - selecting it loads every playlist file in turn (under the
 * same per-frame budget) and presents the union, sorted by label, capped
 * by desktop_menu_all_playlists_list/grid_max_count (0 = no cap). Its
 * companion_core_playlist_path() is this token, as the Qt companion has
 * always used, so desktop_menu_initial_playlist can name it. */
#define COMPANION_ALL_PLAYLISTS_TOKEN "|||ALL|||"

/* The playlist file entry @i of the current view belongs to: the selected
 * playlist normally, or, under All Playlists, the file that entry came
 * from. Edits (delete, associate core) must target this, never the All
 * token. NULL when unknown. */
const char *companion_core_entry_playlist_path(companion_core_t *core,
      size_t i);
/* Entry @i's index within that file (== @i except under All Playlists);
 * (size_t)-1 when unknown. Pair with the path above for edits. */
size_t companion_core_entry_index_in_playlist(companion_core_t *core, size_t i);

/* --- Playlist files ------------------------------------------------ */

/* Rescan the playlist directory. Cheap (directory listing only),
 * completes synchronously and fires on_playlists_changed. */
void companion_core_refresh_playlists(companion_core_t *core);
size_t companion_core_playlist_count(companion_core_t *core);
/* Display name: file name without the .lpl extension. */
const char *companion_core_playlist_name(companion_core_t *core, size_t i);
const char *companion_core_playlist_path(companion_core_t *core, size_t i);

/* --- Selected playlist --------------------------------------------- */

/* Start loading playlist @i. Parsing is spread over
 * companion_core_iterate() calls; on_playlist_changed fires when the
 * entries are available. Selecting while a load is pending aborts the
 * pending load. Returns false if @i is out of range. */
bool companion_core_select_playlist(companion_core_t *core, size_t i);
/* Same as companion_core_select_playlist() for an arbitrary playlist
 * file @path (it need not be in the playlist directory). The selected
 * index becomes the matching entry of the file list, or (size_t)-1. */
bool companion_core_select_playlist_path(companion_core_t *core,
      const char *path);
/* Index of the selected playlist, or (size_t)-1 if none. */
size_t companion_core_selected_playlist(companion_core_t *core);
/* Path of the playlist currently selected / loading ("" if none). */
const char *companion_core_selected_playlist_path(companion_core_t *core);
bool companion_core_playlist_loading(companion_core_t *core);
size_t companion_core_entry_count(companion_core_t *core);
const struct playlist_entry *companion_core_entry(companion_core_t *core,
      size_t i);

/* --- File-system browser ------------------------------------------- */

/* Open @path as the browse directory (NULL / empty -> the configured
 * default content directory, else the filesystem root). Lists it
 * (directories first, then files), replacing the previous listing.
 * Cheap - a single directory read. Returns false only on a listing
 * error, in which case the previous listing is kept. */
bool companion_core_browse_open(companion_core_t *core, const char *path);
/* The directory currently listed ("" before the first open). */
const char *companion_core_browse_dir(companion_core_t *core);
size_t companion_core_browse_count(companion_core_t *core);
/* Whether a browse_open() is still enumerating (show "Loading..."). The
 * previous listing stays readable until the new one lands. */
bool companion_core_browse_busy(companion_core_t *core);
/* Size (bytes; 0 for directories) and modification time (seconds since
 * the epoch; 0 unknown) of entry @i, gathered with the listing off the
 * UI thread so a view never touches the disk to paint. */
uint64_t companion_core_browse_size(companion_core_t *core, size_t i);
int64_t  companion_core_browse_mtime(companion_core_t *core, size_t i);
/* The browser table's columns as the Qt companion shows them, formatted
 * once here for every backend: Size ("12 KB", "1.5 MB", "" for a
 * folder), Type ("Drive", "File Folder", "ZIP File"), Date Modified
 * (local time, "YYYY-MM-DD HH:MM"). Each writes into @s and returns it. */
const char *companion_core_browse_size_str(companion_core_t *core, size_t i,
      char *s, size_t len);
const char *companion_core_browse_type_str(companion_core_t *core, size_t i,
      char *s, size_t len);
const char *companion_core_browse_date_str(companion_core_t *core, size_t i,
      char *s, size_t len);
/* The listing is directories first (".." included), then files: indices
 * [0, dir_count) are directories, [dir_count, count) files. A Qt-style
 * browser shows the first range in its folder pane and the second as
 * the selected folder's content. */
size_t companion_core_browse_dir_count(companion_core_t *core);
/* Go to the parent directory (Qt's "Up"); false at the top. */
bool companion_core_browse_up(companion_core_t *core);

/* Sort the listing by a column, as clicking the browser's header does
 * in every companion: folders always first (and ".." first of all),
 * then the files by name (case-insensitive), size, type (extension) or
 * modification time, ascending or not. Applies to the current listing
 * at once (on_browse_changed fires) and to every listing after it. */
enum companion_browse_column
{
   COMPANION_BROWSE_SORT_NAME = 0,
   COMPANION_BROWSE_SORT_SIZE,
   COMPANION_BROWSE_SORT_TYPE,
   COMPANION_BROWSE_SORT_DATE
};
void companion_core_browse_sort(companion_core_t *core,
      enum companion_browse_column column, bool ascending);
enum companion_browse_column companion_core_browse_sort_column(companion_core_t *core);
bool companion_core_browse_sort_ascending(companion_core_t *core);
/* Display name of entry @i (base name; ".." for the parent link). */
const char *companion_core_browse_name(companion_core_t *core, size_t i);
/* Full path of entry @i. */
const char *companion_core_browse_path(companion_core_t *core, size_t i);
bool companion_core_browse_is_dir(companion_core_t *core, size_t i);
/* Activate entry @i: descend into a directory (re-opens it, fires
 * nothing) or, for a file, load it - through the current core, or, when
 * @pick_core_path is given, that core. Returns 1 when content was
 * requested (the caller may hide its window), 0 when a directory was
 * entered, -1 on error. A file with no usable core and no
 * @pick_core_path returns -1 with *needs_core set and the path in
 * @content, so the caller can open a core picker. */
int companion_core_browse_activate(companion_core_t *core, size_t i,
      const char *pick_core_path, bool *needs_core,
      char *content, size_t content_len);

/* --- Commands (never block; work goes to the task queue) ----------- */

/* Load playlist entry @i with its associated core (or the current core
 * when the entry has none). Returns false when it cannot proceed - a
 * common reason is that the entry needs a core chosen (see
 * companion_core_entry_needs_core); the caller then opens a picker. */
bool companion_core_request_load_entry(companion_core_t *core, size_t i);
/* True when entry @i has no usable core: its own core is empty or
 * "DETECT" and no core is currently loaded, so the entry cannot run
 * until the user picks one. Copies the entry's content path into @s
 * (a picker filters its core list by it). */
bool companion_core_entry_needs_core(companion_core_t *core, size_t i,
      char *s, size_t len);
/* Load arbitrary content. @core_path may be NULL to use the current
 * core; @content_path may be NULL to start the core without content. */
bool companion_core_request_load(companion_core_t *core,
      const char *core_path, const char *content_path);
/* Playlist-style load request, as issued by a companion's Run action:
 * @core_path is sanitised against core_info, @db_name gets its .lpl
 * extension, the running core is unloaded and the load is pushed to
 * the task queue with the menu parked on the quick menu. Any argument
 * but @core_path and @content_path may be NULL. Returns false when
 * the task could not be pushed (the caller reports the failure). */
bool companion_core_request_load_content(companion_core_t *core,
      const char *core_path, const char *content_path,
      const char *label, const char *db_name, const char *crc32);
/* Start the running core without content (contentless cores). */
bool companion_core_start_core(companion_core_t *core);
/* Load a core with no content (HAVE_DYNAMIC): sets the core path,
 * rebuilds core_info and issues CMD_EVENT_LOAD_CORE. Returns false if
 * the core could not be loaded (or the build is not HAVE_DYNAMIC). */
bool companion_core_load_core(companion_core_t *core, const char *path);
/* Unload the running core (CMD_EVENT_UNLOAD_CORE), resetting the menu
 * selection as the companions do. */
bool companion_core_unload_core(companion_core_t *core);
/* Path of the currently loaded core ("" if none). */
const char *companion_core_current_core_path(companion_core_t *core);
/* Copy the default core path of playlist @name (file name without
 * .lpl) into @s. Empty result when the playlist has none or "DETECT".
 * Uses the menu's cached playlist when it is the same file; otherwise
 * parses the file (synchronously - behaviour inherited from the Qt
 * companion, to be moved to a budgeted parse). */
size_t companion_core_playlist_default_core(companion_core_t *core,
      const char *name, char *s, size_t len);
void companion_core_event_command(companion_core_t *core,
      enum event_command cmd);
/* Scan @path (a directory when @directory, else a single file) against
 * the content databases into the playlist directory, on the task
 * queue. on_scan_finished fires when done, after the menu's horizontal
 * list has been reset. Returns false when no scan could be started
 * (no libretrodb in this build, or the task could not be pushed). */
bool companion_core_request_scan(companion_core_t *core, const char *path,
      bool directory, bool show_hidden_files);

/* --- Companion settings (retroarch.cfg) ------------------------------- */

/* The desktop companion's presentation settings live in retroarch.cfg
 * (settings->*.desktop_menu_*), shared by the Qt, Win32 and Cocoa
 * companions and saved with the main config. These accessors are the
 * shape the backends use; values are plain (no toolkit blobs). */

/* View: false = list, true = icons. */
bool companion_core_pref_icon_view(companion_core_t *core);
void companion_core_pref_set_icon_view(companion_core_t *core, bool icons);
/* Thumbnail repository subdir for the icon view / boxart pane
 * (COMPANION_THUMB_BOXART etc.) from desktop_menu_thumbnail_type. */
const char *companion_core_pref_thumbnail_subdir(companion_core_t *core);
/* Playlist file to open at startup ("" = use History). */
const char *companion_core_pref_initial_playlist(companion_core_t *core);
bool companion_core_pref_suggest_loaded_core_first(companion_core_t *core);
bool companion_core_pref_show_hidden_files(companion_core_t *core);
/* Content-browser tab to restore: -1 when not remembering, else 0/1. */
int companion_core_pref_last_tab(companion_core_t *core);
void companion_core_pref_set_last_tab(companion_core_t *core, int tab);
/* Icon-view zoom, 0..100 (desktop_menu_icon_view_zoom). */
unsigned companion_core_pref_icon_view_zoom(companion_core_t *core);
void companion_core_pref_set_icon_view_zoom(companion_core_t *core, unsigned z);
/* Thumbnail type, 0 boxart / 1 screenshot / 2 title / 3 logo. */
unsigned companion_core_pref_thumbnail_type(companion_core_t *core);
void companion_core_pref_set_thumbnail_type(companion_core_t *core, unsigned t);

/* --- Playlist icons ------------------------------------------------ */

/* The icon a playlist list shows for playlist @i: the XMB dot-art asset
 * <assets>/xmb/dot-art/png/<playlist name>.png when it exists, else the
 * generic folder.png from the same set (what the Qt companion shows).
 * Copies the path into @s; returns its length, 0 if neither exists. */
size_t companion_core_playlist_icon_path(companion_core_t *core, size_t i,
      char *s, size_t len);
/* The generic folder icon of the same asset set (folder.png), for
 * playlists without their own and for the file browser. */
size_t companion_core_folder_icon_path(companion_core_t *core,
      char *s, size_t len);

/* --- Thumbnails ---------------------------------------------------- */

/* Subdirectory names of the thumbnail repository layout. */
#define COMPANION_THUMB_BOXART     "Named_Boxarts"
#define COMPANION_THUMB_SCREENSHOT "Named_Snaps"
#define COMPANION_THUMB_TITLE      "Named_Titles"
#define COMPANION_THUMB_LOGO       "Named_Logos"

/* <thumbnails dir>/<db_name>/<subdir>. Returns the length written. */
size_t companion_core_thumbnail_dir(companion_core_t *core,
      const char *db_name, const char *subdir, char *s, size_t len);
/* Thumbnail file for an entry: the label with the characters the
 * thumbnail repository forbids replaced by '_', under
 * companion_core_thumbnail_dir(); the first of .png .jpg .jpeg .bmp .tga
 * that exists, else the .png name (the download / save target). When
 * @content_path is itself an image file it is returned as-is, so image
 * content shows as its own thumbnail. @db_name is the playlist name
 * without .lpl; @label has no extension. Returns the length written. */
size_t companion_core_thumbnail_path(companion_core_t *core,
      const char *db_name, const char *subdir, const char *label,
      const char *content_path, char *s, size_t len);

/* --- Running core -------------------------------------------------- */

/* Library name of the running core ("" if none). */
const char *companion_core_current_core_name(companion_core_t *core);
/* Version string of the running core ("" if none). */
const char *companion_core_current_core_version(companion_core_t *core);
/* True when the running core can start without content. */
bool companion_core_current_core_supports_no_content(companion_core_t *core);

/* --- "Launch with" candidates -------------------------------------- */

enum companion_launch_selection
{
   COMPANION_LAUNCH_CURRENT = 0,      /* the running core */
   COMPANION_LAUNCH_PLAYLIST_SAVED,   /* the entry's own core */
   COMPANION_LAUNCH_PLAYLIST_DEFAULT, /* the playlist's default core */
   COMPANION_LAUNCH_ASK,              /* presentation: "Ask" */
   COMPANION_LAUNCH_LOAD_CORE         /* presentation: "Load Core..." */
};

typedef struct companion_launch_option
{
   char name[NAME_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
   enum companion_launch_selection selection;
} companion_launch_option_t;

/* Cores worth offering to launch a content entry with, in menu order
 * and de-duplicated: the running core (when @suggest_loaded_first and
 * one is loaded), the entry's own core (@entry_core_path /
 * @entry_core_name, skipped when empty or "DETECT"), and the default
 * core of playlist @playlist_name (name without .lpl; for a file
 * browser pass the directory name) resolved through core_info.
 * The presentation appends its own ASK / LOAD_CORE items.
 * Returns the number of options written to @out (at most @max). */
size_t companion_core_launch_options(companion_core_t *core,
      const char *entry_core_path, const char *entry_core_name,
      const char *playlist_name, bool suggest_loaded_first,
      companion_launch_option_t *out, size_t max);

/* --- Thumbnail downloads (HAVE_NETWORKING) -------------------------- */

/* Fetch <db_name>/<subdir>/<label>.png from the thumbnail server into
 * the repository (written via a .partial file, then renamed). One
 * download is in flight at a time; a caller wanting several queues
 * them off on_thumbnail_downloaded. Returns false if the transfer could
 * not be started (no networking, or one is already running). */
bool companion_core_thumbnail_download(companion_core_t *core,
      const char *db_name, const char *label, const char *subdir);
/* Fetch the <db_name>.zip thumbnail pack and extract it over the
 * repository (existing files are replaced; ones that cannot be deleted
 * are renamed aside with .tmp). on_thumbnail_pack_finished reports. */
bool companion_core_thumbnail_pack_download(companion_core_t *core,
      const char *db_name);
/* Cancel the transfer in flight, if any. */
void companion_core_download_cancel(companion_core_t *core);
bool companion_core_download_active(companion_core_t *core);

/* --- Core information panel ---------------------------------------- */

/* Status codes carried in string_list_elem_attr.i for the
 * core-info value list returned by companion_core_core_info_rows(). */
enum companion_core_info_row_status
{
   COMPANION_CORE_INFO_ROW_NORMAL = 0,
   /* Firmware section: header rows with a key but empty value. */
   COMPANION_CORE_INFO_ROW_FIRMWARE_NOTE,
   /* Firmware status rows that should render in green. */
   COMPANION_CORE_INFO_ROW_FIRMWARE_PRESENT,
   /* Firmware status rows that should render in red. */
   COMPANION_CORE_INFO_ROW_FIRMWARE_MISSING,
   /* Notes: no key, free-form value. */
   COMPANION_CORE_INFO_ROW_NOTE_NO_KEY
};


/* Fill @keys / @values (parallel, caller-created string_lists) with the
 * rows a core-information panel shows for @core_path: name, label,
 * version, system, authors, permissions, licences, extensions, then the
 * firmware section and free-form notes. Each value's attr.i carries a
 * companion_core_info_row_status. Returns false when there is no
 * information (one "no information" row is still emitted). */
bool companion_core_core_info_rows(const char *core_path,
      struct string_list *keys, struct string_list *values);

/* --- Installed cores (for "associate core" style pickers) ---------- */

size_t companion_core_installed_core_count(companion_core_t *core);
const char *companion_core_installed_core_path(companion_core_t *core,
      size_t i);
/* Display name, falling back to the core file's base name. */
const char *companion_core_installed_core_name(companion_core_t *core,
      size_t i);
const char *companion_core_installed_core_version(companion_core_t *core,
      size_t i);
/* Reorder the installed-core list so the cores that can run
 * @content_path - by its extension, or any member's when it is an
 * archive, using core_info's matcher - come first, and return how many
 * there are. Indices 0..n-1 of the accessors above then name them.
 * NULL / empty @content_path leaves the order and returns the count. */
size_t companion_core_installed_cores_supporting(companion_core_t *core,
      const char *content_path);

/* --- Playlist editing ---------------------------------------------- */

/* Open playlist file @path for reading or editing. When it is the
 * playlist the menu currently has cached, that object is returned and
 * *owned is false (edits then go through the object the menu reads);
 * otherwise the file is parsed (synchronously) and *owned is true.
 * Release with companion_core_playlist_release(); NULL on failure. */
playlist_t *companion_core_playlist_open(companion_core_t *core,
      const char *path, bool *owned);
/* Write @playlist to disk if @write, then free it if @owned. */
void companion_core_playlist_release(companion_core_t *core,
      playlist_t *playlist, bool owned, bool write);
/* Hidden playlists: the comma-separated file-name list Qt's context
 * menu maintains (desktop_menu_hidden_playlists in retroarch.cfg).
 * @path is a playlist file path; only its file name is stored.
 * A hidden playlist is not in the listing at all: companion_core_
 * playlist_count / _name / _path skip it, so every backend shows the
 * same rows at the same indices. The hidden ones are enumerated
 * separately, for the menu that puts them back. */
bool companion_core_playlist_is_hidden(companion_core_t *core, const char *path);
void companion_core_playlist_set_hidden(companion_core_t *core,
      const char *path, bool hidden);
size_t      companion_core_hidden_count(companion_core_t *core);
const char *companion_core_hidden_name(companion_core_t *core, size_t i);
const char *companion_core_hidden_path(companion_core_t *core, size_t i);

/* Qt's New Playlist... / Delete Playlist...: an empty .lpl in the
 * playlists directory (refused if one by that name is there), and
 * deleting a file from that directory (specials refused). Both refresh
 * the listing. */
bool companion_core_playlist_new(companion_core_t *core, const char *name,
      char *out_path, size_t len);
bool companion_core_playlist_delete(companion_core_t *core, const char *path);

/* Rename playlist file @path to @new_name (no directory, no extension)
 * within the playlists directory, as Qt's rename does: special
 * playlists (history, favorites - outside that directory) cannot be
 * renamed. Writes @out_path (the new file) on success. */
bool companion_core_playlist_rename(companion_core_t *core,
      const char *path, const char *new_name, char *out_path, size_t len);

/* Add content to playlist file @playlist_path, as Qt's drop / "Add
 * files" does: each of @paths that is a file is pushed (label = file
 * name without extension, db_name = the playlist's name); a directory
 * is walked recursively. @core_path / @core_name may be NULL (DETECT).
 * Returns the number added; the playlist is written once at the end
 * and on_playlist_changed fires if it is the one shown. */
size_t companion_core_playlist_add_files(companion_core_t *core,
      const char *playlist_path, const char *const *paths, size_t n,
      const char *core_path, const char *core_name);

/* Install an image dropped on a thumbnail pane as entry's thumbnail of
 * @type (COMPANION_THUMB_*), as Qt's changeThumbnail does: decoded,
 * downscaled to desktop_menu_thumbnail_max_size if set, written as
 * PNG under the repository path (directories created). Writes the
 * file path to @out_path on success. @image_path is a file; the
 * caller's engine should forget() that path afterwards. */
bool companion_core_thumbnail_install(companion_core_t *core,
      const char *db_name, const char *type, const char *label,
      const char *image_path, char *out_path, size_t len);

/* --- Options (Qt's View > Options dialog) --------------------------------
 * The desktop-menu settings that dialog edits, as a typed table every
 * backend can render as rows: a label, a kind, the current value as a
 * string, and a setter from a string. Settings are written to the
 * config by RetroArch as usual. */
enum companion_setting_kind
{
   COMPANION_SETTING_BOOL = 0,   /* "0" / "1" */
   COMPANION_SETTING_UINT,       /* decimal */
   COMPANION_SETTING_STRING,     /* free text */
   COMPANION_SETTING_CHOICE      /* one of companion_core_setting_choice() */
};
size_t      companion_core_setting_count(companion_core_t *core);
const char *companion_core_setting_label(companion_core_t *core, size_t i);
enum companion_setting_kind companion_core_setting_kind(companion_core_t *core, size_t i);
/* Current value as text into @s; returns @s. */
const char *companion_core_setting_get(companion_core_t *core, size_t i, char *s, size_t len);
/* Set from text (a bool takes 0/1/true/false; a choice its label or
 * index); false when the text is not acceptable. */
bool        companion_core_setting_set(companion_core_t *core, size_t i, const char *text);
size_t      companion_core_setting_choice_count(companion_core_t *core, size_t i);
const char *companion_core_setting_choice(companion_core_t *core, size_t i, size_t c);

/* --- Core Options (Qt's Core Options dialog) ---------------------------
 * The running core's options as a flat table: one row per visible
 * option with a description, its possible values (labels) and the
 * current one. Indices are into that table for this snapshot; refresh
 * after a core change. Nothing is toolkit-specific. */
size_t      companion_core_option_count(companion_core_t *core);
const char *companion_core_option_desc(companion_core_t *core, size_t i);
const char *companion_core_option_info(companion_core_t *core, size_t i);
size_t      companion_core_option_value_count(companion_core_t *core, size_t i);
const char *companion_core_option_value_label(companion_core_t *core, size_t i, size_t v);
size_t      companion_core_option_current(companion_core_t *core, size_t i);   /* value index */
/* Set option @i to value index @v (written when the core flushes, as
 * the menu does); reset to its default. */
void        companion_core_option_set(companion_core_t *core, size_t i, size_t v);
void        companion_core_option_reset(companion_core_t *core, size_t i);
void        companion_core_option_reset_all(companion_core_t *core);

/* --- Shader parameters (Qt's Shader Parameters dialog) -----------------
 * The menu shader's parameters: description, range, step, current.
 * Edits apply through CMD_EVENT_SHADERS_APPLY_CHANGES. */
size_t      companion_core_shader_param_count(companion_core_t *core);
const char *companion_core_shader_param_desc(companion_core_t *core, size_t i);
bool        companion_core_shader_param_range(companion_core_t *core, size_t i,
      float *min, float *max, float *step, float *initial);
float       companion_core_shader_param_current(companion_core_t *core, size_t i);
void        companion_core_shader_param_set(companion_core_t *core, size_t i, float v);
void        companion_core_shader_param_reset(companion_core_t *core, size_t i);
/* The shader preset's own path ("" when none), for the dialog's title. */
const char *companion_core_shader_path(companion_core_t *core);
void        companion_core_shader_apply(companion_core_t *core);

/* Replace entry @index of playlist file @path with @entry and write. */
bool companion_core_playlist_update_entry(companion_core_t *core,
      const char *path, size_t index, const struct playlist_entry *entry);
/* Delete entry @index of playlist file @path and write. */
bool companion_core_playlist_delete_entry(companion_core_t *core,
      const char *path, size_t index);
/* Associate playlist file @path with @core_path (resolved through
 * core_info; unknown or NULL -> "DETECT") and write. */
bool companion_core_playlist_set_default_core(companion_core_t *core,
      const char *path, const char *core_path);

/* Bulk add. Opens playlist @path as a private instance (never the
 * menu's cached one), so a cancelled add can be discarded unwritten:
 * commit with companion_core_playlist_release(core, pl, true, true),
 * abort with companion_core_playlist_release(core, pl, true, false). */
playlist_t *companion_core_playlist_open_private(companion_core_t *core,
      const char *path);
/* Content path a playlist entry should carry for @path: an archive
 * holding exactly one file resolves to "archive#file" (not to every
 * member: for MAME/FBA-style content the archive itself is the entry).
 * Copies into @s; returns its length. */
size_t companion_core_resolve_content_path(companion_core_t *core,
      const char *path, char *s, size_t len);
/* Append an entry. NULL / empty @core_path or @core_name become
 * "DETECT"; the crc is the unknown marker. */
bool companion_core_playlist_push(companion_core_t *core,
      playlist_t *playlist, const char *content_path, const char *label,
      const char *core_path, const char *core_name, const char *db_name);

/* --- Window hand-off ------------------------------------------------ */

/* Before a companion raises its window: release the mouse grab, show
 * the cursor and leave fullscreen so the window can be reached. Every
 * backend's toggle does exactly this. */
void companion_core_prepare_show_window(companion_core_t *core);
/* True when RetroArch started in fullscreen (a companion may prefer to
 * stay behind the video window then). */
bool companion_core_video_started_fullscreen(companion_core_t *core);

/* --- Inbound notifications from RetroArch (called by the driver glue) */

void companion_core_status_message(companion_core_t *core,
      const char *msg, unsigned prio, unsigned duration, bool flush);
void companion_core_log_message(companion_core_t *core, const char *msg);
void companion_core_notify_refresh(companion_core_t *core);

RETRO_END_DECLS

#endif
