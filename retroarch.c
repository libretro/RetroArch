/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2015-2017 - Andrés Suárez
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

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#if defined(DEBUG) && defined(HAVE_DRMINGW)
#include "exchndl.h"
#endif
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <math.h>
#include <locale.h>

#include <boolean.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <retro_timers.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <audio/audio_mixer.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>
#include <lists/dir_list.h>
#include <net/net_http.h>

#include "runtime_file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <audio/audio_resampler.h>

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_animation.h"
#include "menu/menu_input.h"
#include "menu/widgets/menu_dialog.h"
#include "menu/widgets/menu_input_dialog.h"
#ifdef HAVE_MENU_WIDGETS
#include "menu/widgets/menu_widgets.h"
#endif
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos-new/cheevos.h"
#endif

#ifdef HAVE_DISCORD
#include "discord/discord.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
#include "network/httpserver/httpserver.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "autosave.h"
#include "command.h"
#include "config.features.h"
#include "content.h"
#include "core_type.h"
#include "core_info.h"
#include "dynamic.h"
#include "driver.h"
#include "input/input_driver.h"
#include "msg_hash.h"
#include "dirs.h"
#include "paths.h"
#include "file_path_special.h"
#include "ui/ui_companion_driver.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#include "audio/audio_driver.h"
#ifdef HAVE_THREADS
#include "../gfx/video_thread_wrapper.h"
#endif
#include "gfx/video_driver.h"
#include "camera/camera_driver.h"
#include "record/record_driver.h"
#include "location/location_driver.h"
#include "wifi/wifi_driver.h"
#include "led/led_driver.h"
#include "midi/midi_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#include "managers/core_option_manager.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"
#include "performance_counters.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#ifdef HAVE_RUNAHEAD
#include "runahead/run_ahead.h"
#endif

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, var ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while(0)

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500
#define HOLD_START_DELAY_SEC 2

#define QUIT_DELAY_USEC 3 * 1000000 /* 3 seconds */

#define DEBUG_INFO_FILENAME "debug_info.txt"

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_STATELESS,
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_NO_PATCH,
   RA_OPT_RECORDCONFIG,
   RA_OPT_SUBSYSTEM,
   RA_OPT_SIZE,
   RA_OPT_FEATURES,
   RA_OPT_VERSION,
   RA_OPT_EOF_EXIT,
   RA_OPT_LOG_FILE,
   RA_OPT_MAX_FRAMES,
   RA_OPT_MAX_FRAMES_SCREENSHOT,
   RA_OPT_MAX_FRAMES_SCREENSHOT_PATH
};

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

static jmp_buf error_sjlj_context;
static enum rarch_core_type current_core_type                   = CORE_TYPE_PLAIN;
static enum rarch_core_type explicit_current_core_type          = CORE_TYPE_PLAIN;
static char error_string[255]                                   = {0};
static char runtime_shader_preset[255]                          = {0};

#ifdef HAVE_THREAD_STORAGE
static sthread_tls_t rarch_tls;
const void *MAGIC_POINTER                                       = (void*)(uintptr_t)0x0DEFACED;
#endif

static retro_bits_t has_set_libretro_device;

static bool has_set_core                                        = false;
#ifdef HAVE_DISCORD
bool discord_is_inited                                         = false;
#endif
static bool rarch_is_inited                                     = false;
static bool rarch_error_on_init                                 = false;
static bool rarch_force_fullscreen                              = false;
static bool rarch_is_switching_display_mode                     = false;
static bool has_set_verbosity                                   = false;
static bool has_set_libretro                                    = false;
static bool has_set_libretro_directory                          = false;
static bool has_set_save_path                                   = false;
static bool has_set_state_path                                  = false;
static bool has_set_netplay_mode                                = false;
static bool has_set_netplay_ip_address                          = false;
static bool has_set_netplay_ip_port                             = false;
static bool has_set_netplay_stateless_mode                      = false;
static bool has_set_netplay_check_frames                        = false;
static bool has_set_ups_pref                                    = false;
static bool has_set_bps_pref                                    = false;
static bool has_set_ips_pref                                    = false;
static bool has_set_log_to_file                                 = false;

static bool rarch_is_sram_load_disabled                         = false;
static bool rarch_is_sram_save_disabled                         = false;
static bool rarch_use_sram                                      = false;
static bool rarch_ups_pref                                      = false;
static bool rarch_bps_pref                                      = false;
static bool rarch_ips_pref                                      = false;

static bool runloop_force_nonblock                              = false;
static bool runloop_paused                                      = false;
static bool runloop_idle                                        = false;
static bool runloop_slowmotion                                  = false;
bool runloop_fastmotion                                         = false;
static bool runloop_shutdown_initiated                          = false;
static bool runloop_core_shutdown_initiated                     = false;
static bool runloop_perfcnt_enable                              = false;
static bool runloop_overrides_active                            = false;
static bool runloop_remaps_core_active                          = false;
static bool runloop_remaps_game_active                          = false;
static bool runloop_remaps_content_dir_active                   = false;
static bool runloop_game_options_active                         = false;
static bool runloop_autosave                                    = false;
static rarch_system_info_t runloop_system;
static struct retro_frame_time_callback runloop_frame_time;
static retro_keyboard_event_t runloop_key_event                 = NULL;
static retro_keyboard_event_t runloop_frontend_key_event        = NULL;
static core_option_manager_t *runloop_core_options              = NULL;
#ifdef HAVE_THREADS
static slock_t *_runloop_msg_queue_lock                         = NULL;
#endif
static msg_queue_t *runloop_msg_queue                           = NULL;

static unsigned runloop_pending_windowed_scale                  = 0;
static unsigned runloop_max_frames                              = 0;
static bool runloop_max_frames_screenshot                       = false;
static char runloop_max_frames_screenshot_path[PATH_MAX_LENGTH] = {0};
static unsigned fastforward_after_frames                        = 0;

static retro_usec_t runloop_frame_time_last                     = 0;
static retro_time_t frame_limit_minimum_time                    = 0.0;
static retro_time_t frame_limit_last_time                       = 0.0;
static retro_time_t libretro_core_runtime_last                  = 0;
static retro_time_t libretro_core_runtime_usec                  = 0;

static char runtime_content_path[PATH_MAX_LENGTH]               = {0};
static char runtime_core_path[PATH_MAX_LENGTH]                  = {0};

static bool log_file_created                                    = false;
static char timestamped_log_file_name[64]                       = {0};

static bool log_file_override_active                            = false;
static char log_file_override_path[PATH_MAX_LENGTH]             = {0};

extern bool input_driver_flushing_input;

static char launch_arguments[4096];

struct bsv_state
{
   bool movie_start_recording;
   bool movie_start_playback;
   bool movie_playback;
   bool eof_exit;
   bool movie_end;

   /* Movie playback/recording support. */
   char movie_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];
};


struct bsv_movie
{
   intfstream_t *file;

   /* A ring buffer keeping track of positions
    * in the file for each frame. */
   size_t *frame_pos;
   size_t frame_mask;
   size_t frame_ptr;

   size_t min_file_pos;

   size_t state_size;
   uint8_t *state;

   bool playback;
   bool first_rewind;
   bool did_rewind;
};

#define BSV_MAGIC          0x42535631

#define MAGIC_INDEX        0
#define SERIALIZER_INDEX   1
#define CRC_INDEX          2
#define STATE_SIZE_INDEX   3

typedef struct bsv_movie bsv_movie_t;

static bsv_movie_t     *bsv_movie_state_handle = NULL;
static struct bsv_state bsv_movie_state;

static bool bsv_movie_init_playback(bsv_movie_t *handle, const char *path)
{
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for playback, path : \"%s\".\n", path);
      return false;
   }

   handle->file              = file;
   handle->playback          = true;

   intfstream_read(handle->file, header, sizeof(uint32_t) * 4);
   /* Compatibility with old implementation that
    * used incorrect documentation. */
   if (swap_if_little32(header[MAGIC_INDEX]) != BSV_MAGIC
         && swap_if_big32(header[MAGIC_INDEX]) != BSV_MAGIC)
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE));
      return false;
   }

   content_crc               = content_get_crc();

   if (content_crc != 0)
      if (swap_if_big32(header[CRC_INDEX]) != content_crc)
         RARCH_WARN("%s.\n", msg_hash_to_str(MSG_CRC32_CHECKSUM_MISMATCH));

   state_size = swap_if_big32(header[STATE_SIZE_INDEX]);

#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   if (state_size)
   {
      retro_ctx_size_info_t info;
      retro_ctx_serialize_info_t serial_info;
      uint8_t *buf       = (uint8_t*)malloc(state_size);

      if (!buf)
         return false;

      handle->state      = buf;
      handle->state_size = state_size;
      if (intfstream_read(handle->file,
               handle->state, state_size) != state_size)
      {
         RARCH_ERR("%s\n", msg_hash_to_str(MSG_COULD_NOT_READ_STATE_FROM_MOVIE));
         return false;
      }

      core_serialize_size( &info);

      if (info.size == state_size)
      {
         serial_info.data_const = handle->state;
         serial_info.size       = state_size;
         core_unserialize(&serial_info);
      }
      else
         RARCH_WARN("%s\n",
               msg_hash_to_str(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION));
   }

   handle->min_file_pos = sizeof(header) + state_size;

   return true;
}

static bool bsv_movie_init_record(bsv_movie_t *handle, const char *path)
{
   retro_ctx_size_info_t info;
   uint32_t state_size       = 0;
   uint32_t content_crc      = 0;
   uint32_t header[4]        = {0};
   intfstream_t *file        = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open BSV file for recording, path : \"%s\".\n", path);
      return false;
   }

   handle->file             = file;

   content_crc              = content_get_crc();

   /* This value is supposed to show up as
    * BSV1 in a HEX editor, big-endian. */
   header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   header[CRC_INDEX]        = swap_if_big32(content_crc);

   core_serialize_size(&info);

   state_size               = (unsigned)info.size;

   header[STATE_SIZE_INDEX] = swap_if_big32(state_size);
#if 0
   RARCH_ERR("----- debug %u -----\n", header[0]);
   RARCH_ERR("----- debug %u -----\n", header[1]);
   RARCH_ERR("----- debug %u -----\n", header[2]);
   RARCH_ERR("----- debug %u -----\n", header[3]);
#endif

   intfstream_write(handle->file, header, 4 * sizeof(uint32_t));

   handle->min_file_pos     = sizeof(header) + state_size;
   handle->state_size       = state_size;

   if (state_size)
   {
      retro_ctx_serialize_info_t serial_info;

      handle->state = (uint8_t*)malloc(state_size);
      if (!handle->state)
         return false;

      serial_info.data = handle->state;
      serial_info.size = state_size;

      core_serialize(&serial_info);

      intfstream_write(handle->file,
            handle->state, state_size);
   }

   return true;
}

static void bsv_movie_free(bsv_movie_t *handle)
{
   if (!handle)
      return;

   intfstream_close(handle->file);
   free(handle->file);

   free(handle->state);
   free(handle->frame_pos);
   free(handle);
}

static bsv_movie_t *bsv_movie_init_internal(const char *path,
      enum rarch_movie_type type)
{
   size_t *frame_pos   = NULL;
   bsv_movie_t *handle = (bsv_movie_t*)calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   if (type == RARCH_MOVIE_PLAYBACK)
   {
      if (!bsv_movie_init_playback(handle, path))
         goto error;
   }
   else if (!bsv_movie_init_record(handle, path))
      goto error;

   /* Just pick something really large
    * ~1 million frames rewind should do the trick. */
   if (!(frame_pos = (size_t*)calloc((1 << 20), sizeof(size_t))))
      goto error;

   handle->frame_pos       = frame_pos;

   handle->frame_pos[0]    = handle->min_file_pos;
   handle->frame_mask      = (1 << 20) - 1;

   return handle;

error:
   bsv_movie_free(handle);
   return NULL;
}

void bsv_movie_frame_rewind(void)
{
   bsv_movie_t *handle = bsv_movie_state_handle;

   if (!handle)
      return;

   handle->did_rewind = true;

   if (     (handle->frame_ptr <= 1)
         && (handle->frame_pos[0] == handle->min_file_pos))
   {
      /* If we're at the beginning... */
      handle->frame_ptr = 0;
      intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
   else
   {
      /* First time rewind is performed, the old frame is simply replayed.
       * However, playing back that frame caused us to read data, and push
       * data to the ring buffer.
       *
       * Sucessively rewinding frames, we need to rewind past the read data,
       * plus another. */
      handle->frame_ptr = (handle->frame_ptr -
            (handle->first_rewind ? 1 : 2)) & handle->frame_mask;
      intfstream_seek(handle->file,
            (int)handle->frame_pos[handle->frame_ptr], SEEK_SET);
   }

   if (intfstream_tell(handle->file) <= (long)handle->min_file_pos)
   {
      /* We rewound past the beginning. */

      if (!handle->playback)
      {
         retro_ctx_serialize_info_t serial_info;

         /* If recording, we simply reset
          * the starting point. Nice and easy. */

         intfstream_seek(handle->file, 4 * sizeof(uint32_t), SEEK_SET);

         serial_info.data = handle->state;
         serial_info.size = handle->state_size;

         core_serialize(&serial_info);

         intfstream_write(handle->file, handle->state, handle->state_size);
      }
      else
         intfstream_seek(handle->file, (int)handle->min_file_pos, SEEK_SET);
   }
}

static bool bsv_movie_init_handle(const char *path,
      enum rarch_movie_type type)
{
   bsv_movie_t *state     = bsv_movie_init_internal(path, type);
   if (!state)
      return false;

   bsv_movie_state_handle = state;
   return true;
}

bool bsv_movie_init(void)
{
   bool set_granularity = false;

   if (bsv_movie_state.movie_start_playback)
   {
      if (!bsv_movie_init_handle(bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_PLAYBACK))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               bsv_movie_state.movie_start_path);
         return false;
      }

      bsv_movie_state.movie_playback = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));

      set_granularity = true;
   }
   else if (bsv_movie_state.movie_start_recording)
   {
      if (!bsv_movie_init_handle(bsv_movie_state.movie_start_path,
                  RARCH_MOVIE_RECORD))
      {
         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_ERR("%s.\n",
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return false;
      }

      {
         char msg[8192];
         snprintf(msg, sizeof(msg),
               "%s \"%s\".",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               bsv_movie_state.movie_start_path);

         runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
               bsv_movie_state.movie_start_path);
      }

      set_granularity = true;
   }

   if (set_granularity)
   {
      settings_t *settings = config_get_ptr();
      configuration_set_uint(settings,
            settings->uints.rewind_granularity, 1);
   }

   return true;
}

#define bsv_movie_is_playback_on() (bsv_movie_state_handle && bsv_movie_state.movie_playback)
#define bsv_movie_is_playback_off() (bsv_movie_state_handle && !bsv_movie_state.movie_playback)

bool bsv_movie_get_input(int16_t *bsv_data)
{
   if (!bsv_movie_is_playback_on())
      return false;
   if (intfstream_read(bsv_movie_state_handle->file, bsv_data, 1) != 1)
   {
      bsv_movie_state.movie_end = true;
      return false;
   }

   *bsv_data = swap_if_big16(*bsv_data);

   return true;
}

void bsv_movie_set_input(int16_t *bsv_data)
{
   if (bsv_data && bsv_movie_is_playback_off())
   {
      *bsv_data = swap_if_big16(*bsv_data);
      intfstream_write(bsv_movie_state_handle->file, bsv_data, 1);
   }
}

bool bsv_movie_ctl(enum bsv_ctl_state state, void *data)
{
   switch (state)
   {
      case BSV_MOVIE_CTL_IS_INITED:
         return (bsv_movie_state_handle != NULL);
      case BSV_MOVIE_CTL_NONE:
      default:
         return false;
   }

   return true;
}

void bsv_movie_set_path(const char *path)
{
   strlcpy(bsv_movie_state.movie_path,
         path, sizeof(bsv_movie_state.movie_path));
}

void bsv_movie_deinit(void)
{
   if (!bsv_movie_state_handle)
      return;

   bsv_movie_free(bsv_movie_state_handle);
   bsv_movie_state_handle = NULL;
}

static bool runloop_check_movie_init(void)
{
   char msg[16384], path[8192];
   settings_t *settings       = config_get_ptr();

   msg[0] = path[0]           = '\0';

   configuration_set_uint(settings, settings->uints.rewind_granularity, 1);

   if (settings->ints.state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            bsv_movie_state.movie_path,
            settings->ints.state_slot);
   else
      strlcpy(path, bsv_movie_state.movie_path, sizeof(path));

   strlcat(path,
         file_path_str(FILE_PATH_BSV_EXTENSION),
         sizeof(path));

   snprintf(msg, sizeof(msg), "%s \"%s\".",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   bsv_movie_init_handle(path, RARCH_MOVIE_RECORD);

   if (!bsv_movie_state_handle)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
      return false;
   }

   runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
         path);

   return true;
}

bool bsv_movie_check(void)
{
   if (!bsv_movie_state_handle)
      return runloop_check_movie_init();

   if (bsv_movie_state.movie_playback)
   {
      /* Checks if movie is being played back. */
      if (!bsv_movie_state.movie_end)
         return false;
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED), 2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

      command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

      bsv_movie_state.movie_end      = false;
      bsv_movie_state.movie_playback = false;

      return true;
   }

   /* Checks if movie is being recorded. */
   if (!bsv_movie_state_handle)
      return false;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED), 2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

   command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

   return true;
}

/**
 * find_driver_nonempty:
 * @label              : string of driver type to be found.
 * @i                  : index of driver.
 * @str                : identifier name of the found driver
 *                       gets written to this string.
 * @len                : size of @str.
 *
 * Find driver based on @label.
 *
 * Returns: NULL if no driver based on @label found, otherwise
 * pointer to driver.
 **/
static const void *find_driver_nonempty(const char *label, int i,
      char *s, size_t len)
{
   const void *drv = NULL;

   if (string_is_equal(label, "camera_driver"))
   {
      drv = camera_driver_find_handle(i);
      if (drv)
         strlcpy(s, camera_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "location_driver"))
   {
      drv = location_driver_find_handle(i);
      if (drv)
         strlcpy(s, location_driver_find_ident(i), len);
   }
#ifdef HAVE_MENU
   else if (string_is_equal(label, "menu_driver"))
   {
      drv = menu_driver_find_handle(i);
      if (drv)
         strlcpy(s, menu_driver_find_ident(i), len);
   }
#endif
   else if (string_is_equal(label, "input_driver"))
   {
      drv = input_driver_find_handle(i);
      if (drv)
         strlcpy(s, input_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "input_joypad_driver"))
   {
      drv = joypad_driver_find_handle(i);
      if (drv)
         strlcpy(s, joypad_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "video_driver"))
   {
      drv = video_driver_find_handle(i);
      if (drv)
         strlcpy(s, video_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "audio_driver"))
   {
      drv = audio_driver_find_handle(i);
      if (drv)
         strlcpy(s, audio_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "record_driver"))
   {
      drv = record_driver_find_handle(i);
      if (drv)
         strlcpy(s, record_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "midi_driver"))
   {
      drv = midi_driver_find_handle(i);
      if (drv)
         strlcpy(s, midi_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "audio_resampler_driver"))
   {
      drv = audio_resampler_driver_find_handle(i);
      if (drv)
         strlcpy(s, audio_resampler_driver_find_ident(i), len);
   }
   else if (string_is_equal(label, "wifi_driver"))
   {
      drv = wifi_driver_find_handle(i);
      if (drv)
         strlcpy(s, wifi_driver_find_ident(i), len);
   }

   return drv;
}

