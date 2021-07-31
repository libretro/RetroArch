/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2019-2021 - Brian Weiss
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

#ifndef __RARCH_CHEEVOS_LOCALS_H
#define __RARCH_CHEEVOS_LOCALS_H

#include "../deps/rcheevos/include/rc_runtime.h"

#include "../deps/rcheevos/src/rcheevos/rc_libretro.h"

#include <../command.h>
#include <../verbosity.h>
#include <boolean.h>
#include <queues/task_queue.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/************************************************************************
 * Logging                                                              *
 ************************************************************************/

/* Define this macro to get extra-verbose log for cheevos. */
#define CHEEVOS_VERBOSE

#define RCHEEVOS_TAG "[RCHEEVOS]: "
#define CHEEVOS_FREE(p) do { void* q = (void*)p; if (q) free(q); } while (0)

#ifdef CHEEVOS_VERBOSE
 #define CHEEVOS_LOG RARCH_LOG
 #define CHEEVOS_ERR RARCH_ERR
#else
 void rcheevos_log(const char *fmt, ...);
 #define CHEEVOS_LOG rcheevos_log
 #define CHEEVOS_ERR RARCH_ERR
#endif

/************************************************************************
 * State                                                                *
 ************************************************************************/

enum
{
   RCHEEVOS_ACTIVE_SOFTCORE = 1 << 0,
   RCHEEVOS_ACTIVE_HARDCORE = 1 << 1,
   RCHEEVOS_ACTIVE_UNOFFICIAL = 1 << 2,
   RCHEEVOS_ACTIVE_UNSUPPORTED = 1 << 3
};

typedef struct rcheevos_racheevo_t
{
  const char* title;
  const char* description;
  const char* badge;
  const char* memaddr;
  unsigned id;
  unsigned points;

  retro_time_t unlock_time;
  uint8_t active;

#ifdef HAVE_MENU
  uint8_t menu_bucket;
  uint8_t menu_progress;
  uint8_t menu_badge_grayscale;
  uintptr_t menu_badge_texture;
#endif

} rcheevos_racheevo_t;

typedef struct rcheevos_ralboard_t
{
  const char* title;
  const char* description;
  const char* mem;
  unsigned id;
  unsigned format;
} rcheevos_ralboard_t;


enum rcheevos_load_state
{
   RCHEEVOS_LOAD_STATE_NONE,
   RCHEEVOS_LOAD_STATE_IDENTIFYING_GAME,
   RCHEEVOS_LOAD_STATE_FETCHING_GAME_DATA,
   RCHEEVOS_LOAD_STATE_STARTING_SESSION,
   RCHEEVOS_LOAD_STATE_FETCHING_BADGES,
   RCHEEVOS_LOAD_STATE_DONE,
   RCHEEVOS_LOAD_STATE_UNKNOWN_GAME,
   RCHEEVOS_LOAD_STATE_NETWORK_ERROR,
   RCHEEVOS_LOAD_STATE_LOGIN_FAILED,
   RCHEEVOS_LOAD_STATE_ABORTED
};

typedef struct rcheevos_load_info_t
{
   enum rcheevos_load_state state;
   int  hashes_tried;
   int  outstanding_requests;
#ifdef HAVE_THREADS
   slock_t* request_lock;
#endif
} rcheevos_load_info_t;

typedef struct rcheevos_game_info_t
{
   int   id;
   int   console_id;
   char* title;
   char  hash[33];

   rcheevos_racheevo_t* achievements;
   rcheevos_ralboard_t* leaderboards;

   unsigned achievement_count;
   unsigned leaderboard_count;

} rcheevos_game_info_t;

#ifdef HAVE_MENU

typedef struct rcheevos_menuitem_t
{
   rcheevos_racheevo_t* cheevo;
   enum msg_hash_enums state_label_idx;
} rcheevos_menuitem_t;

void rcheevos_menu_reset_badges(void);

#endif

typedef struct rcheevos_locals_t
{
   rc_runtime_t runtime;              /* rcheevos runtime state */
   rcheevos_game_info_t game;         /* information about the current game */
   rc_libretro_memory_regions_t memory;/* achievement addresses to core memory mappings */

#ifdef HAVE_THREADS
   enum event_command queued_command; /* action queued by background thread to be run on main thread */
#endif

   char username[32];                 /* case-corrected username */
   char token[32];                    /* user's session token */
   char user_agent_prefix[128];       /* RetroArch/OS version information */
   char user_agent_core[256];         /* RetroArch/OS/Core version information */

#ifdef HAVE_MENU
   rcheevos_menuitem_t* menuitems;    /* array of items for the achievements quick menu */
   unsigned menuitem_capacity;        /* maximum number of items in the menuitems array */
   unsigned menuitem_count;           /* current number of items in the menuitems array */
#endif

   rcheevos_load_info_t load_info;    /* load info */

   bool hardcore_active;              /* hardcore functionality is active */
   bool loaded;                       /* load task has completed */
   bool core_supports;                /* false if core explicitly disables achievements */
   bool leaderboards_enabled;         /* leaderboards are enabled */
   bool leaderboard_notifications;    /* leaderboard notifications are enabled */
   bool leaderboard_trackers;         /* leaderboard trackers are enabled */
} rcheevos_locals_t;

rcheevos_locals_t* get_rcheevos_locals(void);
void rcheevos_begin_load_state(enum rcheevos_load_state state);
int rcheevos_end_load_state(void);
bool rcheevos_load_aborted(void);

#ifdef HAVE_THREADS
 #define CHEEVOS_LOCK(l)   do { slock_lock(l); } while (0)
 #define CHEEVOS_UNLOCK(l) do { slock_unlock(l); } while (0)
#else
 #define CHEEVOS_LOCK(l)
 #define CHEEVOS_UNLOCK(l)
#endif

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_LOCALS_H */
