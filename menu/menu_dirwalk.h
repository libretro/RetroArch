/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#ifndef __MENU_DIRWALK_H
#define __MENU_DIRWALK_H

#include <stdlib.h>
#include <boolean.h>

#include <lists/string_list.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Budgeted, cancellable directory enumeration for interactive list
 * building.
 *
 * The displaylist walk sites all share one shape: enumerate a single
 * directory, sort, append to the menu list - and all of them do it
 * blocking on the thread that drives the UI.  This module gives that
 * shape a budget without changing its result:
 *
 *  - A request first runs the walk synchronously under one shared
 *    I/O window (task_nbio_slice_*).  Small and medium directories -
 *    the overwhelming case - complete on the spot and the caller
 *    proceeds exactly as before: same single enumeration, same
 *    single sort, cost merely capped.
 *  - A directory too large for the window continues on a task
 *    (moving off the UI thread entirely when Threaded Tasks is on).
 *    On completion the registered refresh callback fires; the
 *    repopulation it triggers re-issues the same request and
 *    receives the finished, sorted list.  Nothing is enumerated or
 *    sorted twice.
 *
 * One pending walk exists at a time (the menu shows one list at a
 * time); a request with a different identity cancels the previous
 * walk.  All entry points below are main-thread only - the same
 * thread that builds displaylists and runs task callbacks - which is
 * what makes the module lock-free: the task handler owns its walk
 * state exclusively while running, and hand-off happens in the
 * task's main-thread completion callback. */

enum menu_dirwalk_status
{
   MENU_DIRWALK_FAILED  = -1,   /* directory unreadable or allocation
                                   failure: show the existing error
                                   entry, nothing is pending */
   MENU_DIRWALK_PENDING =  0,   /* walk continues in the background:
                                   show a placeholder; the refresh
                                   callback fires on completion */
   MENU_DIRWALK_DONE    =  1    /* *out_list holds the completed,
                                   sorted list; ownership transfers
                                   to the caller (string_list_free) */
};

/* Consumer tags: which displaylist a completed walk belongs to.
 * Part of the request identity and handed to the refresh callback. */
enum menu_dirwalk_tag
{
   MENU_DIRWALK_TAG_NONE = 0,
   MENU_DIRWALK_TAG_FILEBROWSER,
   MENU_DIRWALK_TAG_PLAYLISTS,
   MENU_DIRWALK_TAG_ADD_TO_PLAYLIST,
   MENU_DIRWALK_TAG_PLAYLIST_MANAGER,
   MENU_DIRWALK_TAG_PL_THUMBNAILS,
   MENU_DIRWALK_TAG_CORES
};

enum menu_dirwalk_sort
{
   MENU_DIRWALK_SORT_NONE = 0,
   MENU_DIRWALK_SORT_DIR_FIRST,    /* dir_list_sort(list, true) */
   MENU_DIRWALK_SORT_IGNORE_EXT    /* dir_list_sort_ignore_ext(list, true) */
};

/**
 * menu_dirwalk_request:
 * @dir                : directory to enumerate (must be non-NULL).
 * @ext                : extension filter as dir_list_new() takes it,
 *                       or NULL.
 * @include_dirs       : include directories in the listing.
 * @include_hidden     : include hidden files and directories.
 * @include_compressed : include compressed files even when not in @ext.
 * @sort_mode          : sort applied once, on completion.
 * @tag                : caller-chosen consumer tag, passed to the
 *                       refresh callback so the repopulation path can
 *                       tell which list finished.
 * @out_list           : on MENU_DIRWALK_DONE receives the completed
 *                       sorted list; untouched otherwise.
 *
 * Identity is the full parameter set (@dir, @ext, flags, @sort_mode,
 * @tag).  Re-issuing the identical request while its walk is pending
 * returns MENU_DIRWALK_PENDING without restarting; issuing it after
 * the walk completed returns MENU_DIRWALK_DONE exactly once and
 * consumes the result.  A request with any other identity cancels
 * whatever was pending first.
 **/
enum menu_dirwalk_status menu_dirwalk_request(
      const char *dir, const char *ext,
      bool include_dirs, bool include_hidden, bool include_compressed,
      enum menu_dirwalk_sort sort_mode, unsigned tag,
      struct string_list **out_list);

/**
 * menu_dirwalk_cancel:
 *
 * Cancels any pending walk and discards any completed-but-unconsumed
 * result.  Called when the user navigates away from the list a walk
 * was feeding, and from menu teardown.  Safe when nothing is pending.
 **/
void menu_dirwalk_cancel(void);

/**
 * menu_dirwalk_pending:
 *
 * True while a walk is in flight or a completed result awaits
 * consumption.  For the displaylist watchdog and tests.
 **/
bool menu_dirwalk_pending(void);

/**
 * menu_dirwalk_set_refresh_cb:
 *
 * Registers the function fired (on the main thread, from the task
 * completion callback) when a background walk finishes - normally
 * the hook that marks menu entries for refresh.  Fires on both
 * success and failure of the walk; the re-issued request tells the
 * caller which.  Pass NULL to unregister.  This indirection keeps
 * the module free of menu-state dependencies, which is what lets
 * the sample oracle drive it standalone.
 **/
void menu_dirwalk_set_refresh_cb(void (*cb)(unsigned tag));

RETRO_END_DECLS

#endif