/**
 * driver_find_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
static int driver_find_index(const char * label, const char *drv)
{
   unsigned i;
   char str[256];

   str[0] = '\0';

   for (i = 0;
         find_driver_nonempty(label, i, str, sizeof(str)) != NULL; i++)
   {
      if (string_is_empty(str))
         break;
      if (string_is_equal_noncase(drv, str))
         return i;
   }

   return -1;
}

static bool driver_find_first(const char *label, char *s, size_t len)
{
   find_driver_nonempty(label, 0, s, len);
   return true;
}

/**
 * driver_find_last:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find last driver in driver array.
 **/
static bool driver_find_last(const char *label, char *s, size_t len)
{
   unsigned i;

   for (i = 0;
         find_driver_nonempty(label, i, s, len) != NULL; i++)
   {}

   if (i)
      find_driver_nonempty(label, i-1, s, len);
   else
      driver_find_first(label, s, len);

   return true;
}

/**
 * driver_find_prev:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find previous driver in driver array.
 **/
static bool driver_find_prev(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i > 0)
   {
      find_driver_nonempty(label, i - 1, s, len);
      return true;
   }

   RARCH_WARN(
         "Couldn't find any previous driver (current one: \"%s\").\n", s);
   return false;
}

/**
 * driver_find_next:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find next driver in driver array.
 **/
bool driver_find_next(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i >= 0 && string_is_not_equal(s, "null"))
   {
      find_driver_nonempty(label, i + 1, s, len);
      return true;
   }

   RARCH_WARN("%s (current one: \"%s\").\n",
         msg_hash_to_str(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER),
         s);
   return false;
}

static void driver_adjust_system_rates(void)
{
   audio_driver_monitor_adjust_system_rates();
   video_driver_monitor_adjust_system_rates();

   if (!video_driver_get_ptr(false))
      return;

   if (runloop_force_nonblock)
      command_event(CMD_EVENT_VIDEO_SET_NONBLOCKING_STATE, NULL);
   else
      driver_set_nonblock_state();
}

/**
 * driver_set_nonblock_state:
 *
 * Sets audio and video drivers to nonblock state (if enabled).
 *
 * If nonblock state is false, sets
 * blocking state for both audio and video drivers instead.
 **/
void driver_set_nonblock_state(void)
{
   bool                 enable = input_driver_is_nonblock_state();

   /* Only apply non-block-state for video if we're using vsync. */
   if (video_driver_is_active() && video_driver_get_ptr(false))
   {
      settings_t *settings = config_get_ptr();
      bool video_nonblock  = enable;

      if (!settings->bools.video_vsync || runloop_force_nonblock)
         video_nonblock = true;
      video_driver_set_nonblock_state(video_nonblock);
   }

   audio_driver_set_nonblocking_state(enable);
}

/**
 * driver_update_system_av_info:
 * @data               : pointer to new A/V info
 *
 * Update the system Audio/Video information.
 * Will reinitialize audio/video drivers.
 * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool driver_update_system_av_info(const struct retro_system_av_info *info)
{
   struct retro_system_av_info *av_info    = video_viewport_get_system_av_info();
   settings_t *settings = config_get_ptr();

   memcpy(av_info, info, sizeof(*av_info));
   command_event(CMD_EVENT_REINIT, NULL);

   /* Cannot continue recording with different parameters.
    * Take the easiest route out and just restart the recording. */
   if (recording_driver_get_data_ptr())
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT),
            2, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);
      command_event(CMD_EVENT_RECORD_INIT, NULL);
   }

   /* Hide mouse cursor in fullscreen after
    * a RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO call. */
   if (settings->bools.video_fullscreen)
      video_driver_hide_mouse();

   return true;
}

/**
 * drivers_init:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
void drivers_init(int flags)
{
   bool video_is_threaded = false;
   settings_t *settings = config_get_ptr();

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   menu_driver_ctl(RARCH_MENU_CTL_SET_OWN_DRIVER, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates();

   /* Initialize video driver */
   if (flags & DRIVER_VIDEO_MASK)
   {
      struct retro_hw_render_callback *hwr =
         video_driver_get_hw_context();

      video_driver_monitor_reset();
      video_driver_init(&video_is_threaded);

      if (!video_driver_is_video_cache_context_ack()
            && hwr->context_reset)
         hwr->context_reset();
      video_driver_unset_video_cache_context_ack();

      runloop_frame_time_last        = 0;
   }

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init();
      audio_driver_new_devices_list();
   }

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (camera_driver_ctl(RARCH_CAMERA_CTL_IS_ACTIVE, NULL))
         camera_driver_ctl(RARCH_CAMERA_CTL_INIT, NULL);
   }

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (location_driver_ctl(RARCH_LOCATION_CTL_IS_ACTIVE, NULL))
         init_location();
   }

   core_info_init_current_core();

#ifdef HAVE_MENU
#ifdef HAVE_MENU_WIDGETS
   if (settings->bools.menu_enable_widgets
      && video_driver_has_widgets())
   {
      menu_widgets_init(video_is_threaded);
      menu_widgets_context_reset(video_is_threaded);
   }
#endif

   if (flags & DRIVER_VIDEO_MASK)
   {
      /* Initialize menu driver */
      if (flags & DRIVER_MENU_MASK)
         menu_driver_init(video_is_threaded);
   }
#else
   /* Qt uses core info, even if the menu is disabled */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
   {
      /* Keep non-throttled state as good as possible. */
      if (input_driver_is_nonblock_state())
         driver_set_nonblock_state();
   }

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init();

   /* Initialize MIDI  driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init();
}

/**
 * uninit_drivers:
 * @flags              : Bitmask of drivers to deinitialize.
 *
 * Deinitializes drivers.
 *
 *
 * @flags determines which drivers get deinitialized.
 **/

/**
 * Driver ownership - set this to true if the platform in question needs to 'own'
 * the respective handle and therefore skip regular RetroArch
 * driver teardown/reiniting procedure.
 *
 * If  to true, the 'free' function will get skipped. It is
 * then up to the driver implementation to properly handle
 * 'reiniting' inside the 'init' function and make sure it
 * returns the existing handle instead of allocating and
 * returning a pointer to a new handle.
 *
 * Typically, if a driver intends to make use of this, it should
 * set this to true at the end of its 'init' function.
 **/
void driver_uninit(int flags)
{
   core_info_deinit_list();
   core_info_free_current_core();

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
#if defined(HAVE_MENU_WIDGETS)
      /* This absolutely has to be done before video_driver_free()
       * is called/completes, otherwise certain menu drivers
       * (e.g. Vulkan) will segfault */
      menu_widgets_context_destroy();
      menu_widgets_free();
#endif
      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
      menu_driver_free();
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      location_driver_ctl(RARCH_LOCATION_CTL_DEINIT, NULL);

   if ((flags & DRIVER_CAMERA_MASK))
      camera_driver_ctl(RARCH_CAMERA_CTL_DEINIT, NULL);

   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
      video_driver_free();

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit();

   if ((flags & DRIVER_VIDEO_MASK))
      video_driver_destroy_data();

   if ((flags & DRIVER_INPUT_MASK))
      input_driver_destroy_data();

   if ((flags & DRIVER_AUDIO_MASK))
      audio_driver_destroy_data();

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   switch (state)
   {
      case RARCH_DRIVER_CTL_DEINIT:
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         /* Tear down menu widgets no matter what
          * in case the handle is lost in the threaded
          * video driver in the meantime
          * (breaking video_driver_has_widgets) */
         menu_widgets_context_destroy();
         menu_widgets_free();

#endif
         video_driver_destroy();
         audio_driver_destroy();
         input_driver_destroy();
#ifdef HAVE_MENU
         menu_driver_destroy();
#endif
         location_driver_ctl(RARCH_LOCATION_CTL_DESTROY, NULL);
         camera_driver_ctl(RARCH_CAMERA_CTL_DESTROY, NULL);
         wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);
         core_uninit_libretro_callbacks();
         break;
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz = (float*)data;
            video_monitor_set_refresh_rate(*hz);
            audio_driver_monitor_set_rate();
            driver_adjust_system_rates();
         }
         break;
      case RARCH_DRIVER_CTL_UPDATE_SYSTEM_AV_INFO:
         {
            const struct retro_system_av_info **info = (const struct retro_system_av_info**)data;
            if (info)
               return driver_update_system_av_info(*info);
         }
         return false;
      case RARCH_DRIVER_CTL_FIND_FIRST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_first(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_LAST:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_last(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_PREV:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_prev(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_NEXT:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            return driver_find_next(drv->label, drv->s, drv->len);
         }
      case RARCH_DRIVER_CTL_FIND_INDEX:
         {
            driver_ctx_info_t *drv = (driver_ctx_info_t*)data;
            if (!drv)
               return false;
            drv->len = driver_find_index(drv->label, drv->s);
         }
         break;
      case RARCH_DRIVER_CTL_NONE:
      default:
         break;
   }

   return true;
}

void rarch_core_runtime_tick(void)
{
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (av_info && av_info->timing.fps)
   {
      retro_time_t frame_time = (1.0 / av_info->timing.fps) * 1000000;

      /* Account for slow motion */
      if (runloop_slowmotion)
      {
         settings_t *settings = config_get_ptr();
         if (settings)
            frame_time = (retro_time_t)((double)frame_time * settings->floats.slowmotion_ratio);
      }
      /* Account for fast forward */
      else if (runloop_fastmotion)
      {
         /* Doing it this way means we miss the first frame after
          * turning fast forward on, but it saves the overhead of
          * having to do:
          *    retro_time_t current_usec = cpu_features_get_time_usec();
          *    libretro_core_runtime_last = current_usec;
          * every frame when fast forward is off. */
         retro_time_t current_usec = cpu_features_get_time_usec();

         if (current_usec - libretro_core_runtime_last < frame_time)
            frame_time = current_usec - libretro_core_runtime_last;

         libretro_core_runtime_last = current_usec;
      }

      libretro_core_runtime_usec += frame_time;
   }
}

static void update_runtime_log(bool log_per_core)
{
   /* Initialise runtime log file */
   runtime_log_t *runtime_log = runtime_log_init(runtime_content_path, runtime_core_path, log_per_core);

   if (!runtime_log)
      return;

   /* Add additional runtime */
   runtime_log_add_runtime_usec(runtime_log, libretro_core_runtime_usec);

   /* Update 'last played' entry */
   runtime_log_set_last_played_now(runtime_log);

   /* Save runtime log file */
   runtime_log_save(runtime_log);

   /* Clean up */
   free(runtime_log);
}

#ifdef HAVE_THREADS
void runloop_msg_queue_lock(void)
{
   slock_lock(_runloop_msg_queue_lock);
}

void runloop_msg_queue_unlock(void)
{
   slock_unlock(_runloop_msg_queue_lock);
}
#endif

static void retroarch_msg_queue_deinit(void)
{
#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif

   if (!runloop_msg_queue)
      return;

   msg_queue_free(runloop_msg_queue);

#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
   slock_free(_runloop_msg_queue_lock);
   _runloop_msg_queue_lock = NULL;
#endif

   runloop_msg_queue = NULL;
}

static void retroarch_msg_queue_init(void)
{
   retroarch_msg_queue_deinit();
   runloop_msg_queue = msg_queue_new(8);

#ifdef HAVE_THREADS
   _runloop_msg_queue_lock = slock_new();
#endif
}

static void retroarch_override_setting_free_state(void)
{
   unsigned i;
   for (i = 0; i < RARCH_OVERRIDE_SETTING_LAST; i++)
   {
      if (i == RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE)
      {
         unsigned j;
         for (j = 0; j < MAX_USERS; j++)
            retroarch_override_setting_unset((enum rarch_override_setting)(i), &j);
      }
      else
         retroarch_override_setting_unset((enum rarch_override_setting)(i), NULL);
   }
}

static void global_free(void)
{
   global_t *global = NULL;

   content_deinit();

   path_deinit_subsystem();
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);
   command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

   rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
   rarch_is_sram_load_disabled           = false;
   rarch_is_sram_save_disabled           = false;
   rarch_use_sram                        = false;
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_PATCH_BLOCKED, NULL);
   runloop_overrides_active              = false;
   runloop_remaps_core_active            = false;
   runloop_remaps_game_active            = false;
   runloop_remaps_content_dir_active     = false;

   core_unset_input_descriptors();

   global = global_get_ptr();
   path_clear_all();
   dir_clear_all();
   if (global)
   {
      if (!string_is_empty(global->name.remapfile))
         free(global->name.remapfile);
      memset(global, 0, sizeof(struct global));
   }
   retroarch_override_setting_free_state();
}

static void retroarch_print_features(void)
{
   frontend_driver_attach_console();
   puts("");
   puts("Features:");

   _PSUPP(SUPPORTS_LIBRETRODB,      "LibretroDB",      "LibretroDB support");
   _PSUPP(SUPPORTS_COMMAND,         "Command",         "Command interface support");
   _PSUPP(SUPPORTS_NETWORK_COMMAND, "Network Command", "Network Command interface "
         "support");

   _PSUPP(SUPPORTS_SDL,             "SDL",             "SDL input/audio/video drivers");
   _PSUPP(SUPPORTS_SDL2,            "SDL2",            "SDL2 input/audio/video drivers");
   _PSUPP(SUPPORTS_X11,             "X11",             "X11 input/video drivers");
   _PSUPP(SUPPORTS_WAYLAND,         "wayland",         "Wayland input/video drivers");
   _PSUPP(SUPPORTS_THREAD,          "Threads",         "Threading support");

   _PSUPP(SUPPORTS_VULKAN,          "Vulkan",          "Vulkan video driver");
   _PSUPP(SUPPORTS_METAL,           "Metal",           "Metal video driver");
   _PSUPP(SUPPORTS_OPENGL,          "OpenGL",          "OpenGL   video driver support");
   _PSUPP(SUPPORTS_OPENGLES,        "OpenGL ES",       "OpenGLES video driver support");
   _PSUPP(SUPPORTS_XVIDEO,          "XVideo",          "Video driver");
   _PSUPP(SUPPORTS_UDEV,            "UDEV",            "UDEV/EVDEV input driver support");
   _PSUPP(SUPPORTS_EGL,             "EGL",             "Video context driver");
   _PSUPP(SUPPORTS_KMS,             "KMS",             "Video context driver");
   _PSUPP(SUPPORTS_VG,              "OpenVG",          "Video context driver");

   _PSUPP(SUPPORTS_COREAUDIO,       "CoreAudio",       "Audio driver");
   _PSUPP(SUPPORTS_COREAUDIO3,      "CoreAudioV3",     "Audio driver");
   _PSUPP(SUPPORTS_ALSA,            "ALSA",            "Audio driver");
   _PSUPP(SUPPORTS_OSS,             "OSS",             "Audio driver");
   _PSUPP(SUPPORTS_JACK,            "Jack",            "Audio driver");
   _PSUPP(SUPPORTS_RSOUND,          "RSound",          "Audio driver");
   _PSUPP(SUPPORTS_ROAR,            "RoarAudio",       "Audio driver");
   _PSUPP(SUPPORTS_PULSE,           "PulseAudio",      "Audio driver");
   _PSUPP(SUPPORTS_DSOUND,          "DirectSound",     "Audio driver");
   _PSUPP(SUPPORTS_WASAPI,          "WASAPI",     "Audio driver");
   _PSUPP(SUPPORTS_XAUDIO,          "XAudio2",         "Audio driver");
   _PSUPP(SUPPORTS_AL,              "OpenAL",          "Audio driver");
   _PSUPP(SUPPORTS_SL,              "OpenSL",          "Audio driver");

   _PSUPP(SUPPORTS_7ZIP,            "7zip",            "7zip extraction support");
   _PSUPP(SUPPORTS_ZLIB,            "zlib",            ".zip extraction support");

   _PSUPP(SUPPORTS_DYLIB,           "External",        "External filter and plugin support");

   _PSUPP(SUPPORTS_CG,              "Cg",              "Fragment/vertex shader driver");
   _PSUPP(SUPPORTS_GLSL,            "GLSL",            "Fragment/vertex shader driver");
   _PSUPP(SUPPORTS_HLSL,            "HLSL",            "Fragment/vertex shader driver");

   _PSUPP(SUPPORTS_SDL_IMAGE,       "SDL_image",       "SDL_image image loading");
   _PSUPP(SUPPORTS_RPNG,            "rpng",            "PNG image loading/encoding");
   _PSUPP(SUPPORTS_RJPEG,            "rjpeg",           "JPEG image loading");
   _PSUPP(SUPPORTS_DYNAMIC,         "Dynamic",         "Dynamic run-time loading of "
                                              "libretro library");
   _PSUPP(SUPPORTS_FFMPEG,          "FFmpeg",          "On-the-fly recording of gameplay "
                                              "with libavcodec");

   _PSUPP(SUPPORTS_FREETYPE,        "FreeType",        "TTF font rendering driver");
   _PSUPP(SUPPORTS_CORETEXT,        "CoreText",        "TTF font rendering driver "
                                              "(for OSX and/or iOS)");
   _PSUPP(SUPPORTS_NETPLAY,         "Netplay",         "Peer-to-peer netplay");
   _PSUPP(SUPPORTS_PYTHON,          "Python",          "Script support in shaders");

   _PSUPP(SUPPORTS_LIBUSB,          "Libusb",          "Libusb support");

   _PSUPP(SUPPORTS_COCOA,           "Cocoa",           "Cocoa UI companion support "
                                              "(for OSX and/or iOS)");

   _PSUPP(SUPPORTS_QT,              "Qt",              "Qt UI companion support");
   _PSUPP(SUPPORTS_V4L2,            "Video4Linux2",    "Camera driver");
}

static void retroarch_print_version(void)
{
   char str[255];
   frontend_driver_attach_console();
   str[0] = '\0';

   fprintf(stderr, "%s: %s -- v%s",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND),
         PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   printf(" -- %s --\n", retroarch_git_version);
#endif
   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   fprintf(stdout, "%s", str);
   fprintf(stdout, "Built: %s\n", __DATE__);
}

/**
 * retroarch_print_help:
 *
 * Prints help message explaining the program's commandline switches.
 **/
