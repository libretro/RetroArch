/*  RetroArch - A frontend for libretro.
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

#include <boolean.h>
#include <command.h>
#include <queues/task_queue.h>
#include <verbosity.h>

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
  unsigned points;
  unsigned id;
  unsigned active;
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

   bool hardcore_active;              /* hardcore functionality is active */
   bool loaded;                       /* load task has completed */
   bool core_supports;                /* false if core explicitly disables achievements */
   bool leaderboards_enabled;         /* leaderboards are enabled */
   bool leaderboard_notifications;    /* leaderboard notifications are enabled */
   bool leaderboard_trackers;         /* leaderboard trackers are enabled */
} rcheevos_locals_t;

rcheevos_locals_t* get_rcheevos_locals();

RETRO_END_DECLS

#endif
