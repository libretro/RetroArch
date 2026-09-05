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
} companion_callbacks_t;

/* Lifecycle */
companion_core_t *companion_core_new(const companion_callbacks_t *cb,
      void *ud);
void companion_core_free(companion_core_t *core);

/* Advance pending budgeted work (playlist parse, ...) for at most
 * @budget_us microseconds. Call from the backend's iterate / idle
 * path. Never blocks past the budget. */
void companion_core_iterate(companion_core_t *core, unsigned budget_us);

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

/* --- Commands (never block; work goes to the task queue) ----------- */

/* Load playlist entry @i with its associated core (or the current core
 * when the entry has none). */
bool companion_core_request_load_entry(companion_core_t *core, size_t i);
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

/* --- Installed cores (for "associate core" style pickers) ---------- */

size_t companion_core_installed_core_count(companion_core_t *core);
const char *companion_core_installed_core_path(companion_core_t *core,
      size_t i);
const char *companion_core_installed_core_name(companion_core_t *core,
      size_t i);

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

/* --- Inbound notifications from RetroArch (called by the driver glue) */

void companion_core_status_message(companion_core_t *core,
      const char *msg, unsigned prio, unsigned duration, bool flush);
void companion_core_log_message(companion_core_t *core, const char *msg);
void companion_core_notify_refresh(companion_core_t *core);

RETRO_END_DECLS

#endif