static void retroarch_print_help(const char *arg0)
{
   frontend_driver_attach_console();
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   puts("  -h, --help            Show this help message.");
   puts("  -v, --verbose         Verbose logging.");
   puts("      --log-file FILE   Log messages to FILE.");
   puts("      --version         Show version.");
   puts("      --features        Prints available features compiled into "
         "program.");
#ifdef HAVE_MENU
   puts("      --menu            Do not require content or libretro core to "
         "be loaded,\n"
        "                        starts directly in menu. If no arguments "
        "are passed to\n"
        "                        the program, it is equivalent to using "
        "--menu as only argument.");
#endif
   puts("  -s, --save=PATH       Path for save files (*.srm).");
   puts("  -S, --savestate=PATH  Path for the save state files (*.state).");
   puts("  -f, --fullscreen      Start the program in fullscreen regardless "
         "of config settings.");
   puts("  -c, --config=FILE     Path for config file."
#ifdef _WIN32
         "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe."
         "\n\t\tIf a default config is not found, the program will attempt to"
         "create one."
#else
         "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/"
         "retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\t"
         "and $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, "
         "the program will attempt to create one based on the \n\t\t"
         "skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg). \n"
#endif
         );
   puts("      --appendconfig=FILE\n"
        "                        Extra config files are loaded in, "
        "and take priority over\n"
        "                        config selected in -c (or default). "
        "Multiple configs are\n"
        "                        delimited by '|'.");
#ifdef HAVE_DYNAMIC
   puts("  -L, --libretro=FILE   Path to libretro implementation. "
         "Overrides any config setting.");
#endif
   puts("      --subsystem=NAME  Use a subsystem of the libretro core. "
         "Multiple content\n"
        "                        files are loaded as multiple arguments. "
        "If a content\n"
        "                        file is skipped, use a blank (\"\") "
        "command line argument.\n"
        "                        Content must be loaded in an order "
        "which depends on the\n"
        "                        particular subsystem used. See verbose "
        "log output to learn\n"
        "                        how a particular subsystem wants content "
        "to be loaded.\n");

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected "
          "to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT "
          "(1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of "
          "the device (1 to %d).\n", MAX_USERS);
   puts("                        Format is PORT:ID, where ID is a number "
         "corresponding to the particular device.");

   puts("  -P, --bsvplay=FILE    Playback a BSV movie file.");
   puts("  -R, --bsvrecord=FILE  Start recording a BSV movie file from "
         "the beginning.");
   puts("      --eof-exit        Exit upon reaching the end of the "
         "BSV movie file.");
   puts("  -M, --sram-mode=MODE  SRAM handling mode. MODE can be "
         "'noload-nosave',\n"
        "                        'noload-save', 'load-nosave' or "
        "'load-save'.\n"
        "                        Note: 'noload-save' implies that "
        "save files *WILL BE OVERWRITTEN*.");

#ifdef HAVE_NETWORKING
   puts("  -H, --host            Host netplay as user 1.");
   puts("  -C, --connect=HOST    Connect to netplay server as user 2.");
   puts("      --port=PORT       Port used to netplay. Default is 55435.");
   puts("      --stateless       Use \"stateless\" mode for netplay");
   puts("                        (requires a very fast network).");
   puts("      --check-frames=NUMBER\n"
        "                        Check frames when using netplay.");
#if defined(HAVE_NETWORK_CMD)
   puts("      --command         Sends a command over UDP to an already "
         "running program process.");
   puts("      Available commands are listed if command is invalid.");
#endif

#endif
   puts("      --nick=NICK       Picks a username (for use with netplay). "
         "Not mandatory.");

   puts("  -r, --record=FILE     Path to record video file.\n        "
         "Using .mkv extension is recommended.");
   puts("      --recordconfig    Path to settings used during recording.");
   puts("      --size=WIDTHxHEIGHT\n"
        "                        Overrides output video size when recording.");
   puts("  -U, --ups=FILE        Specifies path for UPS patch that will be "
         "applied to content.");
   puts("      --bps=FILE        Specifies path for BPS patch that will be "
         "applied to content.");
   puts("      --ips=FILE        Specifies path for IPS patch that will be "
         "applied to content.");
   puts("      --no-patch        Disables all forms of content patching.");
   puts("  -D, --detach          Detach program from the running console. "
         "Not relevant for all platforms.");
   puts("      --max-frames=NUMBER\n"
        "                        Runs for the specified number of frames, "
        "then exits.");
   puts("      --max-frames-ss\n"
        "                        Takes a screenshot at the end of max-frames.");
   puts("      --max-frames-ss-path=FILE\n"
        "                        Path to save the screenshot to at the end of max-frames.\n");
}

#define FFMPEG_RECORD_ARG "r:"

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETWORKING
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#define BSV_MOVIE_ARG "P:R:M:"

/**
 * retroarch_parse_input_and_config:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to program and loads the config file,
 * with command line options overriding the config file.
 *
 **/
static void retroarch_parse_input_and_config(int argc, char *argv[])
{
   static bool first_run = true;
   const char *optstring = NULL;
   bool explicit_menu    = false;
   unsigned i;
   global_t  *global     = global_get_ptr();

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro",           1, NULL, 'L' },
#endif
      { "menu",               0, NULL, RA_OPT_MENU },
      { "help",               0, NULL, 'h' },
      { "save",               1, NULL, 's' },
      { "fullscreen",         0, NULL, 'f' },
      { "record",             1, NULL, 'r' },
      { "recordconfig",       1, NULL, RA_OPT_RECORDCONFIG },
      { "size",               1, NULL, RA_OPT_SIZE },
      { "verbose",            0, NULL, 'v' },
      { "config",             1, NULL, 'c' },
      { "appendconfig",       1, NULL, RA_OPT_APPENDCONFIG },
      { "nodevice",           1, NULL, 'N' },
      { "dualanalog",         1, NULL, 'A' },
      { "device",             1, NULL, 'd' },
      { "savestate",          1, NULL, 'S' },
      { "bsvplay",            1, NULL, 'P' },
      { "bsvrecord",          1, NULL, 'R' },
      { "sram-mode",          1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",               0, NULL, 'H' },
      { "connect",            1, NULL, 'C' },
      { "stateless",          0, NULL, RA_OPT_STATELESS },
      { "check-frames",       1, NULL, RA_OPT_CHECK_FRAMES },
      { "port",               1, NULL, RA_OPT_PORT },
#if defined(HAVE_NETWORK_CMD)
      { "command",            1, NULL, RA_OPT_COMMAND },
#endif
#endif
      { "nick",               1, NULL, RA_OPT_NICK },
      { "ups",                1, NULL, 'U' },
      { "bps",                1, NULL, RA_OPT_BPS },
      { "ips",                1, NULL, RA_OPT_IPS },
      { "no-patch",           0, NULL, RA_OPT_NO_PATCH },
      { "detach",             0, NULL, 'D' },
      { "features",           0, NULL, RA_OPT_FEATURES },
      { "subsystem",          1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",         1, NULL, RA_OPT_MAX_FRAMES },
      { "max-frames-ss",      0, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT },
      { "max-frames-ss-path", 1, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT_PATH },
      { "eof-exit",           0, NULL, RA_OPT_EOF_EXIT },
      { "version",            0, NULL, RA_OPT_VERSION },
      { "log-file",           1, NULL, RA_OPT_LOG_FILE },
      { NULL, 0, NULL, 0 }
   };

   if (first_run)
   {
      /* Copy the args into a buffer so launch arguments can be reused */
      for (i = 0; i < (unsigned)argc; i++)
      {
         strlcat(launch_arguments, argv[i], sizeof(launch_arguments));
         strlcat(launch_arguments, " ", sizeof(launch_arguments));
      }
      string_trim_whitespace_left(launch_arguments);
      string_trim_whitespace_right(launch_arguments);

      first_run = false;
   }

   /* Handling the core type is finicky. Based on the arguments we pass in,
    * we handle it differently.
    * Some current cases which track desired behavior and how it is supposed to work:
    *
    * Dynamically linked RA:
    * ./retroarch                            -> CORE_TYPE_DUMMY
    * ./retroarch -v                         -> CORE_TYPE_DUMMY + verbose
    * ./retroarch --menu                     -> CORE_TYPE_DUMMY
    * ./retroarch --menu -v                  -> CORE_TYPE_DUMMY + verbose
    * ./retroarch -L contentless-core        -> CORE_TYPE_PLAIN
    * ./retroarch -L content-core            -> CORE_TYPE_PLAIN + FAIL (This currently crashes)
    * ./retroarch [-L content-core] ROM      -> CORE_TYPE_PLAIN
    * ./retroarch <-L or ROM> --menu         -> FAIL
    *
    * The heuristic here seems to be that if we use the -L CLI option or
    * optind < argc at the end we should set CORE_TYPE_PLAIN.
    * To handle --menu, we should ensure that CORE_TYPE_DUMMY is still set
    * otherwise, fail early, since the CLI options are non-sensical.
    * We could also simply ignore --menu in this case to be more friendly with
    * bogus arguments.
    */

   if (!has_set_core)
      retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   rarch_ctl(RARCH_CTL_USERNAME_UNSET, NULL);
   rarch_ctl(RARCH_CTL_UNSET_UPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_IPS_PREF, NULL);
   rarch_ctl(RARCH_CTL_UNSET_BPS_PREF, NULL);
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';

   rarch_ctl(RARCH_CTL_UNSET_OVERRIDES_ACTIVE, NULL);

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:c:U:DN:d:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

#ifdef ORBIS
   argv = &(argv[2]);
   argc = argc - 2;
#endif

#ifndef HAVE_MENU
   if (argc == 1)
   {
      printf("%s\n", msg_hash_to_str(MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN));
      retroarch_print_help(argv[0]);
      exit(0);
   }
#endif

   /* First pass: Read the config file path and any directory overrides, so
    * they're in place when we load the config */
   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

#if 0
         fprintf(stderr, "c is: %c (%d), optarg is: [%s]\n", c, c, string_is_empty(optarg) ? "" : optarg);
#endif

         if (c == -1)
            break;

         switch (c)
         {
            case 'h':
               retroarch_print_help(argv[0]);
               exit(0);

            case 'c':
               RARCH_LOG("Set config file to : %s\n", optarg);
               path_set(RARCH_PATH_CONFIG, optarg);
               break;

            case RA_OPT_APPENDCONFIG:
               path_set(RARCH_PATH_CONFIG_APPEND, optarg);
               break;

            case 's':
               strlcpy(global->name.savefile, optarg,
                     sizeof(global->name.savefile));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);
               break;

            case 'S':
               strlcpy(global->name.savestate, optarg,
                     sizeof(global->name.savestate));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
               break;

            /* Must handle '?' otherwise you get an infinite loop */
            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
               break;
            /* All other arguments are handled in the second pass */
         }
      }
   }

   /* Load the config file now that we know what it is */
   config_load();

   /* Second pass: All other arguments override the config file */
   optind = 1;

   if (argc)
   {
      for (;;)
      {
         int c = getopt_long(argc, argv, optstring, opts, NULL);

         if (c == -1)
            break;

         switch (c)
         {
            case 'd':
               {
                  unsigned new_port;
                  unsigned id              = 0;
                  struct string_list *list = string_split(optarg, ":");
                  int    port              = 0;

                  if (list && list->size == 2)
                  {
                     port = (int)strtol(list->elems[0].data, NULL, 0);
                     id   = (unsigned)strtoul(list->elems[1].data, NULL, 0);
                  }
                  string_list_free(list);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n", msg_hash_to_str(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port -1;

                  input_config_set_device(new_port, id);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'A':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("Connect dualanalog to a valid port.\n");
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;

                  input_config_set_device(new_port, RETRO_DEVICE_ANALOG);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'f':
               rarch_force_fullscreen = true;
               break;

            case 'v':
               verbosity_enable();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
               break;

            case 'N':
               {
                  unsigned new_port;
                  int port = (int)strtol(optarg, NULL, 0);

                  if (port < 1 || port > MAX_USERS)
                  {
                     RARCH_ERR("%s\n",
                           msg_hash_to_str(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT));
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
                  new_port = port - 1;
                  input_config_set_device(port - 1, RETRO_DEVICE_NONE);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'r':
               strlcpy(global->record.path, optarg,
                     sizeof(global->record.path));
               if (recording_is_enabled())
                  recording_set_state(true);
               break;

   #ifdef HAVE_DYNAMIC
            case 'L':
               {
                  int path_stats = path_stat(optarg);

                  if ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
                  {
                     settings_t *settings  = config_get_ptr();

                     path_clear(RARCH_PATH_CORE);
                     strlcpy(settings->paths.directory_libretro, optarg,
                           sizeof(settings->paths.directory_libretro));

                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);
                     RARCH_WARN("Using old --libretro behavior. "
                           "Setting libretro_directory to \"%s\" instead.\n",
                           optarg);
                  }
                  else if ((path_stats & RETRO_VFS_STAT_IS_VALID) != 0)
                  {
                     path_set(RARCH_PATH_CORE, optarg);
                     retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);

                     /* We requested explicit core, so use PLAIN core type. */
                     retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);
                  }
                  else
                  {
                     RARCH_WARN("--libretro argument \"%s\" is neither a file nor directory. Ignoring.\n",
                           optarg);
                  }
               }
               break;
   #endif
            case 'P':
            case 'R':
               strlcpy(bsv_movie_state.movie_start_path, optarg,
                     sizeof(bsv_movie_state.movie_start_path));

               if (c == 'P')
                  bsv_movie_state.movie_start_playback = true;
               else
                  bsv_movie_state.movie_start_playback = false;

               if (c == 'R')
                  bsv_movie_state.movie_start_recording = true;
               else
                  bsv_movie_state.movie_start_recording = false;
               break;

            case 'M':
               if (string_is_equal(optarg, "noload-nosave"))
               {
                  rarch_is_sram_load_disabled = true;
                  rarch_is_sram_save_disabled = true;
               }
               else if (string_is_equal(optarg, "noload-save"))
                  rarch_is_sram_load_disabled = true;
               else if (string_is_equal(optarg, "load-nosave"))
                  rarch_is_sram_save_disabled = true;
               else if (string_is_not_equal(optarg, "load-save"))
               {
                  RARCH_ERR("Invalid argument in --sram-mode.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

   #ifdef HAVE_NETWORKING
            case 'H':
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
               break;

            case 'C':
               {
                  settings_t *settings  = config_get_ptr();
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
                  strlcpy(settings->paths.netplay_server, optarg,
                        sizeof(settings->paths.netplay_server));
               }
               break;

            case RA_OPT_STATELESS:
               {
                  settings_t *settings  = config_get_ptr();

                  configuration_set_bool(settings,
                        settings->bools.netplay_stateless_mode, true);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
               }
               break;

            case RA_OPT_CHECK_FRAMES:
               {
                  settings_t *settings  = config_get_ptr();
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);

                  configuration_set_int(settings,
                        settings->ints.netplay_check_frames,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

            case RA_OPT_PORT:
               {
                  settings_t *settings  = config_get_ptr();
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
                  configuration_set_uint(settings,
                        settings->uints.netplay_port,
                        (int)strtoul(optarg, NULL, 0));
               }
               break;

   #if defined(HAVE_NETWORK_CMD)
            case RA_OPT_COMMAND:
   #ifdef HAVE_COMMAND
               if (command_network_send((const char*)optarg))
                  exit(0);
               else
                  retroarch_fail(1, "network_cmd_send()");
   #endif
               break;
   #endif

   #endif

            case RA_OPT_BPS:
               strlcpy(global->name.bps, optarg,
                     sizeof(global->name.bps));
               rarch_bps_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
               break;

            case 'U':
               strlcpy(global->name.ups, optarg,
                     sizeof(global->name.ups));
               rarch_ups_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
               break;

            case RA_OPT_IPS:
               strlcpy(global->name.ips, optarg,
                     sizeof(global->name.ips));
               rarch_ips_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
               break;

            case RA_OPT_NO_PATCH:
               rarch_ctl(RARCH_CTL_SET_PATCH_BLOCKED, NULL);
               break;

            case 'D':
               frontend_driver_detach_console();
               break;

            case RA_OPT_MENU:
               explicit_menu = true;
               break;

            case RA_OPT_NICK:
               {
                  settings_t *settings  = config_get_ptr();

                  rarch_ctl(RARCH_CTL_USERNAME_SET, NULL);

                  strlcpy(settings->paths.username, optarg,
                        sizeof(settings->paths.username));
               }
               break;

            case RA_OPT_SIZE:
               {
                  unsigned recording_width  = 0;
                  unsigned recording_height = 0;

                  recording_driver_get_size(&recording_width,
                        &recording_height);

                  if (sscanf(optarg, "%ux%u",
                           &recording_width,
                           &recording_height) != 2)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     retroarch_print_help(argv[0]);
                     retroarch_fail(1, "retroarch_parse_input()");
                  }
               }
               break;

            case RA_OPT_RECORDCONFIG:
               strlcpy(global->record.config, optarg,
                     sizeof(global->record.config));
               break;

            case RA_OPT_MAX_FRAMES:
               runloop_max_frames  = (unsigned)strtoul(optarg, NULL, 10);
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT:
               runloop_max_frames_screenshot = true;
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT_PATH:
               strlcpy(runloop_max_frames_screenshot_path, optarg, sizeof(runloop_max_frames_screenshot_path));
               break;

            case RA_OPT_SUBSYSTEM:
               path_set(RARCH_PATH_SUBSYSTEM, optarg);
               break;

            case RA_OPT_FEATURES:
               retroarch_print_features();
               exit(0);

            case RA_OPT_EOF_EXIT:
               bsv_movie_state.eof_exit = true;
               break;

            case RA_OPT_VERSION:
               retroarch_print_version();
               exit(0);

            case RA_OPT_LOG_FILE:
               {
                  settings_t *settings  = config_get_ptr();

                  /* Enable 'log to file' */
                  configuration_set_bool(settings,
                        settings->bools.log_to_file, true);

                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL);

                  /* Cache log file path override */
                  log_file_override_active = true;
                  strlcpy(log_file_override_path, optarg, sizeof(log_file_override_path));
               }
               break;

            case 'c':
            case 'h':
            case RA_OPT_APPENDCONFIG:
            case 's':
            case 'S':
               break; /* Handled in the first pass */

            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");

            default:
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
               retroarch_fail(1, "retroarch_parse_input()");
         }
      }
   }

   if (verbosity_is_enabled())
      rarch_log_file_init();

#ifdef HAVE_GIT_VERSION
   RARCH_LOG("RetroArch %s (Git %s)\n",
         PACKAGE_VERSION, retroarch_git_version);
#endif

   if (explicit_menu)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         retroarch_fail(1, "retroarch_parse_input()");
      }
#ifdef HAVE_DYNAMIC
      else
      {
         /* Allow stray -L arguments to go through to workaround cases
          * where it's used as "config file".
          *
          * This seems to still be the case for Android, which
          * should be properly fixed. */
         retroarch_set_current_core_type(CORE_TYPE_DUMMY, false);
      }
#endif
   }

   if (optind < argc)
   {
      bool subsystem_path_is_empty = path_is_empty(RARCH_PATH_SUBSYSTEM);

      /* We requested explicit ROM, so use PLAIN core type. */
      retroarch_set_current_core_type(CORE_TYPE_PLAIN, false);

      if (subsystem_path_is_empty)
         path_set(RARCH_PATH_NAMES, (const char*)argv[optind]);
      else
         path_set_special(argv + optind, argc - optind);
   }

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(global->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, global->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(global->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, global->name.savestate);
}

static bool drivers_set_active(void)
{
   video_driver_set_active();
   audio_driver_set_active();

   return true;
}

