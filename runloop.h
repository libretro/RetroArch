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
#include <queues/message_queue.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "dynamic.h"
#include "configuration.h"
#include "core_option_manager.h"
#include "performance_counters.h"
#include "state_manager.h"
#include "tasks/tasks_internal.h"

/* Arbitrary twenty subsystems limit */
#define SUBSYSTEM_MAX_SUBSYSTEMS 20

/* Arbitrary 10 roms for each subsystem limit */
#define SUBSYSTEM_MAX_SUBSYSTEM_ROMS 10

#ifdef HAVE_THREADS
#define RUNLOOP_MSG_QUEUE_LOCK(runloop_st) slock_lock((runloop_st)->msg_queue_lock)
#define RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st) slock_unlock((runloop_st)->msg_queue_lock)
#else
#define RUNLOOP_MSG_QUEUE_LOCK(runloop_st) (void)(runloop_st)
#define RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st) (void)(runloop_st)
#endif

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

enum runloop_flags
{
   RUNLOOP_FLAG_MAX_FRAMES_SCREENSHOT             = (1 << 0),
   RUNLOOP_FLAG_HAS_SET_CORE                      = (1 << 1),
   RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT           = (1 << 2),
   RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB             = (1 << 3),
   RUNLOOP_FLAG_IS_SRAM_LOAD_DISABLED             = (1 << 4),
   RUNLOOP_FLAG_IS_SRAM_SAVE_DISABLED             = (1 << 5),
   RUNLOOP_FLAG_USE_SRAM                          = (1 << 6),
   RUNLOOP_FLAG_PATCH_BLOCKED                     = (1 << 7),
   RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE         = (1 << 8),
   RUNLOOP_FLAG_OVERRIDES_ACTIVE                  = (1 << 9),
   RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE               = (1 << 10),
   RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE             = (1 << 11),
   RUNLOOP_FLAG_REMAPS_CORE_ACTIVE                = (1 << 12),
   RUNLOOP_FLAG_REMAPS_GAME_ACTIVE                = (1 << 13),
   RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE         = (1 << 14),
   RUNLOOP_FLAG_SHUTDOWN_INITIATED                = (1 << 15),
   RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED           = (1 << 16),
   RUNLOOP_FLAG_CORE_RUNNING                      = (1 << 17),
   RUNLOOP_FLAG_AUTOSAVE                          = (1 << 18),
   RUNLOOP_FLAG_HAS_VARIABLE_UPDATE               = (1 << 19),
   RUNLOOP_FLAG_INPUT_IS_DIRTY                    = (1 << 20),
   RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN    = (1 << 21),
   RUNLOOP_FLAG_RUNAHEAD_AVAILABLE                = (1 << 22),
   RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE = (1 << 23),
   RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY        = (1 << 24),
   RUNLOOP_FLAG_SLOWMOTION                        = (1 << 25),
   RUNLOOP_FLAG_FASTMOTION                        = (1 << 26),
   RUNLOOP_FLAG_PAUSED                            = (1 << 27),
   RUNLOOP_FLAG_IDLE                              = (1 << 28),
   RUNLOOP_FLAG_FOCUSED                           = (1 << 29)
};

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

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   int port_map[MAX_USERS];
#endif
#endif

   runloop_core_status_msg_t core_status_msg;

   unsigned msg_queue_delay;
   unsigned pending_windowed_scale;
   unsigned max_frames;
   unsigned audio_latency;
   unsigned fastforward_after_frames;
   unsigned perf_ptr_libretro;
   unsigned subsystem_current_count;
   unsigned entry_state_slot;
   unsigned video_swap_interval_auto;

   fastmotion_overrides_t fastmotion_override; /* float alignment */

   retro_bits_t has_set_libretro_device;        /* uint32_t alignment */

   enum rarch_core_type current_core_type;
   enum rarch_core_type explicit_current_core_type;
   enum poll_type_override_t core_poll_type_override;
#if defined(HAVE_RUNAHEAD)
   enum rarch_core_type last_core_type;
#endif

   uint32_t flags;

   char runtime_content_path_basename[8192];
   char current_library_name[NAME_MAX_LENGTH];
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

   struct
   {
      char *remapfile;
      char savefile[8192];
      char savestate[8192];
      char cheatfile[8192];
      char ups[8192];
      char bps[8192];
      char ips[8192];
      char label[8192];
   } name;

   bool is_inited;
   bool missing_bios;
   bool force_nonblock;
   bool perfcnt_enable;
};

typedef struct runloop runloop_state_t;

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_IS_EOF() || (((input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_END) && (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_EOF_EXIT)))
#else
#define BSV_MOVIE_IS_EOF()
#endif

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define RUNLOOP_TIME_TO_EXIT(quit_key_pressed) ((runloop_state.flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED) || quit_key_pressed || !is_alive BSV_MOVIE_IS_EOF() || ((runloop_state.max_frames != 0) && (frame_count >= runloop_state.max_frames)) || runloop_exec)

RETRO_BEGIN_DECLS

void runloop_path_fill_names(void);

/**
 * runloop_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
bool runloop_environment_cb(unsigned cmd, void *data);

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category);

void runloop_set_current_core_type(
      enum rarch_core_type type, bool explicitly_set);

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on successful run,
 * Returns 1 if we have to wait until button input in order
 * to wake up the loop.
 * Returns -1 if we forcibly quit out of the
 * RetroArch iteration loop.
 **/
int runloop_iterate(void);

void runloop_perf_log(void);

void runloop_system_info_free(void);

bool runloop_path_init_subsystem(void);

/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : Pointer to system info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(
      const char *path,
      struct retro_system_info *info,
      bool *load_no_content);

void runloop_performance_counter_register(
		struct retro_perf_counter *perf);

void runloop_runtime_log_deinit(
      runloop_state_t *runloop_st,
      bool content_runtime_log,
      bool content_runtime_log_aggregate,
      const char *dir_runtime_log,
      const char *dir_playlist);

void runloop_event_deinit_core(void);

#ifdef HAVE_RUNAHEAD
void runloop_runahead_clear_variables(runloop_state_t *runloop_st);
#endif

bool runloop_event_init_core(
      settings_t *settings,
      void *input_data,
      enum rarch_core_type type);

void runloop_pause_checks(void);

float runloop_set_frame_limit(
      const struct retro_system_av_info *av_info,
      float fastforward_ratio);

float runloop_get_fastforward_ratio(
      settings_t *settings,
      struct retro_fastforwarding_override *fastmotion_override);

void runloop_set_video_swap_interval(
      bool vrr_runloop_enable,
      bool crt_switching_active,
      unsigned swap_interval_config,
      float audio_max_timing_skew,
      float video_refresh_rate,
      double input_fps);
unsigned runloop_get_video_swap_interval(
      unsigned swap_interval_config);

void runloop_task_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush);

void runloop_frame_time_free(void);

void runloop_fastmotion_override_free(void);

void runloop_audio_buffer_status_free(void);

bool secondary_core_ensure_exists(settings_t *settings);

void runloop_core_options_cb_free(void);

void runloop_log_counters(
      struct retro_perf_counter **counters, unsigned num);

void runloop_secondary_core_destroy(void);

void runloop_msg_queue_deinit(void);

void runloop_msg_queue_init(void);

void runloop_path_init_savefile(void);

void runloop_path_set_basename(const char *path);

void runloop_path_init_savefile(void);

void runloop_path_set_names(void);

uint32_t runloop_get_flags(void);

runloop_state_t *runloop_state_get_ptr(void);

RETRO_END_DECLS

#endif
