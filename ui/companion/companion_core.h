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
/* Index of the selected playlist, or (size_t)-1 if none. */
size_t companion_core_selected_playlist(companion_core_t *core);
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

/* --- Inbound notifications from RetroArch (called by the driver glue) */

void companion_core_status_message(companion_core_t *core,
      const char *msg, unsigned prio, unsigned duration, bool flush);
void companion_core_log_message(companion_core_t *core, const char *msg);
void companion_core_notify_refresh(companion_core_t *core);

RETRO_END_DECLS

#endif