bool retroarch_validate_game_options(char *s, size_t len, bool mkdir)
{
   char *config_directory                 = NULL;
   size_t str_size                        = PATH_MAX_LENGTH * sizeof(char);
   const char *core_name                  = runloop_system.info.library_name;
   const char *game_name                  = path_basename(path_get(RARCH_PATH_BASENAME));

   if (string_is_empty(core_name) || string_is_empty(game_name))
      return false;

   config_directory                       = (char*)malloc(str_size);
   config_directory[0]                    = '\0';

   fill_pathname_application_special(config_directory,
         str_size, APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* Concatenate strings into full paths for game_path */
   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         file_path_str(FILE_PATH_OPT_EXTENSION),
         len);

   if (mkdir)
   {
      char *core_path  = (char*)malloc(str_size);
      core_path[0]     = '\0';

      fill_pathname_join(core_path,
            config_directory, core_name, str_size);

      if (!path_is_directory(core_path))
         path_mkdir(core_path);

      free(core_path);
   }

   free(config_directory);
   return true;
}

/* Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now. */
static void retroarch_validate_cpu_features(void)
{
   uint64_t cpu = cpu_features_get();
   (void)cpu;

#ifdef __MMX__
   if (!(cpu & RETRO_SIMD_MMX))
      FAIL_CPU("MMX");
#endif
#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

static void retroarch_main_init_media(enum rarch_content_type cont_type,
      bool builtin_mediaplayer, bool builtin_imageviewer)
{
   switch (cont_type)
   {
      case RARCH_CONTENT_MOVIE:
      case RARCH_CONTENT_MUSIC:
         if (builtin_mediaplayer)
         {
            /* TODO/FIXME - it needs to become possible to 
             * switch between FFmpeg and MPV at runtime */
#if defined(HAVE_MPV)
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_MPV, false);
#elif defined(HAVE_FFMPEG)
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
         }
         break;
#ifdef HAVE_IMAGEVIEWER
      case RARCH_CONTENT_IMAGE:
         if (builtin_imageviewer)
         {
            retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
            retroarch_set_current_core_type(CORE_TYPE_IMAGEVIEWER, false);
         }
         break;
#endif
#ifdef HAVE_EASTEREGG
      case RARCH_CONTENT_GONG:
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
         retroarch_set_current_core_type(CORE_TYPE_GONG, false);
         break;
#endif
      default:
         break;
   }
}

/**
 * retroarch_main_init:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Initializes the program.
 *
 * Returns: true on success, otherwise false if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[])
{
   bool init_failed  = false;
   global_t  *global = global_get_ptr();
#if defined(DEBUG) && defined(HAVE_DRMINGW)
   char log_file_name[128];
#endif

   drivers_set_active();

   if (setjmp(error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN), error_string);
      return false;
   }

   rarch_error_on_init = true;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   retroarch_parse_input_and_config(argc, argv);

   if (verbosity_is_enabled())
   {
      char str[128];
      const char *cpu_model = NULL;
      str[0] = '\0';

      cpu_model = frontend_driver_get_cpu_model_name();

      RARCH_LOG_OUTPUT("=== Build =======================================\n");

      if (!string_is_empty(cpu_model))
         RARCH_LOG_OUTPUT("CPU Model Name: %s\n", cpu_model);

      retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
      RARCH_LOG_OUTPUT("%s: %s\n", msg_hash_to_str(MSG_CAPABILITIES), str);
      RARCH_LOG_OUTPUT("Built: %s\n", __DATE__);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", retroarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

#if defined(DEBUG) && defined(HAVE_DRMINGW)
   RARCH_LOG("Initializing Dr.MingW Exception handler\n");
   fill_str_dated_filename(log_file_name, "crash",
         "log", sizeof(log_file_name));
   ExcHndlInit();
   ExcHndlSetLogFileNameA(log_file_name);
#endif

   retroarch_validate_cpu_features();

   rarch_ctl(RARCH_CTL_TASK_INIT, NULL);

   {
      const char    *fullpath  = path_get(RARCH_PATH_CONTENT);
      settings_t     *settings = config_get_ptr();

      if (!string_is_empty(fullpath))
      {
         settings_t     *settings          = config_get_ptr();
         bool builtin_imageviewer          = false;
         bool builtin_mediaplayer          = false;
         enum rarch_content_type cont_type = path_is_media_type(fullpath);
         
         if (settings)
         {
            builtin_imageviewer   = settings->bools.multimedia_builtin_imageviewer_enable;
            builtin_mediaplayer   = settings->bools.multimedia_builtin_mediaplayer_enable;
         }

         retroarch_main_init_media(cont_type, builtin_mediaplayer,
               builtin_imageviewer);
      }
   }

   /* Pre-initialize all drivers 
    * Attempts to find a default driver for
    * all driver types.
    */
   audio_driver_find_driver();
   video_driver_find_driver();
   input_driver_find_driver();
   camera_driver_ctl(RARCH_CAMERA_CTL_FIND_DRIVER, NULL);
   wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);
   find_location_driver();
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_FIND_DRIVER, NULL);
#endif

   /* Attempt to initialize core */
   if (has_set_core)
   {
      has_set_core = false;
      if (!command_event(CMD_EVENT_CORE_INIT, &explicit_current_core_type))
         init_failed = true;
   }
   else if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
      init_failed = true;

   /* Handle core initialization failure */
   if (init_failed)
   {
      /* Check if menu was active prior to core initialization */
      if (!content_launched_from_cli()
#ifdef HAVE_MENU
          || menu_driver_is_alive()
#endif
         )
      {
         /* Attempt initializing dummy core */
         current_core_type = CORE_TYPE_DUMMY;
         if (!command_event(CMD_EVENT_CORE_INIT, &current_core_type))
            goto error;
      }
      else
      {
         /* Fall back to regular error handling */
         goto error;
      }
   }

   command_event(CMD_EVENT_CHEATS_INIT, NULL);
   drivers_init(DRIVERS_CMD_ALL);
   command_event(CMD_EVENT_COMMAND_INIT, NULL);
   command_event(CMD_EVENT_REMOTE_INIT, NULL);
   command_event(CMD_EVENT_MAPPER_INIT, NULL);
   command_event(CMD_EVENT_REWIND_INIT, NULL);
   command_event(CMD_EVENT_CONTROLLERS_INIT, NULL);
   if (!string_is_empty(global->record.path))
      command_event(CMD_EVENT_RECORD_INIT, NULL);

   path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   rarch_error_on_init     = false;
   rarch_is_inited         = true;

#ifdef HAVE_DISCORD
   if (command_event(CMD_EVENT_DISCORD_INIT, NULL))
      discord_is_inited = true;

   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_MENU;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif

#ifdef HAVE_MENU
   {
      settings_t *settings = config_get_ptr();

      if (settings->bools.audio_enable_menu)
         audio_driver_load_menu_sounds();
   }
#endif

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   rarch_is_inited         = false;

   return false;
}

bool retroarch_is_on_main_thread(void)
{
#ifdef HAVE_THREAD_STORAGE
   if (sthread_tls_get(&rarch_tls) != MAGIC_POINTER)
      return false;
#endif
   return true;
}

void rarch_menu_running(void)
{
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings                    = config_get_ptr();
#endif
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_SET_TOGGLE, NULL);

   /* Prevent stray input */
   input_driver_set_flushing_input();

   if (settings && settings->bools.audio_enable_menu && settings->bools.audio_enable_menu_bgm)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif
#ifdef HAVE_OVERLAY
   if (settings && settings->bools.input_overlay_hide_in_menu)
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
#endif
}

void rarch_menu_running_finished(void)
{
#if defined(HAVE_MENU) || defined(HAVE_OVERLAY)
   settings_t *settings                    = config_get_ptr();
#endif
#ifdef HAVE_MENU
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_TOGGLE, NULL);

   /* Prevent stray input */
   input_driver_set_flushing_input();

   /* Stop menu background music before we exit the menu */
   if (settings && settings->bools.audio_enable_menu && settings->bools.audio_enable_menu_bgm)
      audio_driver_mixer_stop_stream(AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif
   video_driver_set_texture_enable(false, false);
#ifdef HAVE_OVERLAY
   if (settings && settings->bools.input_overlay_hide_in_menu)
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
#endif
}

/**
 * rarch_game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   size_t game_path_size = 8192 * sizeof(char);
   char *game_path       = (char*)malloc(game_path_size);

   game_path[0] ='\0';

   if (!retroarch_validate_game_options(game_path,
            game_path_size, false))
      goto error;
   if (!config_file_exists(game_path))
      goto error;

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_path);
   *output = strdup(game_path);
   free(game_path);
   return true;

error:
   free(game_path);
   return false;
}

bool rarch_ctl(enum rarch_ctl_state state, void *data)
{
   static bool has_set_username        = false;
   static bool rarch_block_config_read = false;
   static bool rarch_patch_blocked     = false;
   static bool runloop_missing_bios    = false;
   /* TODO/FIXME - not used right now? */

   switch(state)
   {
      case RARCH_CTL_IS_PATCH_BLOCKED:
         return rarch_patch_blocked;
      case RARCH_CTL_SET_PATCH_BLOCKED:
         rarch_patch_blocked = true;
         break;
      case RARCH_CTL_UNSET_PATCH_BLOCKED:
         rarch_patch_blocked = false;
         break;
      case RARCH_CTL_IS_BPS_PREF:
         return rarch_bps_pref;
      case RARCH_CTL_UNSET_BPS_PREF:
         rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return rarch_ups_pref;
      case RARCH_CTL_UNSET_UPS_PREF:
         rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return rarch_ips_pref;
      case RARCH_CTL_UNSET_IPS_PREF:
         rarch_ips_pref = false;
         break;
      case RARCH_CTL_IS_DUMMY_CORE:
         return (current_core_type == CORE_TYPE_DUMMY);
      case RARCH_CTL_USERNAME_SET:
         has_set_username = true;
         break;
      case RARCH_CTL_USERNAME_UNSET:
         has_set_username = false;
         break;
      case RARCH_CTL_HAS_SET_USERNAME:
         return has_set_username;
      case RARCH_CTL_IS_INITED:
         return rarch_is_inited;
      case RARCH_CTL_DESTROY:
         has_set_username        = false;
         rarch_is_inited         = false;
         rarch_error_on_init     = false;
         rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);

         retroarch_msg_queue_deinit();
         driver_uninit(DRIVERS_CMD_ALL);
         command_event(CMD_EVENT_LOG_FILE_DEINIT, NULL);

         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         rarch_ctl(RARCH_CTL_DATA_DEINIT, NULL);
         config_free();
         break;
      case RARCH_CTL_PREINIT:
         libretro_free_system_info(&runloop_system.info);
         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

         rarch_config_init();

         driver_ctl(RARCH_DRIVER_CTL_DEINIT,  NULL);
         rarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
         global_free();
         break;
      case RARCH_CTL_MAIN_DEINIT:
         if (!rarch_is_inited)
            return false;
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
         command_event(CMD_EVENT_COMMAND_DEINIT, NULL);
         command_event(CMD_EVENT_REMOTE_DEINIT, NULL);
         command_event(CMD_EVENT_MAPPER_DEINIT, NULL);

         command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);

         command_event(CMD_EVENT_RECORD_DEINIT, NULL);

         event_save_files();

         command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         command_event(CMD_EVENT_CHEATS_DEINIT, NULL);
         command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);

         command_event(CMD_EVENT_CORE_DEINIT, NULL);

         content_deinit();

         path_deinit_subsystem();
         path_deinit_savefile();

         rarch_is_inited         = false;

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_delete(&rarch_tls);
#endif
         break;
      case RARCH_CTL_INIT:
         if (rarch_is_inited)
            driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
         sthread_tls_create(&rarch_tls);
         sthread_tls_set(&rarch_tls, MAGIC_POINTER);
#endif
         drivers_set_active();
         {
            uint8_t i;
            for (i = 0; i < MAX_USERS; i++)
               input_config_set_device(i, RETRO_DEVICE_JOYPAD);
         }
         rarch_ctl(RARCH_CTL_HTTPSERVER_INIT, NULL);
         retroarch_msg_queue_init();
         break;
      case RARCH_CTL_IS_SRAM_LOAD_DISABLED:
         return rarch_is_sram_load_disabled;
      case RARCH_CTL_IS_SRAM_SAVE_DISABLED:
         return rarch_is_sram_save_disabled;
      case RARCH_CTL_IS_SRAM_USED:
         return rarch_use_sram;
      case RARCH_CTL_SET_SRAM_ENABLE:
         {
            bool contentless = false;
            bool is_inited   = false;
            content_get_status(&contentless, &is_inited);
            rarch_use_sram = (current_core_type == CORE_TYPE_PLAIN)
               && !contentless;
         }
         break;
      case RARCH_CTL_SET_SRAM_ENABLE_FORCE:
         rarch_use_sram = true;
         break;
      case RARCH_CTL_UNSET_SRAM_ENABLE:
         rarch_use_sram = false;
         break;
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         rarch_block_config_read = false;
         break;
      case RARCH_CTL_IS_BLOCK_CONFIG_READ:
         return rarch_block_config_read;
      case RARCH_CTL_SYSTEM_INFO_INIT:
         core_get_system_info(&runloop_system.info);

         if (!runloop_system.info.library_name)
            runloop_system.info.library_name = msg_hash_to_str(MSG_UNKNOWN);
         if (!runloop_system.info.library_version)
            runloop_system.info.library_version = "v0";

         video_driver_set_title_buf();

         strlcpy(runloop_system.valid_extensions,
               runloop_system.info.valid_extensions ?
               runloop_system.info.valid_extensions : DEFAULT_EXT,
               sizeof(runloop_system.valid_extensions));
         break;
      case RARCH_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            *idx = (unsigned)core_option_manager_size(runloop_core_options);
         }
         break;
      case RARCH_CTL_HAS_CORE_OPTIONS:
         return (runloop_core_options != NULL);
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts)
               return false;
            *coreopts = runloop_core_options;
         }
         break;
      case RARCH_CTL_SYSTEM_INFO_FREE:
         if (runloop_system.subsystem.data)
            free(runloop_system.subsystem.data);
         runloop_system.subsystem.data = NULL;
         runloop_system.subsystem.size = 0;

         if (runloop_system.ports.data)
            free(runloop_system.ports.data);
         runloop_system.ports.data = NULL;
         runloop_system.ports.size = 0;

         if (runloop_system.mmaps.descriptors)
            free((void *)runloop_system.mmaps.descriptors);
         runloop_system.mmaps.descriptors     = NULL;
         runloop_system.mmaps.num_descriptors = 0;

         rarch_ctl(RARCH_CTL_UNSET_KEY_EVENT, NULL);
         runloop_frontend_key_event = NULL;

         audio_driver_unset_callback();

         runloop_system.info.library_name          = NULL;
         runloop_system.info.library_version       = NULL;
         runloop_system.info.valid_extensions      = NULL;
         runloop_system.info.need_fullpath         = false;
         runloop_system.info.block_extract         = false;

         memset(&runloop_system, 0, sizeof(rarch_system_info_t));
         break;
      case RARCH_CTL_SET_FRAME_TIME_LAST:
         runloop_frame_time_last        = 0;
         break;
      case RARCH_CTL_SET_OVERRIDES_ACTIVE:
         runloop_overrides_active = true;
         break;
      case RARCH_CTL_UNSET_OVERRIDES_ACTIVE:
         runloop_overrides_active = false;
         break;
      case RARCH_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_overrides_active;
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE:
         runloop_remaps_core_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return runloop_remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE:
         runloop_remaps_game_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return runloop_remaps_game_active;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_remaps_content_dir_active = true;
         break;
      case RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_remaps_content_dir_active = false;
         break;
      case RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE:
         return runloop_remaps_content_dir_active;
      case RARCH_CTL_SET_MISSING_BIOS:
         runloop_missing_bios = true;
         break;
      case RARCH_CTL_UNSET_MISSING_BIOS:
         runloop_missing_bios = false;
         break;
      case RARCH_CTL_IS_MISSING_BIOS:
         return runloop_missing_bios;
      case RARCH_CTL_IS_GAME_OPTIONS_ACTIVE:
         return runloop_game_options_active;
      case RARCH_CTL_SET_FRAME_LIMIT:
         {
            settings_t *settings       = config_get_ptr();
            struct retro_system_av_info *av_info =
               video_viewport_get_system_av_info();
            float fastforward_ratio              =
               (settings->floats.fastforward_ratio == 0.0f)
               ? 1.0f : settings->floats.fastforward_ratio;

            frame_limit_last_time    = cpu_features_get_time_usec();
            frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f
                  / (av_info->timing.fps * fastforward_ratio));
         }
         break;
      case RARCH_CTL_CONTENT_RUNTIME_LOG_INIT:
      {
         const char *content_path = path_get(RARCH_PATH_CONTENT);
         const char *core_path = path_get(RARCH_PATH_CORE);

         libretro_core_runtime_last = cpu_features_get_time_usec();
         libretro_core_runtime_usec = 0;

         /* Have to cache content and core path here, otherwise
          * logging fails if new content is loaded without
          * closing existing content
          * i.e. RARCH_PATH_CONTENT and RARCH_PATH_CORE get
          * updated when the new content is loaded, which
          * happens *before* RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT
          * -> using RARCH_PATH_CONTENT and RARCH_PATH_CORE
          *    directly in RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT
          *    can therefore lead to the runtime of the currently
          *    loaded content getting written to the *new*
          *    content's log file... */
         memset(runtime_content_path, 0, sizeof(runtime_content_path));
         memset(runtime_core_path, 0, sizeof(runtime_core_path));

         if (!string_is_empty(content_path))
            strlcpy(runtime_content_path, content_path, sizeof(runtime_content_path));

         if (!string_is_empty(core_path))
            strlcpy(runtime_core_path, core_path, sizeof(runtime_core_path));

         break;
      }
      case RARCH_CTL_CONTENT_RUNTIME_LOG_DEINIT:
      {
         settings_t *settings = config_get_ptr();
         unsigned hours = 0;
         unsigned minutes = 0;
         unsigned seconds = 0;
         char log[PATH_MAX_LENGTH] = {0};
         int n = 0;

         runtime_log_convert_usec2hms(libretro_core_runtime_usec, &hours, &minutes, &seconds);

         n = snprintf(log, sizeof(log),
               "Content ran for a total of: %02u hours, %02u minutes, %02u seconds.",
               hours, minutes, seconds);
         if ((n < 0) || (n >= PATH_MAX_LENGTH))
            n = 0; /* Just silence any potential gcc warnings... */
         RARCH_LOG("%s\n",log);

         /* Only write to file if content has run for a non-zero length of time */
         if (libretro_core_runtime_usec > 0)
         {
            /* Per core logging */
            if (settings->bools.content_runtime_log)
               update_runtime_log(true);

            /* Aggregate logging */
            if (settings->bools.content_runtime_log_aggregate)
               update_runtime_log(false);
         }

         /* Reset runtime + content/core paths, to prevent any
          * possibility of duplicate logging */
         libretro_core_runtime_usec = 0;
         memset(runtime_content_path, 0, sizeof(runtime_content_path));
         memset(runtime_core_path, 0, sizeof(runtime_core_path));

         break;
      }
      case RARCH_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_perfcnt_enable;
         }
         break;
      case RARCH_CTL_SET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = true;
         break;
      case RARCH_CTL_UNSET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = false;
         break;
      case RARCH_CTL_IS_PERFCNT_ENABLE:
         return runloop_perfcnt_enable;
      case RARCH_CTL_SET_NONBLOCK_FORCED:
         runloop_force_nonblock = true;
         break;
      case RARCH_CTL_UNSET_NONBLOCK_FORCED:
         runloop_force_nonblock = false;
         break;
      case RARCH_CTL_IS_NONBLOCK_FORCED:
         return runloop_force_nonblock;
      case RARCH_CTL_SET_FRAME_TIME:
         {
            const struct retro_frame_time_callback *info =
               (const struct retro_frame_time_callback*)data;
#ifdef HAVE_NETWORKING
            /* retro_run() will be called in very strange and
             * mysterious ways, have to disable it. */
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               return false;
#endif
            runloop_frame_time = *info;
         }
         break;
      case RARCH_CTL_GET_WINDOWED_SCALE:
         {
            unsigned **scale = (unsigned**)data;
            if (!scale)
               return false;
            *scale       = (unsigned*)&runloop_pending_windowed_scale;
         }
         break;
      case RARCH_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_pending_windowed_scale = *idx;
         }
         break;
      case RARCH_CTL_FRAME_TIME_FREE:
         memset(&runloop_frame_time, 0,
               sizeof(struct retro_frame_time_callback));
         runloop_frame_time_last           = 0;
         runloop_max_frames                = 0;
         break;
      case RARCH_CTL_STATE_FREE:
         runloop_perfcnt_enable            = false;
         runloop_idle                      = false;
         runloop_paused                    = false;
         runloop_slowmotion                = false;
         runloop_overrides_active          = false;
         runloop_autosave                  = false;
         rarch_ctl(RARCH_CTL_FRAME_TIME_FREE, NULL);
         break;
      case RARCH_CTL_IS_IDLE:
         return runloop_idle;
      case RARCH_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_idle = *ptr;
         }
         break;
      case RARCH_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_paused = *ptr;
         }
         break;
      case RARCH_CTL_IS_PAUSED:
         return runloop_paused;
      case RARCH_CTL_TASK_INIT:
         {
#ifdef HAVE_THREADS
            settings_t *settings = config_get_ptr();
            bool threaded_enable = settings->bools.threaded_data_runloop_enable;
#else
            bool threaded_enable = false;
#endif
            task_queue_deinit();
            task_queue_init(threaded_enable, runloop_task_msg_queue_push);
         }
         break;
      case RARCH_CTL_SET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = true;
         break;
      case RARCH_CTL_SET_SHUTDOWN:
         runloop_shutdown_initiated = true;
         break;
      case RARCH_CTL_UNSET_SHUTDOWN:
         runloop_shutdown_initiated = false;
         break;
      case RARCH_CTL_IS_SHUTDOWN:
         return runloop_shutdown_initiated;
      case RARCH_CTL_DATA_DEINIT:
         task_queue_deinit();
         break;
      case RARCH_CTL_IS_CORE_OPTION_UPDATED:
         if (!runloop_core_options)
            return false;
         return  core_option_manager_updated(runloop_core_options);
      case RARCH_CTL_CORE_OPTION_PREV:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_prev(runloop_core_options, *idx);
         }
         break;
      case RARCH_CTL_CORE_OPTION_NEXT:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_manager_next(runloop_core_options, *idx);
         }
         break;
      case RARCH_CTL_CORE_OPTIONS_GET:
         {
            settings_t *settings = config_get_ptr();
            unsigned log_level   = settings->uints.libretro_log_level;

            struct retro_variable *var = (struct retro_variable*)data;

            if (!runloop_core_options || !var)
               return false;

            core_option_manager_get(runloop_core_options, var);

            if (log_level == RETRO_LOG_DEBUG)
            {
               RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);
               RARCH_LOG("\t%s\n", var->value ? var->value :
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
            }
         }
         break;
      case RARCH_CTL_CORE_OPTIONS_INIT:
         {
            settings_t *settings              = config_get_ptr();
            char *game_options_path           = NULL;
            bool ret                          = false;
            const struct retro_variable *vars =
               (const struct retro_variable*)data;

            if (settings && settings->bools.game_specific_options)
               ret = rarch_game_specific_options(&game_options_path);

            if (ret)
            {
               runloop_game_options_active = true;
               runloop_core_options        =
                  core_option_manager_new(game_options_path, vars);
               free(game_options_path);
            }
            else
            {
               char buf[PATH_MAX_LENGTH];
               const char *options_path          = NULL;

               buf[0] = '\0';

               if (settings)
                  options_path = settings->paths.path_core_options;

               if (string_is_empty(options_path) && !path_is_empty(RARCH_PATH_CONFIG))
               {
                  fill_pathname_resolve_relative(buf, path_get(RARCH_PATH_CONFIG),
                        file_path_str(FILE_PATH_CORE_OPTIONS_CONFIG), sizeof(buf));
                  options_path = buf;
               }

               runloop_game_options_active = false;

               if (!string_is_empty(options_path))
                  runloop_core_options =
                     core_option_manager_new(options_path, vars);
            }

         }
         break;
      case RARCH_CTL_CORE_OPTIONS_DEINIT:
         {
            if (!runloop_core_options)
               return false;

            /* check if game options file was just created and flush
               to that file instead */
            if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            {
               core_option_manager_flush_game_specific(runloop_core_options,
                     path_get(RARCH_PATH_CORE_OPTIONS));
               path_clear(RARCH_PATH_CORE_OPTIONS);
            }
            else
               core_option_manager_flush(runloop_core_options);

            if (runloop_game_options_active)
               runloop_game_options_active = false;

            if (runloop_core_options)
               core_option_manager_free(runloop_core_options);
            runloop_core_options          = NULL;
         }
         break;
      case RARCH_CTL_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_key_event;
         }
         break;
      case RARCH_CTL_UNSET_KEY_EVENT:
         runloop_key_event          = NULL;
         break;
      case RARCH_CTL_FRONTEND_KEY_EVENT_GET:
         {
            retro_keyboard_event_t **key_event =
               (retro_keyboard_event_t**)data;
            if (!key_event)
               return false;
            *key_event = &runloop_frontend_key_event;
         }
         break;
      case RARCH_CTL_HTTPSERVER_INIT:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_init(8888);
