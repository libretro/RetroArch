/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RUNLOOP_H
#define __RUNLOOP_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "core_option_manager.h"

enum  runloop_state
{
   RUNLOOP_STATE_ITERATE = 0,
   RUNLOOP_STATE_POLLED_AND_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

typedef struct runloop_ctx_msg_info
{
   const char *msg;
   unsigned prio;
   unsigned duration;
   bool flush;
} runloop_ctx_msg_info_t;

/* Contains the current retro_fastforwarding_override
 * parameters along with any pending updates triggered
 * by RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE */
typedef struct fastmotion_overrides
{
   struct retro_fastforwarding_override current;
   struct retro_fastforwarding_override next;
   bool pending;
} fastmotion_overrides_t;

typedef struct
{
   unsigned priority;
   float duration;
   char str[128];
   bool set;
} runloop_core_status_msg_t;

/* Contains all callbacks associated with
 * core options.
 * > At present there is only a single
 *   callback, 'update_display' - but we
 *   may wish to add more in the future
 *   (e.g. for directly informing a core of
 *   core option value changes, or getting/
 *   setting extended/non-standard option
 *   value data types) */
typedef struct core_options_callbacks
{
   retro_core_options_update_display_callback_t update_display;
} core_options_callbacks_t;

struct runloop
{
   retro_usec_t frame_time_last;        /* int64_t alignment */

   msg_queue_t msg_queue;                        /* ptr alignment */
#ifdef HAVE_THREADS
   slock_t *msg_queue_lock;
#endif
   size_t msg_queue_size;

   core_option_manager_t *core_options;
   core_options_callbacks_t core_options_callback; /* ptr alignment */

   retro_keyboard_event_t key_event;             /* ptr alignment */
   retro_keyboard_event_t frontend_key_event;    /* ptr alignment */

   rarch_system_info_t system;                   /* ptr alignment */
   struct retro_frame_time_callback frame_time;  /* ptr alignment */
   struct retro_audio_buffer_status_callback audio_buffer_status; /* ptr alignment */
   unsigned pending_windowed_scale;
   unsigned max_frames;
   unsigned audio_latency;

   fastmotion_overrides_t fastmotion_override; /* float alignment */

   bool missing_bios;
   bool force_nonblock;
   bool paused;
   bool idle;
   bool focused;
   bool slowmotion;
   bool fastmotion;
   bool shutdown_initiated;
   bool core_shutdown_initiated;
   bool core_running;
   bool perfcnt_enable;
   bool game_options_active;
   bool folder_options_active;
   bool autosave;
#ifdef HAVE_CONFIGFILE
   bool overrides_active;
#endif
   bool remaps_core_active;
   bool remaps_game_active;
   bool remaps_content_dir_active;
#ifdef HAVE_SCREENSHOTS
   bool max_frames_screenshot;
   char max_frames_screenshot_path[PATH_MAX_LENGTH];
#endif
};

typedef struct runloop runloop_state_t;

#ifdef HAVE_THREADS
#define RUNLOOP_MSG_QUEUE_LOCK(runloop) slock_lock(runloop.msg_queue_lock)
#define RUNLOOP_MSG_QUEUE_UNLOCK(runloop) slock_unlock(runloop.msg_queue_lock)
#else
#define RUNLOOP_MSG_QUEUE_LOCK(p_runloop)
#define RUNLOOP_MSG_QUEUE_UNLOCK(p_runloop)
#endif

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define RUNLOOP_TIME_TO_EXIT(quit_key_pressed) (runloop_state.shutdown_initiated || quit_key_pressed || !is_alive BSV_MOVIE_IS_EOF(p_rarch) || ((runloop_state.max_frames != 0) && (frame_count >= runloop_state.max_frames)) || runloop_exec)

RETRO_BEGIN_DECLS

RETRO_END_DECLS

#endif
