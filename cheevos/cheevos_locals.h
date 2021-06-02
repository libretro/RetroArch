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

#include "cheevos_memory.h"

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
   RCHEEVOS_ACTIVE_UNOFFICIAL = 1 << 2
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

typedef struct rcheevos_rapatchdata_t
{
   char* title;
   rcheevos_racheevo_t* core;
   rcheevos_racheevo_t* unofficial;
   rcheevos_ralboard_t* lboards;
   char* richpresence_script;

   unsigned game_id;
   unsigned console_id;
   unsigned core_count;
   unsigned unofficial_count;
   unsigned lboard_count;
} rcheevos_rapatchdata_t;

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
   rcheevos_rapatchdata_t patchdata;  /* achievement/leaderboard data from the server */
   rcheevos_memory_regions_t memory;  /* achievement addresses to core memory mappings */

   retro_task_t* task;                /* load task */
#ifdef HAVE_THREADS
   slock_t* task_lock;                /* mutex for starting/stopping load task */
   enum event_command queued_command; /* action queued by background thread to be run on main thread */
#endif

   char token[32];                    /* user's session token */
   char hash[33];                     /* retroachievements hash for current content */
   char user_agent_prefix[128];       /* RetroArch/OS version information */

#ifdef HAVE_MENU
   rcheevos_menuitem_t* menuitems;    /* array of items for the achievements quick menu */
   unsigned menuitem_capacity;        /* maximum number of items in the menuitems array */
   unsigned menuitem_count;           /* current number of items in the menuitems array */
#endif

   bool hardcore_active;              /* hardcore functionality is active */
   bool loaded;                       /* load task has completed */
   bool core_supports;                /* false if core explicitly disables achievements */
   bool leaderboards_enabled;         /* leaderboards are enabled */
   bool leaderboard_notifications;    /* leaderboard notifications are enabled */
   bool leaderboard_trackers;         /* leaderboard trackers are enabled */
} rcheevos_locals_t;

rcheevos_locals_t* get_rcheevos_locals();

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_LOCALS_H */