#endif
         break;
      case RARCH_CTL_HTTPSERVER_DESTROY:
#if defined(HAVE_HTTPSERVER) && defined(HAVE_ZLIB)
         httpserver_destroy();
#endif
         break;
      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}

bool retroarch_is_forced_fullscreen(void)
{
   return rarch_force_fullscreen;
}

void retroarch_unset_forced_fullscreen(void)
{
   rarch_force_fullscreen = false;
}

bool retroarch_is_switching_display_mode(void)
{
   return rarch_is_switching_display_mode;
}

void retroarch_set_switching_display_mode(void)
{
   rarch_is_switching_display_mode = true;
}

void retroarch_unset_switching_display_mode(void)
{
   rarch_is_switching_display_mode = false;
}

/* set a runtime shader preset without overwriting the settings value */
void retroarch_set_shader_preset(const char* preset)
{
   if (!string_is_empty(preset))
      strlcpy(runtime_shader_preset, preset, sizeof(runtime_shader_preset));
   else
      runtime_shader_preset[0] = '\0';
}

/* unset a runtime shader preset */
void retroarch_unset_shader_preset(void)
{
   runtime_shader_preset[0] = '\0';
}

/* get the name of the current shader preset */
char* retroarch_get_shader_preset(void)
{
   settings_t *settings = config_get_ptr();
   if (!settings->bools.video_shader_enable)
      return NULL;

   if (!string_is_empty(runtime_shader_preset))
      return runtime_shader_preset;
   else if (!string_is_empty(settings->paths.path_shader))
      return settings->paths.path_shader;
   return NULL;
}

bool retroarch_override_setting_is_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               return BIT256_GET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         return has_set_verbosity;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return has_set_libretro;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return has_set_libretro_directory;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return has_set_save_path;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return has_set_state_path;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return has_set_netplay_check_frames;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return has_set_ups_pref;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return has_set_bps_pref;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return has_set_ips_pref;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         return has_set_log_to_file;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

