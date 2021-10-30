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
#include <libretro.h>
#include <dynamic/dylib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "dynamic.h"
#include "core_option_manager.h"
#include "state_manager.h"

/* Arbitrary twenty subsystems limit */
#define SUBSYSTEM_MAX_SUBSYSTEMS 20

/* Arbitrary 10 roms for each subsystem limit */
#define SUBSYSTEM_MAX_SUBSYSTEM_ROMS 10

enum  runloop_state_enum
{
   RUNLOOP_STATE_ITERATE = 0,
   RUNLOOP_STATE_POLLED_AND_SLEEP,
   RUNLOOP_STATE_MENU_ITERATE,
   RUNLOOP_STATE_END,
   RUNLOOP_STATE_QUIT
};

enum poll_type_override_t
{
   POLL_TYPE_OVERRIDE_DONTCARE = 0,
   POLL_TYPE_OVERRIDE_EARLY,
   POLL_TYPE_OVERRIDE_NORMAL,
   POLL_TYPE_OVERRIDE_LATE
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

#ifdef HAVE_RUNAHEAD
typedef bool(*runahead_load_state_function)(const void*, size_t);

typedef void *(*constructor_t)(void);
typedef void  (*destructor_t )(void*);

typedef struct my_list_t
{
   void **data;
   constructor_t constructor;
   destructor_t destructor;
   int capacity;
   int size;
} my_list;
#endif

struct runloop
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   rarch_timer_t shader_delay_timer;            /* int64_t alignment */
#endif
   retro_time_t core_runtime_last;
   retro_time_t core_runtime_usec;
   retro_time_t frame_limit_minimum_time;
   retro_time_t frame_limit_last_time;
   retro_usec_t frame_time_last;                /* int64_t alignment */

   struct retro_core_t        current_core;     /* uint64_t alignment */
#if defined(HAVE_RUNAHEAD)
   uint64_t runahead_last_frame_count;          /* uint64_t alignment */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   struct retro_core_t secondary_core;          /* uint64_t alignment */
#endif
   retro_ctx_load_content_info_t *load_content_info;
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   char    *secondary_library_path;
#endif
   my_list *runahead_save_state_list;
   my_list *input_state_list;
#endif

#ifdef HAVE_REWIND
   struct state_manager_rewind_state rewind_st;
#endif
   struct retro_perf_counter *perf_counters_libretro[MAX_COUNTERS];
   bool    *load_no_content_hook;
   struct string_list *subsystem_fullpaths;
   struct retro_subsystem_info subsystem_data[SUBSYSTEM_MAX_SUBSYSTEMS];
   struct retro_callbacks retro_ctx;                     /* ptr alignment */
   msg_queue_t msg_queue;                                /* ptr alignment */
   retro_input_state_t input_state_callback_original;    /* ptr alignment */
#ifdef HAVE_RUNAHEAD
   function_t retro_reset_callback_original;             /* ptr alignment */
   function_t original_retro_deinit;                     /* ptr alignment */
   function_t original_retro_unload;                     /* ptr alignment */
   runahead_load_state_function
      retro_unserialize_callback_original;               /* ptr alignment */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   struct retro_callbacks secondary_callbacks;           /* ptr alignment */
#endif
#endif
#ifdef HAVE_THREADS
   slock_t *msg_queue_lock;
#endif

   content_state_t            content_st;                /* ptr alignment */
   struct retro_subsystem_rom_info
      subsystem_data_roms[SUBSYSTEM_MAX_SUBSYSTEMS]
      [SUBSYSTEM_MAX_SUBSYSTEM_ROMS];             /* ptr alignment */
   core_option_manager_t *core_options;
   core_options_callbacks_t core_options_callback;/* ptr alignment */

   retro_keyboard_event_t key_event;             /* ptr alignment */
   retro_keyboard_event_t frontend_key_event;    /* ptr alignment */

   rarch_system_info_t system;                   /* ptr alignment */
   struct retro_frame_time_callback frame_time;  /* ptr alignment */
   struct retro_audio_buffer_status_callback audio_buffer_status; /* ptr alignment */
#ifdef HAVE_DYNAMIC
   dylib_t lib_handle;                                   /* ptr alignment */
#endif
#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   dylib_t secondary_lib_handle;                         /* ptr alignment */
#endif
   size_t runahead_save_state_size;
#endif
   size_t msg_queue_size;

   unsigned pending_windowed_scale;
   unsigned max_frames;
   unsigned audio_latency;
   unsigned fastforward_after_frames;
   unsigned perf_ptr_libretro;
   unsigned subsystem_current_count;

   fastmotion_overrides_t fastmotion_override; /* float alignment */

   retro_bits_t has_set_libretro_device;        /* uint32_t alignment */

   enum rarch_core_type current_core_type;
   enum rarch_core_type explicit_current_core_type;
   enum poll_type_override_t core_poll_type_override;
#if defined(HAVE_RUNAHEAD)
   enum rarch_core_type last_core_type;
#endif

   char runtime_content_path_basename[8192];
   char current_library_name[256];
   char current_library_version[256];
   char current_valid_extensions[256];
   char subsystem_path[256];
#ifdef HAVE_SCREENSHOTS
   char max_frames_screenshot_path[PATH_MAX_LENGTH];
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char runtime_shader_preset_path[PATH_MAX_LENGTH];
#endif
   char runtime_content_path[PATH_MAX_LENGTH];
   char runtime_core_path[PATH_MAX_LENGTH];
   char savefile_dir[PATH_MAX_LENGTH];
   char savestate_dir[PATH_MAX_LENGTH];

   bool is_inited;
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
#endif
#ifdef HAVE_RUNAHEAD
   bool has_variable_update;
   bool input_is_dirty;
   bool runahead_save_state_size_known;
   bool request_fast_savestate;
   bool runahead_available;
   bool runahead_secondary_core_available;
   bool runahead_force_input_dirty;
#endif
#ifdef HAVE_PATCH
   bool patch_blocked;
#endif
   bool is_sram_load_disabled;
   bool is_sram_save_disabled;
   bool use_sram;
   bool ignore_environment_cb;
   bool core_set_shared_context;
   bool has_set_core;
};

typedef struct runloop runloop_state_t;

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

runloop_state_t *runloop_state_get_ptr(void);

RETRO_END_DECLS

#endif
