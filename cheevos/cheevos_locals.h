/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2018 - Andre Leiradella
 *  Copyright (C) 2019-2023 - Brian Weiss
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

#define HAVE_RC_CLIENT 1

#include "../deps/rcheevos/include/rc_client.h"
#include "../deps/rcheevos/include/rc_runtime.h"
#include "../deps/rcheevos/src/rc_libretro.h"

#include <boolean.h>
#include <queues/task_queue.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <retro_common_api.h>

#include "../command.h"
#include "../verbosity.h"

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

#ifndef HAVE_RC_CLIENT
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

#ifdef HAVE_GFX_WIDGETS
  int value;
  unsigned value_hash;
  uint8_t active_tracker_id;
#endif

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

#endif /* HAVE_RC_CLIENT */

enum rcheevos_summary_notif
{
   RCHEEVOS_SUMMARY_ALLGAMES = 0,
   RCHEEVOS_SUMMARY_HASCHEEVOS,
   RCHEEVOS_SUMMARY_OFF,
   RCHEEVOS_SUMMARY_LAST
};

#ifndef HAVE_RC_CLIENT

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
   char  badge_name[16];
   const char* hash;
   bool  mastery_placard_shown;

   rc_libretro_hash_set_t hashes;

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

#endif

#else /* HAVE_RC_CLIENT */

#ifdef HAVE_MENU

typedef struct rcheevos_menuitem_t
{
   rc_client_achievement_t* achievement;
   uintptr_t menu_badge_texture;
   uint32_t subset_id;
   uint8_t menu_badge_grayscale;
   enum msg_hash_enums state_label_idx;
} rcheevos_menuitem_t;

#endif

#endif /* HAVE_RC_CLIENT */

typedef struct rcheevos_locals_t
{
#ifdef HAVE_RC_CLIENT
   rc_client_t* client;               /* rcheevos client state */
#else
   rc_runtime_t runtime;              /* rcheevos runtime state */
   rcheevos_game_info_t game;         /* information about the current game */
#endif
   rc_libretro_memory_regions_t memory;/* achievement addresses to core memory mappings */

#ifdef HAVE_THREADS
   enum event_command queued_command; /* action queued by background thread to be run on main thread */
   bool game_placard_requested;       /* request to display game placard */
#endif

#ifndef HAVE_RC_CLIENT
   char displayname[32];              /* name to display in messages */
   char username[32];                 /* case-corrected username */
   char token[32];                    /* user's session token */
#endif
   char user_agent_prefix[128];       /* RetroArch/OS version information */
   char user_agent_core[256];         /* RetroArch/OS/Core version information */

#ifdef HAVE_MENU
   rcheevos_menuitem_t* menuitems;    /* array of items for the achievements quick menu */
   unsigned menuitem_capacity;        /* maximum number of items in the menuitems array */
   unsigned menuitem_count;           /* current number of items in the menuitems array */
#endif

#ifdef HAVE_RC_CLIENT
   bool hardcore_allowed;             /* prevents enabling hardcore if illegal settings detected */
   bool hardcore_being_enabled;       /* allows callers to detect hardcore mode while it's being enabled */
#else

#ifdef HAVE_GFX_WIDGETS
   unsigned active_lboard_trackers;   /* bit mask of active leaderboard tracker ids */
   rcheevos_racheevo_t* tracker_achievement;
   float tracker_progress;
#endif

   rcheevos_load_info_t load_info;    /* load info */

   uint32_t unpaused_frames;          /* number of unpaused frames before next pause is allowed */

   bool hardcore_active;              /* hardcore functionality is active */
   bool loaded;                       /* load task has completed */
#ifdef HAVE_GFX_WIDGETS
   bool assign_new_trackers;          /* a new leaderboard was started and needs a tracker assigned */
#endif

#endif

   bool core_supports;                /* false if core explicitly disables achievements */
} rcheevos_locals_t;

rcheevos_locals_t* get_rcheevos_locals(void);

#ifndef HAVE_RC_CLIENT
void rcheevos_begin_load_state(enum rcheevos_load_state state);
int rcheevos_end_load_state(void);
bool rcheevos_load_aborted(void);
void rcheevos_show_mastery_placard(void);
#endif

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_LOCALS_H */