void retroarch_override_setting_set(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_SET(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = true;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = true;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = true;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         has_set_log_to_file = true;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_unset(enum rarch_override_setting enum_idx, void *data)
{
   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned bit = *val;
               BIT256_CLEAR(has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         has_set_verbosity = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         has_set_libretro = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         has_set_libretro_directory = false;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         has_set_save_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         has_set_state_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         has_set_netplay_check_frames = false;
         break;
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         has_set_ups_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         has_set_bps_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         has_set_ips_pref = false;
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         has_set_log_to_file = false;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len)
{
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu = cpu_features_get();

            if (cpu & RETRO_SIMD_MMX)
               strlcat(s, "MMX ", len);
            if (cpu & RETRO_SIMD_MMXEXT)
               strlcat(s, "MMXEXT ", len);
            if (cpu & RETRO_SIMD_SSE)
               strlcat(s, "SSE1 ", len);
            if (cpu & RETRO_SIMD_SSE2)
               strlcat(s, "SSE2 ", len);
            if (cpu & RETRO_SIMD_SSE3)
               strlcat(s, "SSE3 ", len);
            if (cpu & RETRO_SIMD_SSSE3)
               strlcat(s, "SSSE3 ", len);
            if (cpu & RETRO_SIMD_SSE4)
               strlcat(s, "SSE4 ", len);
            if (cpu & RETRO_SIMD_SSE42)
               strlcat(s, "SSE4.2 ", len);
            if (cpu & RETRO_SIMD_AVX)
               strlcat(s, "AVX ", len);
            if (cpu & RETRO_SIMD_AVX2)
               strlcat(s, "AVX2 ", len);
            if (cpu & RETRO_SIMD_VFPU)
               strlcat(s, "VFPU ", len);
            if (cpu & RETRO_SIMD_NEON)
               strlcat(s, "NEON ", len);
            if (cpu & RETRO_SIMD_VFPV3)
               strlcat(s, "VFPv3 ", len);
            if (cpu & RETRO_SIMD_VFPV4)
               strlcat(s, "VFPv4 ", len);
            if (cpu & RETRO_SIMD_PS)
               strlcat(s, "PS ", len);
            if (cpu & RETRO_SIMD_AES)
               strlcat(s, "AES ", len);
            if (cpu & RETRO_SIMD_VMX)
               strlcat(s, "VMX ", len);
            if (cpu & RETRO_SIMD_VMX128)
               strlcat(s, "VMX128 ", len);
            if (cpu & RETRO_SIMD_ASIMD)
               strlcat(s, "ASIMD ", len);
         }
         break;
      case RARCH_CAPABILITIES_COMPILER:
#if defined(_MSC_VER)
         snprintf(s, len, "%s: MSVC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               _MSC_VER, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
         snprintf(s, len, "%s: SNC (%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
         snprintf(s, len, "%s: MinGW (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
         snprintf(s, len, "%s: Clang/LLVM (%s) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
         snprintf(s, len, "%s: GCC (%d.%d.%d) %u-bit",
               msg_hash_to_str(MSG_COMPILER),
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
               (CHAR_BIT * sizeof(size_t)));
#else
         snprintf(s, len, "%s %u-bit",
               msg_hash_to_str(MSG_UNKNOWN_COMPILER),
               (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
         break;
      default:
      case RARCH_CAPABILITIES_NONE:
         break;
   }

   return 0;
}

void retroarch_set_current_core_type(enum rarch_core_type type, bool explicitly_set)
{
   if (explicitly_set && !has_set_core)
   {
      has_set_core                = true;
      explicit_current_core_type  = type;
      current_core_type           = type;
   }
   else if (!has_set_core)
      current_core_type          = type;
}

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
void retroarch_fail(int error_code, const char *error)
{
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(rarch_error_on_init);

   strlcpy(error_string, error, sizeof(error_string));
   longjmp(error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
#ifdef HAVE_DISCORD
      if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_SHUTDOWN;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
   command_event(CMD_EVENT_DISCORD_DEINIT, NULL);
   discord_is_inited          = false;
#endif

   if (!rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL))
   {
      command_event(CMD_EVENT_AUTOSAVE_STATE, NULL);
      command_event(CMD_EVENT_DISABLE_OVERRIDES, NULL);
      command_event(CMD_EVENT_RESTORE_DEFAULT_SHADER_PRESET, NULL);
      command_event(CMD_EVENT_RESTORE_REMAPS, NULL);
   }

   rarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
   rarch_menu_running_finished();

   return true;
}

global_t *global_get_ptr(void)
{
   static struct global g_extern;
   return &g_extern;
}

void runloop_task_msg_queue_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   if (!menu_widgets_task_msg_queue_push(task, msg, prio, duration, flush))
#endif
      runloop_msg_queue_push(msg, prio, duration, flush, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   runloop_ctx_msg_info_t msg_info;
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
   if (menu_widgets_msg_queue_push(msg,
            roundf((float)duration / 60.0f * 1000.0f), title, icon, category, prio, flush))
      return;
#endif

#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif

   if (flush)
      msg_queue_clear(runloop_msg_queue);

   msg_info.msg      = msg;
   msg_info.prio     = prio;
   msg_info.duration = duration;
   msg_info.flush    = flush;

   if (runloop_msg_queue)
   {
      msg_queue_push(runloop_msg_queue, msg_info.msg,
            msg_info.prio, msg_info.duration,
            title, icon, category);

      ui_companion_driver_msg_queue_push(msg_info.msg,
            msg_info.prio, msg_info.duration, msg_info.flush);
   }

#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
#endif
}

void runloop_get_status(bool *is_paused, bool *is_idle,
      bool *is_slowmotion, bool *is_perfcnt_enable)
{
   *is_paused         = runloop_paused;
   *is_idle           = runloop_idle;
   *is_slowmotion     = runloop_slowmotion;
   *is_perfcnt_enable = runloop_perfcnt_enable;
}

bool runloop_msg_queue_pull(const char **ret)
{
#ifdef HAVE_THREADS
   runloop_msg_queue_lock();
#endif
   if (!ret)
      return false;
   *ret = msg_queue_pull(runloop_msg_queue);
#ifdef HAVE_THREADS
   runloop_msg_queue_unlock();
#endif
   return true;
}

#define bsv_movie_is_end_of_file() (bsv_movie_state.movie_end && bsv_movie_state.eof_exit)

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
#define time_to_exit(quit_key_pressed) (runloop_shutdown_initiated || quit_key_pressed || !is_alive || bsv_movie_is_end_of_file() || ((runloop_max_frames != 0) && (frame_count >= runloop_max_frames)) || runloop_exec)

#define runloop_check_cheevos() (settings->bools.cheevos_enable && rcheevos_loaded && (!rcheevos_cheats_are_enabled && !rcheevos_cheats_were_enabled))

#ifdef HAVE_NETWORKING
/* FIXME: This is an ugly way to tell Netplay this... */
#define runloop_netplay_pause() netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL)
#else
#define runloop_netplay_pause() ((void)0)
#endif

#ifdef HAVE_MENU
static bool input_driver_toggle_button_combo(
      unsigned mode, input_bits_t* p_input)
{
   switch (mode)
   {
      case INPUT_TOGGLE_DOWN_Y_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L3_R3:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
      case INPUT_TOGGLE_L1_R1_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_START_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_L3_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_L_R:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case INPUT_TOGGLE_DOWN_SELECT:
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_SELECT))
            return false;
         break;
      case INPUT_TOGGLE_HOLD_START:
      {
         static rarch_timer_t timer = {0};

         if (!BIT256_GET_PTR(p_input, RETRO_DEVICE_ID_JOYPAD_START))
         {
            /* timer only runs while start is held down */
            rarch_timer_end(&timer);
            return false;
         }

         /* user started holding down the start button, start the timer */
         if (!rarch_timer_is_running(&timer))
            rarch_timer_begin(&timer, HOLD_START_DELAY_SEC);

         rarch_timer_tick(&timer);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            /* start has been held down long enough, stop timer and enter menu */
            rarch_timer_end(&timer);
            return true;
         }

         return false;
      }
      default:
      case INPUT_TOGGLE_NONE:
         return false;
   }

   return true;
}
#endif

#define HOTKEY_CHECK(cmd1, cmd2, cond, cond2) \
   { \
      static bool old_pressed                   = false; \
      bool pressed                              = BIT256_GET(current_input, cmd1); \
      if (pressed && !old_pressed) \
         if (cond) \
            command_event(cmd2, cond2); \
      old_pressed                               = pressed; \
   }

#define HOTKEY_CHECK3(cmd1, cmd2, cmd3, cmd4, cmd5, cmd6) \
   { \
      static bool old_pressed                   = false; \
      static bool old_pressed2                  = false; \
      static bool old_pressed3                  = false; \
      bool pressed                              = BIT256_GET(current_input, cmd1); \
      bool pressed2                             = BIT256_GET(current_input, cmd3); \
      bool pressed3                             = BIT256_GET(current_input, cmd5); \
      if (pressed && !old_pressed) \
         command_event(cmd2, (void*)(intptr_t)0); \
      else if (pressed2 && !old_pressed2) \
         command_event(cmd4, (void*)(intptr_t)0); \
      else if (pressed3 && !old_pressed3) \
         command_event(cmd6, (void*)(intptr_t)0); \
      old_pressed                               = pressed; \
      old_pressed2                              = pressed2; \
      old_pressed3                              = pressed3; \
   }

static enum runloop_state runloop_check_state(
      settings_t *settings,
      bool input_nonblock_state,
      bool runloop_is_paused,
      float fastforward_ratio,
      unsigned *sleep_ms)
{
   input_bits_t current_input;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
   static bool old_focus               = true;
   bool is_focused                     = false;
   bool is_alive                       = false;
   uint64_t frame_count                = 0;
   bool focused                        = true;
   bool pause_nonactive                = settings->bools.pause_nonactive;
   bool rarch_is_initialized           = rarch_is_inited;
#ifdef HAVE_MENU
   bool menu_driver_binding_state      = menu_driver_is_binding_state();
   bool menu_is_alive                  = menu_driver_is_alive();
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
#ifdef HAVE_EASTEREGG
   static uint64_t seq                 = 0;
#endif
#endif

#ifdef HAVE_LIBNX
   /* Should be called once per frame */
   if (!appletMainLoop())
      return RUNLOOP_STATE_QUIT;
#endif

   BIT256_CLEAR_ALL_PTR(&current_input);

#ifdef HAVE_MENU
   if (menu_is_alive && !(settings->bools.menu_unified_controls && !menu_input_dialog_get_display_kb()))
      input_menu_keys_pressed(settings, &current_input);
   else
#endif
      input_keys_pressed(settings, &current_input);

#ifdef HAVE_MENU
   last_input                       = current_input;
   if (
         ((menu_toggle_gamepad_combo != INPUT_TOGGLE_NONE) &&
          input_driver_toggle_button_combo(
             menu_toggle_gamepad_combo, &last_input)))
      BIT256_SET(current_input, RARCH_MENU_TOGGLE);
#endif

   if (input_driver_flushing_input)
   {
      input_driver_flushing_input = false;
      if (bits_any_set(current_input.data, ARRAY_SIZE(current_input.data)))
      {
         BIT256_CLEAR_ALL(current_input);
         if (runloop_is_paused)
            BIT256_SET(current_input, RARCH_PAUSE_TOGGLE);
         input_driver_flushing_input = true;
      }
   }

   if (!video_driver_is_threaded())
   {
      const ui_application_t *application =
         ui_companion_driver_get_application_ptr();
      if (application)
         application->process_events();
   }

   video_driver_get_status(&frame_count, &is_alive, &is_focused);

#ifdef HAVE_MENU
   if (menu_driver_binding_state)
      BIT256_CLEAR_ALL(current_input);
#endif

#ifdef HAVE_OVERLAY
   /* Check next overlay */
   HOTKEY_CHECK(RARCH_OVERLAY_NEXT, CMD_EVENT_OVERLAY_NEXT, true, NULL);
#endif

   /* Check fullscreen toggle */
   {
      bool fullscreen_toggled = !runloop_is_paused
#ifdef HAVE_MENU
         || menu_is_alive;
#else
      ;
#endif
      HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY, CMD_EVENT_FULLSCREEN_TOGGLE,
            fullscreen_toggled, NULL);
   }

   /* Check mouse grab toggle */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE, CMD_EVENT_GRAB_MOUSE_TOGGLE, true, NULL); 

#ifdef HAVE_OVERLAY
   {
      static char prev_overlay_restore = false;
      if (input_keyboard_ctl(
               RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED, NULL))
      {
         prev_overlay_restore  = false;
         command_event(CMD_EVENT_OVERLAY_INIT, NULL);
      }
      else if (prev_overlay_restore)
      {
         if (!settings->bools.input_overlay_hide_in_menu)
            command_event(CMD_EVENT_OVERLAY_INIT, NULL);
         prev_overlay_restore = false;
      }
   }
#endif

   /* Check quit key */
   {
      bool trig_quit_key;
      static bool quit_key     = false;
      static bool old_quit_key = false;
      static bool runloop_exec = false;
      quit_key                 = BIT256_GET(
            current_input, RARCH_QUIT_KEY);
      trig_quit_key            = quit_key && !old_quit_key;
      old_quit_key             = quit_key;

      /* Check double press if enabled */
      if (trig_quit_key && settings->bools.quit_press_twice)
      {
         static retro_time_t quit_key_time   = 0;
         retro_time_t cur_time = cpu_features_get_time_usec();
         trig_quit_key         = (cur_time - quit_key_time < QUIT_DELAY_USEC);
         quit_key_time         = cur_time;

         if (!trig_quit_key)
         {
            float target_hz = 0.0;

            rarch_environment_cb(
                  RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &target_hz);

            runloop_msg_queue_push(msg_hash_to_str(MSG_PRESS_AGAIN_TO_QUIT), 1,
                  QUIT_DELAY_USEC * target_hz / 1000000,
                  true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      if (time_to_exit(trig_quit_key))
      {
         bool quit_runloop = false;

         if ((runloop_max_frames != 0) && (frame_count >= runloop_max_frames)
               && runloop_max_frames_screenshot)
         {
            const char *screenshot_path = NULL;
            bool fullpath               = false;

            if (string_is_empty(runloop_max_frames_screenshot_path))
               screenshot_path          = path_get(RARCH_PATH_BASENAME);
            else
            {
               fullpath                 = true;
               screenshot_path          = runloop_max_frames_screenshot_path;
            }

            RARCH_LOG("Taking a screenshot before exiting...\n");

            /* Take a screenshot before we exit. */
            if (!take_screenshot(settings->paths.directory_screenshot,
                     screenshot_path, false,
                     video_driver_cached_frame_has_valid_framebuffer(), fullpath, false))
            {
               RARCH_ERR("Could not take a screenshot before exiting.\n");
            }
         }

         if (runloop_exec)
            runloop_exec = false;

         if (runloop_core_shutdown_initiated && settings->bools.load_dummy_on_core_shutdown)
         {
            content_ctx_info_t content_info;

            content_info.argc               = 0;
            content_info.argv               = NULL;
            content_info.args               = NULL;
            content_info.environ_get        = NULL;

            if (task_push_start_dummy_core(&content_info))
            {
               /* Loads dummy core instead of exiting RetroArch completely.
                * Aborts core shutdown if invoked. */
               rarch_ctl(RARCH_CTL_UNSET_SHUTDOWN, NULL);
               runloop_core_shutdown_initiated = false;
            }
            else
               quit_runloop = true;
         }
         else
            quit_runloop = true;

         if (quit_runloop)
         {
            old_quit_key                 = quit_key;
            retroarch_main_quit();
            return RUNLOOP_STATE_QUIT;
         }
      }
   }

#if defined(HAVE_MENU)
   menu_animation_update();

#ifdef HAVE_MENU_WIDGETS
   menu_widgets_iterate();
#endif

   if (menu_is_alive)
   {
      enum menu_action action;
      static input_bits_t old_input = {{0}};
      static enum menu_action
         old_action              = MENU_ACTION_CANCEL;
      bool focused               = false;
      input_bits_t trigger_input = current_input;
      global_t *global           = global_get_ptr();

      menu_ctx_iterate_t iter;

      retro_ctx.poll_cb();

      bits_clear_bits(trigger_input.data, old_input.data,
            ARRAY_SIZE(trigger_input.data));

      action                    = (enum menu_action)menu_event(&current_input, &trigger_input);
      focused                   = pause_nonactive ? is_focused : true;
      focused                   = focused && !ui_companion_is_on_foreground();

      iter.action               = action;

      if (global)
      {
         if (action == old_action)
         {
            retro_time_t press_time           = cpu_features_get_time_usec();

            if (action == MENU_ACTION_NOOP)
               global->menu.noop_press_time   = press_time - global->menu.noop_start_time;
            else
               global->menu.action_press_time = press_time - global->menu.action_start_time;
         }
         else
         {
            if (action == MENU_ACTION_NOOP)
            {
               global->menu.noop_start_time      = cpu_features_get_time_usec();
               global->menu.noop_press_time      = 0;

               if (global->menu.prev_action == old_action)
                  global->menu.action_start_time = global->menu.prev_start_time;
               else
                  global->menu.action_start_time = cpu_features_get_time_usec();
            }
            else
            {
               if (  global->menu.prev_action == action &&
                     global->menu.noop_press_time < 200000) /* 250ms */
               {
                  global->menu.action_start_time = global->menu.prev_start_time;
                  global->menu.action_press_time = cpu_features_get_time_usec() - global->menu.action_start_time;
               }
               else
               {
                  global->menu.prev_start_time   = cpu_features_get_time_usec();
                  global->menu.prev_action       = action;
                  global->menu.action_press_time = 0;
               }
            }
         }
      }

      if (!menu_driver_iterate(&iter))
         rarch_menu_running_finished();

      if (focused || !runloop_idle)
      {
         bool libretro_running = menu_display_libretro_running(
               rarch_is_initialized,
               (current_core_type == CORE_TYPE_DUMMY));

         menu_driver_render(runloop_idle, rarch_is_initialized,
               (current_core_type == CORE_TYPE_DUMMY)
               )
            ;
         if (settings->bools.audio_enable_menu &&
               !libretro_running)
            audio_driver_menu_sample();

#ifdef HAVE_EASTEREGG
         {
            bool library_name_is_empty = string_is_empty(runloop_system.info.library_name);

            if (library_name_is_empty && trigger_input.data[0])
            {
               seq |= trigger_input.data[0] & 0xF0;

               if (seq == 1157460427127406720ULL)
               {
                  content_ctx_info_t content_info;
                  content_info.argc                   = 0;
                  content_info.argv                   = NULL;
                  content_info.args                   = NULL;
                  content_info.environ_get            = NULL;

                  task_push_start_builtin_core(
                        &content_info,
                        CORE_TYPE_GONG, NULL, NULL);
               }

               seq <<= 8;
            }
            else if (!library_name_is_empty)
               seq = 0;
         }
#endif
      }

      old_input                 = current_input;
      old_action                = action;

      if (!focused || runloop_idle)
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
   {
#if defined(HAVE_MENU) && defined(HAVE_EASTEREGG)
      seq = 0;
#endif
      if (runloop_idle)
      {
         retro_ctx.poll_cb();
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
      }
   }

   /* Check game focus toggle */
   HOTKEY_CHECK(RARCH_GAME_FOCUS_TOGGLE, CMD_EVENT_GAME_FOCUS_TOGGLE, true, NULL);
   /* Check if we have pressed the UI companion toggle button */
   HOTKEY_CHECK(RARCH_UI_COMPANION_TOGGLE, CMD_EVENT_UI_COMPANION_TOGGLE, true, NULL);

#ifdef HAVE_MENU
   /* Check if we have pressed the menu toggle button */
   {
      static bool old_pressed = false;
      char *menu_driver       = settings->arrays.menu_driver;
      bool pressed            = BIT256_GET(
            current_input, RARCH_MENU_TOGGLE) &&
         !string_is_equal(menu_driver, "null");

      if (menu_event_kb_is_set(RETROK_F1) == 1)
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_initialized &&
                  (current_core_type != CORE_TYPE_DUMMY))
            {
               rarch_menu_running_finished();
               menu_event_kb_set(false, RETROK_F1);
            }
         }
      }
      else if ((!menu_event_kb_is_set(RETROK_F1) &&
               (pressed && !old_pressed)) ||
            (current_core_type == CORE_TYPE_DUMMY))
      {
         if (menu_driver_is_alive())
         {
            if (rarch_is_initialized &&
                  (current_core_type != CORE_TYPE_DUMMY))
               rarch_menu_running_finished();
         }
         else
         {
            menu_display_toggle_set_reason(MENU_TOGGLE_REASON_USER);
            rarch_menu_running();
         }
      }
      else
         menu_event_kb_set(false, RETROK_F1);

      old_pressed             = pressed;
   }

   /* Check if we have pressed the "send debug info" button.
    * Must press 3 times in a row to activate, but it will
    * alert the user of this with each press of the hotkey. */
   {
      int any_i;
      static uint32_t debug_seq   = 0;
      static bool old_pressed     = false;
      static bool old_any_pressed = false;
      bool any_pressed            = false;
      bool pressed                = BIT256_GET(current_input, RARCH_SEND_DEBUG_INFO);

      for (any_i = 0; any_i < ARRAY_SIZE(current_input.data); any_i++)
      {
         if (current_input.data[any_i])
         {
            any_pressed = true;
            break;
         }
      }

      if (pressed && !old_pressed)
         debug_seq |= pressed ? 1 : 0;

      switch (debug_seq)
      {
         case 1: /* pressed hotkey one time */
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO),
                  2, 180, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         case 3: /* pressed hotkey two times */
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO),
                  2, 180, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;
         case 7: /* pressed hotkey third and final time */
            debug_seq = 0;
            command_event(CMD_EVENT_SEND_DEBUG_INFO, NULL);
            break;
      }

      if (any_pressed && !old_any_pressed)
      {
         debug_seq <<= 1;

         if (debug_seq > 7)
            debug_seq = 0;
      }

      old_pressed = pressed;
      old_any_pressed = any_pressed;
   }

   /* Check if we have pressed the FPS toggle button */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   /* Check if we have pressed the netplay host toggle button */
   HOTKEY_CHECK(RARCH_NETPLAY_HOST_TOGGLE, CMD_EVENT_NETPLAY_HOST_TOGGLE, true, NULL);

   if (menu_driver_is_alive())
   {
      if (!settings->bools.menu_throttle_framerate && !fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (pause_nonactive)
      focused                = is_focused;

   /* Check if we have pressed the screenshot toggle button */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);

   /* Check if we have pressed the audio mute toggle button */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL); 

   /* Check if we have pressed the OSK toggle button */
   HOTKEY_CHECK(RARCH_OSK, CMD_EVENT_OSK_TOGGLE, true, NULL); 

   /* Check if we have pressed the recording toggle button */
   HOTKEY_CHECK(RARCH_RECORDING_TOGGLE, CMD_EVENT_RECORDING_TOGGLE, true, NULL); 

   /* Check if we have pressed the AI Service toggle button */
   HOTKEY_CHECK(RARCH_AI_SERVICE, CMD_EVENT_AI_SERVICE_TOGGLE, true, NULL); 

   /* Check if we have pressed the streaming toggle button */
   HOTKEY_CHECK(RARCH_STREAMING_TOGGLE, CMD_EVENT_STREAMING_TOGGLE, true, NULL); 

   if (BIT256_GET(current_input, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (BIT256_GET(current_input, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   /* Check Netplay */
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL); 
#endif

   /* Check if we have pressed the pause button */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool frameadvance_pressed     = BIT256_GET(
            current_input, RARCH_FRAMEADVANCE);
      bool pause_pressed            = BIT256_GET(
            current_input, RARCH_PAUSE_TOGGLE);
      bool trig_frameadvance        = frameadvance_pressed && !old_frameadvance;

      /* Check if libretro pause key was pressed. If so, pause or
       * unpause the libretro core. */

      /* FRAMEADVANCE will set us into pause mode. */
      pause_pressed                |= !runloop_is_paused && trig_frameadvance;

      if (focused && pause_pressed && !old_pause_pressed)
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
      else if (focused && !old_focus)
         command_event(CMD_EVENT_UNPAUSE, NULL);
      else if (!focused && old_focus)
         command_event(CMD_EVENT_PAUSE, NULL);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (runloop_is_paused)
      {
         bool toggle = !runloop_idle ? true : false;

         HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY,
               CMD_EVENT_FULLSCREEN_TOGGLE, true, &toggle);

         /* Check if it's not oneshot */
         if (!(trig_frameadvance || BIT256_GET(current_input, RARCH_REWIND)))
            focused = false;
      }
   }

   if (!focused)
   {
      retro_ctx.poll_cb();
      return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }

   /* Check if we have pressed the fast forward button */
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   {
      static bool old_button_state      = false;
      static bool old_hold_button_state = false;
      bool new_button_state             = BIT256_GET(
            current_input, RARCH_FAST_FORWARD_KEY);
      bool new_hold_button_state        = BIT256_GET(
            current_input, RARCH_FAST_FORWARD_HOLD_KEY);

      if (new_button_state && !old_button_state)
      {
         if (input_nonblock_state)
         {
            input_driver_unset_nonblock_state();
            runloop_fastmotion       = false;
            fastforward_after_frames = 1;
         }
         else
         {
            input_driver_set_nonblock_state();
            runloop_fastmotion = true;
         }
         driver_set_nonblock_state();
      }
      else if (old_hold_button_state != new_hold_button_state)
      {
         if (new_hold_button_state)
         {
            input_driver_set_nonblock_state();
            runloop_fastmotion = true;
         }
         else
         {
            input_driver_unset_nonblock_state();
            runloop_fastmotion       = false;
            fastforward_after_frames = 1;
         }
         driver_set_nonblock_state();
      }

      /* Display the fast forward state to the user, if needed. */
      if (runloop_fastmotion)
      {
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         if (!menu_widgets_set_fast_forward(true))
#endif
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      else
         menu_widgets_set_fast_forward(false);
#endif

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;
   }

   /* Check if we have pressed any of the state slot buttons */
   {
      static bool old_should_slot_increase = false;
      static bool old_should_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_input, RARCH_STATE_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_input, RARCH_STATE_SLOT_MINUS);
      bool should_set                      = false;
      int cur_state_slot                   = settings->ints.state_slot;

      /* Checks if the state increase/decrease keys have been pressed
       * for this frame. */
      if (should_slot_increase && !old_should_slot_increase)
      {
         configuration_set_int(settings, settings->ints.state_slot, cur_state_slot + 1);

         should_set = true;
      }
      else if (should_slot_decrease && !old_should_slot_decrease)
      {
         if (cur_state_slot > 0)
            configuration_set_int(settings, settings->ints.state_slot, cur_state_slot - 1);

         should_set = true;
      }

      if (should_set)
      {
         char msg[128];
         msg[0] = '\0';

         snprintf(msg, sizeof(msg), "%s: %d",
               msg_hash_to_str(MSG_STATE_SLOT),
               settings->ints.state_slot);

         runloop_msg_queue_push(msg, 2, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         RARCH_LOG("%s\n", msg);
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }

   /* Check if we have pressed any of the savestate buttons */
   HOTKEY_CHECK(RARCH_SAVE_STATE_KEY, CMD_EVENT_SAVE_STATE, true, NULL);
   HOTKEY_CHECK(RARCH_LOAD_STATE_KEY, CMD_EVENT_LOAD_STATE, true, NULL);

#ifdef HAVE_CHEEVOS
   rcheevos_hardcore_active = settings->bools.cheevos_enable
      && settings->bools.cheevos_hardcore_mode_enable
      && rcheevos_loaded && !rcheevos_hardcore_paused;

   if (rcheevos_hardcore_active && rcheevos_state_loaded_flag)
   {
      rcheevos_hardcore_paused = true;
      runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   if (!rcheevos_hardcore_active)
#endif
   {
      char s[128];
      bool rewinding = false;
      unsigned t     = 0;

      s[0]           = '\0';

      rewinding      = state_manager_check_rewind(BIT256_GET(current_input, RARCH_REWIND),
            settings->uints.rewind_granularity, runloop_paused, s, sizeof(s), &t);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      if (!menu_widgets_ready())
#endif
         if (rewinding)
            runloop_msg_queue_push(s, 0, t, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      menu_widgets_set_rewind(rewinding);
#endif
   }

   /* Checks if slowmotion toggle/hold was being pressed and/or held. */
#ifdef HAVE_CHEEVOS
   if (!rcheevos_hardcore_active)
#endif
   {
      static bool old_slowmotion_button_state      = false;
      static bool old_slowmotion_hold_button_state = false;
      bool new_slowmotion_button_state             = BIT256_GET(
            current_input, RARCH_SLOWMOTION_KEY);
      bool new_slowmotion_hold_button_state        = BIT256_GET(
            current_input, RARCH_SLOWMOTION_HOLD_KEY);

      if (new_slowmotion_button_state && !old_slowmotion_button_state)
         runloop_slowmotion = !runloop_slowmotion;
      else if (old_slowmotion_hold_button_state != new_slowmotion_hold_button_state)
         runloop_slowmotion = new_slowmotion_hold_button_state;

      if (runloop_slowmotion)
      {
         if (settings->bools.video_black_frame_insertion)
            if (!runloop_idle)
               video_driver_cached_frame();

#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
         if (!menu_widgets_ready())
         {
#endif
            if (state_manager_frame_is_reversed())
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 1, 1, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_SLOW_MOTION), 1, 1, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
#if defined(HAVE_MENU) && defined(HAVE_MENU_WIDGETS)
      }
#endif

      old_slowmotion_button_state                  = new_slowmotion_button_state;
      old_slowmotion_hold_button_state             = new_slowmotion_hold_button_state;
   }

   /* Check movie record toggle */
   HOTKEY_CHECK(RARCH_BSV_RECORD_TOGGLE, CMD_EVENT_BSV_RECORDING_TOGGLE, true, NULL);

   /* Check shader prev/next */
   HOTKEY_CHECK(RARCH_SHADER_NEXT, CMD_EVENT_SHADER_NEXT, true, NULL);
   HOTKEY_CHECK(RARCH_SHADER_PREV, CMD_EVENT_SHADER_PREV, true, NULL);

   /* Check if we have pressed any of the disk buttons */
   HOTKEY_CHECK3(
         RARCH_DISK_EJECT_TOGGLE, CMD_EVENT_DISK_EJECT_TOGGLE,
         RARCH_DISK_NEXT,         CMD_EVENT_DISK_NEXT,
         RARCH_DISK_PREV,         CMD_EVENT_DISK_PREV);

   /* Check if we have pressed the reset button */
   HOTKEY_CHECK(RARCH_RESET, CMD_EVENT_RESET, true, NULL);

   /* Check cheats */
   HOTKEY_CHECK3(
         RARCH_CHEAT_INDEX_PLUS,  CMD_EVENT_CHEAT_INDEX_PLUS,
         RARCH_CHEAT_INDEX_MINUS, CMD_EVENT_CHEAT_INDEX_MINUS,
         RARCH_CHEAT_TOGGLE,      CMD_EVENT_CHEAT_TOGGLE);

   if (settings->bools.video_shader_watch_files)
   {
      static rarch_timer_t timer = {0};
      static bool need_to_apply = false;

      if (video_shader_check_for_changes())
      {
         need_to_apply = true;

         if (!rarch_timer_is_running(&timer))
         {
            /* rarch_timer_t convenience functions only support whole seconds. */

            /* rarch_timer_begin */
            timer.timeout_end = cpu_features_get_time_usec() + SHADER_FILE_WATCH_DELAY_MSEC * 1000;
            timer.timer_begin = true;
            timer.timer_end   = false;
         }
      }

      /* If a file is modified atomically (moved/renamed from a different file), we have no idea how long that might take.
       * If we're trying to re-apply shaders immediately after changes are made to the original file(s), the filesystem might be in an in-between
       * state where the new file hasn't been moved over yet and the original file was already deleted. This leaves us no choice
       * but to wait an arbitrary amount of time and hope for the best.
       */
      if (need_to_apply)
      {
         /* rarch_timer_tick */
         timer.current    = cpu_features_get_time_usec();
         timer.timeout_us = (timer.timeout_end - timer.current);

         if (!timer.timer_end && rarch_timer_has_expired(&timer))
         {
            rarch_timer_end(&timer);
            need_to_apply = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   return RUNLOOP_STATE_ITERATE;
}

void runloop_set(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = true;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

void runloop_unset(enum runloop_action action)
{
   switch (action)
   {
      case RUNLOOP_ACTION_AUTOSAVE:
         runloop_autosave = false;
         break;
      case RUNLOOP_ACTION_NONE:
         break;
   }
}

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until
 * button input in order to wake up the loop,
 * -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(unsigned *sleep_ms)
{
   unsigned i;
   bool runloop_is_paused                       = runloop_paused;
   bool input_nonblock_state                    = input_driver_is_nonblock_state();
   settings_t *settings                         = config_get_ptr();
   float fastforward_ratio                      = settings->floats.fastforward_ratio;
   unsigned video_frame_delay                   = settings->uints.video_frame_delay;
   bool vrr_runloop_enable                      = settings->bools.vrr_runloop_enable;
   unsigned max_users                           = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

#ifdef HAVE_DISCORD
   if (discord_is_inited)
      discord_run_callbacks();
#endif

   if (runloop_frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */
      retro_usec_t runloop_last_frame_time = runloop_frame_time_last;
      retro_time_t current                 = cpu_features_get_time_usec();
      bool is_locked_fps                   = (runloop_is_paused || input_nonblock_state)
         | !!recording_data;
      retro_time_t delta                   = (!runloop_last_frame_time || is_locked_fps) ?
         runloop_frame_time.reference
         : (current - runloop_last_frame_time);

      if (is_locked_fps)
         runloop_frame_time_last = 0;
      else
      {
         float slowmotion_ratio  = settings->floats.slowmotion_ratio;

         runloop_frame_time_last = current;

         if (runloop_slowmotion)
            delta /= slowmotion_ratio;
      }

      runloop_frame_time.callback(delta);
   }

   switch ((enum runloop_state)
         runloop_check_state(
            settings,
            input_nonblock_state,
            runloop_is_paused,
            fastforward_ratio,
            sleep_ms))
   {
      case RUNLOOP_STATE_QUIT:
         frame_limit_last_time = 0.0;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
         runloop_netplay_pause();
         *sleep_ms = 10;
         return 1;
      case RUNLOOP_STATE_END:
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) 
               && settings->bools.menu_pause_libretro)
            runloop_netplay_pause();
#endif
         goto end;
      case RUNLOOP_STATE_MENU_ITERATE:
#ifdef HAVE_NETWORKING
         runloop_netplay_pause();
         return 0;
#endif
      case RUNLOOP_STATE_ITERATE:
         break;
   }

   if (runloop_autosave)
      autosave_lock();

   /* Used for rewinding while playback/record. */
   if (bsv_movie_state_handle)
      bsv_movie_state_handle->frame_pos[bsv_movie_state_handle->frame_ptr]
         = intfstream_tell(bsv_movie_state_handle->file);

   camera_driver_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_push_analog_dpad(general_binds, dpad_mode);
      input_push_analog_dpad(auto_binds,    dpad_mode);
   }

   if ((video_frame_delay > 0) && !input_nonblock_state)
      retro_sleep(video_frame_delay);

#ifdef HAVE_RUNAHEAD
   {
      unsigned run_ahead_num_frames = settings->uints.run_ahead_frames;
      /* Run Ahead Feature replaces the call to core_run in this loop */
      if (settings->bools.run_ahead_enabled && run_ahead_num_frames > 0
#ifdef HAVE_NETWORKING
            && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL)
#endif
         )
         run_ahead(run_ahead_num_frames, settings->bools.run_ahead_secondary_instance);
      else
         core_run();
   }
#else
   {
      core_run();
   }
#endif

   /* Increment runtime tick counter after each call to
    * core_run() or run_ahead() */
   rarch_core_runtime_tick();

#ifdef HAVE_CHEEVOS
   if (runloop_check_cheevos())
      rcheevos_test();
#endif
   cheat_manager_apply_retro_cheats();

#ifdef HAVE_DISCORD
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_GAME;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif

   for (i = 0; i < max_users; i++)
   {
      struct retro_keybind *general_binds = input_config_binds[i];
      struct retro_keybind *auto_binds    = input_autoconf_binds[i];
      enum analog_dpad_mode dpad_mode     = (enum analog_dpad_mode)settings->uints.input_analog_dpad_mode[i];

      if (dpad_mode == ANALOG_DPAD_NONE)
         continue;

      input_pop_analog_dpad(general_binds);
      input_pop_analog_dpad(auto_binds);
   }

   if (bsv_movie_state_handle)
   {
      bsv_movie_state_handle->frame_ptr    =
         (bsv_movie_state_handle->frame_ptr + 1)
         & bsv_movie_state_handle->frame_mask;

      bsv_movie_state_handle->first_rewind =
         !bsv_movie_state_handle->did_rewind;
      bsv_movie_state_handle->did_rewind   = false;
   }

   if (runloop_autosave)
      autosave_unlock();

   /* Condition for max speed x0.0 when vrr_runloop is off to skip that part */
   if (!(fastforward_ratio || vrr_runloop_enable))
      return 0;

end:
   if (vrr_runloop_enable)
   {
      struct retro_system_av_info *av_info =
         video_viewport_get_system_av_info();

      /* Sync on video only, block audio later. */
      if (fastforward_after_frames && settings->bools.audio_sync)
      {
         if (fastforward_after_frames == 1)
            command_event(CMD_EVENT_AUDIO_SET_NONBLOCKING_STATE, NULL);

         fastforward_after_frames++;

         if (fastforward_after_frames == 6)
         {
            command_event(CMD_EVENT_AUDIO_SET_BLOCKING_STATE, NULL);
            fastforward_after_frames = 0;
         }
      }

      /* Fast Forward for max speed x0.0 */
      if (!fastforward_ratio && runloop_fastmotion)
         return 0;

      frame_limit_minimum_time =
         (retro_time_t)roundf(1000000.0f / (av_info->timing.fps *
                  (runloop_fastmotion ? fastforward_ratio : 1.0f)));
   }

   {
      retro_time_t to_sleep_ms  = (
            (frame_limit_last_time + frame_limit_minimum_time)
            - cpu_features_get_time_usec()) / 1000;

      if (to_sleep_ms > 0)
      {
         *sleep_ms              = (unsigned)to_sleep_ms;
         /* Combat jitter a bit. */
         frame_limit_last_time += frame_limit_minimum_time;
         return 1;
      }
   }

   frame_limit_last_time  = cpu_features_get_time_usec();

   return 0;
}

rarch_system_info_t *runloop_get_system_info(void)
{
   return &runloop_system;
}

struct retro_system_info *runloop_get_libretro_system_info(void)
{
   struct retro_system_info *system = &runloop_system.info;
   return system;
}

char *get_retroarch_launch_arguments(void)
{
   return launch_arguments;
}

void rarch_force_video_driver_fallback(const char *driver)
{
   settings_t        *settings = config_get_ptr();
   ui_msg_window_t *msg_window = NULL;

   strlcpy(settings->arrays.video_driver,
         driver, sizeof(settings->arrays.video_driver));

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[PATH_MAX_LENGTH];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }

   exit(1);
}

void rarch_get_cpu_architecture_string(char *cpu_arch_str, size_t len)
{
   enum frontend_architecture arch = frontend_driver_get_cpu_architecture();

   if (!cpu_arch_str || !len)
      return;

   switch (arch)
   {
      case FRONTEND_ARCH_X86:
         strlcpy(cpu_arch_str, "x86", len);
         break;
      case FRONTEND_ARCH_X86_64:
         strlcpy(cpu_arch_str, "x64", len);
         break;
      case FRONTEND_ARCH_PPC:
         strlcpy(cpu_arch_str, "PPC", len);
         break;
      case FRONTEND_ARCH_ARM:
         strlcpy(cpu_arch_str, "ARM", len);
         break;
      case FRONTEND_ARCH_ARMV7:
         strlcpy(cpu_arch_str, "ARMv7", len);
         break;
      case FRONTEND_ARCH_ARMV8:
         strlcpy(cpu_arch_str, "ARMv8", len);
         break;
      case FRONTEND_ARCH_MIPS:
         strlcpy(cpu_arch_str, "MIPS", len);
         break;
      case FRONTEND_ARCH_TILE:
         strlcpy(cpu_arch_str, "Tilera", len);
         break;
      case FRONTEND_ARCH_NONE:
      default:
         strlcpy(cpu_arch_str,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               len);
         break;
   }
}

bool rarch_write_debug_info(void)
{
   int i;
   char str[PATH_MAX_LENGTH];
   char debug_filepath[PATH_MAX_LENGTH];
   gfx_ctx_mode_t mode_info              = {0};
   settings_t                  *settings = config_get_ptr();
   RFILE *file                           = NULL;
   const frontend_ctx_driver_t *frontend = frontend_get_ptr();
   const char *cpu_model                 = NULL;
   const char *path_config               = path_get(RARCH_PATH_CONFIG);
   unsigned lang                         = 
      *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);

   str[0]                                = 
      debug_filepath[0]                  = '\0';

   /* Only print our debug info in English */
   if (lang != RETRO_LANGUAGE_ENGLISH)
      msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, RETRO_LANGUAGE_ENGLISH);

   fill_pathname_resolve_relative(
         debug_filepath,
         path_config,
         DEBUG_INFO_FILENAME,
         sizeof(debug_filepath));

   file = filestream_open(debug_filepath,
         RETRO_VFS_FILE_ACCESS_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);

   if (!file)
   {
      RARCH_ERR("Could not open debug info file for writing: %s\n", debug_filepath);
      goto error;
   }

#ifdef HAVE_MENU
   {
      time_t time_;
      char timedate[255];

      timedate[0] = '\0';

      time(&time_);

      setlocale(LC_TIME, "");

      strftime(timedate, sizeof(timedate),
            "%Y-%m-%d %H:%M:%S", localtime(&time_));

      filestream_printf(file, "Log Date/Time: %s\n", timedate);
   }
#endif
   filestream_printf(file, "RetroArch Version: %s\n", PACKAGE_VERSION);

#ifdef HAVE_LAKKA
   if (frontend->get_lakka_version)
   {
      frontend->get_lakka_version(str, sizeof(str));
      filestream_printf(file, "Lakka Version: %s\n", str);
      str[0] = '\0';
   }
#endif

   filestream_printf(file, "RetroArch Build Date: %s\n", __DATE__);
#ifdef HAVE_GIT_VERSION
   filestream_printf(file, "RetroArch Git Commit: %s\n", retroarch_git_version);
#endif

   filestream_printf(file, "\n");

   cpu_model = frontend_driver_get_cpu_model_name();

   if (!string_is_empty(cpu_model))
      filestream_printf(file, "CPU Model Name: %s\n", cpu_model);

   retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));
   filestream_printf(file, "CPU Capabilities: %s\n", str);

   str[0] = '\0';

   rarch_get_cpu_architecture_string(str, sizeof(str));

   filestream_printf(file, "CPU Architecture: %s\n", str);
   filestream_printf(file, "CPU Cores: %u\n", cpu_features_get_core_amount());

   {
      uint64_t memory_used = frontend_driver_get_used_memory();
      uint64_t memory_total = frontend_driver_get_total_memory();

      filestream_printf(file, "Memory: %" PRIu64 "/%" PRIu64 " MB\n", memory_used / 1024 / 1024, memory_total / 1024 / 1024);
   }

   filestream_printf(file, "GPU Device: %s\n", !string_is_empty(video_driver_get_gpu_device_string()) ?
         video_driver_get_gpu_device_string() : "n/a");
   filestream_printf(file, "GPU API/Driver Version: %s\n", !string_is_empty(video_driver_get_gpu_api_version_string()) ?
         video_driver_get_gpu_api_version_string() : "n/a");

   filestream_printf(file, "\n");

   video_context_driver_get_video_size(&mode_info);

   filestream_printf(file, "Window Resolution: %u x %u\n", mode_info.width, mode_info.height);

   {
      float width = 0, height = 0, refresh = 0.0f;
      gfx_ctx_metrics_t metrics;
      
      metrics.type  = DISPLAY_METRIC_PIXEL_WIDTH;
      metrics.value = &width;

      video_context_driver_get_metrics(&metrics);

      metrics.type = DISPLAY_METRIC_PIXEL_HEIGHT;
      metrics.value = &height;
      video_context_driver_get_metrics(&metrics);

      video_context_driver_get_refresh_rate(&refresh);

      filestream_printf(file, "Monitor Resolution: %d x %d @ %.2f Hz (configured for %.2f Hz)\n", (int)width, (int)height, refresh, settings->floats.video_refresh_rate);
   }

   filestream_printf(file, "\n");

   str[0] = '\0';

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   filestream_printf(file, "%s\n", str);

   str[0] = '\0';

   filestream_printf(file, "Frontend Identifier: %s\n", frontend->ident);

   if (frontend->get_name)
   {
      frontend->get_name(str, sizeof(str));
      filestream_printf(file, "Frontend Name: %s\n", str);
      str[0] = '\0';
   }

   if (frontend->get_os)
   {
      int major = 0, minor = 0;
      const char *warning = "";

      frontend->get_os(str, sizeof(str), &major, &minor);

      if (strstr(str, "Build 16299"))
         warning = " (WARNING: Fall Creator's Update detected... OpenGL performance may be low)";

      filestream_printf(file, "Frontend OS: %s (v%d.%d)%s\n", str, major, minor, warning);

      str[0] = '\0';
   }

   filestream_printf(file, "\n");
   filestream_printf(file, "Input Devices (autoconfig is %s):\n", settings->bools.input_autodetect_enable ? "enabled" : "disabled");

   for (i = 0; i < 4; i++)
   {
      if (input_is_autoconfigured(i))
      {
         unsigned retro_id;
         unsigned rebind  = 0;
         unsigned device  = settings->uints.input_libretro_device[i];

         device          &= RETRO_DEVICE_MASK;

         if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
         {
            for (retro_id = 0; retro_id < RARCH_ANALOG_BIND_LIST_END; retro_id++)
            {
               char descriptor[300];
               const struct retro_keybind *keybind   = &input_config_binds[i][retro_id];
               const struct retro_keybind *auto_bind = (const struct retro_keybind*)
                  input_config_get_bind_auto(i, retro_id);

               input_config_get_bind_string(descriptor,
                     keybind, auto_bind, sizeof(descriptor));

               if (!strstr(descriptor, "Auto") 
                     && auto_bind 
                     && !auto_bind->valid 
                     && (auto_bind->joykey != 0xFFFF)
                     && !string_is_empty(auto_bind->joykey_label))
                  rebind++;
            }
         }

         if (rebind)
            filestream_printf(file, "  - Port #%d autoconfigured (WARNING: %u keys rebinded):\n", i, rebind);
         else
            filestream_printf(file, "  - Port #%d autoconfigured:\n", i);

         filestream_printf(file, "      - Device name: %s (#%d)\n",
            input_config_get_device_name(i),
            input_autoconfigure_get_device_name_index(i));
         filestream_printf(file, "      - Display name: %s\n",
            input_config_get_device_display_name(i) ?
            input_config_get_device_display_name(i) : "N/A");
         filestream_printf(file, "      - Config path: %s\n",
            input_config_get_device_display_name(i) ?
            input_config_get_device_config_path(i) : "N/A");
         filestream_printf(file, "      - VID/PID: %d/%d (0x%04X/0x%04X)\n",
            input_config_get_vid(i), input_config_get_pid(i),
            input_config_get_vid(i), input_config_get_pid(i));
      }
      else
         filestream_printf(file, "  - Port #%d not autoconfigured\n", i);
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Drivers:\n");

   {
      gfx_ctx_ident_t ident_info = {0};
      const input_driver_t *input_driver;
      const input_device_driver_t *joypad_driver;
      const char *driver;
#ifdef HAVE_MENU
      driver = menu_driver_ident();

      if (string_is_equal(driver, settings->arrays.menu_driver))
         filestream_printf(file, "  - Menu: %s\n",
               !string_is_empty(driver) ? driver : "n/a");
      else
         filestream_printf(file, "  - Menu: %s (configured for %s)\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a",
               !string_is_empty(settings->arrays.menu_driver) 
               ? settings->arrays.menu_driver 
               : "n/a");
#endif
      driver =
#ifdef HAVE_THREADS
      (video_driver_is_threaded()) ?
      video_thread_get_ident() :
#endif
      video_driver_get_ident();

      if (string_is_equal(driver, settings->arrays.video_driver))
         filestream_printf(file, "  - Video: %s\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a");
      else
         filestream_printf(file, "  - Video: %s (configured for %s)\n",
               !string_is_empty(driver) 
               ? driver 
               : "n/a",
               !string_is_empty(settings->arrays.video_driver) 
               ? settings->arrays.video_driver 
               : "n/a");

      video_context_driver_get_ident(&ident_info);
      filestream_printf(file, "  - Video Context: %s\n",
            ident_info.ident ? ident_info.ident : "n/a");

      driver = audio_driver_get_ident();

      if (string_is_equal(driver, settings->arrays.audio_driver))
         filestream_printf(file, "  - Audio: %s\n",
               !string_is_empty(driver) ? driver : "n/a");
      else
         filestream_printf(file, "  - Audio: %s (configured for %s)\n",
               !string_is_empty(driver) ? driver : "n/a",
               !string_is_empty(settings->arrays.audio_driver) ? settings->arrays.audio_driver : "n/a");

      input_driver = input_get_ptr();

      if (input_driver && string_is_equal(input_driver->ident, settings->arrays.input_driver))
         filestream_printf(file, "  - Input: %s\n", !string_is_empty(input_driver->ident) ? input_driver->ident : "n/a");
      else
         filestream_printf(file, "  - Input: %s (configured for %s)\n", !string_is_empty(input_driver->ident) ? input_driver->ident : "n/a", !string_is_empty(settings->arrays.input_driver) ? settings->arrays.input_driver : "n/a");

      joypad_driver = (input_driver->get_joypad_driver ? input_driver->get_joypad_driver(input_driver_get_data()) : NULL);

      if (joypad_driver && string_is_equal(joypad_driver->ident, settings->arrays.input_joypad_driver))
         filestream_printf(file, "  - Joypad: %s\n", !string_is_empty(joypad_driver->ident) ? joypad_driver->ident : "n/a");
      else
         filestream_printf(file, "  - Joypad: %s (configured for %s)\n", !string_is_empty(joypad_driver->ident) ? joypad_driver->ident : "n/a", !string_is_empty(settings->arrays.input_joypad_driver) ? settings->arrays.input_joypad_driver : "n/a");
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Configuration related settings:\n");
   filestream_printf(file, "  - Save on exit: %s\n", settings->bools.config_save_on_exit ? "yes" : "no");
   filestream_printf(file, "  - Load content-specific core options automatically: %s\n", settings->bools.game_specific_options ? "yes" : "no");
   filestream_printf(file, "  - Load override files automatically: %s\n", settings->bools.auto_overrides_enable ? "yes" : "no");
   filestream_printf(file, "  - Load remap files automatically: %s\n", settings->bools.auto_remaps_enable ? "yes" : "no");
   filestream_printf(file, "  - Sort saves in folders: %s\n", settings->bools.sort_savefiles_enable ? "yes" : "no");
   filestream_printf(file, "  - Sort states in folders: %s\n", settings->bools.sort_savestates_enable ? "yes" : "no");
   filestream_printf(file, "  - Write saves in content dir: %s\n", settings->bools.savefiles_in_content_dir ? "yes" : "no");
   filestream_printf(file, "  - Write savestates in content dir: %s\n", settings->bools.savestates_in_content_dir ? "yes" : "no");

   filestream_printf(file, "\n");

   filestream_printf(file, "Auto load state: %s\n", settings->bools.savestate_auto_load ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "Auto save state: %s\n", settings->bools.savestate_auto_save ? "yes" : "no");

   filestream_printf(file, "\n");

   filestream_printf(file, "Buildbot cores URL: %s\n", settings->paths.network_buildbot_url);
   filestream_printf(file, "Auto-extract downloaded archives: %s\n", settings->bools.network_buildbot_auto_extract_archive ? "yes" : "no");

   {
      size_t count = 0;
      core_info_list_t *core_info_list = NULL;
      struct string_list *list = NULL;
      const char *ext = file_path_str(FILE_PATH_RDB_EXTENSION);

      /* remove dot */
      if (!string_is_empty(ext) && ext[0] == '.' && strlen(ext) > 1)
         ext++;

      core_info_get_list(&core_info_list);

      if (core_info_list)
         count = core_info_list->count;

      filestream_printf(file, "Core info: %u entries\n", count);

      count = 0;

      list = dir_list_new(settings->paths.path_content_database, ext, false, true, false, true);

      if (list)
      {
         count = list->size;
         string_list_free(list);
      }

      filestream_printf(file, "Databases: %u entries\n", count);
   }

   filestream_printf(file, "\n");

   filestream_printf(file, "Performance and latency-sensitive features (may have a large impact depending on the core):\n");
   filestream_printf(file, "  - Video:\n");
   filestream_printf(file, "      - Runahead: %s\n", settings->bools.run_ahead_enabled ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "      - Rewind: %s\n", settings->bools.rewind_enable ? "yes (WARNING: not compatible with all cores)" : "no");
   filestream_printf(file, "      - Hard GPU Sync: %s\n", settings->bools.video_hard_sync ? "yes" : "no");
   filestream_printf(file, "      - Frame Delay: %u frames\n", settings->uints.video_frame_delay);
   filestream_printf(file, "      - Max Swapchain Images: %u\n", settings->uints.video_max_swapchain_images);
   filestream_printf(file, "      - Max Run Speed: %.1f x\n", settings->floats.fastforward_ratio);
   filestream_printf(file, "      - Sync to exact content framerate: %s\n", settings->bools.vrr_runloop_enable ? "yes (note: designed for G-Sync/FreeSync displays only)" : "no");
   filestream_printf(file, "      - Fullscreen: %s\n", settings->bools.video_fullscreen ? "yes" : "no");
   filestream_printf(file, "      - Windowed Fullscreen: %s\n", settings->bools.video_windowed_fullscreen ? "yes" : "no");
   filestream_printf(file, "      - Threaded Video: %s\n", settings->bools.video_threaded ? "yes" : "no");
   filestream_printf(file, "      - Vsync: %s\n", settings->bools.video_vsync ? "yes" : "no");
   filestream_printf(file, "      - Vsync Swap Interval: %u frames\n", settings->uints.video_swap_interval);
   filestream_printf(file, "      - Black Frame Insertion: %s\n", settings->bools.video_black_frame_insertion ? "yes" : "no");
   filestream_printf(file, "      - Bilinear Filtering: %s\n", settings->bools.video_smooth ? "yes" : "no");
   filestream_printf(file, "      - Video CPU Filter: %s\n", !string_is_empty(settings->paths.path_softfilter_plugin) ? settings->paths.path_softfilter_plugin : "n/a");
   filestream_printf(file, "      - CRT SwitchRes: %s\n", (settings->uints.crt_switch_resolution > CRT_SWITCH_NONE) ? "yes" : "no");
   filestream_printf(file, "      - Video Shared Context: %s\n", settings->bools.video_shared_context ? "yes" : "no");

   {
      video_shader_ctx_t shader_info = {0};

      video_shader_driver_get_current_shader(&shader_info);

      if (shader_info.data)
      {
         if (string_is_equal(shader_info.data->path, settings->paths.path_shader))
            filestream_printf(file, "      - Video Shader: %s\n", !string_is_empty(settings->paths.path_shader) ? settings->paths.path_shader : "n/a");
         else
            filestream_printf(file, "      - Video Shader: %s (configured for %s)\n", !string_is_empty(shader_info.data->path) ? shader_info.data->path : "n/a", !string_is_empty(settings->paths.path_shader) ? settings->paths.path_shader : "n/a");
      }
      else
         filestream_printf(file, "      - Video Shader: n/a\n");
   }

   filestream_printf(file, "  - Audio:\n");
   filestream_printf(file, "      - Audio Enabled: %s\n", settings->bools.audio_enable ? "yes" : "no (WARNING: content framerate will be incorrect)");
   filestream_printf(file, "      - Audio Sync: %s\n", settings->bools.audio_sync ? "yes" : "no (WARNING: content framerate will be incorrect)");

   {
      const char *s = NULL;

      switch (settings->uints.audio_resampler_quality)
      {
         case RESAMPLER_QUALITY_DONTCARE:
            s = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DONT_CARE);
            break;
         case RESAMPLER_QUALITY_LOWEST:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWEST);
            break;
         case RESAMPLER_QUALITY_LOWER:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_LOWER);
            break;
         case RESAMPLER_QUALITY_HIGHER:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHER);
            break;
         case RESAMPLER_QUALITY_HIGHEST:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_HIGHEST);
            break;
         case RESAMPLER_QUALITY_NORMAL:
            s = msg_hash_to_str(MSG_RESAMPLER_QUALITY_NORMAL);
            break;
      }

      filestream_printf(file, "      - Resampler Quality: %s\n", !string_is_empty(s) ? s : "n/a");
   }

   filestream_printf(file, "      - Audio Latency: %u ms\n", settings->uints.audio_latency);
   filestream_printf(file, "      - Dynamic Rate Control (DRC): %.3f\n", *audio_get_float_ptr(AUDIO_ACTION_RATE_CONTROL_DELTA));
   filestream_printf(file, "      - Max Timing Skew: %.2f\n", settings->floats.audio_max_timing_skew);
   filestream_printf(file, "      - Output Rate: %u Hz\n", settings->uints.audio_out_rate);
   filestream_printf(file, "      - DSP Plugin: %s\n", !string_is_empty(settings->paths.path_audio_dsp_plugin) ? settings->paths.path_audio_dsp_plugin : "n/a");

   {
      core_info_list_t *core_info_list = NULL;
      bool                       found = false;

      filestream_printf(file, "\n");
      filestream_printf(file, "Firmware files found:\n");

      core_info_get_list(&core_info_list);

      if (core_info_list)
      {
         unsigned i;

         for (i = 0; i < core_info_list->count; i++)
         {
            core_info_t *info = &core_info_list->list[i];

            if (!info)
               continue;

            if (info->firmware_count)
            {
               unsigned j;
               bool core_found = false;

               for (j = 0; j < info->firmware_count; j++)
               {
                  core_info_firmware_t *firmware = &info->firmware[j];
                  char path[PATH_MAX_LENGTH];

                  if (!firmware)
                     continue;

                  path[0] = '\0';

                  fill_pathname_join(
                        path,
                        settings->paths.directory_system,
                        firmware->path,
                        sizeof(path));

                  if (filestream_exists(path))
                  {
                     found = true;

                     if (!core_found)
                     {
                        core_found = true;
                        filestream_printf(file, "  - %s:\n", !string_is_empty(info->core_name) ? info->core_name : path_basename(info->path));
                     }

                     filestream_printf(file, "      - %s (%s)\n", firmware->path, firmware->optional ? "optional" : "required");
                  }
               }
            }
         }
      }

      if (!found)
         filestream_printf(file, "  - n/a\n");
   }

   filestream_close(file);

   RARCH_LOG("Wrote debug info to %s\n", debug_filepath);

   msg_hash_set_uint(MSG_HASH_USER_LANGUAGE, lang);

   return true;

