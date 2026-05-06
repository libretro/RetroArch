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

#include "../deps/rcheevos/include/rc_client.h"
#include "../deps/rcheevos/include/rc_runtime.h"
#include "../deps/rcheevos/src/rc_libretro.h"

#include <boolean.h>
#include <queues/task_queue.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
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

#define RCHEEVOS_TAG "[RCHEEVOS] "
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

enum rcheevos_summary_notif
{
   RCHEEVOS_SUMMARY_ALLGAMES = 0,
   RCHEEVOS_SUMMARY_HASCHEEVOS,
   RCHEEVOS_SUMMARY_OFF,
   RCHEEVOS_SUMMARY_LAST
};

#ifdef HAVE_MENU

typedef struct rcheevos_menuitem_t
{
   const rc_client_achievement_t* achievement;
   uintptr_t menu_badge_texture;
   uint32_t subset_id;
   uint8_t menu_badge_grayscale;
   enum msg_hash_enums state_label_idx;
} rcheevos_menuitem_t;

#endif

typedef struct rcheevos_locals_t
{
   rc_client_t* client;               /* rcheevos client state */
   rc_libretro_memory_regions_t memory;/* achievement addresses to core memory mappings */

#ifdef HAVE_THREADS
   /* Action queued by a background thread (e.g. the rcheevos
    * client's HTTP completion thread) to be dispatched on the
    * main thread by rcheevos_test. Atomic because writers can
    * be on a worker thread while the main-thread reader is
    * polling per frame; plain enum access would be a data race
    * with no memory barrier. */
   retro_atomic_int_t queued_command;

   /* Bumped on every load lifecycle reset (rcheevos_load and
    * rcheevos_unload). Captured at rc_client_begin_identify_and_load_game
    * time and passed through the callback's userdata; the
    * background callback compares its captured value against the
    * current value at completion and drops stale completions
    * whose target rcheevos_locals state has been reset out from
    * under them. Without this, a load for game A that completes
    * after the user has closed content and started loading game B
    * would write FINALIZE_LOAD into queued_command and cause the
    * main thread to apply game A's finalization to game B's
    * memory map. */
   retro_atomic_int_t load_generation;
#endif

   char user_agent_prefix[128];       /* RetroArch/OS version information */
   char user_agent_core[256];         /* RetroArch/OS/Core version information */

#ifdef HAVE_MENU
   rcheevos_menuitem_t* menuitems;    /* array of items for the achievements quick menu */
   unsigned menuitem_capacity;        /* maximum number of items in the menuitems array */
   unsigned menuitem_count;           /* current number of items in the menuitems array */
#endif

   bool hardcore_allowed;             /* prevents enabling hardcore if illegal settings detected */
   bool hardcore_requires_reload;     /* prevents enabling hardcore until the core is reloaded */
   bool hardcore_being_enabled;       /* allows callers to detect hardcore mode while it's being enabled */

   bool core_supports;                /* false if core explicitly disables achievements */
   bool badges_loaded;                /* true once all badges have been loaded */
   bool badges_loading;               /* true if the download queue is running */
} rcheevos_locals_t;

rcheevos_locals_t* get_rcheevos_locals(void);

RETRO_END_DECLS

#endif /* __RARCH_CHEEVOS_LOCALS_H */