error:
   return false;
}

#ifdef HAVE_NETWORKING
static void send_debug_info_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   if (task_data)
   {
      http_transfer_data_t *data = (http_transfer_data_t*)task_data;

      if (!data || data->len == 0)
      {
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
               2, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
         free(task_data);
         return;
      }

      /* don't use string_is_equal() here instead of the memcmp() because the data isn't NULL-terminated */
      if (!string_is_empty(data->data) && data->len >= 2 && !memcmp(data->data, "OK", 2))
      {
         char buf[32] = {0};
         struct string_list *list;

         memcpy(buf, data->data, data->len);

         list = string_split(buf, " ");

         if (list && list->size > 1)
         {
            unsigned id = 0;
            char msg[PATH_MAX_LENGTH];

            msg[0] = '\0';

            sscanf(list->elems[1].data, "%u", &id);

            snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SENT_DEBUG_INFO), id);

            RARCH_LOG("%s\n", msg);

            runloop_msg_queue_push(
                  msg,
                  2, 600, true,
                  NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
        }

        if (list)
           string_list_free(list);
      }
      else
      {
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

         runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
               2, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      }

      free(task_data);
   }
   else
   {
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO));

      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_SEND_DEBUG_INFO),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   }
}
#endif

void rarch_send_debug_info(void)
{
#ifdef HAVE_NETWORKING
   char debug_filepath[PATH_MAX_LENGTH];
   const char *url             = "http://lobby.libretro.com/debuginfo/add/";
   char *info_buf              = NULL;
   const size_t param_buf_size = 65535;
   char *param_buf             = (char*)malloc(param_buf_size);
   char *param_buf_tmp         = NULL;
   int param_buf_pos           = 0;
   int64_t len                 = 0;
   const char *path_config     = path_get(RARCH_PATH_CONFIG);
   bool       info_written     = rarch_write_debug_info();

   debug_filepath[0]           = 
      param_buf[0]             = '\0';

   fill_pathname_resolve_relative(
         debug_filepath,
         path_config,
         DEBUG_INFO_FILENAME,
         sizeof(debug_filepath));

   if (info_written)
      filestream_read_file(debug_filepath, (void**)&info_buf, &len);

   if (string_is_empty(info_buf) || len == 0 || !info_written)
   {
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_FAILED_TO_SAVE_DEBUG_INFO),
            2, 180, true,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      goto finish;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_SENDING_DEBUG_INFO));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_SENDING_DEBUG_INFO),
         2, 180, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   param_buf_pos = strlcpy(param_buf, "info=", param_buf_size);
   param_buf_tmp = param_buf + param_buf_pos;

   net_http_urlencode(&param_buf_tmp, info_buf);

   strlcat(param_buf, param_buf_tmp, param_buf_size - param_buf_pos);

   task_push_http_post_transfer(url, param_buf, true, NULL, send_debug_info_cb, NULL);

finish:
   if (param_buf)
      free(param_buf);
   if (info_buf)
      free(info_buf);
#endif
}

void rarch_log_file_init(void)
{
   char log_directory[PATH_MAX_LENGTH];
   char log_file_path[PATH_MAX_LENGTH];
   FILE             *fp       = NULL;
   settings_t *settings       = config_get_ptr();
   bool log_to_file           = settings->bools.log_to_file;
   bool log_to_file_timestamp = settings->bools.log_to_file_timestamp;
   bool logging_to_file       = is_logging_to_file();
   
   log_directory[0]           = '\0';
   log_file_path[0]           = '\0';
   
   /* If this is the first run, generate a timestamped log
    * file name (do this even when not outputting timestamped
    * log files, since user may decide to switch at any moment...) */
   if (string_is_empty(timestamped_log_file_name))
   {
      char format[256];
      time_t cur_time      = time(NULL);
      const struct tm *tm_ = localtime(&cur_time);
      
      format[0] = '\0';
      strftime(format, sizeof(format), "retroarch__%Y_%m_%d__%H_%M_%S", tm_);
      fill_pathname_noext(timestamped_log_file_name, format,
            file_path_str(FILE_PATH_EVENT_LOG_EXTENSION),
            sizeof(timestamped_log_file_name));
   }
   
   /* If nothing has changed, do nothing */
   if ((!log_to_file && !logging_to_file) ||
       (log_to_file && logging_to_file))
      return;
   
   /* If we are currently logging to file and wish to stop,
    * de-initialise existing logger... */
   if (!log_to_file && logging_to_file)
   {
      retro_main_log_file_deinit();
      /* ...and revert to console */
      retro_main_log_file_init(NULL, false);
      return;
   }
   
   /* If we reach this point, then we are not currently
    * logging to file, and wish to do so */
   
   /* > Check whether we are already logging to console */
   fp = (FILE*)retro_main_log_file();
   /* De-initialise existing logger */
   if (fp)
      retro_main_log_file_deinit();
   
   /* > Get directory/file paths */
   if (log_file_override_active)
   {
      /* Get log directory */
      const char *last_slash        = find_last_slash(log_file_override_path);

      if (last_slash)
      {
         char tmp_buf[PATH_MAX_LENGTH] = {0};
         size_t path_length            = last_slash + 1 - log_file_override_path;

         if ((path_length > 1) && (path_length < PATH_MAX_LENGTH))
            strlcpy(tmp_buf, log_file_override_path, path_length * sizeof(char));
         strlcpy(log_directory, tmp_buf, sizeof(log_directory));
      }
      
      /* Get log file path */
      strlcpy(log_file_path, log_file_override_path, sizeof(log_file_path));
   }
   else if (!string_is_empty(settings->paths.log_dir))
   {
      /* Get log directory */
      strlcpy(log_directory, settings->paths.log_dir, sizeof(log_directory));
      
      /* Get log file path */
      fill_pathname_join(log_file_path, settings->paths.log_dir,
            log_to_file_timestamp 
            ? timestamped_log_file_name 
            : file_path_str(FILE_PATH_DEFAULT_EVENT_LOG),
            sizeof(log_file_path));
   }
   
   /* > Attempt to initialise log file */
   if (!string_is_empty(log_file_path))
   {
      /* Create log directory, if required */
      if (!string_is_empty(log_directory))
      {
         if (!path_is_directory(log_directory))
         {
            if (!path_mkdir(log_directory))
            {
               /* Re-enable console logging and output error message */
               retro_main_log_file_init(NULL, false);
               RARCH_ERR("Failed to create system event log directory: %s\n", log_directory);
               return;
            }
         }
      }
      
      /* When RetroArch is launched, log file is overwritten.
       * On subsequent calls within the same session, it is appended to. */
      retro_main_log_file_init(log_file_path, log_file_created);
      if (is_logging_to_file())
         log_file_created = true;
      return;
   }
   
   /* If we reach this point, then something went wrong...
    * Just fall back to console logging */
   retro_main_log_file_init(NULL, false);
   RARCH_ERR("Failed to initialise system event file logging...\n");
}

void rarch_log_file_deinit(void)
{
   FILE *fp = NULL;
   
   /* De-initialise existing logger, if currently logging to file */
   if (is_logging_to_file())
      retro_main_log_file_deinit();
   
   /* If logging is currently disabled... */
   fp = (FILE*)retro_main_log_file();
   if (!fp) /* ...initialise logging to console */
      retro_main_log_file_init(NULL, false);
}

enum retro_language rarch_get_language_from_iso(const char *iso639)
{
   unsigned i;
   enum retro_language lang = RETRO_LANGUAGE_ENGLISH;

   struct lang_pair
   {
      const char *iso639;
      enum retro_language lang;
   };

   const struct lang_pair pairs[] =
   {
      {"en", RETRO_LANGUAGE_ENGLISH},
      {"ja", RETRO_LANGUAGE_JAPANESE},
      {"fr", RETRO_LANGUAGE_FRENCH},
      {"es", RETRO_LANGUAGE_SPANISH},
      {"de", RETRO_LANGUAGE_GERMAN},
      {"it", RETRO_LANGUAGE_ITALIAN},
      {"nl", RETRO_LANGUAGE_DUTCH},
      {"pt_BR", RETRO_LANGUAGE_PORTUGUESE_BRAZIL},
      {"pt_PT", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"pt", RETRO_LANGUAGE_PORTUGUESE_PORTUGAL},
      {"ru", RETRO_LANGUAGE_RUSSIAN},
      {"ko", RETRO_LANGUAGE_KOREAN},
      {"zh_CN", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_SG", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"zh_HK", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh_TW", RETRO_LANGUAGE_CHINESE_TRADITIONAL},
      {"zh", RETRO_LANGUAGE_CHINESE_SIMPLIFIED},
      {"eo", RETRO_LANGUAGE_ESPERANTO},
      {"pl", RETRO_LANGUAGE_POLISH},
      {"vi", RETRO_LANGUAGE_VIETNAMESE},
      {"ar", RETRO_LANGUAGE_ARABIC},
      {"el", RETRO_LANGUAGE_GREEK},
   };
   
   if (string_is_empty(iso639))
      return lang;

   for (i = 0; i < ARRAY_SIZE(pairs); i++)
   {
      if (strcasestr(iso639, pairs[i].iso639))
      {
         lang = pairs[i].lang;
         break;
      }
   }

   return lang;
}
