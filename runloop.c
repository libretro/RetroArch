/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-Andr� Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andr�s Su�rez (input mapper code)
 *  Copyright (C) 2016-2017 - Gregor Richards (network code)
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

#if defined(DINGUX)
#include <sys/types.h>
#include <unistd.h>
#endif

#if (defined(__linux__) || defined(__unix__) || defined(DINGUX)) && !defined(EMSCRIPTEN)
#include <signal.h>
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0500 || defined(_XBOX)
#ifndef LEGACY_WIN32
#define LEGACY_WIN32
#endif
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#include <objbase.h>
#include <process.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <locale.h>

#include <boolean.h>
#include <clamping.h>
#include <string/stdstring.h>
#include <dynamic/dylib.h>
#include <file/config_file.h>
#include <lists/string_list.h>
#include <memalign.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <encodings/utf.h>
#include <time/rtime.h>

#include <libretro.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#include <features/features_cpu.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <lists/dir_list.h>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifdef HAVE_LIBNX
#include <switch.h>
#endif

#if defined(HAVE_LAKKA) || defined(HAVE_LIBNX)
#include "switch_performance_profiles.h"
#endif

#if defined(ANDROID)
#include "play_feature_delivery/play_feature_delivery.h"
#endif

#ifdef HAVE_PRESENCE
#include "network/presence.h"
#endif
#ifdef HAVE_DISCORD
#include "network/discord.h"
#endif

#include "config.def.h"

#include "runtime_file.h"
#include "runloop.h"
#include "camera/camera_driver.h"
#include "location_driver.h"
#include "record/record_driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif

#include <audio/audio_resampler.h>

#include "audio/audio_driver.h"
#include "gfx/gfx_animation.h"
#include "gfx/gfx_display.h"
#include "gfx/gfx_thumbnail.h"
#include "gfx/video_filter.h"

#include "input/input_osk.h"

#ifdef HAVE_MENU
#include "menu/menu_cbs.h"
#include "menu/menu_driver.h"
#include "menu/menu_input.h"
#include "menu/menu_dialog.h"
#include "menu/menu_input_bind_dialog.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu/menu_shader.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#include "input/input_keymaps.h"
#include "input/input_remapping.h"

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#include "cheevos/cheevos_menu.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#include "network/netplay/netplay_private.h"
#ifdef HAVE_WIFI
#include "network/wifi_driver.h"
#endif
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "autosave.h"
#include "command.h"
#include "config.features.h"
#include "cores/internal_cores.h"
#include "content.h"
#include "core_info.h"
#include "dynamic.h"
#include "defaults.h"
#include "driver.h"
#include "msg_hash.h"
#include "paths.h"
#include "file_path_special.h"
#include "ui/ui_companion_driver.h"
#include "verbosity.h"

#include "frontend/frontend_driver.h"
#ifdef HAVE_THREADS
#include "gfx/video_thread_wrapper.h"
#endif
#include "gfx/video_display_server.h"
#ifdef HAVE_CRTSWITCHRES
#include "gfx/video_crt_switch.h"
#endif
#ifdef HAVE_BLUETOOTH
#include "bluetooth/bluetooth_driver.h"
#endif
#include "misc/cpufreq/cpufreq.h"
#include "led/led_driver.h"
#include "midi_driver.h"
#include "location_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#include "core_option_manager.h"
#ifdef HAVE_CHEATS
#include "cheat_manager.h"
#endif
#ifdef HAVE_REWIND
#include "state_manager.h"
#endif
#include "tasks/task_content.h"
#include "tasks/task_file_transfer.h"
#include "tasks/task_powerstate.h"
#include "tasks/tasks_internal.h"
#include "performance_counters.h"

#include "version.h"
#include "version_git.h"

#include "retroarch.h"

#include "accessibility.h"

#if defined(HAVE_SDL) || defined(HAVE_SDL2) || defined(HAVE_SDL_DINGUX)
#include "SDL.h"
#endif

#ifdef HAVE_LAKKA
#include "lakka.h"
#endif

#define SHADER_FILE_WATCH_DELAY_MSEC 500

#define QUIT_DELAY_USEC 3 * 1000000 /* 3 seconds */

#define DEFAULT_NETWORK_GAMEPAD_PORT 55400
#define UDP_FRAME_PACKETS 16

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

#ifdef HAVE_DYNAMIC
#define SYMBOL(x) do { \
   function_t func = dylib_proc(lib_handle_local, #x); \
   memcpy(&current_core->x, &func, sizeof(func)); \
   if (!current_core->x) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "init_libretro_symbols()"); } \
} while (0)
#else
#define SYMBOL(x) current_core->x = x
#endif

#define SYMBOL_DUMMY(x) current_core->x = libretro_dummy_##x

#ifdef HAVE_FFMPEG
#define SYMBOL_FFMPEG(x) current_core->x = libretro_ffmpeg_##x
#endif

#ifdef HAVE_MPV
#define SYMBOL_MPV(x) current_core->x = libretro_mpv_##x
#endif

#ifdef HAVE_IMAGEVIEWER
#define SYMBOL_IMAGEVIEWER(x) current_core->x = libretro_imageviewer_##x
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#define SYMBOL_NETRETROPAD(x) current_core->x = libretro_netretropad_##x
#endif

#if defined(HAVE_VIDEOPROCESSOR)
#define SYMBOL_VIDEOPROCESSOR(x) current_core->x = libretro_videoprocessor_##x
#endif

#define CORE_SYMBOLS(x) \
            x(retro_init); \
            x(retro_deinit); \
            x(retro_api_version); \
            x(retro_get_system_info); \
            x(retro_get_system_av_info); \
            x(retro_set_environment); \
            x(retro_set_video_refresh); \
            x(retro_set_audio_sample); \
            x(retro_set_audio_sample_batch); \
            x(retro_set_input_poll); \
            x(retro_set_input_state); \
            x(retro_set_controller_port_device); \
            x(retro_reset); \
            x(retro_run); \
            x(retro_serialize_size); \
            x(retro_serialize); \
            x(retro_unserialize); \
            x(retro_cheat_reset); \
            x(retro_cheat_set); \
            x(retro_load_game); \
            x(retro_load_game_special); \
            x(retro_unload_game); \
            x(retro_get_region); \
            x(retro_get_memory_data); \
            x(retro_get_memory_size);

#ifdef _WIN32
#define PERF_LOG_FMT "[PERF]: Avg (%s): %I64u ticks, %I64u runs.\n"
#else
#define PERF_LOG_FMT "[PERF]: Avg (%s): %llu ticks, %llu runs.\n"
#endif

static runloop_state_t runloop_state      = {0};

/* GLOBAL POINTER GETTERS */
runloop_state_t *runloop_state_get_ptr(void)
{
   return &runloop_state;
}

#ifdef HAVE_REWIND
bool state_manager_frame_is_reversed(void)
{
   return runloop_state.rewind_st.frame_is_reversed;
}
#endif

content_state_t *content_state_get_ptr(void)
{
   return &runloop_state.content_st;
}

/* Get the current subsystem rom id */
unsigned content_get_subsystem_rom_id(void)
{
   return runloop_state.content_st.pending_subsystem_rom_id;
}

/* Get the current subsystem */
int content_get_subsystem(void)
{
   return runloop_state.content_st.pending_subsystem_id;
}

struct retro_perf_counter **retro_get_perf_counter_libretro(void)
{
   return runloop_state.perf_counters_libretro;
}

unsigned retro_get_perf_count_libretro(void)
{
   return runloop_state.perf_ptr_libretro;
}

void runloop_performance_counter_register(struct retro_perf_counter *perf)
{
   if (     perf->registered 
         || runloop_state.perf_ptr_libretro >= MAX_COUNTERS)
      return;

   runloop_state.perf_counters_libretro[runloop_state.perf_ptr_libretro++] = perf;
   perf->registered = true;
}

void runloop_log_counters(
      struct retro_perf_counter **counters, unsigned num)
{
   int i;
   for (i = 0; i < num; i++)
   {
      if (counters[i]->call_cnt)
      {
         RARCH_LOG(PERF_LOG_FMT,
               counters[i]->ident,
               (uint64_t)counters[i]->total /
               (uint64_t)counters[i]->call_cnt,
               (uint64_t)counters[i]->call_cnt);
      }
   }
}

void runloop_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   runloop_log_counters(runloop_state.perf_counters_libretro,
         runloop_state.perf_ptr_libretro);
}

static bool runloop_environ_cb_get_system_info(unsigned cmd, void *data)
{
   runloop_state_t *runloop_st  = &runloop_state;
   rarch_system_info_t *system  = &runloop_st->system;

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
         *runloop_st->load_no_content_hook = *(const bool*)data;
         break;
      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         size_t i, j, size;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         settings_t *settings    = config_get_ptr();
         unsigned log_level      = settings->uints.libretro_log_level;

         runloop_st->subsystem_current_count = 0;

         RARCH_LOG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Subsystem ID: %d\nSpecial game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  i,
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         size = i;

         if (log_level == RETRO_LOG_DEBUG)
         {
            RARCH_LOG("Subsystems: %d\n", i);
            if (size > SUBSYSTEM_MAX_SUBSYSTEMS)
               RARCH_WARN("Subsystems exceed subsystem max, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEMS);
         }

         if (system)
         {
            for (i = 0; i < size && i < SUBSYSTEM_MAX_SUBSYSTEMS; i++)
            {
               struct retro_subsystem_info *subsys_info = &runloop_st->subsystem_data[i];
               struct retro_subsystem_rom_info *subsys_rom_info = runloop_st->subsystem_data_roms[i];
               /* Nasty, but have to do it like this since
                * the pointers are const char *
                * (if we don't free them, we get a memory leak) */
               if (!string_is_empty(subsys_info->desc))
                  free((char *)subsys_info->desc);
               if (!string_is_empty(subsys_info->ident))
                  free((char *)subsys_info->ident);
               subsys_info->desc     = strdup(info[i].desc);
               subsys_info->ident    = strdup(info[i].ident);
               subsys_info->id       = info[i].id;
               subsys_info->num_roms = info[i].num_roms;

               if (log_level == RETRO_LOG_DEBUG)
                  if (subsys_info->num_roms > SUBSYSTEM_MAX_SUBSYSTEM_ROMS)
                     RARCH_WARN("Subsystems exceed subsystem max roms, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEM_ROMS);

               for (j = 0; j < subsys_info->num_roms && j < SUBSYSTEM_MAX_SUBSYSTEM_ROMS; j++)
               {
                  /* Nasty, but have to do it like this since
                   * the pointers are const char *
                   * (if we don't free them, we get a memory leak) */
                  if (!string_is_empty(subsys_rom_info[j].desc))
                     free((char *)
                           subsys_rom_info[j].desc);
                  if (!string_is_empty(
                           subsys_rom_info[j].valid_extensions))
                     free((char *)
                           subsys_rom_info[j].valid_extensions);
                  subsys_rom_info[j].desc             = 
                     strdup(info[i].roms[j].desc);
                  subsys_rom_info[j].valid_extensions = 
                     strdup(info[i].roms[j].valid_extensions);
                  subsys_rom_info[j].required         = 
                     info[i].roms[j].required;
                  subsys_rom_info[j].block_extract    = 
                     info[i].roms[j].block_extract;
                  subsys_rom_info[j].need_fullpath    = 
                     info[i].roms[j].need_fullpath;
               }

               subsys_info->roms = subsys_rom_info;
            }

            runloop_st->subsystem_current_count =
               size <= SUBSYSTEM_MAX_SUBSYSTEMS
               ? (unsigned)size
               : SUBSYSTEM_MAX_SUBSYSTEMS;
         }
         break;
      }
      default:
         return false;
   }

   return true;
}


#ifdef HAVE_DYNAMIC
/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
void libretro_get_environment_info(
      void (*func)(retro_environment_t),
      bool *load_no_content)
{
   runloop_state_t *runloop_st      = &runloop_state;

   runloop_st->load_no_content_hook = load_no_content;

   /* load_no_content gets set in this callback. */
   func(runloop_environ_cb_get_system_info);

   /* It's possible that we just set get_system_info callback
    * to the currently running core.
    *
    * Make sure we reset it to the actual environment callback.
    * Ignore any environment callbacks here in case we're running
    * on the non-current core. */
   runloop_st->flags |=  RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB;
   func(runloop_environment_cb);
   runloop_st->flags &= ~RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB;
}

static dylib_t load_dynamic_core(const char *path, char *buf, 
		size_t size)
{
#if defined(ANDROID)
   /* Can't resolve symlinks when dealing with cores
    * installed via play feature delivery, because the
    * source files have non-standard file names (which
    * will not be recognised by regular core handling
    * routines) */
   bool resolve_symlinks = !play_feature_delivery_enabled();
#else
   bool resolve_symlinks = true;
#endif

   /* Can't lookup symbols in itself on UWP */
#if !(defined(__WINRT__) || defined(WINAPI_FAMILY) && WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   if (dylib_proc(NULL, "retro_init"))
   {
      /* Try to verify that -lretro was not linked in from other modules
       * since loading it dynamically and with -l will fail hard. */
      RARCH_ERR("Serious problem. RetroArch wants to load libretro cores"
            " dynamically, but it is already linked.\n");
      RARCH_ERR("This could happen if other modules RetroArch depends on "
            "link against libretro directly.\n");
      RARCH_ERR("Proceeding could cause a crash. Aborting ...\n");
      retroarch_fail(1, "load_dynamic_core()");
   }
#endif

   /* Need to use absolute path for this setting. It can be
    * saved to content history, and a relative path would
    * break in that scenario. */
   path_resolve_realpath(buf, size, resolve_symlinks);
   return dylib_load(path);
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *info, bool *load_no_content)
{
   dylib_t lib = dylib_load(path);
   void (*proc)(struct retro_system_info*);

   if (!lib)
      return NULL;

   proc = (void (*)(struct retro_system_info*))
      dylib_proc(lib, "retro_get_system_info");

   if (!proc)
   {
      dylib_close(lib);
      return NULL;
   }

   proc(info);

   if (load_no_content)
   {
      void (*set_environ)(retro_environment_t);
      *load_no_content = false;
      set_environ = (void (*)(retro_environment_t))
         dylib_proc(lib, "retro_set_environment");

      if (set_environ)
         libretro_get_environment_info(set_environ, load_no_content);
   }

   return lib;
}
#endif

static void runloop_update_runtime_log(
      runloop_state_t *runloop_st,
      const char *dir_runtime_log,
      const char *dir_playlist,
      bool log_per_core)
{
   /* Initialise runtime log file */
   runtime_log_t *runtime_log   = runtime_log_init(
         runloop_st->runtime_content_path,
         runloop_st->runtime_core_path,
         dir_runtime_log,
         dir_playlist,
         log_per_core);

   if (!runtime_log)
      return;

   /* Add additional runtime */
   runtime_log_add_runtime_usec(runtime_log,
         runloop_st->core_runtime_usec);

   /* Update 'last played' entry */
   runtime_log_set_last_played_now(runtime_log);

   /* Save runtime log file */
   runtime_log_save(runtime_log);

   /* Clean up */
   free(runtime_log);
}


void runloop_runtime_log_deinit(
      runloop_state_t *runloop_st,
      bool content_runtime_log,
      bool content_runtime_log_aggregate,
      const char *dir_runtime_log,
      const char *dir_playlist)
{
   if (verbosity_is_enabled())
   {
      int n;
      char log[PATH_MAX_LENGTH] = {0};
      unsigned hours            = 0;
      unsigned minutes          = 0;
      unsigned seconds          = 0;

      runtime_log_convert_usec2hms(
            runloop_st->core_runtime_usec,
            &hours, &minutes, &seconds);

      n                         =
         snprintf(log, sizeof(log),
               "[Core]: Content ran for a total of:"
               " %02u hours, %02u minutes, %02u seconds.",
               hours, minutes, seconds);
      if ((n < 0) || (n >= PATH_MAX_LENGTH))
         n = 0; /* Just silence any potential gcc warnings... */
      (void)n;
      RARCH_LOG("%s\n",log);
   }

   /* Only write to file if content has run for a non-zero length of time */
   if (runloop_st->core_runtime_usec > 0)
   {
      /* Per core logging */
      if (content_runtime_log)
         runloop_update_runtime_log(runloop_st, dir_runtime_log, dir_playlist, true);

      /* Aggregate logging */
      if (content_runtime_log_aggregate)
         runloop_update_runtime_log(runloop_st, dir_runtime_log, dir_playlist, false);
   }

   /* Reset runtime + content/core paths, to prevent any
    * possibility of duplicate logging */
   runloop_st->core_runtime_usec = 0;
   memset(runloop_st->runtime_content_path, 0,
         sizeof(runloop_st->runtime_content_path));
   memset(runloop_st->runtime_core_path, 0,
         sizeof(runloop_st->runtime_core_path));
}

static bool runloop_clear_all_thread_waits(
      unsigned clear_threads, void *data)
{
   if (clear_threads > 0)
      audio_driver_start(false);
   else
      audio_driver_stop();

   return true;
}

static bool dynamic_verify_hw_context(
      const char *video_ident,
      bool driver_switch_enable,
      enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   if (driver_switch_enable)
      return true;

   switch (type)
   {
      case RETRO_HW_CONTEXT_VULKAN:
         if (!string_is_equal(video_ident, "vulkan"))
            return false;
         break;
#if defined(HAVE_OPENGL_CORE)
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         if (!string_is_equal(video_ident, "glcore"))
            return false;
         break;
#else
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#endif
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
         if (!string_is_equal(video_ident, "gl") &&
             !string_is_equal(video_ident, "glcore"))
            return false;
         break;
      case RETRO_HW_CONTEXT_DIRECT3D:
         if (!(string_is_equal(video_ident, "d3d11") && major == 11))
            return false;
         break;
      default:
         break;
   }

   return true;
}

static bool dynamic_request_hw_context(enum retro_hw_context_type type,
      unsigned minor, unsigned major)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_NONE:
         RARCH_LOG("Requesting no HW context.\n");
         break;

      case RETRO_HW_CONTEXT_VULKAN:
#ifdef HAVE_VULKAN
         RARCH_LOG("Requesting Vulkan context.\n");
         break;
#else
         RARCH_ERR("Requesting Vulkan context, but RetroArch is not compiled against Vulkan. Cannot use HW context.\n");
         return false;
#endif

#if defined(HAVE_OPENGLES)

#if (defined(HAVE_OPENGLES2) || defined(HAVE_OPENGLES3))
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_LOG("Requesting OpenGLES%u context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         break;

#if defined(HAVE_OPENGLES3)
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
#ifndef HAVE_OPENGLES3_2
         if (major == 3 && minor == 2)
         {
            RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch is compiled against a lesser version. Cannot use HW context.\n",
                  major, minor);
            return false;
         }
#endif
#if !defined(HAVE_OPENGLES3_2) && !defined(HAVE_OPENGLES3_1)
         if (major == 3 && minor == 1)
         {
            RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch is compiled against a lesser version. Cannot use HW context.\n",
                  major, minor);
            return false;
         }
#endif
         RARCH_LOG("Requesting OpenGLES%u.%u context.\n",
               major, minor);
         break;
#endif

#endif
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
         RARCH_ERR("Requesting OpenGL context, but RetroArch "
               "is compiled against OpenGLES. Cannot use HW context.\n");
         return false;

#elif defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
         RARCH_ERR("Requesting OpenGLES%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               type == RETRO_HW_CONTEXT_OPENGLES2 ? 2 : 3);
         return false;

      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         RARCH_ERR("Requesting OpenGLES%u.%u context, but RetroArch "
               "is compiled against OpenGL. Cannot use HW context.\n",
               major, minor);
         return false;

      case RETRO_HW_CONTEXT_OPENGL:
         RARCH_LOG("Requesting OpenGL context.\n");
         break;

      case RETRO_HW_CONTEXT_OPENGL_CORE:
         /* TODO/FIXME - we should do a check here to see if
          * the requested core GL version is supported */
         RARCH_LOG("Requesting core OpenGL context (%u.%u).\n",
               major, minor);
         break;
#endif

#if defined(HAVE_D3D9) || defined(HAVE_D3D11)
      case RETRO_HW_CONTEXT_DIRECT3D:
         switch (major)
         {
#ifdef HAVE_D3D9
            case 9:
               RARCH_LOG("Requesting D3D9 context.\n");
               break;
#endif
#ifdef HAVE_D3D11
            case 11:
               RARCH_LOG("Requesting D3D11 context.\n");
               break;
#endif
            default:
               RARCH_LOG("Requesting unknown context.\n");
               return false;
         }
         break;
#endif

      default:
         RARCH_LOG("Requesting unknown context.\n");
         return false;
   }

   return true;
}

static void libretro_log_cb(
      enum retro_log_level level,
      const char *fmt, ...)
{
   va_list vp;
   settings_t        *settings = config_get_ptr();
   unsigned libretro_log_level = settings->uints.libretro_log_level;

   if ((unsigned)level < libretro_log_level)
      return;

   if (!verbosity_is_enabled())
      return;

   va_start(vp, fmt);

   switch (level)
   {
      case RETRO_LOG_DEBUG:
         RARCH_LOG_V("[libretro DEBUG]", fmt, vp);
         break;

      case RETRO_LOG_INFO:
         RARCH_LOG_OUTPUT_V("[libretro INFO]", fmt, vp);
         break;

      case RETRO_LOG_WARN:
         RARCH_WARN_V("[libretro WARN]", fmt, vp);
         break;

      case RETRO_LOG_ERROR:
         RARCH_ERR_V("[libretro ERROR]", fmt, vp);
         break;

      default:
         break;
   }

   va_end(vp);
}

static size_t mmap_add_bits_down(size_t n)
{
   n |= n >>  1;
   n |= n >>  2;
   n |= n >>  4;
   n |= n >>  8;
   n |= n >> 16;

   /* double shift to avoid warnings on 32bit (it's dead code,
    * but compilers suck) */
   if (sizeof(size_t) > 4)
      n |= n >> 16 >> 16;

   return n;
}

static size_t mmap_inflate(size_t addr, size_t mask)
{
    while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;

      /* to put in an 1 bit instead, OR in tmp+1 */
      addr       = ((addr & ~tmp) << 1) | (addr & tmp);
      mask       = mask & (mask - 1);
   }

   return addr;
}

static size_t mmap_reduce(size_t addr, size_t mask)
{
   while (mask)
   {
      size_t tmp = (mask - 1) & ~mask;
      addr       = (addr & tmp) | ((addr >> 1) & ~tmp);
      mask       = (mask & (mask - 1)) >> 1;
   }

   return addr;
}


static size_t mmap_highest_bit(size_t n)
{
   n = mmap_add_bits_down(n);
   return n ^ (n >> 1);
}

static bool mmap_preprocess_descriptors(
      rarch_memory_descriptor_t *first, unsigned count)
{
   size_t                      top_addr = 1;
   rarch_memory_descriptor_t *desc      = NULL;
   const rarch_memory_descriptor_t *end = first + count;
   size_t             highest_reachable = 0;

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select != 0)
         top_addr |= desc->core.select;
      else
         top_addr |= desc->core.start + desc->core.len - 1;
   }

   top_addr = mmap_add_bits_down(top_addr);

   for (desc = first; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         if (desc->core.len == 0)
            return false;

         if ((desc->core.len & (desc->core.len - 1)) != 0)
            return false;

         desc->core.select = top_addr & ~mmap_inflate(mmap_add_bits_down(desc->core.len - 1),
               desc->core.disconnect);
      }

      if (desc->core.len == 0)
         desc->core.len = mmap_add_bits_down(mmap_reduce(top_addr & ~desc->core.select,
                  desc->core.disconnect)) + 1;

      if (desc->core.start & ~desc->core.select)
         return false;

      highest_reachable = mmap_inflate(desc->core.len - 1, desc->core.disconnect);

      /* Disconnect unselected bits that are too high to ever
       * index into the core's buffer. Higher addresses will
       * repeat / mirror the buffer as long as they match select */
      while (mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect) >
                mmap_highest_bit(highest_reachable))
         desc->core.disconnect |= mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect);
   }

   return true;
}

static void runloop_deinit_core_options(
      bool game_options_active,
      const char *path_core_options,
      core_option_manager_t *core_options)
{
   /* Check whether game-specific options file is being used */
   if (!string_is_empty(path_core_options))
   {
      config_file_t *conf_tmp = NULL;

      /* We only need to save configuration settings for
       * the current core
       * > If game-specific options file exists, have
       *   to read it (to ensure file only gets written
       *   if config values change)
       * > Otherwise, create a new, empty config_file_t
       *   object */
      if (path_is_valid(path_core_options))
         conf_tmp = config_file_new_from_path_to_string(path_core_options);

      if (!conf_tmp)
         conf_tmp = config_file_new_alloc();

      if (conf_tmp)
      {
         core_option_manager_flush(
               core_options,
               conf_tmp);
         RARCH_LOG("[Core]: Saved %s-specific core options to \"%s\".\n",
               game_options_active ? "game" : "folder", path_core_options);
         config_file_write(conf_tmp, path_core_options, true);
         config_file_free(conf_tmp);
         conf_tmp = NULL;
      }
      path_clear(RARCH_PATH_CORE_OPTIONS);
   }
   else
   {
      const char *path = core_options->conf_path;
      core_option_manager_flush(
            core_options,
            core_options->conf);
      RARCH_LOG("[Core]: Saved core options file to \"%s\".\n", path);
      config_file_write(core_options->conf, path, true);
   }

   if (core_options)
      core_option_manager_free(core_options);
}

static bool validate_per_core_options(char *s,
      size_t len, bool mkdir,
      const char *core_name, const char *game_name)
{
   char config_directory[PATH_MAX_LENGTH];
   config_directory[0] = '\0';

   if (   (!s)
       || (len < 1)
       || string_is_empty(core_name)
       || string_is_empty(game_name))
      return false;

   fill_pathname_application_special(config_directory,
         sizeof(config_directory), APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   fill_pathname_join_special_ext(s,
         config_directory, core_name, game_name,
         ".opt", len);

   /* No need to make a directory if file already exists... */
   if (mkdir && !path_is_valid(s))
   {
      char new_path[PATH_MAX_LENGTH];
      fill_pathname_join_special(new_path,
            config_directory, core_name, sizeof(new_path));
      if (!path_is_directory(new_path))
         path_mkdir(new_path);
   }

   return true;
}


static bool validate_game_options(
      const char *core_name,
      char *s, size_t len, bool mkdir)
{
   const char *game_name = path_basename_nocompression(path_get(RARCH_PATH_BASENAME));
   return validate_per_core_options(s, len, mkdir,
         core_name, game_name);
}

/**
 * game_specific_options:
 *
 * @return true if a game specific core
 * options path has been found, otherwise false.
 **/
static bool validate_game_specific_options(char **output)
{
   char game_options_path[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st = &runloop_state;
   game_options_path[0]        = '\0';

   if (!validate_game_options(
            runloop_st->system.info.library_name,
            game_options_path,
            sizeof(game_options_path), false) ||
       !path_is_valid(game_options_path))
      return false;

   RARCH_LOG("[Core]: %s \"%s\".\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_options_path);
   *output = strdup(game_options_path);
   return true;
}

static bool validate_folder_options(
      char *s, size_t len, bool mkdir)
{
   char folder_name[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st = &runloop_state;
   const char *core_name       = runloop_st->system.info.library_name;
   const char *game_path       = path_get(RARCH_PATH_BASENAME);

   folder_name[0] = '\0';

   if (string_is_empty(game_path))
      return false;

   fill_pathname_parent_dir_name(folder_name,
         game_path, sizeof(folder_name));

   return validate_per_core_options(s, len, mkdir,
         core_name, folder_name);
}


/**
 * validate_folder_specific_options:
 *
 * @return true if a folder specific core
 * options path has been found, otherwise false.
 **/
static bool validate_folder_specific_options(
      char **output)
{
   char folder_options_path[PATH_MAX_LENGTH];
   folder_options_path[0] ='\0';

   if (!validate_folder_options(
            folder_options_path,
            sizeof(folder_options_path), false) ||
       !path_is_valid(folder_options_path))
      return false;

   RARCH_LOG("[Core]: %s \"%s\".\n",
         msg_hash_to_str(MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         folder_options_path);
   *output = strdup(folder_options_path);
   return true;
}

/**
 * runloop_init_core_options_path:
 *
 * Fetches core options path for current core/content
 * - path: path from which options should be read
 *   from/saved to
 * - src_path: in the event that 'path' file does not
 *   yet exist, provides source path from which initial
 *   options should be extracted
 *
 *   NOTE: caller must ensure 
 *   path and src_path are NULL-terminated
 *
 **/
static void runloop_init_core_options_path(
      settings_t *settings,
      char *path, size_t len,
      char *src_path, size_t src_len)
{
   char *game_options_path        = NULL;
   char *folder_options_path      = NULL;
   runloop_state_t *runloop_st    = &runloop_state;
   bool game_specific_options     = settings->bools.game_specific_options;

   /* Check whether game-specific options exist */
   if (game_specific_options &&
       validate_game_specific_options(&game_options_path))
   {
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, game_options_path);
      runloop_st->flags &= ~RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;

      /* Copy options path */
      strlcpy(path, game_options_path, len);

      free(game_options_path);
   }
   /* Check whether folder-specific options exist */
   else if (game_specific_options &&
            validate_folder_specific_options(
               &folder_options_path))
   {
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, folder_options_path);
      runloop_st->flags &= ~RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;

      /* Copy options path */
      strlcpy(path, folder_options_path, len);

      free(folder_options_path);
   }
   else
   {
      char global_options_path[PATH_MAX_LENGTH];
      char per_core_options_path[PATH_MAX_LENGTH];
      bool per_core_options_exist   = false;
      bool per_core_options         = !settings->bools.global_core_options;
      const char *path_core_options = settings->paths.path_core_options;

      per_core_options_path[0]      = '\0';

      if (per_core_options)
      {
         const char *core_name      = runloop_st->system.info.library_name;
         /* Get core-specific options path
          * > if validate_per_core_options() returns
          *   false, then per-core options are disabled (due to
          *   unknown system errors...) */
         per_core_options = validate_per_core_options(
               per_core_options_path, sizeof(per_core_options_path), true,
               core_name, core_name);

         /* If we can use per-core options, check whether an options
          * file already exists */
         if (per_core_options)
            per_core_options_exist = path_is_valid(per_core_options_path);
      }

      /* If not using per-core options, or if a per-core options
       * file does not yet exist, must fetch 'global' options path */
      if (!per_core_options || !per_core_options_exist)
      {
         const char *options_path   = path_core_options;

         if (!string_is_empty(options_path))
            strlcpy(global_options_path,
                  options_path, sizeof(global_options_path));
         else if (!path_is_empty(RARCH_PATH_CONFIG))
            fill_pathname_resolve_relative(
                  global_options_path, path_get(RARCH_PATH_CONFIG),
                  FILE_PATH_CORE_OPTIONS_CONFIG, sizeof(global_options_path));
      }

      /* Allocate correct path/src_path strings */
      if (per_core_options)
      {
         strlcpy(path, per_core_options_path, len);

         if (!per_core_options_exist)
            strlcpy(src_path, global_options_path, src_len);
      }
      else
         strlcpy(path, global_options_path, len);

      /* Notify system that we *do not* have a valid core options
       * options override */
      runloop_st->flags &= ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                           | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
   }
}

static core_option_manager_t *runloop_init_core_options(
      settings_t *settings,
      const struct retro_core_options_v2 *options_v2)
{
   bool categories_enabled = settings->bools.core_option_category_enable;
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];

   /* Ensure these are NULL-terminated */
   options_path[0]     = '\0';
   src_options_path[0] = '\0';

   /* Get core options file path */
   runloop_init_core_options_path(settings,
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      return core_option_manager_new(options_path,
            src_options_path, options_v2,
            categories_enabled);
   return NULL;
}

static core_option_manager_t *runloop_init_core_variables(
      settings_t *settings, const struct retro_variable *vars)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];

   /* Ensure these are NULL-terminated */
   options_path[0]     = '\0';
   src_options_path[0] = '\0';

   /* Get core options file path */
   runloop_init_core_options_path(
         settings,
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));

   if (!string_is_empty(options_path))
      return core_option_manager_new_vars(options_path, src_options_path, vars);
   return NULL;
}

static void runloop_core_msg_queue_push(
      struct retro_system_av_info *av_info,
      const struct retro_message_ext *msg)
{
   double fps;
   unsigned duration_frames;
   enum message_queue_category category;

   /* Assign category */
   switch (msg->level)
   {
      case RETRO_LOG_WARN:
         category = MESSAGE_QUEUE_CATEGORY_WARNING;
         break;
      case RETRO_LOG_ERROR:
         category = MESSAGE_QUEUE_CATEGORY_ERROR;
         break;
      case RETRO_LOG_INFO:
      case RETRO_LOG_DEBUG:
      default:
         category = MESSAGE_QUEUE_CATEGORY_INFO;
         break;
   }

   /* Get duration in frames */
   fps             = (av_info && (av_info->timing.fps > 0)) ? av_info->timing.fps : 60.0;
   duration_frames = (unsigned)((fps * (float)msg->duration / 1000.0f) + 0.5f);

   /* Note: Do not flush the message queue here - a core
    * may need to send multiple notifications simultaneously */
   runloop_msg_queue_push(msg->msg,
         msg->priority, duration_frames,
         false, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
         category);
}

static void core_performance_counter_start(
      struct retro_perf_counter *perf)
{
   runloop_state_t *runloop_st = &runloop_state;
   bool runloop_perfcnt_enable = runloop_st->perfcnt_enable;

   if (runloop_perfcnt_enable)
   {
      perf->call_cnt++;
      perf->start              = cpu_features_get_perf_counter();
   }
}

static void core_performance_counter_stop(struct retro_perf_counter *perf)
{
   runloop_state_t *runloop_st = &runloop_state;
   bool runloop_perfcnt_enable = runloop_st->perfcnt_enable;

   if (runloop_perfcnt_enable)
      perf->total += cpu_features_get_perf_counter() - perf->start;
}


bool runloop_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   runloop_state_t *runloop_st            = &runloop_state;
   recording_state_t *recording_st        = recording_state_get_ptr();
   settings_t         *settings           = config_get_ptr();
   rarch_system_info_t *system            = &runloop_st->system;
   bool ignore_environment_cb             = runloop_st->flags &
      RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB;

   if (ignore_environment_cb)
      return false;

   /* RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE gets called
    * by every core on every frame. Handle it first,
    * to avoid the overhead of traversing the subsequent
    * (enormous) case statement */
   if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
   {
      if (runloop_st->core_options)
         *(bool*)data = runloop_st->core_options->updated;
      else
         *(bool*)data = false;

      return true;
   }

   switch (cmd)
   {
      case RETRO_ENVIRONMENT_GET_OVERSCAN:
         {
            bool video_crop_overscan = settings->bools.video_crop_overscan;
            *(bool*)data = !video_crop_overscan;
            RARCH_LOG("[Environ]: GET_OVERSCAN: %u\n",
                  (unsigned)!video_crop_overscan);
         }
         break;

      case RETRO_ENVIRONMENT_GET_CAN_DUPE:
         *(bool*)data = true;
         RARCH_LOG("[Environ]: GET_CAN_DUPE: true\n");
         break;

      case RETRO_ENVIRONMENT_GET_VARIABLE:
         {
            unsigned log_level         = settings->uints.libretro_log_level;
            struct retro_variable *var = (struct retro_variable*)data;
            size_t opt_idx;

            if (!var)
               return true;

            var->value = NULL;

            if (!runloop_st->core_options)
            {
               RARCH_LOG("[Environ]: GET_VARIABLE %s: not implemented.\n",
                     var->key);
               return true;
            }

#ifdef HAVE_RUNAHEAD
            if (runloop_st->core_options->updated)
               runloop_st->flags |= RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
#endif
            runloop_st->core_options->updated = false;

            if (core_option_manager_get_idx(runloop_st->core_options,
                  var->key, &opt_idx))
               var->value = core_option_manager_get_val(
                     runloop_st->core_options, opt_idx);

            if (log_level == RETRO_LOG_DEBUG)
            {
               char s[128];
               s[0] = '\0';

               snprintf(s, sizeof(s), "[Environ]: GET_VARIABLE: %s = \"%s\"\n",
                     var->key, var->value ? var->value :
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
               RARCH_LOG("%s", s);
            }
         }

         break;

      case RETRO_ENVIRONMENT_SET_VARIABLE:
         {
            unsigned log_level               = settings->uints.libretro_log_level;
            const struct retro_variable *var = (const struct retro_variable*)data;
            size_t opt_idx;
            size_t val_idx;

            /* If core passes NULL to the callback, return
             * value indicates whether callback is supported */
            if (!var)
               return true;

            if (string_is_empty(var->key) ||
                string_is_empty(var->value))
               return false;

            if (!runloop_st->core_options)
            {
               RARCH_LOG("[Environ]: SET_VARIABLE %s: not implemented.\n",
                     var->key);
               return false;
            }

            /* Check whether key is valid */
            if (!core_option_manager_get_idx(runloop_st->core_options,
                  var->key, &opt_idx))
            {
               RARCH_LOG("[Environ]: SET_VARIABLE %s: invalid key.\n",
                     var->key);
               return false;
            }

            /* Check whether value is valid */
            if (!core_option_manager_get_val_idx(runloop_st->core_options,
                  opt_idx, var->value, &val_idx))
            {
               RARCH_LOG("[Environ]: SET_VARIABLE %s: invalid value: %s\n",
                     var->key, var->value);
               return false;
            }

            /* Update option value if core-requested value
             * is not currently set */
            if (val_idx != runloop_st->core_options->opts[opt_idx].index)
               core_option_manager_set_val(runloop_st->core_options,
                     opt_idx, val_idx, true);

            if (log_level == RETRO_LOG_DEBUG)
               RARCH_LOG("[Environ]: SET_VARIABLE %s:\n\t%s\n",
                     var->key, var->value);
         }

         break;

      /* SET_VARIABLES: Legacy path */
      case RETRO_ENVIRONMENT_SET_VARIABLES:
         RARCH_LOG("[Environ]: SET_VARIABLES.\n");

         {
            core_option_manager_t *new_vars = NULL;
            if (runloop_st->core_options)
            {
               runloop_deinit_core_options(
                     runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags           &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options     = NULL;
            }
            if ((new_vars = runloop_init_core_variables(
                  settings,
                  (const struct retro_variable *)data)))
               runloop_st->core_options     = new_vars;
         }

         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
         RARCH_LOG("[Environ]: SET_CORE_OPTIONS.\n");

         {
            /* Parse core_option_definition array to
             * create retro_core_options_v2 struct */
            struct retro_core_options_v2 *options_v2 =
                  core_option_manager_convert_v1(
                        (const struct retro_core_option_definition*)data);

            if (runloop_st->core_options)
            {
               runloop_deinit_core_options(
                     runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options          = NULL;
            }

            if (options_v2)
            {
               /* Initialise core options */
               core_option_manager_t *new_vars = runloop_init_core_options(settings, options_v2);
               if (new_vars)
                  runloop_st->core_options   = new_vars;
               /* Clean up */
               core_option_manager_free_converted(options_v2);
            }
         }
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.\n");

         {
            /* Parse core_options_intl to create
             * retro_core_options_v2 struct */
            struct retro_core_options_v2 *options_v2 =
                  core_option_manager_convert_v1_intl(
                        (const struct retro_core_options_intl*)data);

            if (runloop_st->core_options)
            {
               runloop_deinit_core_options(
                     runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options          = NULL;
            }

            if (options_v2)
            {
               /* Initialise core options */
               core_option_manager_t *new_vars = runloop_init_core_options(settings, options_v2);

               if (new_vars)
                  runloop_st->core_options = new_vars;

               /* Clean up */
               core_option_manager_free_converted(options_v2);
            }
         }
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2.\n");

         {
            core_option_manager_t *new_vars                = NULL;
            const struct retro_core_options_v2 *options_v2 =
                  (const struct retro_core_options_v2 *)data;
            bool categories_enabled                        =
                  settings->bools.core_option_category_enable;

            if (runloop_st->core_options)
            {
               runloop_deinit_core_options(
                     runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options          = NULL;
            }

            if (options_v2)
            {
               new_vars = runloop_init_core_options(settings, options_v2);
               if (new_vars)
                  runloop_st->core_options = new_vars;
            }

            /* Return value does not indicate success.
             * Callback returns 'true' if core option
             * categories are supported/enabled,
             * otherwise 'false'. */
            return categories_enabled;
         }
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL.\n");

         {
            /* Parse retro_core_options_v2_intl to create
             * retro_core_options_v2 struct */
            core_option_manager_t *new_vars          = NULL;
            struct retro_core_options_v2 *options_v2 =
                  core_option_manager_convert_v2_intl(
                        (const struct retro_core_options_v2_intl*)data);
            bool categories_enabled                  =
                  settings->bools.core_option_category_enable;

            if (runloop_st->core_options)
            {
               runloop_deinit_core_options(
                     runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options          = NULL;
            }

            if (options_v2)
            {
               /* Initialise core options */
               new_vars = runloop_init_core_options(settings, options_v2);
               if (new_vars)
                  runloop_st->core_options = new_vars;

               /* Clean up */
               core_option_manager_free_converted(options_v2);
            }

            /* Return value does not indicate success.
             * Callback returns 'true' if core option
             * categories are supported/enabled,
             * otherwise 'false'. */
            return categories_enabled;
         }
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
         RARCH_DBG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY.\n");

         {
            const struct retro_core_option_display *core_options_display =
                  (const struct retro_core_option_display *)data;

            if (runloop_st->core_options && core_options_display)
               core_option_manager_set_visible(
                     runloop_st->core_options,
                     core_options_display->key,
                     core_options_display->visible);
         }
         break;

      case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK:
         RARCH_DBG("[Environ]: RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK.\n");

         {
            const struct retro_core_options_update_display_callback
                  *update_display_callback =
                        (const struct retro_core_options_update_display_callback*)data;

            if (update_display_callback &&
                update_display_callback->callback)
               runloop_st->core_options_callback.update_display =
                     update_display_callback->callback;
            else
               runloop_st->core_options_callback.update_display = NULL;
         }
         break;

      case RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION:
         RARCH_LOG("[Environ]: GET_MESSAGE_INTERFACE_VERSION.\n");
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_SET_MESSAGE:
      {
         const struct retro_message *msg = (const struct retro_message*)data;
#if defined(HAVE_GFX_WIDGETS)
         dispgfx_widget_t *p_dispwidget  = dispwidget_get_ptr();
         if (p_dispwidget->active)
            gfx_widget_set_libretro_message(
                  msg->msg,
                  roundf((float)msg->frames / 60.0f * 1000.0f));
         else
#endif
            runloop_msg_queue_push(msg->msg, 3, msg->frames,
                  true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                  MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("[Environ]: SET_MESSAGE: %s\n", msg->msg);
         break;
      }

      case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
      {
         const struct retro_message_ext *msg =
            (const struct retro_message_ext*)data;

         /* Log message, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_OSD)
         {
            settings_t *settings = config_get_ptr();
            unsigned log_level   = settings->uints.frontend_log_level;
            switch (msg->level)
            {
               case RETRO_LOG_DEBUG:
                  if (log_level == RETRO_LOG_DEBUG)
                     RARCH_LOG("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_WARN:
                  RARCH_WARN("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_ERROR:
                  RARCH_ERR("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
               case RETRO_LOG_INFO:
               default:
                  RARCH_LOG("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
                  break;
            }
         }

         /* Display message via OSD, if required */
         if (msg->target != RETRO_MESSAGE_TARGET_LOG)
         {
            switch (msg->type)
            {
               /* Handle 'status' messages */
               case RETRO_MESSAGE_TYPE_STATUS:

                  /* Note: We need to lock a mutex here. Strictly
                   * speaking, 'core_status_msg' is not part
                   * of the message queue, but:
                   * - It may be implemented as a queue in the future
                   * - It seems unnecessary to create a new slock_t
                   *   object for this type of message when
                   *   _runloop_msg_queue_lock is already available
                   * We therefore just call runloop_msg_queue_lock()/
                   * runloop_msg_queue_unlock() in this case */
                  RUNLOOP_MSG_QUEUE_LOCK(runloop_st);

                  /* If a message is already set, only overwrite
                   * it if the new message has the same or higher
                   * priority */
                  if (!runloop_st->core_status_msg.set ||
                      (runloop_st->core_status_msg.priority <= msg->priority))
                  {
                     if (!string_is_empty(msg->msg))
                     {
                        strlcpy(runloop_st->core_status_msg.str, msg->msg,
                              sizeof(runloop_st->core_status_msg.str));

                        runloop_st->core_status_msg.duration = (float)msg->duration;
                        runloop_st->core_status_msg.set      = true;
                     }
                     else
                     {
                        /* Ensure sane behaviour if core sends an
                         * empty message */
                        runloop_st->core_status_msg.str[0] = '\0';
                        runloop_st->core_status_msg.priority = 0;
                        runloop_st->core_status_msg.duration = 0.0f;
                        runloop_st->core_status_msg.set      = false;
                     }
                  }

                  RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
                  break;

#if defined(HAVE_GFX_WIDGETS)
               /* Handle 'alternate' non-queued notifications */
               case RETRO_MESSAGE_TYPE_NOTIFICATION_ALT:
                  {
                     video_driver_state_t *video_st = 
                        video_state_get_ptr();
                     dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
                     if (p_dispwidget->active)
                        gfx_widget_set_libretro_message(
                              msg->msg, msg->duration);
                     else
                        runloop_core_msg_queue_push(
                              &video_st->av_info, msg);

                  }
                  break;

               /* Handle 'progress' messages */
               case RETRO_MESSAGE_TYPE_PROGRESS:
                  {
                     video_driver_state_t *video_st = 
                        video_state_get_ptr();
                     dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
                     if (p_dispwidget->active)
                        gfx_widget_set_progress_message(
                              msg->msg, msg->duration,
                              msg->priority, msg->progress);
                     else
                        runloop_core_msg_queue_push(
                              &video_st->av_info, msg);

                  }
                  break;
#endif
               /* Handle standard (queued) notifications */
               case RETRO_MESSAGE_TYPE_NOTIFICATION:
               default:
                  {
                     video_driver_state_t *video_st = 
                        video_state_get_ptr();
                     runloop_core_msg_queue_push(
                           &video_st->av_info, msg);
                  }
                  break;
            }
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_ROTATION:
      {
         unsigned rotation       = *(const unsigned*)data;
         bool video_allow_rotate = settings->bools.video_allow_rotate;

         RARCH_LOG("[Environ]: SET_ROTATION: %u\n", rotation);
         if (!video_allow_rotate)
            return false;

         if (system)
            system->rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
      {
#ifdef HAVE_MENU
         struct menu_state *menu_st = menu_state_get_ptr();
#endif
         /* This case occurs when a core (internally)
          * requests a shutdown event */
         RARCH_LOG("[Environ]: SHUTDOWN.\n");

	 runloop_st->flags |= RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED
                            | RUNLOOP_FLAG_SHUTDOWN_INITIATED;
#ifdef HAVE_MENU
         /* Ensure that menu stack is flushed appropriately
          * after the core has stopped running */
         if (menu_st)
         {
            const char *content_path = path_get(RARCH_PATH_CONTENT);

            menu_st->flags |= MENU_ST_FLAG_PENDING_ENV_SHUTDOWN_FLUSH;
            if (!string_is_empty(content_path))
               strlcpy(menu_st->pending_env_shutdown_content_path,
                     content_path,
                     sizeof(menu_st->pending_env_shutdown_content_path));
            else
               menu_st->pending_env_shutdown_content_path[0] = '\0';
         }
#endif
         break;
      }

      case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
         if (system)
         {
            system->performance_level = *(const unsigned*)data;
            RARCH_LOG("[Environ]: PERFORMANCE_LEVEL: %u.\n",
                  system->performance_level);
         }
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         {
            const char *dir_system          = settings->paths.directory_system;
            bool systemfiles_in_content_dir = settings->bools.systemfiles_in_content_dir;
            if (string_is_empty(dir_system) || systemfiles_in_content_dir)
            {
               const char *fullpath = path_get(RARCH_PATH_CONTENT);
               if (!string_is_empty(fullpath))
               {
                  char tmp_path[PATH_MAX_LENGTH];
                  if (string_is_empty(dir_system))
                     RARCH_WARN("[Environ]: SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                           fullpath);
                  strlcpy(tmp_path, fullpath, sizeof(tmp_path));
                  path_basedir(tmp_path);
                  dir_set(RARCH_DIR_SYSTEM, tmp_path);
               }

               *(const char**)data = dir_get_ptr(RARCH_DIR_SYSTEM);
               RARCH_LOG("[Environ]: SYSTEM_DIRECTORY: \"%s\".\n",
                     dir_system);
            }
            else
            {
               *(const char**)data = dir_system;
               RARCH_LOG("[Environ]: SYSTEM_DIRECTORY: \"%s\".\n",
                     dir_system);
            }
         }
         break;

      case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
         RARCH_LOG("[Environ]: GET_SAVE_DIRECTORY.\n");
         *(const char**)data = runloop_st->savefile_dir;
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->paths.username ?
            settings->paths.username : NULL;
         RARCH_LOG("[Environ]: GET_USERNAME: \"%s\".\n",
               settings->paths.username);
         break;

      case RETRO_ENVIRONMENT_GET_LANGUAGE:
#ifdef HAVE_LANGEXTRA
         {
            unsigned user_lang = *msg_hash_get_uint(MSG_HASH_USER_LANGUAGE);
            *(unsigned *)data  = user_lang;
            RARCH_LOG("[Environ]: GET_LANGUAGE: \"%u\".\n", user_lang);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
      {
         video_driver_state_t *video_st  = 
            video_state_get_ptr();
         enum retro_pixel_format pix_fmt =
            *(const enum retro_pixel_format*)data;

         switch (pix_fmt)
         {
            case RETRO_PIXEL_FORMAT_0RGB1555:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: 0RGB1555.\n");
               break;

            case RETRO_PIXEL_FORMAT_RGB565:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: RGB565.\n");
               break;
            case RETRO_PIXEL_FORMAT_XRGB8888:
               RARCH_LOG("[Environ]: SET_PIXEL_FORMAT: XRGB8888.\n");
               break;
            default:
               return false;
         }

         video_st->pix_fmt = pix_fmt;
         break;
      }

      case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
      {
         static const char *libretro_btn_desc[]    = {
            "B (bottom)", "Y (left)", "Select", "Start",
            "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
            "A (right)", "X (up)",
            "L", "R", "L2", "R2", "L3", "R3",
         };

         if (system)
         {
            unsigned retro_id;
            const struct retro_input_descriptor *desc = NULL;
            memset((void*)&system->input_desc_btn, 0,
                  sizeof(system->input_desc_btn));

            desc = (const struct retro_input_descriptor*)data;

            for (; desc->description; desc++)
            {
               unsigned retro_port = desc->port;

               retro_id            = desc->id;

               if (desc->port >= MAX_USERS)
                  continue;

               if (desc->id >= RARCH_FIRST_CUSTOM_BIND)
                  continue;

               switch (desc->device)
               {
                  case RETRO_DEVICE_JOYPAD:
                     system->input_desc_btn[retro_port]
                        [retro_id] = desc->description;
                     break;
                  case RETRO_DEVICE_ANALOG:
                     switch (retro_id)
                     {
                        case RETRO_DEVICE_ID_ANALOG_X:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_X_PLUS]  = desc->description;
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                                 break;
                              case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_ANALOG_Y:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                                 break;
                              case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                                 system->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_JOYPAD_R2:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_BUTTON:
                                 system->input_desc_btn[retro_port]
                                    [retro_id] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_JOYPAD_L2:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_BUTTON:
                                 system->input_desc_btn[retro_port]
                                    [retro_id] = desc->description;
                                 break;
                           }
                           break;
                     }
                     break;
               }
            }

            RARCH_LOG("[Environ]: SET_INPUT_DESCRIPTORS:\n");

            {
               unsigned log_level      = settings->uints.libretro_log_level;

               if (log_level == RETRO_LOG_DEBUG)
               {
                  unsigned input_driver_max_users =
                     settings->uints.input_max_users;
                  for (p = 0; p < input_driver_max_users; p++)
                  {
                     unsigned mapped_port = settings->uints.input_remap_ports[p];

                     for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                     {
                        const char *description = system->input_desc_btn[mapped_port][retro_id];

                        if (!description)
                           continue;

                        RARCH_LOG("   RetroPad, Port %u, Button \"%s\" => \"%s\"\n",
                              p + 1, libretro_btn_desc[retro_id], description);
                     }
                  }
               }
            }

            runloop_st->current_core.has_set_input_descriptors = true;
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
      {
         input_driver_state_t 
            *input_st                               = input_state_get_ptr();
         const struct retro_keyboard_callback *info =
            (const struct retro_keyboard_callback*)data;
         retro_keyboard_event_t *frontend_key_event = &runloop_st->frontend_key_event;
         retro_keyboard_event_t *key_event          = &runloop_st->key_event;

         RARCH_LOG("[Environ]: SET_KEYBOARD_CALLBACK.\n");
         if (key_event)
            *key_event                  = info->callback;

         if (frontend_key_event && key_event)
            *frontend_key_event         = *key_event;

         /* If a core calls RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK,
          * then it is assumed that game focus mode is desired */
         input_st->game_focus_state.core_requested = true;

         break;
      }

      case RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION:
         RARCH_LOG("[Environ]: GET_DISK_CONTROL_INTERFACE_VERSION.\n");
         /* Current API version is 1 */
         *(unsigned *)data = 1;
         break;

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE:
         {
            const struct retro_disk_control_callback *control_cb =
                  (const struct retro_disk_control_callback*)data;

            if (system)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_INTERFACE.\n");
               disk_control_set_callback(&system->disk_control, control_cb);
            }
         }
         break;

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
         {
            const struct retro_disk_control_ext_callback *control_cb =
                  (const struct retro_disk_control_ext_callback*)data;

            if (system)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_EXT_INTERFACE.\n");
               disk_control_set_ext_callback(&system->disk_control, control_cb);
            }
         }
         break;

      case RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER:
      {
         unsigned *cb = (unsigned*)data;
         settings_t *settings          = config_get_ptr();
         const char *video_driver_name = settings->arrays.video_driver;
         bool driver_switch_enable     = settings->bools.driver_switch_enable;

         RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER, video driver name: %s.\n", video_driver_name);

         if (string_is_equal(video_driver_name, "glcore"))
         {
             *cb = RETRO_HW_CONTEXT_OPENGL_CORE;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_OPENGL_CORE.\n");
         }
         else if (string_is_equal(video_driver_name, "gl"))
         {
             *cb = RETRO_HW_CONTEXT_OPENGL;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_OPENGL.\n");
         }
         else if (string_is_equal(video_driver_name, "vulkan"))
         {
             *cb = RETRO_HW_CONTEXT_VULKAN;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_VULKAN.\n");
         }
         else if (!strncmp(video_driver_name, "d3d", 3))
         {
             *cb = RETRO_HW_CONTEXT_DIRECT3D;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_DIRECT3D.\n");
         }
         else
         {
             *cb = RETRO_HW_CONTEXT_NONE;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_NONE.\n");
         }

         if (!driver_switch_enable)
         {
            RARCH_LOG("[Environ]: Driver switching disabled, GET_PREFERRED_HW_RENDER will be ignored.\n");
            return false;
         }

         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_RENDER:
      case RETRO_ENVIRONMENT_SET_HW_RENDER | RETRO_ENVIRONMENT_EXPERIMENTAL:
      {
         settings_t *settings                 = config_get_ptr();
         struct retro_hw_render_callback *cb  =
            (struct retro_hw_render_callback*)data;
         video_driver_state_t *video_st       = 
            video_state_get_ptr();
         struct retro_hw_render_callback *hwr =
            VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);

         if (!cb)
         {
            RARCH_ERR("[Environ]: SET_HW_RENDER - No valid callback passed, returning...\n");
            return false;
         }

         RARCH_LOG("[Environ]: SET_HW_RENDER, context type: %s.\n", hw_render_context_name(cb->context_type, cb->version_major, cb->version_minor));

         if (!dynamic_request_hw_context(
                  cb->context_type, cb->version_minor, cb->version_major))
         {
            RARCH_ERR("[Environ]: SET_HW_RENDER - Dynamic request HW context failed.\n");
            return false;
         }

         if (!dynamic_verify_hw_context(
                  settings->arrays.video_driver,
                  settings->bools.driver_switch_enable,
                  cb->context_type, cb->version_minor, cb->version_major))
         {
            RARCH_ERR("[Environ]: SET_HW_RENDER: Dynamic verify HW context failed.\n");
            return false;
         }

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL_CORE)
         /* TODO/FIXME - should check first if an OpenGL
          * driver is running */
         if (cb->context_type == RETRO_HW_CONTEXT_OPENGL_CORE)
         {
            /* Ensure that the rest of the frontend knows
             * we have a core context */
            gfx_ctx_flags_t flags;
            flags.flags = 0;
            BIT32_SET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

            video_context_driver_set_flags(&flags);
         }
#endif

         cb->get_current_framebuffer = video_driver_get_current_framebuffer;
         cb->get_proc_address        = video_driver_get_proc_address;

         /* Old ABI. Don't copy garbage. */
         if (cmd & RETRO_ENVIRONMENT_EXPERIMENTAL)
         {
            memcpy(hwr,
                  cb, offsetof(struct retro_hw_render_callback, stencil));
            memset((uint8_t*)hwr + offsetof(struct retro_hw_render_callback, stencil),
               0, sizeof(*cb) - offsetof(struct retro_hw_render_callback, stencil));
         }
         else
            memcpy(hwr, cb, sizeof(*cb));
         RARCH_LOG("Reached end of SET_HW_RENDER.\n");
         break;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
      {
         bool state = *(const bool*)data;
         RARCH_LOG("[Environ]: SET_SUPPORT_NO_GAME: %s.\n", state ? "yes" : "no");

         if (state)
            content_set_does_not_need_content();
         else
            content_unset_does_not_need_content();
         break;
      }

      case RETRO_ENVIRONMENT_GET_LIBRETRO_PATH:
      {
         const char **path = (const char**)data;
         RARCH_LOG("[Environ]: GET_LIBRETRO_PATH.\n");
#ifdef HAVE_DYNAMIC
         *path = path_get(RARCH_PATH_CORE);
#else
         *path = NULL;
#endif
         break;
      }

      case RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK:
#ifdef HAVE_THREADS
      {
         recording_state_t 
            *recording_st            = recording_state_get_ptr();
         audio_driver_state_t 
            *audio_st                = audio_state_get_ptr();
         const struct 
            retro_audio_callback *cb = (const struct retro_audio_callback*)data;
         RARCH_LOG("[Environ]: SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (recording_st->data) /* A/V sync is a must. */
            return false;
         if (cb)
            audio_st->callback = *cb;
      }
      break;
#else
      return false;
#endif

      case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
      {
         const struct retro_frame_time_callback *info =
            (const struct retro_frame_time_callback*)data;

         RARCH_LOG("[Environ]: SET_FRAME_TIME_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         /* retro_run() will be called in very strange and
          * mysterious ways, have to disable it. */
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         runloop_st->frame_time = *info;
         break;
      }

      case RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK:
      {
         const struct retro_audio_buffer_status_callback *info =
            (const struct retro_audio_buffer_status_callback*)data;

         RARCH_LOG("[Environ]: SET_AUDIO_BUFFER_STATUS_CALLBACK.\n");

         if (info)
            runloop_st->audio_buffer_status.callback = info->callback;
         else
            runloop_st->audio_buffer_status.callback = NULL;

         break;
      }

      case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
      {
         unsigned audio_latency_default = settings->uints.audio_latency;
         unsigned audio_latency_current =
               (runloop_st->audio_latency > audio_latency_default) ?
                     runloop_st->audio_latency : audio_latency_default;
         unsigned audio_latency_new;

         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY.\n");

         /* Sanitise input latency value */
         runloop_st->audio_latency    = 0;
         if (data)
            runloop_st->audio_latency = *(const unsigned*)data;
         if (runloop_st->audio_latency > 512)
         {
            RARCH_WARN("[Environ]: Requested audio latency of %u ms - limiting to maximum of 512 ms.\n",
                  runloop_st->audio_latency);
            runloop_st->audio_latency = 512;
         }

         /* Determine new set-point latency value */
         if (runloop_st->audio_latency >= audio_latency_default)
            audio_latency_new = runloop_st->audio_latency;
         else
         {
            if (runloop_st->audio_latency != 0)
               RARCH_WARN("[Environ]: Requested audio latency of %u ms is less than frontend default of %u ms."
                     " Using frontend default...\n",
                     runloop_st->audio_latency, audio_latency_default);

            audio_latency_new = audio_latency_default;
         }

         /* Check whether audio driver requires reinitialisation
          * (Identical to RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO,
          * without video driver initialisation) */
         if (audio_latency_new != audio_latency_current)
         {
            recording_state_t 
               *recording_st      = recording_state_get_ptr();
            bool video_fullscreen = settings->bools.video_fullscreen;
            int reinit_flags      = DRIVERS_CMD_ALL &
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK | DRIVER_MENU_MASK);

            RARCH_LOG("[Environ]: Setting audio latency to %u ms.\n", audio_latency_new);

            command_event(CMD_EVENT_REINIT, &reinit_flags);
            video_driver_set_aspect_ratio();

            /* Cannot continue recording with different 
             * parameters.
             * Take the easiest route out and just restart 
             * the recording. */

            if (recording_st->data)
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT),
                     2, 180, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               command_event(CMD_EVENT_RECORD_DEINIT, NULL);
               command_event(CMD_EVENT_RECORD_INIT, NULL);
            }

            /* Hide mouse cursor in fullscreen mode */
            if (video_fullscreen)
               video_driver_hide_mouse();
         }

         break;
      }

      case RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE:
      {
         struct retro_rumble_interface *iface =
            (struct retro_rumble_interface*)data;

         RARCH_LOG("[Environ]: GET_RUMBLE_INTERFACE.\n");
         iface->set_rumble_state = input_set_rumble_state;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES:
      {
         uint64_t *mask       = (uint64_t*)data;
         input_driver_state_t 
            *input_st         = input_state_get_ptr();

         RARCH_LOG("[Environ]: GET_INPUT_DEVICE_CAPABILITIES.\n");
         if (     !input_st->current_driver->get_capabilities
               || !input_st->current_data)
            return false;
         *mask = input_driver_get_capabilities();
         break;
      }

      case RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE:
      {
         settings_t *settings                 = config_get_ptr();
         bool input_sensors_enable            = settings->bools.input_sensors_enable;
         struct retro_sensor_interface *iface = (struct retro_sensor_interface*)data;

         RARCH_LOG("[Environ]: GET_SENSOR_INTERFACE.\n");

         if (!input_sensors_enable)
            return false;

         iface->set_sensor_state = input_set_sensor_state;
         iface->get_sensor_input = input_get_sensor_state;
         break;
      }
      case RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE:
      {
         struct retro_camera_callback *cb =
            (struct retro_camera_callback*)data;
         camera_driver_state_t *camera_st = camera_state_get_ptr();

         RARCH_LOG("[Environ]: GET_CAMERA_INTERFACE.\n");
         cb->start                        = driver_camera_start;
         cb->stop                         = driver_camera_stop;

         camera_st->cb                    = *cb;
         camera_st->active                = (cb->caps != 0);
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
      {
         struct retro_location_callback *cb =
            (struct retro_location_callback*)data;
         location_driver_state_t 
            *location_st                    = location_state_get_ptr();

         RARCH_LOG("[Environ]: GET_LOCATION_INTERFACE.\n");
         cb->start                       = driver_location_start;
         cb->stop                        = driver_location_stop;
         cb->get_position                = driver_location_get_position;
         cb->set_interval                = driver_location_set_interval;

         if (system)
            system->location_cb          = *cb;

         location_st->active             = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
      {
         struct retro_log_callback *cb = (struct retro_log_callback*)data;

         RARCH_LOG("[Environ]: GET_LOG_INTERFACE.\n");
         cb->log = libretro_log_cb;
         break;
      }

      case RETRO_ENVIRONMENT_GET_PERF_INTERFACE:
      {
         struct retro_perf_callback *cb = (struct retro_perf_callback*)data;

         RARCH_LOG("[Environ]: GET_PERF_INTERFACE.\n");
         cb->get_time_usec    = cpu_features_get_time_usec;
         cb->get_cpu_features = cpu_features_get;
         cb->get_perf_counter = cpu_features_get_perf_counter;

         cb->perf_register    = runloop_performance_counter_register;
         cb->perf_start       = core_performance_counter_start;
         cb->perf_stop        = core_performance_counter_stop;
         cb->perf_log         = runloop_perf_log;
         break;
      }

      case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
      {
         const char **dir            = (const char**)data;
         const char *dir_core_assets = settings->paths.directory_core_assets;

         *dir = *dir_core_assets ?
            dir_core_assets : NULL;
         RARCH_LOG("[Environ]: CORE_ASSETS_DIRECTORY: \"%s\".\n",
               dir_core_assets);
         break;
      }

      case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
      /**
       * Update the system Audio/Video information.
       * Will reinitialize audio/video drivers if needed.
       * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
       **/
      {
         const struct retro_system_av_info **info = (const struct retro_system_av_info**)&data;
         video_driver_state_t *video_st           = video_state_get_ptr();
         struct retro_system_av_info *av_info     = &video_st->av_info;
         if (data)
         {
            int reinit_flags                      = DRIVERS_CMD_ALL;
            settings_t *settings                  = config_get_ptr();
            float refresh_rate                    = (*info)->timing.fps;
            unsigned crt_switch_resolution        = settings->uints.crt_switch_resolution;
            bool video_fullscreen                 = settings->bools.video_fullscreen;
            bool video_switch_refresh_rate        = false;
            bool no_video_reinit                  = true;

            /* Refresh rate switch for regular displays */
            if (video_display_server_has_resolution_list())
               video_switch_refresh_rate_maybe(&refresh_rate, &video_switch_refresh_rate);

            no_video_reinit                       = (
                     crt_switch_resolution == 0
                  && video_switch_refresh_rate == false
                  && data
                  && ((*info)->geometry.max_width  == av_info->geometry.max_width)
                  && ((*info)->geometry.max_height == av_info->geometry.max_height));

            /* First set new refresh rate and display rate, then after REINIT do
             * another display rate change to make sure the change stays */
            if (video_switch_refresh_rate && video_display_server_set_refresh_rate(refresh_rate))
               video_monitor_set_refresh_rate(refresh_rate);

            /* When not doing video reinit, we also must not do input and menu
             * reinit, otherwise the input driver crashes and the menu gets
             * corrupted. */
            if (no_video_reinit)
               reinit_flags = 
                  DRIVERS_CMD_ALL & 
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK |
                                        DRIVER_MENU_MASK);

            RARCH_LOG("[Environ]: SET_SYSTEM_AV_INFO: %ux%u, Aspect: %.3f, FPS: %.2f, Sample rate: %.2f Hz.\n",
                  (*info)->geometry.base_width, (*info)->geometry.base_height,
                  (*info)->geometry.aspect_ratio,
                  (*info)->timing.fps,
                  (*info)->timing.sample_rate);

            memcpy(av_info, *info, sizeof(*av_info));
            video_st->core_frame_time = 1000000 /
                  ((video_st->av_info.timing.fps > 0.0) ?
                        video_st->av_info.timing.fps : 60.0);

            command_event(CMD_EVENT_REINIT, &reinit_flags);
            if (no_video_reinit)
               video_driver_set_aspect_ratio();

            if (video_switch_refresh_rate)
               video_display_server_set_refresh_rate(refresh_rate);

            /* Cannot continue recording with different parameters.
             * Take the easiest route out and just restart 
             * the recording. */
            if (recording_st->data)
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
            if (video_fullscreen)
               video_driver_hide_mouse();

            /* Recalibrate frame delay target */
            if (settings->bools.video_frame_delay_auto)
               video_st->frame_delay_target = 0;

            return true;
         }
         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
      {
         unsigned i;
         const struct retro_subsystem_info *info =
            (const struct retro_subsystem_info*)data;
         unsigned log_level   = settings->uints.libretro_log_level;

         if (log_level == RETRO_LOG_DEBUG)
            RARCH_LOG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            unsigned j;
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("Special game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_LOG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (system)
         {
            struct retro_subsystem_info *info_ptr = NULL;
            free(system->subsystem.data);
            system->subsystem.data = NULL;
            system->subsystem.size = 0;

            info_ptr = (struct retro_subsystem_info*)
               malloc(i * sizeof(*info_ptr));

            if (!info_ptr)
               return false;

            system->subsystem.data = info_ptr;

            memcpy(system->subsystem.data, info,
                  i * sizeof(*system->subsystem.data));
            system->subsystem.size                   = i;
            runloop_st->current_core.has_set_subsystems = true;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info =
            (const struct retro_controller_info*)data;
         unsigned log_level      = settings->uints.libretro_log_level;

         RARCH_LOG("[Environ]: SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_LOG("   Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("      %s (ID: %u)\n", info[i].types[j].desc,
                     info[i].types[j].id);
         }

         if (system)
         {
            struct retro_controller_info *info_ptr = NULL;

            free(system->ports.data);
            system->ports.data = NULL;
            system->ports.size = 0;

            info_ptr = (struct retro_controller_info*)calloc(i, sizeof(*info_ptr));
            if (!info_ptr)
               return false;

            system->ports.data = info_ptr;
            memcpy(system->ports.data, info,
                  i * sizeof(*system->ports.data));
            system->ports.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      {
         if (system)
         {
            unsigned i;
            const struct retro_memory_map *mmaps   =
               (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;
            unsigned int log_level                 = settings->uints.libretro_log_level;

            RARCH_LOG("[Environ]: SET_MEMORY_MAPS.\n");
            free((void*)system->mmaps.descriptors);
            system->mmaps.descriptors     = 0;
            system->mmaps.num_descriptors = 0;
            descriptors = (rarch_memory_descriptor_t*)
               calloc(mmaps->num_descriptors,
                     sizeof(*descriptors));

            if (!descriptors)
               return false;

            system->mmaps.descriptors     = descriptors;
            system->mmaps.num_descriptors = mmaps->num_descriptors;

            for (i = 0; i < mmaps->num_descriptors; i++)
               system->mmaps.descriptors[i].core = mmaps->descriptors[i];

            mmap_preprocess_descriptors(descriptors, mmaps->num_descriptors);

            if (log_level != RETRO_LOG_DEBUG)
               break;

            if (sizeof(void *) == 8)
               RARCH_LOG("           ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_LOG("           ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

            for (i = 0; i < system->mmaps.num_descriptors; i++)
            {
               const rarch_memory_descriptor_t *desc =
                  &system->mmaps.descriptors[i];
               char flags[7];

               flags[0] = 'M';
               if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
                  flags[1] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
                  flags[1] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
                  flags[1] = '2';
               else
                  flags[1] = '1';

               flags[2] = 'A';
               if ((desc->core.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
                  flags[3] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
                  flags[3] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
                  flags[3] = '2';
               else
                  flags[3] = '1';

               flags[4] = (desc->core.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
               flags[5] = (desc->core.flags & RETRO_MEMDESC_CONST) ? 'C' : 'c';
               flags[6] = 0;

               RARCH_LOG("           %03u %s %p %08X %08X %08X %08X %08X %s\n",
                     i + 1, flags, desc->core.ptr, desc->core.offset, desc->core.start,
                     desc->core.select, desc->core.disconnect, desc->core.len,
                     desc->core.addrspace ? desc->core.addrspace : "");
            }
         }
         else
         {
            RARCH_WARN("[Environ]: SET_MEMORY_MAPS, but system pointer not initialized..\n");
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_GEOMETRY:
      {
         video_driver_state_t *video_st           = video_state_get_ptr();
         struct retro_system_av_info *av_info     = &video_st->av_info;
         struct retro_game_geometry  *geom        = (struct retro_game_geometry*)&av_info->geometry;
         const struct retro_game_geometry *in_geom= (const struct retro_game_geometry*)data;

         if (!geom)
            return false;

         /* Can potentially be called every frame,
          * don't do anything unless required. */
         if (  (geom->base_width   != in_geom->base_width)  ||
               (geom->base_height  != in_geom->base_height) ||
               (geom->aspect_ratio != in_geom->aspect_ratio))
         {
            geom->base_width   = in_geom->base_width;
            geom->base_height  = in_geom->base_height;
            geom->aspect_ratio = in_geom->aspect_ratio;

            RARCH_LOG("[Environ]: SET_GEOMETRY: %ux%u, Aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            video_driver_set_aspect_ratio();

            /* TODO: Figure out what to do, if anything, with 
               recording. */
         }
         else
         {
            RARCH_LOG("[Environ]: SET_GEOMETRY.\n");
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER:
      {
         video_driver_state_t *video_st = video_state_get_ptr();
         struct retro_framebuffer *fb   = (struct retro_framebuffer*)data;
         if (
                  video_st->poke
               && video_st->poke->get_current_software_framebuffer
               && video_st->poke->get_current_software_framebuffer(
                  video_st->data, fb))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE:
      {
         video_driver_state_t *video_st = video_state_get_ptr();
         const struct retro_hw_render_interface **iface = (const struct retro_hw_render_interface **)data;
         if (
                  video_st->poke
               && video_st->poke->get_hw_render_interface
               && video_st->poke->get_hw_render_interface(
                  video_st->data, iface))
            return true;

         return false;
      }

      case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
#ifdef HAVE_CHEEVOS
         {
            bool state = *(const bool*)data;
            RARCH_LOG("[Environ]: SET_SUPPORT_ACHIEVEMENTS: %s.\n", state ? "yes" : "no");
            rcheevos_set_support_cheevos(state);
         }
#endif
         break;

      case RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE:
      {
         video_driver_state_t *video_st  = 
            video_state_get_ptr();
         const struct retro_hw_render_context_negotiation_interface *iface =
            (const struct retro_hw_render_context_negotiation_interface*)data;
         RARCH_LOG("[Environ]: SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE.\n");
         video_st->hw_render_context_negotiation = iface;
         break;
      }

      case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
      {
         uint64_t *quirks = (uint64_t *) data;
         RARCH_LOG("[Environ]: SET_SERIALIZATION_QUIRKS.\n");
         runloop_st->current_core.serialization_quirks_v = *quirks;
         break;
      }

      case RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT:
#ifdef HAVE_LIBNX
         RARCH_LOG("[Environ]: SET_HW_SHARED_CONTEXT - ignored for now.\n");
         /* TODO/FIXME - Force this off for now for Switch
          * until shared HW context can work there */
         return false;
#else
         RARCH_LOG("[Environ]: SET_HW_SHARED_CONTEXT.\n");
         runloop_st->flags |= RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT;
#endif
         break;

      case RETRO_ENVIRONMENT_GET_VFS_INTERFACE:
      {
         const uint32_t supported_vfs_version = 3;
         static struct retro_vfs_interface vfs_iface =
         {
            /* VFS API v1 */
            retro_vfs_file_get_path_impl,
            retro_vfs_file_open_impl,
            retro_vfs_file_close_impl,
            retro_vfs_file_size_impl,
            retro_vfs_file_tell_impl,
            retro_vfs_file_seek_impl,
            retro_vfs_file_read_impl,
            retro_vfs_file_write_impl,
            retro_vfs_file_flush_impl,
            retro_vfs_file_remove_impl,
            retro_vfs_file_rename_impl,
            /* VFS API v2 */
            retro_vfs_file_truncate_impl,
            /* VFS API v3 */
            retro_vfs_stat_impl,
            retro_vfs_mkdir_impl,
            retro_vfs_opendir_impl,
            retro_vfs_readdir_impl,
            retro_vfs_dirent_get_name_impl,
            retro_vfs_dirent_is_dir_impl,
            retro_vfs_closedir_impl
         };

         struct retro_vfs_interface_info *vfs_iface_info = (struct retro_vfs_interface_info *) data;
         if (vfs_iface_info->required_interface_version <= supported_vfs_version)
         {
            RARCH_LOG("[Environ]: GET_VFS_INTERFACE. Core requested version >= V%d, providing V%d.\n",
                  vfs_iface_info->required_interface_version, supported_vfs_version);
            vfs_iface_info->required_interface_version = supported_vfs_version;
            vfs_iface_info->iface                      = &vfs_iface;
            system->supports_vfs = true;
         }
         else
         {
            RARCH_WARN("[Environ]: GET_VFS_INTERFACE. Core requested version V%d which is higher than what we support (V%d).\n",
                  vfs_iface_info->required_interface_version, supported_vfs_version);
            return false;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_LED_INTERFACE:
      {
         struct retro_led_interface *ledintf =
            (struct retro_led_interface *)data;
         if (ledintf)
            ledintf->set_led_state = led_driver_set_led;
         RARCH_LOG("[Environ]: GET_LED_INTERFACE.\n");
      }
      break;

      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         int result           = 0;
         video_driver_state_t 
            *video_st         = video_state_get_ptr();
         audio_driver_state_t 
            *audio_st         = audio_state_get_ptr();
         if (    !(audio_st->flags & AUDIO_FLAG_SUSPENDED)
               && (audio_st->flags & AUDIO_FLAG_ACTIVE))
            result |= 2;
         if (      (video_st->flags & VIDEO_FLAG_ACTIVE)
               && !(video_st->current_video->frame == video_null.frame))
            result |= 1;
#ifdef HAVE_RUNAHEAD
         if (audio_st->flags & AUDIO_FLAG_HARD_DISABLE)
            result |= 8;
#endif
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_REPLAYING, NULL))
            result &= ~(1|2);
#endif
#if defined(HAVE_RUNAHEAD) || defined(HAVE_NETWORKING)
         /* Deprecated.
            Use RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT instead. */
         /* TODO/FIXME: Get rid of this ugly hack. */
         if (runloop_st->flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE)
            result |= 4;
#endif
         if (data)
         {
            int* result_p = (int*)data;
            *result_p = result;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT:
      {
         int result = RETRO_SAVESTATE_CONTEXT_NORMAL;

#if defined(HAVE_RUNAHEAD) || defined(HAVE_NETWORKING)
         if (runloop_st->flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE)
         {
#ifdef HAVE_NETWORKING
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               result = RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY;
            else
#endif
            {
#ifdef HAVE_RUNAHEAD
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
               settings_t *settings = config_get_ptr();

               if (      settings->bools.run_ahead_secondary_instance
                     && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE)
                     &&  secondary_core_ensure_exists(settings))
                  result = RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_BINARY;
               else
#endif
                  result = RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_INSTANCE;
#endif
            }
         }
#endif

         if (data)
            *(int*)data = result;

         break;
      }

      case RETRO_ENVIRONMENT_GET_MIDI_INTERFACE:
      {
         struct retro_midi_interface *midi_interface =
               (struct retro_midi_interface *)data;

         if (midi_interface)
         {
            midi_interface->input_enabled  = midi_driver_input_enabled;
            midi_interface->output_enabled = midi_driver_output_enabled;
            midi_interface->read           = midi_driver_read;
            midi_interface->write          = midi_driver_write;
            midi_interface->flush          = midi_driver_flush;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_FASTFORWARDING:
         *(bool *)data = ((runloop_st->flags & RUNLOOP_FLAG_FASTMOTION) > 0);
         break;

      case RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE:
      {
         struct retro_fastforwarding_override *fastforwarding_override =
               (struct retro_fastforwarding_override *)data;

         /* Record new retro_fastforwarding_override parameters
          * and schedule application on the the next call of
          * runloop_check_state() */
         if (fastforwarding_override)
         {
            memcpy(&runloop_st->fastmotion_override.next,
                  fastforwarding_override,
                  sizeof(runloop_st->fastmotion_override.next));
            runloop_st->fastmotion_override.pending = true;
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_THROTTLE_STATE:
      {
         video_driver_state_t 
            *video_st                                = 
            video_state_get_ptr();
         struct retro_throttle_state *throttle_state =
               (struct retro_throttle_state *)data;
         audio_driver_state_t *audio_st              =
            audio_state_get_ptr();

         bool menu_opened = false;
         bool core_paused = runloop_st->flags & RUNLOOP_FLAG_PAUSED;
         bool no_audio    = ((audio_st->flags & AUDIO_FLAG_SUSPENDED) 
               || !(audio_st->flags & AUDIO_FLAG_ACTIVE));
         float core_fps   = (float)video_st->av_info.timing.fps;

#ifdef HAVE_REWIND
         if (runloop_st->rewind_st.frame_is_reversed)
         {
            throttle_state->mode = RETRO_THROTTLE_REWINDING;
            throttle_state->rate = 0.0f;
            break; /* ignore vsync */
         }
#endif

#ifdef HAVE_MENU
         menu_opened = menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE;
         if (menu_opened)
#ifdef HAVE_NETWORKING
            core_paused = settings->bools.menu_pause_libretro &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            core_paused = settings->bools.menu_pause_libretro;
#endif

#endif

         if (core_paused)
         {
            throttle_state->mode = RETRO_THROTTLE_FRAME_STEPPING;
            throttle_state->rate = 0.0f;
            break; /* ignore vsync */
         }

         /* Base mode and rate. */
         throttle_state->mode = RETRO_THROTTLE_NONE;
         throttle_state->rate = core_fps;

         if (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
         {
            throttle_state->mode  = RETRO_THROTTLE_FAST_FORWARD;
            throttle_state->rate *= runloop_get_fastforward_ratio(
                  settings, &runloop_st->fastmotion_override.current);
         }
         else if ((runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
               && !no_audio)
         {
            throttle_state->mode = RETRO_THROTTLE_SLOW_MOTION;
            throttle_state->rate /= (settings->floats.slowmotion_ratio > 0.0f ?
                  settings->floats.slowmotion_ratio : 1.0f);
         }

         /* VSync overrides the mode if the rate is limited by the display. */
         if (menu_opened || /* Menu currently always runs with vsync on. */
               (settings->bools.video_vsync && !runloop_st->force_nonblock
                     && !(input_state_get_ptr()->flags & INP_FLAG_NONBLOCKING)))
         {
            float refresh_rate = video_driver_get_refresh_rate();
            if (refresh_rate == 0.0f)
               refresh_rate = settings->floats.video_refresh_rate;
            if (refresh_rate < throttle_state->rate || !throttle_state->rate)
            {
               /* Keep the mode as fast forward even if vsync limits it. */
               if (refresh_rate < core_fps)
                  throttle_state->mode = RETRO_THROTTLE_VSYNC;
               throttle_state->rate = refresh_rate;
            }
         }

         /* Special behavior while audio output is not available. */
         if (no_audio && throttle_state->mode != RETRO_THROTTLE_FAST_FORWARD
                      && throttle_state->mode != RETRO_THROTTLE_VSYNC)
         {
            /* Keep base if frame limiter matching the core is active. */
            retro_time_t core_limit     = (core_fps 
                  ? (retro_time_t)(1000000.0f / core_fps)
                  : (retro_time_t)0);
            retro_time_t frame_limit    = runloop_st->frame_limit_minimum_time;
            if (abs((int)(core_limit - frame_limit)) > 10)
            {
               throttle_state->mode     = RETRO_THROTTLE_UNBLOCKED;
               throttle_state->rate     = 0.0f;
            }
         }
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
         /* Just falldown, the function will return true */
         break;

      case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
         RARCH_LOG("[Environ]: GET_CORE_OPTIONS_VERSION.\n");
         /* Current API version is 2 */
         *(unsigned *)data = 2;
         break;

      case RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE:
      {
         /* Try to use the polled refresh rate first.  */
         float target_refresh_rate = video_driver_get_refresh_rate();

         /* If the above function failed [possibly because it is not
          * implemented], use the refresh rate set in the config instead. */
         if (target_refresh_rate == 0.0f)
         {
            if (settings)
               target_refresh_rate = settings->floats.video_refresh_rate;
         }

         *(float *)data = target_refresh_rate;
         break;
      }

      case RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS:
         *(unsigned *)data = settings->uints.input_max_users;
         break;

      /* Private environment callbacks.
       *
       * Should all be properly addressed in version 2.
       * */

      case RETRO_ENVIRONMENT_POLL_TYPE_OVERRIDE:
         {
            const unsigned *poll_type_data = (const unsigned*)data;

            if (poll_type_data)
               runloop_st->core_poll_type_override = (enum poll_type_override_t)*poll_type_data;
         }
         break;

      case RETRO_ENVIRONMENT_GET_CLEAR_ALL_THREAD_WAITS_CB:
         *(retro_environment_t *)data = runloop_clear_all_thread_waits;
         break;

      case RETRO_ENVIRONMENT_SET_SAVE_STATE_IN_BACKGROUND:
         {
            bool state = *(const bool*)data;
            RARCH_LOG("[Environ]: SET_SAVE_STATE_IN_BACKGROUND: %s.\n", state ? "yes" : "no");

            set_save_state_in_background(state);

         }
         break;

      case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
         {
            const struct retro_system_content_info_override *overrides =
                  (const struct retro_system_content_info_override *)data;

            RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE.\n");

            /* Passing NULL always results in 'success' - this
             * allows cores to test for frontend support of
             * the RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE and
             * RETRO_ENVIRONMENT_GET_GAME_INFO_EXT callbacks */
            if (!overrides)
               return true;

            return content_file_override_set(overrides);
         }
         break;

      case RETRO_ENVIRONMENT_GET_GAME_INFO_EXT:
         {
            content_state_t *p_content                       =
                  content_state_get_ptr();
            const struct retro_game_info_ext **game_info_ext =
                  (const struct retro_game_info_ext **)data;

            RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_GET_GAME_INFO_EXT.\n");

            if (!game_info_ext)
               return false;

            if (p_content &&
                p_content->content_list &&
                p_content->content_list->game_info_ext)
               *game_info_ext = p_content->content_list->game_info_ext;
            else
            {
               RARCH_ERR("[Environ]: Failed to retrieve extended game info.\n");
               *game_info_ext = NULL;
               return false;
            }
         }
         break;

      default:
         RARCH_LOG("[Environ]: UNSUPPORTED (#%u).\n", cmd);
         return false;
   }

   return true;
}

bool libretro_get_system_info(
      const char *path,
      struct retro_system_info *info,
      bool *load_no_content)
{
   struct retro_system_info dummy_info;
#ifdef HAVE_DYNAMIC
   dylib_t lib;
#endif
   runloop_state_t *runloop_st  = &runloop_state;

   if (string_ends_with_size(path,
            "builtin", strlen(path), STRLEN_CONST("builtin")))
      return false;

   dummy_info.library_name      = NULL;
   dummy_info.library_version   = NULL;
   dummy_info.valid_extensions  = NULL;
   dummy_info.need_fullpath     = false;
   dummy_info.block_extract     = false;

#ifdef HAVE_DYNAMIC
   if (!(lib = libretro_get_system_info_lib(
         path, &dummy_info, load_no_content)))
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
            path);
      RARCH_ERR("Error(s): %s\n", dylib_error());
      return false;
   }
#else
   if (load_no_content)
   {
      runloop_st->load_no_content_hook = load_no_content;

      /* load_no_content gets set in this callback. */
      retro_set_environment(runloop_environ_cb_get_system_info);

      /* It's possible that we just set get_system_info callback
       * to the currently running core.
       *
       * Make sure we reset it to the actual environment callback.
       * Ignore any environment callbacks here in case we're running
       * on the non-current core. */
      runloop_st->flags |=  RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB;
      retro_set_environment(runloop_environment_cb);
      runloop_st->flags &= ~RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB;
   }

   retro_get_system_info(&dummy_info);
#endif

   memcpy(info, &dummy_info, sizeof(*info));

   runloop_st->current_library_name[0]    = '\0';
   runloop_st->current_library_version[0] = '\0';
   runloop_st->current_valid_extensions[0] = '\0';

   if (!string_is_empty(dummy_info.library_name))
      strlcpy(runloop_st->current_library_name,
            dummy_info.library_name,
            sizeof(runloop_st->current_library_name));
   if (!string_is_empty(dummy_info.library_version))
      strlcpy(runloop_st->current_library_version,
            dummy_info.library_version,
            sizeof(runloop_st->current_library_version));

   if (dummy_info.valid_extensions)
      strlcpy(runloop_st->current_valid_extensions,
            dummy_info.valid_extensions,
            sizeof(runloop_st->current_valid_extensions));

   info->library_name     = runloop_st->current_library_name;
   info->library_version  = runloop_st->current_library_version;
   info->valid_extensions = runloop_st->current_valid_extensions;

#ifdef HAVE_DYNAMIC
   dylib_close(lib);
#endif
   return true;
}

/**
 * init_libretro_symbols_custom:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Setup libretro callback symbols.
 * 
 * @return true on success, or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols_custom(
      runloop_state_t *runloop_st,
      enum rarch_core_type type,
      struct retro_core_t *current_core,
      const char *lib_path,
      void *_lib_handle_p)
{
#ifdef HAVE_DYNAMIC
   /* the library handle for use with the SYMBOL macro */
   dylib_t lib_handle_local;
#endif

   switch (type)
   {
      case CORE_TYPE_PLAIN:
         {
#ifdef HAVE_DYNAMIC
#ifdef HAVE_RUNAHEAD
            dylib_t *lib_handle_p = (dylib_t*)_lib_handle_p;
            if (!lib_path || !lib_handle_p)
#endif
            {
               const char *path = path_get(RARCH_PATH_CORE);

               if (string_is_empty(path))
               {
                  RARCH_ERR("[Core]: Frontend is built for dynamic libretro cores, but "
                        "path is not set. Cannot continue.\n");
                  retroarch_fail(1, "init_libretro_symbols()");
               }

               RARCH_LOG("[Core]: Loading dynamic libretro core from: \"%s\"\n",
                     path);

               if (!(runloop_st->lib_handle = load_dynamic_core(
                           path,
                           path_get_ptr(RARCH_PATH_CORE),
                           path_get_realsize(RARCH_PATH_CORE)
                           )))
               {
                  const char *failed_open_str =
                     msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE);
                  RARCH_ERR("%s: \"%s\"\nError(s): %s\n", failed_open_str,
                        path, dylib_error());
                  runloop_msg_queue_push(failed_open_str,
                        1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  return false;
               }
               lib_handle_local = runloop_st->lib_handle;
            }
#ifdef HAVE_RUNAHEAD
            else
            {
               /* for a secondary core, we already have a
                * primary library loaded, so we can skip
                * some checks and just load the library */
               retro_assert(lib_path != NULL && lib_handle_p != NULL);
               lib_handle_local = dylib_load(lib_path);

               if (!lib_handle_local)
                  return false;
               *lib_handle_p = lib_handle_local;
            }
#endif
#endif

            CORE_SYMBOLS(SYMBOL);
         }
         break;
      case CORE_TYPE_DUMMY:
         CORE_SYMBOLS(SYMBOL_DUMMY);
         break;
      case CORE_TYPE_FFMPEG:
#ifdef HAVE_FFMPEG
         CORE_SYMBOLS(SYMBOL_FFMPEG);
#endif
         break;
      case CORE_TYPE_MPV:
#ifdef HAVE_MPV
         CORE_SYMBOLS(SYMBOL_MPV);
#endif
         break;
      case CORE_TYPE_IMAGEVIEWER:
#ifdef HAVE_IMAGEVIEWER
         CORE_SYMBOLS(SYMBOL_IMAGEVIEWER);
#endif
         break;
      case CORE_TYPE_NETRETROPAD:
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         CORE_SYMBOLS(SYMBOL_NETRETROPAD);
#endif
         break;
      case CORE_TYPE_VIDEO_PROCESSOR:
#if defined(HAVE_VIDEOPROCESSOR)
         CORE_SYMBOLS(SYMBOL_VIDEOPROCESSOR);
#endif
         break;
   }

   return true;
}

/**
 * init_libretro_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Initializes libretro symbols and
 * setups environment callback functions.
 *
 * @return true on success, or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols(
      runloop_state_t *runloop_st,
      enum rarch_core_type type,
      struct retro_core_t *current_core)
{
   /* Load symbols */
   if (!init_libretro_symbols_custom(runloop_st,
            type, current_core, NULL, NULL))
      return false;

#ifdef HAVE_RUNAHEAD
   /* remember last core type created, so creating a
    * secondary core will know what core type to use. */
   runloop_st->last_core_type = type;
#endif
   return true;
}

uint32_t runloop_get_flags(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->flags;
}

void runloop_system_info_free(void)
{
   runloop_state_t *runloop_st   = &runloop_state;
   rarch_system_info_t *sys_info = &runloop_st->system;

   if (sys_info->subsystem.data)
      free(sys_info->subsystem.data);
   if (sys_info->ports.data)
      free(sys_info->ports.data);
   if (sys_info->mmaps.descriptors)
      free((void *)sys_info->mmaps.descriptors);

   sys_info->subsystem.data                           = NULL;
   sys_info->subsystem.size                           = 0;

   sys_info->ports.data                               = NULL;
   sys_info->ports.size                               = 0;

   sys_info->mmaps.descriptors                        = NULL;
   sys_info->mmaps.num_descriptors                    = 0;

   sys_info->info.library_name                        = NULL;
   sys_info->info.library_version                     = NULL;
   sys_info->info.valid_extensions                    = NULL;
   sys_info->info.need_fullpath                       = false;
   sys_info->info.block_extract                       = false;

   runloop_st->key_event                              = NULL;
   runloop_st->frontend_key_event                     = NULL;

   memset(&runloop_st->system, 0, sizeof(rarch_system_info_t));
}

/**
 * uninit_libretro_symbols:
 *
 * Frees libretro core.
 *
 * Frees all core options, associated state, and
 * unbinds all libretro callback symbols.
 **/
static void uninit_libretro_symbols(
      struct retro_core_t *current_core)
{
   runloop_state_t 
	   *runloop_st = &runloop_state;
   input_driver_state_t 
      *input_st        = input_state_get_ptr();
   audio_driver_state_t 
      *audio_st        = audio_state_get_ptr();
   camera_driver_state_t 
      *camera_st       = camera_state_get_ptr();
   location_driver_state_t 
      *location_st     = location_state_get_ptr();
#ifdef HAVE_DYNAMIC
   if (runloop_st->lib_handle)
      dylib_close(runloop_st->lib_handle);
   runloop_st->lib_handle = NULL;
#endif

   memset(current_core, 0, sizeof(struct retro_core_t));

   runloop_st->flags &= ~RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT;

   if (runloop_st->core_options)
   {
      runloop_deinit_core_options(
            runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE,
            path_get(RARCH_PATH_CORE_OPTIONS),
            runloop_st->core_options);
      runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
      runloop_st->core_options          = NULL;
   }
   runloop_system_info_free();
   audio_st->callback.callback                   = NULL;
   audio_st->callback.set_state                  = NULL;
   runloop_frame_time_free();
   runloop_audio_buffer_status_free();
   input_game_focus_free();
   runloop_fastmotion_override_free();
   runloop_core_options_cb_free();
   runloop_st->video_swap_interval_auto          = 1;
   camera_st->active                             = false;
   location_st->active                           = false;

   /* Core has finished utilising the input driver;
    * reset 'analog input requested' flags */
   memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));

   /* Performance counters no longer valid. */
   runloop_st->perf_ptr_libretro  = 0;
   memset(runloop_st->perf_counters_libretro, 0,
         sizeof(runloop_st->perf_counters_libretro));
}

#if defined(HAVE_RUNAHEAD)
static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   int i;
   runloop_state_t      *runloop_st = &runloop_state;

   if (!runloop_st->input_state_list)
      return 0;

   /* find list item */
   for (i = 0; i < runloop_st->input_state_list->size; i++)
   {
      input_list_element *element =
         (input_list_element*)runloop_st->input_state_list->data[i];

      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index))
      {
         if (id < element->state_size)
            return element->state[id];
         return 0;
      }
   }
   return 0;
}

static void free_retro_ctx_load_content_info(struct
      retro_ctx_load_content_info *dest)
{
   if (!dest)
      return;

   string_list_free((struct string_list*)dest->content);
   if (dest->info)
      free(dest->info);

   dest->info    = NULL;
   dest->content = NULL;
}

static struct retro_game_info* clone_retro_game_info(const
      struct retro_game_info *src)
{
   struct retro_game_info *dest = (struct retro_game_info*)malloc(
         sizeof(struct retro_game_info));

   if (!dest)
      return NULL;

   /* content_file_init() guarantees that all
    * elements of the source retro_game_info
    * struct will persist for the lifetime of
    * the core. This means we do not have to
    * copy any data; pointer assignment is
    * sufficient */
   dest->path = src->path;
   dest->data = src->data;
   dest->size = src->size;
   dest->meta = src->meta;

   return dest;
}

static struct retro_ctx_load_content_info
*clone_retro_ctx_load_content_info(
      const struct retro_ctx_load_content_info *src)
{
   struct retro_ctx_load_content_info *dest = NULL;
   if (!src || src->special)
      return NULL;   /* refuse to deal with the Special field */

   dest          = (struct retro_ctx_load_content_info*)
      malloc(sizeof(*dest));

   if (!dest)
      return NULL;

   dest->info       = NULL;
   dest->content    = NULL;
   dest->special    = NULL;

   if (src->info)
      dest->info    = clone_retro_game_info(src->info);
   if (src->content)
      dest->content = string_list_clone(src->content);

   return dest;
}

static void set_load_content_info(
      runloop_state_t *runloop_st,
      const retro_ctx_load_content_info_t *ctx)
{
   free_retro_ctx_load_content_info(runloop_st->load_content_info);
   free(runloop_st->load_content_info);
   runloop_st->load_content_info = clone_retro_ctx_load_content_info(ctx);
}

/* RUNAHEAD - SECONDARY CORE  */
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
static void strcat_alloc(char **dst, const char *s)
{
   size_t len1;
   char *src          = *dst;

   if (!src)
   {
      if (s)
      {
         size_t   len = strlen(s);
         if (len != 0)
         {
            char *_dst= (char*)malloc(len + 1);
            strcpy_literal(_dst, s);
            src       = _dst;
         }
         else
            src       = NULL;
      }
      else
         src          = (char*)calloc(1,1);

      *dst            = src;
      return;
   }

   if (!s)
      return;

   len1               = strlen(src);

   if (!(src = (char*)realloc(src, len1 + strlen(s) + 1)))
      return;

   *dst               = src;
   strcpy_literal(src + len1, s);
}

void runloop_secondary_core_destroy(void)
{
   runloop_state_t *runloop_st      = &runloop_state;
   if (!runloop_st->secondary_lib_handle)
      return;

   /* unload game from core */
   if (runloop_st->secondary_core.retro_unload_game)
      runloop_st->secondary_core.retro_unload_game();
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;

   /* deinit */
   if (runloop_st->secondary_core.retro_deinit)
      runloop_st->secondary_core.retro_deinit();
   memset(&runloop_st->secondary_core, 0, sizeof(struct retro_core_t));

   dylib_close(runloop_st->secondary_lib_handle);
   runloop_st->secondary_lib_handle = NULL;
   filestream_delete(runloop_st->secondary_library_path);
   if (runloop_st->secondary_library_path)
      free(runloop_st->secondary_library_path);
   runloop_st->secondary_library_path = NULL;
}

static char *get_tmpdir_alloc(const char *override_dir)
{
   const char *src    = NULL;
   char *path         = NULL;
#ifdef _WIN32
#ifdef LEGACY_WIN32
   DWORD plen         = GetTempPath(0, NULL) + 1;

   if (!(path = (char*)malloc(plen * sizeof(char))))
      return NULL;

   path[plen - 1]     = 0;
   GetTempPath(plen, path);
#else
   DWORD plen         = GetTempPathW(0, NULL) + 1;
   wchar_t *wide_str  = (wchar_t*)malloc(plen * sizeof(wchar_t));

   if (!wide_str)
      return NULL;

   wide_str[plen - 1] = 0;
   GetTempPathW(plen, wide_str);

   path               = utf16_to_utf8_string_alloc(wide_str);
   free(wide_str);
#endif
#else
#if defined ANDROID
   src                = override_dir;
#else
   {
      char *tmpdir    = getenv("TMPDIR");
      if (tmpdir)
         src          = tmpdir;
      else
         src          = "/tmp";
   }
#endif
   if (src)
   {
      size_t   len    = strlen(src);
      if (len != 0)
      {
         char *dst    = (char*)malloc(len + 1);
         strcpy_literal(dst, src);
         path         = dst;
      }
   }
   else
      path            = (char*)calloc(1,1);
#endif
   return path;
}

static bool write_file_with_random_name(char **temp_dll_path,
      const char *tmp_path, const void* data, ssize_t dataSize)
{
   int i;
   size_t ext_len;
   char number_buf[32];
   bool okay                = false;
   const char *prefix       = "tmp";
   char *ext                = NULL;
   time_t time_value        = time(NULL);
   unsigned _number_value   = (unsigned)time_value;
   const char *src          = path_get_extension(*temp_dll_path);

   if (src)
   {
      size_t   len          = strlen(src);
      if (len != 0)
      {
         char *dst          = (char*)malloc(len + 1);
         strcpy_literal(dst, src);
         ext                = dst;
      }
   }
   else
      ext                   = (char*)calloc(1,1);

   ext_len                  = strlen(ext);

   if (ext_len > 0)
   {
      strcat_alloc(&ext, ".");
      memmove(ext + 1, ext, ext_len);
      ext[0] = '.';
      ext_len++;
   }

   /* Try up to 30 'random' filenames before giving up */
   for (i = 0; i < 30; i++)
   {
      int number_value = _number_value * 214013 + 2531011;
      int number       = (number_value >> 14) % 100000;

      snprintf(number_buf, sizeof(number_buf), "%05d", number);

      if (*temp_dll_path)
         free(*temp_dll_path);
      *temp_dll_path = NULL;

      strcat_alloc(temp_dll_path, tmp_path);
      strcat_alloc(temp_dll_path, PATH_DEFAULT_SLASH());
      strcat_alloc(temp_dll_path, prefix);
      strcat_alloc(temp_dll_path, number_buf);
      strcat_alloc(temp_dll_path, ext);

      if (filestream_write_file(*temp_dll_path, data, dataSize))
      {
         okay = true;
         break;
      }
   }

   if (ext)
      free(ext);
   ext = NULL;
   return okay;
}


static char *copy_core_to_temp_file(
      const char *core_path,
      const char *dir_libretro)
{
   char tmp_path[PATH_MAX_LENGTH];
   bool  failed                = false;
   char  *tmpdir               = NULL;
   char  *tmp_dll_path         = NULL;
   void  *dll_file_data        = NULL;
   int64_t  dll_file_size      = 0;
   const char  *core_base_name = path_basename_nocompression(core_path);

   if (strlen(core_base_name) == 0)
      return NULL;

   if (!(tmpdir = get_tmpdir_alloc(dir_libretro)))
      return NULL;

   fill_pathname_join_special(tmp_path,
         tmpdir, "retroarch_temp",
         sizeof(tmp_path));

   if (!path_mkdir(tmp_path))
   {
      failed = true;
      goto end;
   }

   if (!filestream_read_file(core_path, &dll_file_data, &dll_file_size))
   {
      failed = true;
      goto end;
   }

   strcat_alloc(&tmp_dll_path, tmp_path);
   strcat_alloc(&tmp_dll_path, PATH_DEFAULT_SLASH());
   strcat_alloc(&tmp_dll_path, core_base_name);

   if (!filestream_write_file(tmp_dll_path, dll_file_data, dll_file_size))
   {
      /* try other file names */
      if (!write_file_with_random_name(&tmp_dll_path,
               tmp_path, dll_file_data, dll_file_size))
         failed = true;
   }

end:
   if (tmpdir)
      free(tmpdir);
   if (dll_file_data)
      free(dll_file_data);

   tmpdir              = NULL;
   dll_file_data       = NULL;

   if (!failed)
      return tmp_dll_path;

   if (tmp_dll_path)
      free(tmp_dll_path);

   tmp_dll_path     = NULL;

   return NULL;
}

static bool runloop_environment_secondary_core_hook(
      unsigned cmd, void *data)
{
   runloop_state_t *runloop_st    = &runloop_state;
   bool result                    = runloop_environment_cb(cmd, data);

   if (runloop_st->flags & RUNLOOP_FLAG_HAS_VARIABLE_UPDATE)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p                      = (bool*)data;
         *bool_p                           = true;
         runloop_st->flags                &= ~RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         runloop_st->flags &= ~RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
   }
   return result;
}

static void runloop_clear_controller_port_map(void)
{
   int port;
   runloop_state_t *runloop_st   = &runloop_state;
   for (port = 0; port < MAX_USERS; port++)
      runloop_st->port_map[port] = -1;
}

static bool secondary_core_create(runloop_state_t *runloop_st,
      settings_t *settings)
{
   const enum rarch_core_type
      last_core_type           = runloop_st->last_core_type;
   rarch_system_info_t *info   = &runloop_st->system;
   unsigned num_active_users   = settings->uints.input_max_users;
   uint8_t flags               = content_get_flags();

   if (   last_core_type != CORE_TYPE_PLAIN          ||
         !runloop_st->load_content_info              ||
          runloop_st->load_content_info->special)
      return false;

   if (runloop_st->secondary_library_path)
      free(runloop_st->secondary_library_path);
   runloop_st->secondary_library_path = NULL;
   runloop_st->secondary_library_path = copy_core_to_temp_file(
		   path_get(RARCH_PATH_CORE),
		   settings->paths.directory_libretro);

   if (!runloop_st->secondary_library_path)
      return false;

   /* Load Core */
   if (!init_libretro_symbols_custom(runloop_st,
            CORE_TYPE_PLAIN, &runloop_st->secondary_core,
            runloop_st->secondary_library_path,
            &runloop_st->secondary_lib_handle))
      return false;

   runloop_st->secondary_core.symbols_inited = true;
   runloop_st->secondary_core.retro_set_environment(
         runloop_environment_secondary_core_hook);
#ifdef HAVE_RUNAHEAD
   runloop_st->flags               |= RUNLOOP_FLAG_HAS_VARIABLE_UPDATE;
#endif

   runloop_st->secondary_core.retro_init();

   runloop_st->secondary_core.inited = (flags & CONTENT_ST_FLAG_IS_INITED);

   /* Load Content */
   /* disabled due to crashes */
   if ( !runloop_st->load_content_info ||
         runloop_st->load_content_info->special)
      return false;

   if ( (runloop_st->load_content_info->content->size > 0) &&
         runloop_st->load_content_info->content->elems[0].data)
   {
      runloop_st->secondary_core.game_loaded = 
         runloop_st->secondary_core.retro_load_game(
               runloop_st->load_content_info->info);
      if (!runloop_st->secondary_core.game_loaded)
         goto error;
   }
   else if (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
   {
      runloop_st->secondary_core.game_loaded = 
         runloop_st->secondary_core.retro_load_game(NULL);
      if (!runloop_st->secondary_core.game_loaded)
         goto error;
   }
   else
      runloop_st->secondary_core.game_loaded = false;

   if (!runloop_st->secondary_core.inited)
      goto error;

   core_set_default_callbacks(&runloop_st->secondary_callbacks);
   runloop_st->secondary_core.retro_set_video_refresh(
         runloop_st->secondary_callbacks.frame_cb);
   runloop_st->secondary_core.retro_set_audio_sample(
         runloop_st->secondary_callbacks.sample_cb);
   runloop_st->secondary_core.retro_set_audio_sample_batch(
         runloop_st->secondary_callbacks.sample_batch_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);
   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);

   if (info)
   {
      int port;
      for (port = 0; port < MAX_USERS; port++)
      {
         if (port < info->ports.size)
         {
            unsigned device = (port < num_active_users) ?
                  runloop_st->port_map[port] : RETRO_DEVICE_NONE;

            runloop_st->secondary_core.retro_set_controller_port_device(
                  port, device);
         }
      }
   }

   runloop_clear_controller_port_map();

   return true;

error:
   runloop_secondary_core_destroy();
   return false;
}

bool secondary_core_ensure_exists(settings_t *settings)
{
   runloop_state_t *runloop_st   = &runloop_state;
   if (!runloop_st->secondary_lib_handle)
      if (!secondary_core_create(runloop_st, settings))
         return false;
   return true;
}

#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC)
static bool secondary_core_deserialize(settings_t *settings,
      const void *data, size_t size)
{
   bool ret = false;

   if (secondary_core_ensure_exists(settings))
   {
      runloop_state_t *runloop_st = &runloop_state;

      runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
      ret                = runloop_st->secondary_core.retro_unserialize(data, size);
      runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   }
   else
      runloop_secondary_core_destroy();

   return ret;
}
#endif

static void remember_controller_port_device(long port, long device)
{
   runloop_state_t *runloop_st   = &runloop_state;
   if (port >= 0 && port < MAX_USERS)
      runloop_st->port_map[port] = (int)device;
   if (     runloop_st->secondary_lib_handle
         && runloop_st->secondary_core.retro_set_controller_port_device)
      runloop_st->secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
}

static void secondary_core_input_poll_null(void) { }

static bool secondary_core_run_use_last_input(void)
{
   retro_input_poll_t old_poll_function;
   retro_input_state_t old_input_function;
   runloop_state_t *runloop_st = &runloop_state;

   if (!secondary_core_ensure_exists(config_get_ptr()))
   {
      runloop_secondary_core_destroy();
      return false;
   }

   old_poll_function                        = runloop_st->secondary_callbacks.poll_cb;
   old_input_function                       = runloop_st->secondary_callbacks.state_cb;

   runloop_st->secondary_callbacks.poll_cb  = secondary_core_input_poll_null;
   runloop_st->secondary_callbacks.state_cb = input_state_get_last;

   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);

   runloop_st->secondary_core.retro_run();
   runloop_st->secondary_callbacks.poll_cb  = old_poll_function;
   runloop_st->secondary_callbacks.state_cb = old_input_function;

   runloop_st->secondary_core.retro_set_input_poll(
         runloop_st->secondary_callbacks.poll_cb);
   runloop_st->secondary_core.retro_set_input_state(
         runloop_st->secondary_callbacks.state_cb);

   return true;
}
#else
void runloop_secondary_core_destroy(void) { }
static void remember_controller_port_device(long port, long device) { }
static void runloop_clear_controller_port_map(void)                 { }
#endif

static void mylist_resize(my_list *list,
      int new_size, bool run_constructor)
{
   int i;
   int new_capacity;
   int old_size;
   void *element    = NULL;
   if (new_size < 0)
      new_size      = 0;
   new_capacity     = new_size;
   old_size         = list->size;

   if (new_size == old_size)
      return;

   if (new_size > list->capacity)
   {
      if (new_capacity < list->capacity * 2)
         new_capacity = list->capacity * 2;

      /* try to realloc */
      list->data      = (void**)realloc(
            (void*)list->data, new_capacity * sizeof(void*));

      for (i = list->capacity; i < new_capacity; i++)
         list->data[i] = NULL;

      list->capacity = new_capacity;
   }

   if (new_size <= list->size)
   {
      for (i = new_size; i < list->size; i++)
      {
         element = list->data[i];

         if (element)
         {
            list->destructor(element);
            list->data[i] = NULL;
         }
      }
   }
   else
   {
      for (i = list->size; i < new_size; i++)
      {
         list->data[i] = NULL;
         if (run_constructor)
            list->data[i] = list->constructor();
      }
   }

   list->size = new_size;
}

static void *mylist_add_element(my_list *list)
{
   int old_size = list->size;
   if (list)
      mylist_resize(list, old_size + 1, true);
   return list->data[old_size];
}

static void mylist_destroy(my_list **list_p)
{
   my_list *list = NULL;
   if (!list_p)
      return;

   list = *list_p;

   if (list)
   {
      mylist_resize(list, 0, false);
      free(list->data);
      free(list);
      *list_p = NULL;
   }
}

static void mylist_create(my_list **list_p, int initial_capacity,
      constructor_t constructor, destructor_t destructor)
{
   my_list *list        = NULL;

   if (!list_p)
      return;

   list                = *list_p;
   if (list)
      mylist_destroy(list_p);

   list               = (my_list*)malloc(sizeof(my_list));
   *list_p            = list;
   list->size         = 0;
   list->constructor  = constructor;
   list->destructor   = destructor;
   list->data         = (void**)calloc(initial_capacity, sizeof(void*));
   list->capacity     = initial_capacity;
}

static void *input_list_element_constructor(void)
{
   void *ptr                   = malloc(sizeof(input_list_element));
   input_list_element *element = (input_list_element*)ptr;

   element->port               = 0;
   element->device             = 0;
   element->index              = 0;
   element->state              = (int16_t*)calloc(NAME_MAX_LENGTH,
         sizeof(int16_t));
   element->state_size         = NAME_MAX_LENGTH;

   return ptr;
}

static void input_list_element_realloc(
      input_list_element *element,
      unsigned int new_size)
{
   if (new_size > element->state_size)
   {
      element->state = (int16_t*)realloc(element->state,
            new_size * sizeof(int16_t));
      memset(&element->state[element->state_size], 0,
            (new_size - element->state_size) * sizeof(int16_t));
      element->state_size = new_size;
   }
}

static void input_list_element_expand(
      input_list_element *element, unsigned int new_index)
{
   unsigned int new_size = element->state_size;
   if (new_size == 0)
      new_size = 32;
   while (new_index >= new_size)
      new_size *= 2;
   input_list_element_realloc(element, new_size);
}

static void input_list_element_destructor(void* element_ptr)
{
   input_list_element *element = (input_list_element*)element_ptr;
   if (!element)
      return;

   free(element->state);
   free(element_ptr);
}

static void input_state_set_last(
      runloop_state_t *runloop_st,
      unsigned port, unsigned device,
      unsigned index, unsigned id, int16_t value)
{
   unsigned i;
   input_list_element *element = NULL;

   if (!runloop_st->input_state_list)
      mylist_create(&runloop_st->input_state_list, 16,
            input_list_element_constructor,
            input_list_element_destructor);

   /* Find list item */
   for (i = 0; i < (unsigned)runloop_st->input_state_list->size; i++)
   {
      element = (input_list_element*)runloop_st->input_state_list->data[i];
      if (  (element->port   == port)   &&
            (element->device == device) &&
            (element->index  == index)
         )
      {
         if (id >= element->state_size)
            input_list_element_expand(element, id);
         element->state[id] = value;
         return;
      }
   }

   element               = NULL;
   if (runloop_st->input_state_list)
      element            = (input_list_element*)
         mylist_add_element(runloop_st->input_state_list);
   if (element)
   {
      element->port         = port;
      element->device       = device;
      element->index        = index;
      if (id >= element->state_size)
         input_list_element_expand(element, id);
      element->state[id]    = value;
   }
}

static int16_t input_state_with_logging(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   runloop_state_t     *runloop_st  = &runloop_state;

   if (runloop_st->input_state_callback_original)
   {
      int16_t result                = 
         runloop_st->input_state_callback_original(
            port, device, index, id);
      int16_t last_input            =
         input_state_get_last(port, device, index, id);
      if (result != last_input)
         runloop_st->flags         |= RUNLOOP_FLAG_INPUT_IS_DIRTY;
      /*arbitrary limit of up to 65536 elements in state array*/
      if (id < 65536)
         input_state_set_last(runloop_st, port, device, index, id, result);

      return result;
   }
   return 0;
}

static void reset_hook(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   runloop_st->flags          |= RUNLOOP_FLAG_INPUT_IS_DIRTY;       
   if (runloop_st->retro_reset_callback_original)
      runloop_st->retro_reset_callback_original();
}

static bool unserialize_hook(const void *buf, size_t size)
{
   runloop_state_t *runloop_st = &runloop_state;
   runloop_st->flags          |= RUNLOOP_FLAG_INPUT_IS_DIRTY;       
   if (runloop_st->retro_unserialize_callback_original)
      return runloop_st->retro_unserialize_callback_original(buf, size);
   return false;
}

static void add_input_state_hook(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs      = &runloop_st->retro_ctx;

   if (!runloop_st->input_state_callback_original)
   {
      runloop_st->input_state_callback_original = cbs->state_cb;
      cbs->state_cb                             = input_state_with_logging;
      runloop_st->current_core.retro_set_input_state(cbs->state_cb);
   }

   if (!runloop_st->retro_reset_callback_original)
   {
      runloop_st->retro_reset_callback_original 
         = runloop_st->current_core.retro_reset;
      runloop_st->current_core.retro_reset   = reset_hook;
   }

   if (!runloop_st->retro_unserialize_callback_original)
   {
      runloop_st->retro_unserialize_callback_original = runloop_st->current_core.retro_unserialize;
      runloop_st->current_core.retro_unserialize      = unserialize_hook;
   }
}

static void remove_input_state_hook(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs      = &runloop_st->retro_ctx;

   if (runloop_st->input_state_callback_original)
   {
      cbs->state_cb                             = 
         runloop_st->input_state_callback_original;
      runloop_st->current_core.retro_set_input_state(cbs->state_cb);
      runloop_st->input_state_callback_original = NULL;
      mylist_destroy(&runloop_st->input_state_list);
   }

   if (runloop_st->retro_reset_callback_original)
   {
      runloop_st->current_core.retro_reset               =
         runloop_st->retro_reset_callback_original;
      runloop_st->retro_reset_callback_original          = NULL;
   }

   if (runloop_st->retro_unserialize_callback_original)
   {
      runloop_st->current_core.retro_unserialize                =
         runloop_st->retro_unserialize_callback_original;
      runloop_st->retro_unserialize_callback_original           = NULL;
   }
}

static void *runahead_save_state_alloc(void)
{
   runloop_state_t     *runloop_st       = &runloop_state;
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)
      malloc(sizeof(retro_ctx_serialize_info_t));

   if (!savestate)
      return NULL;

   savestate->data          = NULL;
   savestate->data_const    = NULL;
   savestate->size          = 0;

   if (     (runloop_st->runahead_save_state_size > 0)
         && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      savestate->data       = malloc(runloop_st->runahead_save_state_size);
      savestate->data_const = savestate->data;
      savestate->size       = runloop_st->runahead_save_state_size;
   }

   return savestate;
}

static void runahead_save_state_free(void *data)
{
   retro_ctx_serialize_info_t *savestate = (retro_ctx_serialize_info_t*)data;
   if (!savestate)
      return;
   free(savestate->data);
   free(savestate);
}

static void runahead_save_state_list_init(
      runloop_state_t *runloop_st,
      size_t save_state_size)
{
   runloop_st->runahead_save_state_size       = save_state_size;
   runloop_st->flags |= RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;

   mylist_create(&runloop_st->runahead_save_state_list, 16,
         runahead_save_state_alloc, runahead_save_state_free);
}

/* Hooks - Hooks to cleanup, and add dirty input hooks */
static void runahead_remove_hooks(runloop_state_t *runloop_st)
{
   if (runloop_st->original_retro_deinit)
   {
      runloop_st->current_core.retro_deinit = 
         runloop_st->original_retro_deinit;
      runloop_st->original_retro_deinit     = NULL;
   }

   if (runloop_st->original_retro_unload)
   {
      runloop_st->current_core.retro_unload_game = 
         runloop_st->original_retro_unload;
      runloop_st->original_retro_unload          = NULL;
   }
   remove_input_state_hook(runloop_st);
}

static void runahead_destroy(runloop_state_t *runloop_st)
{
   mylist_destroy(&runloop_st->runahead_save_state_list);
   runahead_remove_hooks(runloop_st);
   runloop_runahead_clear_variables(runloop_st);
}

static void unload_hook(void)
{
   runloop_state_t     *runloop_st  = &runloop_state;

   runahead_remove_hooks(runloop_st);
   runahead_destroy(runloop_st);
   runloop_secondary_core_destroy();
   if (runloop_st->current_core.retro_unload_game)
      runloop_st->current_core.retro_unload_game();
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;
}

static void runahead_deinit_hook(void)
{
   runloop_state_t     *runloop_st = &runloop_state;

   runahead_remove_hooks(runloop_st);
   runahead_destroy(runloop_st);
   runloop_secondary_core_destroy();
   if (runloop_st->current_core.retro_deinit)
      runloop_st->current_core.retro_deinit();
}

static void runahead_add_hooks(runloop_state_t *runloop_st)
{
   if (!runloop_st->original_retro_deinit)
   {
      runloop_st->original_retro_deinit     = 
         runloop_st->current_core.retro_deinit;
      runloop_st->current_core.retro_deinit = runahead_deinit_hook;
   }

   if (!runloop_st->original_retro_unload)
   {
      runloop_st->original_retro_unload          = runloop_st->current_core.retro_unload_game;
      runloop_st->current_core.retro_unload_game = unload_hook;
   }
   add_input_state_hook(runloop_st);
}

/* Runahead Code */

static void runahead_error(runloop_state_t *runloop_st)
{
   runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_AVAILABLE;
   mylist_destroy(&runloop_st->runahead_save_state_list);
   runahead_remove_hooks(runloop_st);
   runloop_st->runahead_save_state_size       = 0;
   runloop_st->flags                         |= RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;
}

static bool runahead_create(runloop_state_t *runloop_st)
{
   /* get savestate size and allocate buffer */
   retro_ctx_size_info_t info;
   video_driver_state_t *video_st           = video_state_get_ptr();

   core_serialize_size_special(&info);

   runahead_save_state_list_init(runloop_st, info.size);
   if (video_st->flags & VIDEO_FLAG_ACTIVE)
      video_st->flags |=  VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;
   else
      video_st->flags &= ~VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;

   if (      (runloop_st->runahead_save_state_size == 0)
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      runahead_error(runloop_st);
      return false;
   }

   runahead_add_hooks(runloop_st);
   runloop_st->flags |= RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   if (runloop_st->runahead_save_state_list)
      mylist_resize(runloop_st->runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(runloop_state_t *runloop_st)
{
   retro_ctx_serialize_info_t *serialize_info;

   if (!runloop_st->runahead_save_state_list)
      return false;

   serialize_info                  =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   if (core_serialize_special(serialize_info))
      return true;

   runahead_error(runloop_st);
   return false;
}

static bool runahead_load_state(runloop_state_t *runloop_st)
{
   retro_ctx_serialize_info_t *serialize_info = 
      (retro_ctx_serialize_info_t*)
      runloop_st->runahead_save_state_list->data[0];
   bool last_dirty                            = runloop_st->flags & RUNLOOP_FLAG_INPUT_IS_DIRTY;
   bool ret                                   = core_unserialize_special(serialize_info);
   if (last_dirty)
      runloop_st->flags                      |=  RUNLOOP_FLAG_INPUT_IS_DIRTY;
   else
      runloop_st->flags                      &= ~RUNLOOP_FLAG_INPUT_IS_DIRTY;

   if (!ret)
      runahead_error(runloop_st);

   return ret;
}

#if HAVE_DYNAMIC
static bool runahead_load_state_secondary(void)
{
   runloop_state_t                *runloop_st = &runloop_state;
   settings_t                       *settings = config_get_ptr();
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   if (!secondary_core_deserialize(settings, serialize_info->data_const,
         serialize_info->size))
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      runahead_error(runloop_st);

      return false;
   }

   return true;
}
#endif

static void runahead_core_run_use_last_input(runloop_state_t *runloop_st)
{
   struct retro_callbacks *cbs            = &runloop_st->retro_ctx;
   retro_input_poll_t old_poll_function   = cbs->poll_cb;
   retro_input_state_t old_input_function = cbs->state_cb;

   cbs->poll_cb                           = retro_input_poll_null;
   cbs->state_cb                          = input_state_get_last;

   runloop_st->current_core.retro_set_input_poll(cbs->poll_cb);
   runloop_st->current_core.retro_set_input_state(cbs->state_cb);

   runloop_st->current_core.retro_run();

   cbs->poll_cb                           = old_poll_function;
   cbs->state_cb                          = old_input_function;

   runloop_st->current_core.retro_set_input_poll(cbs->poll_cb);
   runloop_st->current_core.retro_set_input_state(cbs->state_cb);
}

static void do_runahead(
      runloop_state_t *runloop_st,
      int runahead_count,
      bool runahead_hide_warnings,
      bool use_secondary)
{
   int frame_number        = 0;
   bool last_frame         = false;
   bool suspended_frame    = false;
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   const bool have_dynamic = true;
#else
   const bool have_dynamic = false;
#endif
   video_driver_state_t 
      *video_st            = video_state_get_ptr();
   uint64_t frame_count    = video_st->frame_count;
   audio_driver_state_t 
      *audio_st            = audio_state_get_ptr();

   if (      runahead_count <= 0 
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE))
      goto force_input_dirty;

   if (!(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN))
   {
      /* Disable runahead if current core reports
       * that it has an insufficient savestate
       * support level */
      if (!core_info_current_supports_runahead())
      {
         runahead_error(runloop_st);
         /* If core is incompatible with runahead,
          * log a warning but do not spam OSD messages.
          * Runahead menu entries are hidden when using
          * incompatible cores, so there is no mechanism
          * for users to respond to notifications. In
          * addition, auto-disabling runahead is a feature,
          * not a cause for 'concern'; OSD warnings should
          * be reserved for when a core reports that it is
          * runahead-compatible but subsequently fails in
          * execution */
         RARCH_WARN("[Run-Ahead]: %s\n", msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD));
         goto force_input_dirty;
      }

      if (!runahead_create(runloop_st))
      {
         const char *runahead_failed_str =
            msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES);
         if (!runahead_hide_warnings)
            runloop_msg_queue_push(runahead_failed_str, 0, 2 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
         goto force_input_dirty;
      }
   }

   /* Check for GUI */
   /* Hack: If we were in the GUI, force a resync. */
   if (frame_count != runloop_st->runahead_last_frame_count + 1)
      runloop_st->flags                  |= RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;

   runloop_st->runahead_last_frame_count  = frame_count;

   if (     !use_secondary
         || !have_dynamic
         || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE))
   {
      /* TODO: multiple savestates for higher performance
       * when not using secondary core */
      for (frame_number = 0; frame_number <= runahead_count; frame_number++)
      {
         last_frame      = frame_number == runahead_count;
         suspended_frame = !last_frame;

         if (suspended_frame)
         {
            audio_st->flags     |=  AUDIO_FLAG_SUSPENDED;
            video_st->flags     &= ~VIDEO_FLAG_ACTIVE;
         }

         if (frame_number == 0)
            core_run();
         else
            runahead_core_run_use_last_input(runloop_st);

         if (suspended_frame)
         {
            if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
               video_st->flags |=  VIDEO_FLAG_ACTIVE;
            else
               video_st->flags &= ~VIDEO_FLAG_ACTIVE;

            audio_st->flags    &= ~AUDIO_FLAG_SUSPENDED;
         }

         if (frame_number == 0)
         {
            if (!runahead_save_state(runloop_st))
            {
               const char *runahead_failed_str =
                  msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE);
               runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
               return;
            }
         }

         if (last_frame)
         {
            if (!runahead_load_state(runloop_st))
            {
               const char *runahead_failed_str =
                  msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE);
               runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
               return;
            }
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      if (!secondary_core_ensure_exists(config_get_ptr()))
      {
         const char *runahead_failed_str =
            msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE);
         runloop_secondary_core_destroy();
         runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
         runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
         goto force_input_dirty;
      }

      /* run main core with video suspended */
      video_st->flags &= ~VIDEO_FLAG_ACTIVE;
      core_run();
      if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
         video_st->flags |=  VIDEO_FLAG_ACTIVE;
      else
         video_st->flags &= ~VIDEO_FLAG_ACTIVE;

      if (     (runloop_st->flags & RUNLOOP_FLAG_INPUT_IS_DIRTY)
            || (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY))
      {
         runloop_st->flags &= ~RUNLOOP_FLAG_INPUT_IS_DIRTY;

         if (!runahead_save_state(runloop_st))
         {
            const char *runahead_failed_str =
               msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE);
            runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
            return;
         }

         if (!runahead_load_state_secondary())
         {
            const char *runahead_failed_str =
               msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE);
            runloop_msg_queue_push(runahead_failed_str, 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_WARN("[Run-Ahead]: %s\n", runahead_failed_str);
            return;
         }

         for (frame_number = 0; frame_number < runahead_count - 1; frame_number++)
         {
            video_st->flags             &= ~VIDEO_FLAG_ACTIVE;
            audio_st->flags             |= AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE;
            if (secondary_core_run_use_last_input())
               runloop_st->flags        |=  RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
            else
               runloop_st->flags        &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
            audio_st->flags             &= ~(AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE);
            if (video_st->flags & VIDEO_FLAG_RUNAHEAD_IS_ACTIVE)
               video_st->flags          |=  VIDEO_FLAG_ACTIVE;
            else
               video_st->flags          &= ~VIDEO_FLAG_ACTIVE;
         }
      }
      audio_st->flags                   |= AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE;
      if (secondary_core_run_use_last_input())
         runloop_st->flags              |=  RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      else
         runloop_st->flags              &= ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
      audio_st->flags                   &= ~(AUDIO_FLAG_SUSPENDED
                                         | AUDIO_FLAG_HARD_DISABLE);
#endif
   }
   runloop_st->flags &= ~RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   return;

force_input_dirty:
   core_run();
   runloop_st->flags |=  RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
}
#endif

static retro_time_t runloop_core_runtime_tick(
      runloop_state_t *runloop_st,
      float slowmotion_ratio,
      retro_time_t current_time)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   retro_time_t frame_time              =
      (1.0 / video_st->av_info.timing.fps) * 1000000;
   bool runloop_slowmotion              = runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION;
   bool runloop_fastmotion              = runloop_st->flags & RUNLOOP_FLAG_FASTMOTION;

   /* Account for slow motion */
   if (runloop_slowmotion)
      return (retro_time_t)((double)frame_time * slowmotion_ratio);

   /* Account for fast forward */
   if (runloop_fastmotion)
   {
      /* Doing it this way means we miss the first frame after
       * turning fast forward on, but it saves the overhead of
       * having to do:
       *    retro_time_t current_usec = cpu_features_get_time_usec();
       *    core_runtime_last         = current_usec;
       * every frame when fast forward is off. */
      retro_time_t current_usec              = current_time;
      retro_time_t potential_frame_time      = current_usec -
         runloop_st->core_runtime_last;
      runloop_st->core_runtime_last          = current_usec;

      if (potential_frame_time < frame_time)
         return potential_frame_time;
   }

   return frame_time;
}

static bool core_unload_game(void)
{
   runloop_state_t *runloop_st  = &runloop_state;

   video_driver_free_hw_context();

   video_driver_set_cached_frame_ptr(NULL);

   if (runloop_st->current_core.game_loaded)
   {
      RARCH_LOG("[Core]: Unloading game..\n");
      runloop_st->current_core.retro_unload_game();
      runloop_st->core_poll_type_override  = POLL_TYPE_OVERRIDE_DONTCARE;
      runloop_st->current_core.game_loaded = false;
   }

   audio_driver_stop();

   return true;
}

static void runloop_apply_fastmotion_override(runloop_state_t *runloop_st, settings_t *settings)
{
   float fastforward_ratio_current;
   video_driver_state_t *video_st                     = video_state_get_ptr();
   bool frame_time_counter_reset_after_fastforwarding = settings ?
         settings->bools.frame_time_counter_reset_after_fastforwarding : false;
   float fastforward_ratio_default                    = settings ?
         settings->floats.fastforward_ratio : 0.0f;
   float fastforward_ratio_last                       =
            (runloop_st->fastmotion_override.current.fastforward &&
                  (runloop_st->fastmotion_override.current.ratio >= 0.0f)) ?
                        runloop_st->fastmotion_override.current.ratio :
                              fastforward_ratio_default;
#if defined(HAVE_GFX_WIDGETS)
   dispgfx_widget_t *p_dispwidget                     = dispwidget_get_ptr();
#endif

   memcpy(&runloop_st->fastmotion_override.current,
         &runloop_st->fastmotion_override.next,
         sizeof(runloop_st->fastmotion_override.current));

   /* Check if 'fastmotion' state has changed */
   if (((runloop_st->flags & RUNLOOP_FLAG_FASTMOTION) > 0) !=
         runloop_st->fastmotion_override.current.fastforward)
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      if (runloop_st->fastmotion_override.current.fastforward)
         runloop_st->flags |=  RUNLOOP_FLAG_FASTMOTION;
      else
         runloop_st->flags &= ~RUNLOOP_FLAG_FASTMOTION;

      if (input_st)
      {
         if (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
            input_st->flags |=  INP_FLAG_NONBLOCKING;
         else
            input_st->flags &= ~INP_FLAG_NONBLOCKING;
      }

      if (!(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
         runloop_st->fastforward_after_frames = 1;

      driver_set_nonblock_state();

      /* Reset frame time counter when toggling
       * fast-forward off, if required */
      if ( !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
          && frame_time_counter_reset_after_fastforwarding)
         video_st->frame_time_count = 0;

      /* Ensure fast forward widget is disabled when
       * toggling fast-forward off
       * (required if RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE
       * is called during core de-initialisation) */
#if defined(HAVE_GFX_WIDGETS)
      if (      p_dispwidget->active 
            && !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
         video_st->flags &= ~VIDEO_FLAG_WIDGETS_FAST_FORWARD;
#endif
   }

   /* Update frame limit, if required */
   fastforward_ratio_current = (runloop_st->fastmotion_override.current.fastforward &&
         (runloop_st->fastmotion_override.current.ratio >= 0.0f)) ?
               runloop_st->fastmotion_override.current.ratio :
                     fastforward_ratio_default;

   if (fastforward_ratio_current != fastforward_ratio_last)
         runloop_st->frame_limit_minimum_time = 
            runloop_set_frame_limit(&video_st->av_info,
                  fastforward_ratio_current);
}


void runloop_event_deinit_core(void)
{
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
   runloop_state_t *runloop_st = &runloop_state;
   settings_t        *settings = config_get_ptr();

   core_unload_game();

   video_driver_set_cached_frame_ptr(NULL);

   if (runloop_st->current_core.inited)
   {
      RARCH_LOG("[Core]: Unloading core..\n");
      runloop_st->current_core.retro_deinit();
   }

   /* retro_deinit() may call
    * RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE
    * (i.e. to ensure that fastforwarding is
    * disabled on core close)
    * > Check for any pending updates */
   if (runloop_st->fastmotion_override.pending)
   {
      runloop_apply_fastmotion_override(runloop_st,
            settings);
      runloop_st->fastmotion_override.pending = false;
   }

   if (     (runloop_st->flags & RUNLOOP_FLAG_REMAPS_CORE_ACTIVE)
         || (runloop_st->flags & RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE)
         || (runloop_st->flags & RUNLOOP_FLAG_REMAPS_GAME_ACTIVE)
         || !string_is_empty(runloop_st->name.remapfile)
      )
   {
      input_remapping_deinit(settings->bools.remap_save_on_exit);
      input_remapping_set_defaults(true);
   }
   else
      input_remapping_restore_global_config(true);

   RARCH_LOG("[Core]: Unloading core symbols..\n");
   uninit_libretro_symbols(&runloop_st->current_core);
   runloop_st->current_core.symbols_inited = false;

   /* Restore original refresh rate, if it has been changed
    * automatically in SET_SYSTEM_AV_INFO */
   if (video_st->video_refresh_rate_original)
      video_display_server_restore_refresh_rate();

   /* Recalibrate frame delay target */
   if (settings->bools.video_frame_delay_auto)
      video_st->frame_delay_target = 0;

   driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_CONFIGFILE
   if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
   {
      /* Reload the original config */
      config_unload_override();
      runloop_st->flags &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;
   }
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   runloop_st->runtime_shader_preset_path[0] = '\0';
#endif
}

static void runloop_path_init_savefile_internal(void)
{
   runloop_state_t *runloop_st = &runloop_state;

   path_deinit_savefile();
   path_init_savefile_new();

   if (!runloop_path_init_subsystem())
      path_init_savefile_rtc(runloop_st->name.savefile);
}

static bool event_init_content(
      settings_t *settings,
      input_driver_state_t *input_st)
{
   runloop_state_t *runloop_st                  = &runloop_state;
#ifdef HAVE_CHEEVOS
   bool cheevos_enable                          =
      settings->bools.cheevos_enable;
   bool cheevos_hardcore_mode_enable            =
      settings->bools.cheevos_hardcore_mode_enable;
#endif
   const enum rarch_core_type current_core_type = runloop_st->current_core_type;
   uint8_t flags                                = content_get_flags();

   if (current_core_type == CORE_TYPE_PLAIN)
      runloop_st->flags |=  RUNLOOP_FLAG_USE_SRAM;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_USE_SRAM;

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (current_core_type == CORE_TYPE_DUMMY)
      return true;

   content_set_subsystem_info();

   /* If core is contentless, just initialise SRAM
    * interface, otherwise fill all content-related
    * paths */
   if (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
      runloop_path_init_savefile_internal();
   else
      runloop_path_fill_names();

   if (!content_init())
      return false;

   command_event_set_savestate_auto_index(settings);

   if (!event_load_save_files(runloop_st->flags &
            RUNLOOP_FLAG_IS_SRAM_LOAD_DISABLED))
      RARCH_LOG("[SRAM]: %s\n",
            msg_hash_to_str(MSG_SKIPPING_SRAM_LOAD));

/*
   Since the operations are asynchronous we can't
   guarantee users will not use auto_load_state to cheat on
   achievements so we forbid auto_load_state from happening
   if cheevos_enable and cheevos_hardcode_mode_enable
   are true.
*/
#ifdef HAVE_CHEEVOS
   if (!cheevos_enable || !cheevos_hardcore_mode_enable)
#endif
   {
      if (runloop_st->entry_state_slot && !command_event_load_entry_state())
         runloop_st->entry_state_slot = 0;
      if (!runloop_st->entry_state_slot && settings->bools.savestate_auto_load)
         command_event_load_auto_state();
   }

#ifdef HAVE_BSV_MOVIE
   bsv_movie_deinit(input_st);
   if (bsv_movie_init(input_st))
   {
      /* Set granularity upon success */
      configuration_set_uint(settings,
            settings->uints.rewind_granularity, 1);
   }
#endif
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);

   return true;
}

static void runloop_runtime_log_init(runloop_state_t *runloop_st)
{
   const char *content_path            = path_get(RARCH_PATH_CONTENT);
   const char *core_path               = path_get(RARCH_PATH_CORE);

   runloop_st->core_runtime_last       = cpu_features_get_time_usec();
   runloop_st->core_runtime_usec       = 0;

   /* Have to cache content and core path here, otherwise
    * logging fails if new content is loaded without
    * closing existing content
    * i.e. RARCH_PATH_CONTENT and RARCH_PATH_CORE get
    * updated when the new content is loaded, which
    * happens *before* command_event_runtime_log_deinit
    * -> using RARCH_PATH_CONTENT and RARCH_PATH_CORE
    *    directly in command_event_runtime_log_deinit
    *    can therefore lead to the runtime of the currently
    *    loaded content getting written to the *new*
    *    content's log file... */
   memset(runloop_st->runtime_content_path,
         0, sizeof(runloop_st->runtime_content_path));
   memset(runloop_st->runtime_core_path,
         0, sizeof(runloop_st->runtime_core_path));

   if (!string_is_empty(content_path))
      strlcpy(runloop_st->runtime_content_path,
            content_path,
            sizeof(runloop_st->runtime_content_path));

   if (!string_is_empty(core_path))
      strlcpy(runloop_st->runtime_core_path,
            core_path,
            sizeof(runloop_st->runtime_core_path));
}

float runloop_set_frame_limit(
      const struct retro_system_av_info *av_info,
      float fastforward_ratio)
{
   if (fastforward_ratio < 1.0f)
      return 0.0f;
   return (retro_time_t)roundf(1000000.0f / 
         (av_info->timing.fps * fastforward_ratio));
}

float runloop_get_fastforward_ratio(
      settings_t *settings,
      struct retro_fastforwarding_override *fastmotion_override)
{
   if (      fastmotion_override->fastforward 
         && (fastmotion_override->ratio >= 0.0f))
      return fastmotion_override->ratio;
   return settings->floats.fastforward_ratio;
}

void runloop_set_video_swap_interval(
      bool vrr_runloop_enable,
      bool crt_switching_active,
      unsigned swap_interval_config,
      float audio_max_timing_skew,
      float video_refresh_rate,
      double input_fps)
{
   runloop_state_t *runloop_st = &runloop_state;
   float core_hz               = input_fps;
   float timing_hz             = crt_switching_active ?
         input_fps : video_refresh_rate;
   float swap_ratio;
   unsigned swap_integer;
   float timing_skew;

   /* If automatic swap interval selection is
    * disabled, just record user-set value */
   if (swap_interval_config != 0)
   {
      runloop_st->video_swap_interval_auto =
            swap_interval_config;
      return;
   }

   /* > If VRR is enabled, swap interval is irrelevant,
    *   just set to 1
    * > If core fps is higher than display refresh rate,
    *   set swap interval to 1
    * > If core fps or display refresh rate are zero,
    *   set swap interval to 1 */
   if (vrr_runloop_enable ||
       (core_hz > timing_hz) ||
       (core_hz <= 0.0f) ||
       (timing_hz <= 0.0f))
   {
      runloop_st->video_swap_interval_auto = 1;
      return;
   }

   /* Check whether display refresh rate is an integer
    * multiple of core fps (within timing skew tolerance) */
   swap_ratio = timing_hz / core_hz;
   swap_integer = (unsigned)(swap_ratio + 0.5f);

   /* > Sanity check: swap interval must be in the
    *   range [1,4] - if we are outside this, then
    *   bail... */
   if ((swap_integer < 1) || (swap_integer > 4))
   {
      runloop_st->video_swap_interval_auto = 1;
      return;
   }

   timing_skew = fabs(1.0f - core_hz / (timing_hz / (float)swap_integer));

   runloop_st->video_swap_interval_auto =
         (timing_skew <= audio_max_timing_skew) ?
               swap_integer : 1;
}

unsigned runloop_get_video_swap_interval(
      unsigned swap_interval_config)
{
   runloop_state_t *runloop_st = &runloop_state;
   return (swap_interval_config == 0) ?
         runloop_st->video_swap_interval_auto :
         swap_interval_config;
}

unsigned int retroarch_get_rotation(void)
{
   settings_t     *settings    = config_get_ptr();
   unsigned     video_rotation = settings->uints.video_rotation;
   return video_rotation + runloop_state.system.rotation;
}

static void retro_run_null(void) { } /* Stub function callback impl. */

static bool core_verify_api_version(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   unsigned api_version        = runloop_st->current_core.retro_api_version();

   RARCH_LOG("[Core]: %s: %u, %s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version,
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION
         );

   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("[Core]: %s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
   return true;
}

static int16_t core_input_state_poll_late(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   runloop_state_t     *runloop_st = &runloop_state;
   if (!runloop_st->current_core.input_polled)
      input_driver_poll();
   runloop_st->current_core.input_polled = true;

   return input_driver_state_wrapper(port, device, idx, id);
}

static void core_input_state_poll_maybe(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   const enum poll_type_override_t
      core_poll_type_override  = runloop_st->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override > POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : runloop_st->current_core.poll_type;
   if (new_poll_type == POLL_TYPE_NORMAL)
      input_driver_poll();
}


static retro_input_state_t core_input_state_poll_return_cb(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   const enum poll_type_override_t
      core_poll_type_override  = runloop_st->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override > POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : runloop_st->current_core.poll_type;
   if (new_poll_type == POLL_TYPE_LATE)
      return core_input_state_poll_late;
   return input_driver_state_wrapper;
}


/**
 * core_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks
 * to default callback functions.
 **/
static bool core_init_libretro_cbs(struct retro_callbacks *cbs)
{
   runloop_state_t *runloop_st  = &runloop_state;
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   runloop_st->current_core.retro_set_video_refresh(video_driver_frame);
   runloop_st->current_core.retro_set_audio_sample(audio_driver_sample);
   runloop_st->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   runloop_st->current_core.retro_set_input_state(state_cb);
   runloop_st->current_core.retro_set_input_poll(core_input_state_poll_maybe);

   core_set_default_callbacks(cbs);

#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return true;

   core_set_netplay_callbacks();
#endif

   return true;
}


static bool core_load(unsigned poll_type_behavior)
{
   video_driver_state_t *video_st     = video_state_get_ptr();
   runloop_state_t *runloop_st        = &runloop_state;
   runloop_st->current_core.poll_type = poll_type_behavior;

   if (!core_verify_api_version())
      return false;
   if (!core_init_libretro_cbs(&runloop_st->retro_ctx))
      return false;

   runloop_st->current_core.retro_get_system_av_info(&video_st->av_info);
   video_st->core_frame_time = 1000000 /
         ((video_st->av_info.timing.fps > 0.0) ?
               video_st->av_info.timing.fps : 60.0);

   return true;
}


bool runloop_event_init_core(
      settings_t *settings,
      void *input_data,
      enum rarch_core_type type)
{
   size_t len;
   runloop_state_t *runloop_st     = &runloop_state;
   input_driver_state_t *input_st  = (input_driver_state_t*)input_data;
   video_driver_state_t *video_st  = video_state_get_ptr();
#ifdef HAVE_CONFIGFILE
   bool auto_overrides_enable      = settings->bools.auto_overrides_enable;
   bool auto_remaps_enable         = false;
   const char *dir_input_remapping = NULL;
#endif
   bool show_set_initial_disk_msg  = false;
   unsigned poll_type_behavior     = 0;
   float fastforward_ratio         = 0.0f;
   rarch_system_info_t *sys_info   = &runloop_st->system;

#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
   {
#ifdef HAVE_UPDATE_CORES
      /* If netplay is enabled, update the core before initializing. */
      const char *path_core = path_get(RARCH_PATH_CORE);

      if (!string_is_empty(path_core) &&
            !string_is_equal(path_core, "builtin"))
      {
         if (task_push_update_single_core(path_core,
               settings->bools.core_updater_auto_backup,
               settings->uints.core_updater_auto_backup_history_size,
               settings->paths.directory_libretro,
               settings->paths.directory_core_assets))
            /* We must wait for the update to finish
               before starting the core. */
            task_queue_wait(NULL, NULL);
      }
#endif

      /* We need this in order for core_info_current_supports_netplay
         to work correctly at init_netplay,
         called later at event_init_content. */
      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
      command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
   }
#endif

   if (!init_libretro_symbols(runloop_st,
            type, &runloop_st->current_core))
      return false;
   if (!runloop_st->current_core.retro_run)
      runloop_st->current_core.retro_run   = retro_run_null;
   runloop_st->current_core.symbols_inited = true;
   runloop_st->current_core.retro_get_system_info(&sys_info->info);

   if (!sys_info->info.library_name)
      sys_info->info.library_name = msg_hash_to_str(MSG_UNKNOWN);
   if (!sys_info->info.library_version)
      sys_info->info.library_version = "v0";

   len = strlcpy(
         video_st->title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         sizeof(video_st->title_buf));
   video_st->title_buf[len  ] = ' ';
   video_st->title_buf[len+1] = '\0';
   len = strlcat(video_st->title_buf,
         sys_info->info.library_name,
         sizeof(video_st->title_buf));
   video_st->title_buf[len  ] = ' ';
   video_st->title_buf[len+1] = '\0';
   strlcat(video_st->title_buf,
         sys_info->info.library_version,
         sizeof(video_st->title_buf));

   strlcpy(sys_info->valid_extensions,
         sys_info->info.valid_extensions ?
         sys_info->info.valid_extensions : DEFAULT_EXT,
         sizeof(sys_info->valid_extensions));

#ifdef HAVE_CONFIGFILE
   if (auto_overrides_enable)
   {
      if (config_load_override(&runloop_st->system))
         runloop_st->flags |=  RUNLOOP_FLAG_OVERRIDES_ACTIVE;
      else
         runloop_st->flags &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;
   }
#endif

   /* Cannot access these settings-related parameters
    * until *after* config overrides have been loaded */
#ifdef HAVE_CONFIGFILE
   auto_remaps_enable        = settings->bools.auto_remaps_enable;
   dir_input_remapping       = settings->paths.directory_input_remapping;
#endif
   show_set_initial_disk_msg = settings->bools.notification_show_set_initial_disk;
   poll_type_behavior        = settings->uints.input_poll_type_behavior;
   fastforward_ratio         = runloop_get_fastforward_ratio(
         settings, &runloop_st->fastmotion_override.current);

#ifdef HAVE_CHEEVOS
   /* assume the core supports achievements unless it tells us otherwise */
   rcheevos_set_support_cheevos(true);
#endif

   /* Load auto-shaders on the next occasion */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   video_st->flags |= VIDEO_FLAG_SHADER_PRESETS_NEED_RELOAD;
   runloop_st->shader_delay_timer.timer_begin = false; /* not initialized */
   runloop_st->shader_delay_timer.timer_end   = false; /* not expired */
#endif

   /* reset video format to libretro's default */
   video_st->pix_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

   runloop_st->current_core.retro_set_environment(runloop_environment_cb);

   /* Load any input remap files
    * > Note that we always cache the current global
    *   input settings when initialising a core
    *   (regardless of whether remap files are loaded)
    *   so settings can be restored when the core is
    *   unloaded - i.e. core remapping options modified
    *   at runtime should not 'bleed through' into the
    *   master config file */
   input_remapping_cache_global_config();
#ifdef HAVE_CONFIGFILE
   if (auto_remaps_enable)
      config_load_remap(dir_input_remapping, &runloop_st->system);
#endif

   /* Per-core saves: reset redirection paths */
   retroarch_path_set_redirect(settings);

   video_driver_set_cached_frame_ptr(NULL);

   runloop_st->current_core.retro_init();
   runloop_st->current_core.inited          = true;

   /* Attempt to set initial disk index */
   disk_control_set_initial_index(
         &sys_info->disk_control,
         path_get(RARCH_PATH_CONTENT),
         runloop_st->savefile_dir);

   if (!event_init_content(settings, input_st))
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
      return false;
   }

   /* Verify that initial disk index was set correctly */
   disk_control_verify_initial_index(&sys_info->disk_control,
         show_set_initial_disk_msg);

   if (!core_load(poll_type_behavior))
      return false;

   runloop_st->frame_limit_minimum_time = 
     runloop_set_frame_limit(&video_st->av_info,
           fastforward_ratio);
   runloop_st->frame_limit_last_time    = cpu_features_get_time_usec();

   runloop_runtime_log_init(runloop_st);
   return true;
}

#ifdef HAVE_RUNAHEAD
void runloop_runahead_clear_variables(runloop_state_t *runloop_st)
{
   video_driver_state_t 
      *video_st                           = video_state_get_ptr();
   runloop_st->runahead_save_state_size   = 0;
   runloop_st->flags                     &= ~RUNLOOP_FLAG_RUNAHEAD_SAVE_STATE_SIZE_KNOWN;
   video_st->flags                       |= VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;
   runloop_st->flags                     |= RUNLOOP_FLAG_RUNAHEAD_AVAILABLE
                                          | RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE
                                          | RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY;
   runloop_st->runahead_last_frame_count  = 0;
}
#endif


void runloop_pause_checks(void)
{
#ifdef HAVE_PRESENCE
   presence_userdata_t userdata;
#endif
   runloop_state_t *runloop_st    = &runloop_state;
   bool is_paused                 = runloop_st->flags & RUNLOOP_FLAG_PAUSED;
   bool is_idle                   = runloop_st->flags & RUNLOOP_FLAG_IDLE;
#if defined(HAVE_GFX_WIDGETS)
   video_driver_state_t *video_st = video_state_get_ptr();
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   bool widgets_active            = p_dispwidget->active;
   if (widgets_active)
   {
      if (is_paused)
         video_st->flags |=  VIDEO_FLAG_WIDGETS_PAUSED;
      else
         video_st->flags &= ~VIDEO_FLAG_WIDGETS_PAUSED;
   }
#endif

   if (is_paused)
   {
#if defined(HAVE_GFX_WIDGETS)
      if (!widgets_active)
#endif
         runloop_msg_queue_push(msg_hash_to_str(MSG_PAUSED), 1,
               1, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);


      if (!is_idle)
         video_driver_cached_frame();

#ifdef HAVE_PRESENCE
      userdata.status = PRESENCE_GAME_PAUSED;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
#endif

#ifndef HAVE_LAKKA_SWITCH
#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_MENU);
#endif
#endif /* #ifndef HAVE_LAKKA_SWITCH */
   }
   else
   {
#ifndef HAVE_LAKKA_SWITCH
#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_CORE);
#endif
#endif /* #ifndef HAVE_LAKKA_SWITCH */
   }

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (p_dispwidget->ai_service_overlay_state == 1)
      gfx_widgets_ai_service_overlay_unload();
#endif
}

void runloop_frame_time_free(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
   memset(&runloop_st->frame_time, 0,
         sizeof(struct retro_frame_time_callback));
   runloop_st->frame_time_last    = 0;
   runloop_st->max_frames         = 0;
}

void runloop_audio_buffer_status_free(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
   memset(&runloop_st->audio_buffer_status, 0,
         sizeof(struct retro_audio_buffer_status_callback));
   runloop_st->audio_latency = 0;
}

void runloop_fastmotion_override_free(void)
{
   runloop_state_t 
	   *runloop_st          = &runloop_state;
   video_driver_state_t 
      *video_st            = video_state_get_ptr();
   settings_t *settings    = config_get_ptr();
   float fastforward_ratio = settings->floats.fastforward_ratio;
   bool reset_frame_limit  = runloop_st->fastmotion_override.current.fastforward &&
         (runloop_st->fastmotion_override.current.ratio >= 0.0f) &&
         (runloop_st->fastmotion_override.current.ratio != fastforward_ratio);

   runloop_st->fastmotion_override.current.ratio          = 0.0f;
   runloop_st->fastmotion_override.current.fastforward    = false;
   runloop_st->fastmotion_override.current.notification   = false;
   runloop_st->fastmotion_override.current.inhibit_toggle = false;

   runloop_st->fastmotion_override.next.ratio             = 0.0f;
   runloop_st->fastmotion_override.next.fastforward       = false;
   runloop_st->fastmotion_override.next.notification      = false;
   runloop_st->fastmotion_override.next.inhibit_toggle    = false;

   runloop_st->fastmotion_override.pending                = false;

   if (reset_frame_limit)
      runloop_st->frame_limit_minimum_time                = 
         runloop_set_frame_limit(&video_st->av_info, fastforward_ratio);
}

void runloop_core_options_cb_free(void)
{
   runloop_state_t 
	   *runloop_st          = &runloop_state;
   /* Only a single core options callback is used at present */
   runloop_st->core_options_callback.update_display = NULL;
}

struct string_list *path_get_subsystem_list(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->subsystem_fullpaths;
}

bool runloop_path_init_subsystem(void)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   runloop_state_t             *runloop_st = &runloop_state;
   rarch_system_info_t             *system = &runloop_st->system;
   bool subsystem_path_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   const char                *savefile_dir = runloop_st->savefile_dir;

   if (!system || subsystem_path_empty)
      return false;

   /* For subsystems, we know exactly which RAM types are supported. */
   /* We'll handle this error gracefully later. */
   if ((info = libretro_find_subsystem_info(
         system->subsystem.data,
         system->subsystem.size,
         path_get(RARCH_PATH_SUBSYSTEM))))
   {
      unsigned num_content = MIN(info->num_roms,
            subsystem_path_empty ?
            0 : (unsigned)runloop_st->subsystem_fullpaths->size);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            char ext[32];
            union string_list_elem_attr attr;
            char savename[PATH_MAX_LENGTH];
            char path[PATH_MAX_LENGTH];
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];
            ext[0]  = '.';
            ext[1]  = '\0';
            strlcat(ext, mem->extension, sizeof(ext));
            strlcpy(savename,
                  runloop_st->subsystem_fullpaths->elems[i].data,
                  sizeof(savename));
            path_remove_extension(savename);

            if (path_is_directory(savefile_dir))
            {
               /* Use SRAM dir */
               /* Redirect content fullpath to save directory. */
               strlcpy(path, savefile_dir, sizeof(path));
               fill_pathname_dir(path, savename, ext, sizeof(path));
            }
            else
               fill_pathname(path, savename, ext, sizeof(path));

            RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               path);

            attr.i = mem->type;
            string_list_append((struct string_list*)savefile_ptr_get(),
                  path, attr);
         }
      }
   }

   /* Let other relevant paths be inferred 
      from the main SRAM location. */
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
   {
      size_t len = strlcpy(runloop_st->name.savefile,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.savefile));
      runloop_st->name.savefile[len  ] = '.';
      runloop_st->name.savefile[len+1] = 's';
      runloop_st->name.savefile[len+2] = 'r';
      runloop_st->name.savefile[len+3] = 'm';
      runloop_st->name.savefile[len+4] = '\0';
   }

   if (path_is_directory(runloop_st->name.savefile))
   {
      fill_pathname_dir(runloop_st->name.savefile,
            runloop_st->runtime_content_path_basename,
            ".srm",
            sizeof(runloop_st->name.savefile));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
            runloop_st->name.savefile);
   }

   return true;
}

void runloop_path_fill_names(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
#ifdef HAVE_BSV_MOVIE
   input_driver_state_t *input_st = input_state_get_ptr();
#endif

   runloop_path_init_savefile_internal();

#ifdef HAVE_BSV_MOVIE
   strlcpy(input_st->bsv_movie_state.movie_path,
         runloop_st->name.savefile,
         sizeof(input_st->bsv_movie_state.movie_path));
#endif

   if (string_is_empty(runloop_st->runtime_content_path_basename))
      return;

   if (string_is_empty(runloop_st->name.ups))
   {
      size_t len = strlcpy(runloop_st->name.ups,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.ups));
      runloop_st->name.ups[len  ] = '.';
      runloop_st->name.ups[len+1] = 'u';
      runloop_st->name.ups[len+2] = 'p';
      runloop_st->name.ups[len+3] = 's';
      runloop_st->name.ups[len+4] = '\0';
   }

   if (string_is_empty(runloop_st->name.bps))
   {
      size_t len = strlcpy(runloop_st->name.bps,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.bps));
      runloop_st->name.bps[len  ] = '.';
      runloop_st->name.bps[len+1] = 'b';
      runloop_st->name.bps[len+2] = 'p';
      runloop_st->name.bps[len+3] = 's';
      runloop_st->name.bps[len+4] = '\0';
   }

   if (string_is_empty(runloop_st->name.ips))
   {
      size_t len = strlcpy(runloop_st->name.ips,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.ips));
      runloop_st->name.ips[len  ] = '.';
      runloop_st->name.ips[len+1] = 'i';
      runloop_st->name.ips[len+2] = 'p';
      runloop_st->name.ips[len+3] = 's';
      runloop_st->name.ips[len+4] = '\0';
   }
}


void runloop_path_init_savefile(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   bool    should_sram_be_used = 
          (runloop_st->flags & RUNLOOP_FLAG_USE_SRAM)
      && !(runloop_st->flags & RUNLOOP_FLAG_IS_SRAM_SAVE_DISABLED);

   if (should_sram_be_used)
      runloop_st->flags |=  RUNLOOP_FLAG_USE_SRAM;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_USE_SRAM;

   if (!(runloop_st->flags & RUNLOOP_FLAG_USE_SRAM))
   {
      RARCH_LOG("[SRAM]: %s\n",
            msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));
      return;
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
}


/* Creates folder and core options stub file for subsequent runs */
bool core_options_create_override(bool game_specific)
{
   char options_path[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st = &runloop_state;
   config_file_t *conf         = NULL;

   options_path[0]             = '\0';

   if (game_specific)
   {
      /* Get options file path (game-specific) */
      if (!validate_game_options(
               runloop_st->system.info.library_name,
               options_path,
               sizeof(options_path), true))
         goto error;
   }
   else
   {
      /* Sanity check - cannot create a folder-specific
       * override if a game-specific override is
       * already active */
      if (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE)
         goto error;

      /* Get options file path (folder-specific) */
      if (!validate_folder_options(
               options_path,
               sizeof(options_path), true))
         goto error;
   }

   /* Open config file */
   if (!(conf = config_file_new_from_path_to_string(options_path)))
      if (!(conf = config_file_new_alloc()))
         goto error;

   /* Write config file */
   core_option_manager_flush(runloop_st->core_options, conf);

   if (!config_file_write(conf, options_path, true))
      goto error;

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   path_set(RARCH_PATH_CORE_OPTIONS, options_path);
   if (game_specific)
      runloop_st->flags |=  RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
   if (!game_specific)
      runloop_st->flags |=  RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;

   config_file_free(conf);
   return true;

error:
   runloop_msg_queue_push(
         msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (conf)
      config_file_free(conf);

   return false;
}

bool core_options_remove_override(bool game_specific)
{
   char new_options_path[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st      = &runloop_state;
   settings_t *settings             = config_get_ptr();
   core_option_manager_t *coreopts  = runloop_st->core_options;
   bool per_core_options            = !settings->bools.global_core_options;
   const char *path_core_options    = settings->paths.path_core_options;
   const char *current_options_path = NULL;
   config_file_t *conf              = NULL;
   bool folder_options_active       = false;

   new_options_path[0]              = '\0';

   /* Sanity check 1 - if there are no core options
    * or no overrides are active, there is nothing to do */
   if (          !coreopts ||
         (       (!(runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE))
              && (!(runloop_st->flags & RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE))
      ))
      return true;

   /* Sanity check 2 - can only remove an override
    * if the specified type is currently active */
   if (      game_specific 
         && !(runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE)
      )
      goto error;

   /* Get current options file path */
   current_options_path = path_get(RARCH_PATH_CORE_OPTIONS);
   if (string_is_empty(current_options_path))
      goto error;

   /* Remove current options file, if required */
   if (path_is_valid(current_options_path))
      filestream_delete(current_options_path);

   /* Reload any existing 'parent' options file
    * > If we have removed a game-specific config,
    *   check whether a folder-specific config
    *   exists */
   if (game_specific &&
       validate_folder_options(
          new_options_path,
          sizeof(new_options_path), false) &&
       path_is_valid(new_options_path))
      folder_options_active = true;

   /* > If a folder-specific config does not exist,
    *   or we removed it, check whether we have a
    *   top-level config file */
   if (!folder_options_active)
   {
      /* Try core-specific options, if enabled */
      if (per_core_options)
      {
         const char *core_name = runloop_st->system.info.library_name;
         per_core_options      = validate_per_core_options(
               new_options_path, sizeof(new_options_path), true,
                     core_name, core_name);
      }

      /* ...otherwise use global options */
      if (!per_core_options)
      {
         if (!string_is_empty(path_core_options))
            strlcpy(new_options_path,
                  path_core_options, sizeof(new_options_path));
         else if (!path_is_empty(RARCH_PATH_CONFIG))
            fill_pathname_resolve_relative(
                  new_options_path, path_get(RARCH_PATH_CONFIG),
                        FILE_PATH_CORE_OPTIONS_CONFIG, sizeof(new_options_path));
      }
   }

   if (string_is_empty(new_options_path))
      goto error;

   /* > If we have a valid file, load it */
   if (folder_options_active ||
       path_is_valid(new_options_path))
   {
      size_t i, j;

      if (!(conf = config_file_new_from_path_to_string(new_options_path)))
         goto error;

      for (i = 0; i < coreopts->size; i++)
      {
         struct config_entry_list *entry = NULL;
         struct core_option      *option = (struct core_option*)&coreopts->opts[i];
         if (!option)
            continue;
         if (!(entry = config_get_entry(conf, option->key)))
            continue;
         if (string_is_empty(entry->value))
            continue;

         /* Set current config value from file entry */
         for (j = 0; j < option->vals->size; j++)
         {
            if (string_is_equal(option->vals->elems[j].data, entry->value))
            {
               option->index = j;
               break;
            }
         }
      }

      coreopts->updated = true;

#ifdef HAVE_CHEEVOS
      rcheevos_validate_config_settings();
#endif
   }

   /* Update runloop status */
   if (folder_options_active)
   {
      path_set(RARCH_PATH_CORE_OPTIONS, new_options_path);
      runloop_st->flags &= ~RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
   }
   else
   {
      path_clear(RARCH_PATH_CORE_OPTIONS);
      runloop_st->flags &= ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                           | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);

      /* Update config file path/object stored in
       * core option manager struct */
      strlcpy(coreopts->conf_path, new_options_path,
            sizeof(coreopts->conf_path));

      if (conf)
      {
         config_file_free(coreopts->conf);
         coreopts->conf = conf;
         conf           = NULL;
      }
   }

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (conf)
      config_file_free(conf);

   return true;

error:
   runloop_msg_queue_push(
         msg_hash_to_str(MSG_ERROR_REMOVING_CORE_OPTIONS_FILE),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (conf)
      config_file_free(conf);

   return false;
}

void core_options_reset(void)
{
   size_t i;
   runloop_state_t *runloop_st     = &runloop_state;
   core_option_manager_t *coreopts = runloop_st->core_options;

   /* If there are no core options, there
    * is nothing to do */
   if (!coreopts || (coreopts->size < 1))
      return;

   for (i = 0; i < coreopts->size; i++)
      coreopts->opts[i].index = coreopts->opts[i].default_index;

   coreopts->updated = true;

#ifdef HAVE_CHEEVOS
   rcheevos_validate_config_settings();
#endif

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_CORE_OPTIONS_RESET),
         1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void core_options_flush(void)
{
   runloop_state_t *runloop_st     = &runloop_state;
   core_option_manager_t *coreopts = runloop_st->core_options;
   const char *path_core_options   = path_get(RARCH_PATH_CORE_OPTIONS);
   const char *core_options_file   = NULL;
   bool success                    = false;
   char msg[256];

   msg[0] = '\0';

   /* If there are no core options, there
    * is nothing to do */
   if (!coreopts || (coreopts->size < 1))
      return;

   /* Check whether game/folder-specific options file
    * is being used */
   if (!string_is_empty(path_core_options))
   {
      config_file_t *conf_tmp = NULL;
      bool path_valid         = path_is_valid(path_core_options); 

      /* Attempt to load existing file */
      if (path_valid)
         conf_tmp = config_file_new_from_path_to_string(path_core_options);

      /* Create new file if required */
      if (!conf_tmp)
         conf_tmp = config_file_new_alloc();

      if (conf_tmp)
      {
         core_option_manager_flush(runloop_st->core_options, conf_tmp);

         success = config_file_write(conf_tmp, path_core_options, true);
         config_file_free(conf_tmp);
      }
   }
   else
   {
      /* We are using the 'default' core options file */
      path_core_options = runloop_st->core_options->conf_path;

      if (!string_is_empty(path_core_options))
      {
         core_option_manager_flush(
               runloop_st->core_options,
               runloop_st->core_options->conf);

         /* We must *guarantee* that a file gets written
          * to disk if any options differ from the current
          * options file contents. Must therefore handle
          * the case where the 'default' file does not
          * exist (e.g. if it gets deleted manually while
          * a core is running) */
         if (!path_is_valid(path_core_options))
            runloop_st->core_options->conf->modified = true;

         success = config_file_write(runloop_st->core_options->conf,
               path_core_options, true);
      }
   }

   /* Get options file name for display purposes */
   if (!string_is_empty(path_core_options))
      core_options_file = path_basename_nocompression(path_core_options);

   if (string_is_empty(core_options_file))
      core_options_file = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);

   if (success)
   {
      /* Log result */
      RARCH_LOG(
            "[Core]: Saved core options to \"%s\".\n",
            path_core_options ? path_core_options : "UNKNOWN");

      snprintf(msg, sizeof(msg), "%s \"%s\"",
            msg_hash_to_str(MSG_CORE_OPTIONS_FLUSHED),
            core_options_file);
   }
   else
   {
      /* Log result */
      RARCH_LOG(
            "[Core]: Failed to save core options to \"%s\".\n",
            path_core_options ? path_core_options : "UNKNOWN");

      snprintf(msg, sizeof(msg), "%s \"%s\"",
            msg_hash_to_str(MSG_CORE_OPTIONS_FLUSH_FAILED),
            core_options_file);
   }

   runloop_msg_queue_push(
         msg, 1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
#if defined(HAVE_GFX_WIDGETS)
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   bool widgets_active            = p_dispwidget->active;
#endif
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings           = config_get_ptr();
   bool accessibility_enable      = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   access_state_t *access_st      = access_state_get_ptr();
#endif
   runloop_state_t *runloop_st    = &runloop_state;

   RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
      accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            (char*) msg, 0);
#endif
#if defined(HAVE_GFX_WIDGETS)
   if (widgets_active)
   {
      gfx_widgets_msg_queue_push(
            NULL,
            msg,
            roundf((float)duration / 60.0f * 1000.0f),
            title,
            icon,
            category,
            prio,
            flush,
#ifdef HAVE_MENU
            menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE
#else
            false
#endif
            );
      duration = duration * 60 / 1000;
   }
   else
#endif
   {
      if (flush)
         msg_queue_clear(&runloop_st->msg_queue);

      msg_queue_push(&runloop_st->msg_queue, msg,
            prio, duration,
            title, icon, category);

      runloop_st->msg_queue_size = msg_queue_size(
            &runloop_st->msg_queue);
   }

   ui_companion_driver_msg_queue_push(
         msg, prio, duration, flush);

   RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
}

#ifdef HAVE_MENU
/* Display the libretro core's framebuffer onscreen. */
static bool display_menu_libretro(
      runloop_state_t *runloop_st,
      input_driver_state_t *input_st,
      float slowmotion_ratio,
      bool libretro_running,
      retro_time_t current_time)
{
   bool runloop_idle             = runloop_st->flags & RUNLOOP_FLAG_IDLE;
   video_driver_state_t*video_st = video_state_get_ptr();

   if (     video_st->poke
         && video_st->poke->set_texture_enable)
      video_st->poke->set_texture_enable(video_st->data, true, false);

   if (libretro_running)
   {
      if (!(input_st->flags & INP_FLAG_BLOCK_LIBRETRO_INPUT))
         input_st->flags |= INP_FLAG_BLOCK_LIBRETRO_INPUT;

      core_run();
      runloop_st->core_runtime_usec       +=
         runloop_core_runtime_tick(runloop_st, slowmotion_ratio, current_time);
      input_st->flags                     &= ~INP_FLAG_BLOCK_LIBRETRO_INPUT;

      return false;
   }

   if (runloop_idle)
   {
#ifdef HAVE_PRESENCE
      presence_userdata_t userdata;
      userdata.status = PRESENCE_GAME_PAUSED;

      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
#endif
      return false;
   }

   return true;
}
#endif

#define HOTKEY_CHECK(cmd1, cmd2, cond, cond2) \
   { \
      static bool old_pressed                   = false; \
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
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
      bool pressed                              = BIT256_GET(current_bits, cmd1); \
      bool pressed2                             = BIT256_GET(current_bits, cmd3); \
      bool pressed3                             = BIT256_GET(current_bits, cmd5); \
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

static enum runloop_state_enum runloop_check_state(
      bool error_on_init,
      settings_t *settings,
      retro_time_t current_time)
{
   input_bits_t current_bits;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
   uico_driver_state_t  *uico_st       = uico_state_get_ptr();
   input_driver_state_t *input_st      = input_state_get_ptr();
   video_driver_state_t *video_st      = video_state_get_ptr();
   gfx_display_t            *p_disp    = disp_get_ptr();
   runloop_state_t *runloop_st         = &runloop_state;
   static bool old_focus               = true;
   struct retro_callbacks *cbs         = &runloop_st->retro_ctx;
   bool is_focused                     = false;
   bool is_alive                       = false;
   uint64_t frame_count                = 0;
   bool focused                        = true;
   bool rarch_is_initialized           = runloop_st->is_inited;
   bool runloop_paused                 = runloop_st->flags & RUNLOOP_FLAG_PAUSED;
   bool pause_nonactive                = settings->bools.pause_nonactive;
   unsigned quit_gamepad_combo         = settings->uints.input_quit_gamepad_combo;
#ifdef HAVE_MENU
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
   bool menu_driver_binding_state      = menu_st->flags &
MENU_ST_FLAG_IS_BINDING;
   bool menu_is_alive                  = menu_st->flags & MENU_ST_FLAG_ALIVE;
   bool display_kb                     = menu_input_dialog_get_display_kb();
#endif
#if defined(HAVE_GFX_WIDGETS)
   dispgfx_widget_t *p_dispwidget      = dispwidget_get_ptr();
   bool widgets_active                 = p_dispwidget->active;
#endif
#ifdef HAVE_CHEEVOS
   bool cheevos_hardcore_active        = false;
#endif

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (p_dispwidget->ai_service_overlay_state == 3)
   {
      command_event(CMD_EVENT_PAUSE, NULL);
      p_dispwidget->ai_service_overlay_state = 1;
   }
#endif

#ifdef HAVE_LIBNX
   /* Should be called once per frame */
   if (!appletMainLoop())
      return RUNLOOP_STATE_QUIT;
#endif

#ifdef _3DS
   /* Should be called once per frame */
   if (!aptMainLoop())
      return RUNLOOP_STATE_QUIT;
#endif

   BIT256_CLEAR_ALL_PTR(&current_bits);

   input_st->flags    &= ~(INP_FLAG_BLOCK_LIBRETRO_INPUT 
                         | INP_FLAG_BLOCK_HOTKEY);

   if (input_st->flags & INP_FLAG_KB_MAPPING_BLOCKED)
      input_st->flags |= INP_FLAG_BLOCK_HOTKEY;

   input_driver_collect_system_input(input_st, settings, &current_bits);

#ifdef HAVE_MENU
   last_input                       = current_bits;
   if (
         ((menu_toggle_gamepad_combo != INPUT_COMBO_NONE) &&
          input_driver_button_combo(
             menu_toggle_gamepad_combo,
             current_time,
             &last_input)))
      BIT256_SET(current_bits, RARCH_MENU_TOGGLE);

   if (menu_st->input_driver_flushing_input > 0)
   {
      bool input_active = bits_any_set(current_bits.data, ARRAY_SIZE(current_bits.data));

      if (!input_active)
         menu_st->input_driver_flushing_input = (menu_st->input_driver_flushing_input - 1);

      if (input_active || (menu_st->input_driver_flushing_input > 0))
      {
         BIT256_CLEAR_ALL(current_bits);
         if (runloop_paused)
            BIT256_SET(current_bits, RARCH_PAUSE_TOGGLE);
      }
   }
#endif


   if (!VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st))
   {
      const ui_application_t *application = uico_st->drv
         ? uico_st->drv->application
         : NULL;
      if (application)
         application->process_events();
   }

   frame_count = video_st->frame_count;
   is_alive    = video_st->current_video
      ? video_st->current_video->alive(video_st->data)
      : true;
   is_focused  = VIDEO_HAS_FOCUS(video_st);

#ifdef HAVE_MENU
   if (menu_driver_binding_state)
      BIT256_CLEAR_ALL(current_bits);
#endif

   /* Check fullscreen toggle */
   {
      bool fullscreen_toggled = !runloop_paused
#ifdef HAVE_MENU
         || menu_is_alive;
#else
      ;
#endif
      HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY,
                   CMD_EVENT_FULLSCREEN_TOGGLE,
                   fullscreen_toggled, NULL);
   }

   /* Check mouse grab toggle */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE,
                CMD_EVENT_GRAB_MOUSE_TOGGLE,
                true, NULL);

   /* Automatic mouse grab on focus */
   if (     settings->bools.input_auto_mouse_grab 
         && (is_focused)
         && (is_focused != (((runloop_st->flags & RUNLOOP_FLAG_FOCUSED)) > 0))
         && !(input_st->flags & INP_FLAG_GRAB_MOUSE_STATE))
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   if (is_focused)
      runloop_st->flags |=  RUNLOOP_FLAG_FOCUSED;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_FOCUSED;

#ifdef HAVE_OVERLAY
   if (settings->bools.input_overlay_enable)
   {
      static char prev_overlay_restore               = false;
      static unsigned last_width                     = 0;
      static unsigned last_height                    = 0;
      unsigned video_driver_width                    = video_st->width;
      unsigned video_driver_height                   = video_st->height;
      bool check_next_rotation                       = true;
      bool input_overlay_hide_in_menu                = settings->bools.input_overlay_hide_in_menu;
      bool input_overlay_hide_when_gamepad_connected = settings->bools.input_overlay_hide_when_gamepad_connected;
      bool input_overlay_auto_rotate                 = settings->bools.input_overlay_auto_rotate;

      /* Check whether overlay should be hidden
       * when a gamepad is connected */
      if (input_overlay_hide_when_gamepad_connected)
      {
         static bool last_controller_connected = false;
         bool controller_connected             = (input_config_get_device_name(0) != NULL);

         if (controller_connected != last_controller_connected)
         {
            if (controller_connected)
               input_overlay_deinit();
            else
               input_overlay_init();

            last_controller_connected = controller_connected;
         }
      }

      /* Check next overlay */
      HOTKEY_CHECK(RARCH_OVERLAY_NEXT, CMD_EVENT_OVERLAY_NEXT, true, &check_next_rotation);

      /* Ensure overlay is restored after displaying osk */
      if (input_st->flags & INP_FLAG_KB_LINEFEED_ENABLE)
         prev_overlay_restore = true;
      else if (prev_overlay_restore)
      {
         if (!input_overlay_hide_in_menu)
            input_overlay_init();
         prev_overlay_restore = false;
      }

      /* Check whether video aspect has changed */
      if ((video_driver_width  != last_width) ||
          (video_driver_height != last_height))
      {
         /* Update scaling/offset factors */
         command_event(CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, NULL);

         /* Check overlay rotation, if required */
         if (input_overlay_auto_rotate)
            input_overlay_auto_rotate_(
                  video_st->width,
                  video_st->height,
                  settings->bools.input_overlay_enable,
                  input_st->overlay_ptr);

         last_width  = video_driver_width;
         last_height = video_driver_height;
      }
   }
#endif

   /*
   * If the Aspect Ratio is FULL then update the aspect ratio to the 
   * current video driver aspect ratio (The full window)
   * 
   * TODO/FIXME 
   *      Should possibly be refactored to have last width & driver width & height
   *      only be done once when we are using an overlay OR using aspect ratio
   *      full
   */
   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_FULL)
   {
      static unsigned last_width                     = 0;
      static unsigned last_height                    = 0;
      unsigned video_driver_width                    = video_st->width;
      unsigned video_driver_height                   = video_st->height;

      /* Check whether video aspect has changed */
      if ((video_driver_width  != last_width) ||
          (video_driver_height != last_height))
      {
         /* Update set aspect ratio so the full matches the current video width & height */
         command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

         last_width  = video_driver_width;
         last_height = video_driver_height;
      }
   }

   /* Check quit key */
   {
      bool trig_quit_key, quit_press_twice;
      static bool quit_key     = false;
      static bool old_quit_key = false;
      static bool runloop_exec = false;
      quit_key                 = BIT256_GET(
            current_bits, RARCH_QUIT_KEY);
      trig_quit_key            = quit_key && !old_quit_key;
      /* Check for quit gamepad combo */
      if (!trig_quit_key &&
          ((quit_gamepad_combo != INPUT_COMBO_NONE) &&
          input_driver_button_combo(
             quit_gamepad_combo,
             current_time,
             &current_bits)))
        trig_quit_key = true;
      old_quit_key             = quit_key;
      quit_press_twice         = settings->bools.quit_press_twice;

      /* Check double press if enabled */
      if (trig_quit_key && quit_press_twice)
      {
         static retro_time_t quit_key_time   = 0;
         retro_time_t cur_time               = current_time;
         trig_quit_key                       = (cur_time - quit_key_time < QUIT_DELAY_USEC);
         quit_key_time                       = cur_time;

         if (!trig_quit_key)
         {
            float target_hz = 0.0;

            runloop_environment_cb(
                  RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &target_hz);

            runloop_msg_queue_push(msg_hash_to_str(MSG_PRESS_AGAIN_TO_QUIT), 1,
                  QUIT_DELAY_USEC * target_hz / 1000000,
                  true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      if (RUNLOOP_TIME_TO_EXIT(trig_quit_key))
      {
         bool quit_runloop           = false;
#ifdef HAVE_SCREENSHOTS
         unsigned runloop_max_frames = runloop_st->max_frames;

         if ((runloop_max_frames != 0)
               && (frame_count >= runloop_max_frames)
               && (runloop_st->flags & RUNLOOP_FLAG_MAX_FRAMES_SCREENSHOT))
         {
            const char *screenshot_path = NULL;
            bool fullpath               = false;

            if (string_is_empty(runloop_st->max_frames_screenshot_path))
               screenshot_path          = path_get(RARCH_PATH_BASENAME);
            else
            {
               fullpath                 = true;
               screenshot_path          = runloop_st->max_frames_screenshot_path;
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
#endif

         if (runloop_exec)
            runloop_exec = false;

         if (runloop_st->flags & RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED)
         {
            bool load_dummy_core = false;

            runloop_st->flags   &= ~RUNLOOP_FLAG_CORE_SHUTDOWN_INITIATED;

            /* Check whether dummy core should be loaded
             * instead of exiting RetroArch completely
             * (aborts shutdown if invoked) */
            if (settings->bools.load_dummy_on_core_shutdown)
            {
               load_dummy_core    = true;
               runloop_st->flags &= ~RUNLOOP_FLAG_SHUTDOWN_INITIATED;
            }

            /* Unload current core, and load dummy if
             * required */
            if (!command_event(CMD_EVENT_UNLOAD_CORE, &load_dummy_core))
            {
               runloop_st->flags |= RUNLOOP_FLAG_SHUTDOWN_INITIATED;
               quit_runloop       = true;
            }

            if (!load_dummy_core)
               quit_runloop = true;
         }
         else
            quit_runloop                 = true;

         runloop_st->flags              &= ~RUNLOOP_FLAG_CORE_RUNNING;

         if (quit_runloop)
         {
            old_quit_key                 = quit_key;
            return RUNLOOP_STATE_QUIT;
         }
      }
   }

#if defined(HAVE_MENU) || defined(HAVE_GFX_WIDGETS)
   gfx_animation_update(
         current_time,
         settings->bools.menu_timedate_enable,
         settings->floats.menu_ticker_speed,
         video_st->width,
         video_st->height);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_active)
   {
      bool rarch_force_fullscreen = video_st->flags &
         VIDEO_FLAG_FORCE_FULLSCREEN;
      bool video_is_fullscreen    = settings->bools.video_fullscreen ||
            rarch_force_fullscreen;

      RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
      gfx_widgets_iterate(
            p_disp,
            settings,
            video_st->width,
            video_st->height,
            video_is_fullscreen,
            settings->paths.directory_assets,
            settings->paths.path_font,
            VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st));
      RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
   }
#endif

#ifdef HAVE_MENU
   if (menu_is_alive)
   {
      enum menu_action action;
      static input_bits_t old_input = {{0}};
      static enum menu_action
         old_action                 = MENU_ACTION_CANCEL;
      struct menu_state *menu_st    = menu_state_get_ptr();
      bool focused                  = false;
      input_bits_t trigger_input    = current_bits;
      unsigned screensaver_timeout  = settings->uints.menu_screensaver_timeout;

      /* Get current time */
      menu_st->current_time_us      = current_time;

      cbs->poll_cb();

      bits_clear_bits(trigger_input.data, old_input.data,
            ARRAY_SIZE(trigger_input.data));
      action                    = (enum menu_action)menu_event(
            settings,
            &current_bits, &trigger_input, display_kb);
#ifdef HAVE_NETWORKING
      if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
         focused = true;
      else
#endif
      {
         if (pause_nonactive)
            focused = is_focused && !uico_st->is_on_foreground;
         else
            focused = !uico_st->is_on_foreground;
      }

      if (action == old_action)
      {
	      retro_time_t press_time          = current_time;

	      if (action == MENU_ACTION_NOOP)
		      menu_st->noop_press_time      = press_time - menu_st->noop_start_time;
	      else
		      menu_st->action_press_time    = press_time - menu_st->action_start_time;
      }
      else
      {
	      if (action == MENU_ACTION_NOOP)
	      {
		      menu_st->noop_start_time      = current_time;
		      menu_st->noop_press_time      = 0;

		      if (menu_st->prev_action == old_action)
			      menu_st->action_start_time = menu_st->prev_start_time;
		      else
			      menu_st->action_start_time = current_time;
	      }
	      else
	      {
		      if (  menu_st->prev_action == action &&
				      menu_st->noop_press_time < 200000) /* 250ms */
		      {
			      menu_st->action_start_time = menu_st->prev_start_time;
			      menu_st->action_press_time = current_time - menu_st->action_start_time;
		      }
		      else
		      {
			      menu_st->prev_start_time   = current_time;
			      menu_st->prev_action       = action;
			      menu_st->action_press_time = 0;
		      }
	      }
      }

      /* Check whether menu screensaver should be enabled */
      if (   (screensaver_timeout > 0)
          && (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_SUPPORTED)
          && (!(menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE))
          && ((menu_st->current_time_us - menu_st->input_last_time_us) >
               ((retro_time_t)screensaver_timeout * 1000000)))
      {
         menu_ctx_environment_t menu_environ;
         menu_environ.type           = MENU_ENVIRON_ENABLE_SCREENSAVER;
         menu_environ.data           = NULL;
         menu_st->flags             |= MENU_ST_FLAG_SCREENSAVER_ACTIVE;
         menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
      }

      /* Iterate the menu driver for one frame. */

      /* If the user had requested that the Quick Menu
       * be spawned during the previous frame, do this now
       * and exit the function to go to the next frame. */
      if (menu_st->flags & MENU_ST_FLAG_PENDING_QUICK_MENU)
      {
         menu_ctx_list_t list_info;

         /* We are going to push a new menu; ensure
          * that the current one is cached for animation
          * purposes */
         list_info.type   = MENU_LIST_PLAIN;
         list_info.action = 0;
         menu_driver_list_cache(&list_info);

         p_disp->flags   |= GFX_DISP_FLAG_MSG_FORCE;

         generic_action_ok_displaylist_push("", NULL,
               "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);

         menu_st->selection_ptr      = 0;
         menu_st->flags             &= ~MENU_ST_FLAG_PENDING_QUICK_MENU;
      }
      else if (!menu_driver_iterate(
               menu_st,
               p_disp,
               anim_get_ptr(),
               settings,
               action, current_time))
      {
         if (error_on_init)
         {
            content_ctx_info_t content_info = {0};
            task_push_start_dummy_core(&content_info);
         }
         else
            retroarch_menu_running_finished(false);
      }

      if (focused || !(runloop_st->flags & RUNLOOP_FLAG_IDLE))
      {
         bool runloop_is_inited      = runloop_st->is_inited;
#ifdef HAVE_NETWORKING
         bool menu_pause_libretro    = settings->bools.menu_pause_libretro &&
            netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
         bool menu_pause_libretro    = settings->bools.menu_pause_libretro;
#endif
         bool libretro_running       = !menu_pause_libretro
            && runloop_is_inited
            && (runloop_st->current_core_type != CORE_TYPE_DUMMY);

         if (menu)
         {
            if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER)
                  != BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX))
               BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);

            if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER))
               p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;

            if (BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX)
                  && !string_is_empty(menu->menu_state_msg))
            {
               if (menu->driver_ctx->render_messagebox)
                  menu->driver_ctx->render_messagebox(
                        menu->userdata,
                        menu->menu_state_msg);

               if (uico_st->is_on_foreground)
               {
                  if (     uico_st->drv
                        && uico_st->drv->render_messagebox)
                     uico_st->drv->render_messagebox(menu->menu_state_msg);
               }
            }

            if (BIT64_GET(menu->state, MENU_STATE_BLIT))
            {
               if (menu->driver_ctx->render)
                  menu->driver_ctx->render(
                        menu->userdata,
                        video_st->width,
                        video_st->height,
                        runloop_st->flags & RUNLOOP_FLAG_IDLE);
            }

            if (      (menu_st->flags & MENU_ST_FLAG_ALIVE) 
                  && !(runloop_st->flags & RUNLOOP_FLAG_IDLE))
               if (display_menu_libretro(runloop_st, input_st,
                        settings->floats.slowmotion_ratio,
                        libretro_running, current_time))
                  video_driver_cached_frame();

            if (menu->driver_ctx->set_texture)
               menu->driver_ctx->set_texture(menu->userdata);

            menu->state               = 0;
         }

         if (settings->bools.audio_enable_menu &&
               !libretro_running)
            audio_driver_menu_sample();
      }

      old_input                 = current_bits;
      old_action                = action;

      if (!focused || (runloop_st->flags & RUNLOOP_FLAG_IDLE))
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
#endif
   {
      if (runloop_st->flags & RUNLOOP_FLAG_IDLE)
      {
         cbs->poll_cb();
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
      }
   }

   /* Check game focus toggle */
   {
      enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_TOGGLE;
      HOTKEY_CHECK(RARCH_GAME_FOCUS_TOGGLE, CMD_EVENT_GAME_FOCUS_TOGGLE, true, &game_focus_cmd);
   }
   /* Check if we have pressed the UI companion toggle button */
   HOTKEY_CHECK(RARCH_UI_COMPANION_TOGGLE, CMD_EVENT_UI_COMPANION_TOGGLE, true, NULL);
   /* Check close content key */
   HOTKEY_CHECK(RARCH_CLOSE_CONTENT_KEY, CMD_EVENT_CLOSE_CONTENT, true, NULL);

#ifdef HAVE_MENU
   /* Check if we have pressed the menu toggle button */
   {
      static bool old_pressed = false;
      char *menu_driver       = settings->arrays.menu_driver;
      bool pressed            = BIT256_GET(
            current_bits, RARCH_MENU_TOGGLE) &&
         !string_is_equal(menu_driver, "null");
      bool core_type_is_dummy = runloop_st->current_core_type == CORE_TYPE_DUMMY;

      if (menu_st->kb_key_state[RETROK_F1] == 1)
      {
         if (menu_st->flags & MENU_ST_FLAG_ALIVE)
         {
            if (rarch_is_initialized && !core_type_is_dummy)
            {
               retroarch_menu_running_finished(false);
               menu_st->kb_key_state[RETROK_F1] =
                  ((menu_st->kb_key_state[RETROK_F1] & 1) << 1) | false;
            }
         }
      }
      else if ((!menu_st->kb_key_state[RETROK_F1] &&
               (pressed && !old_pressed)) ||
            core_type_is_dummy)
      {
         if (menu_st->flags & MENU_ST_FLAG_ALIVE)
         {
            if (rarch_is_initialized && !core_type_is_dummy)
               retroarch_menu_running_finished(false);
         }
         else
            retroarch_menu_running();
      }
      else
         menu_st->kb_key_state[RETROK_F1] =
            ((menu_st->kb_key_state[RETROK_F1] & 1) << 1) | false;

      old_pressed             = pressed;
   }
#endif

   /* Check if we have pressed the FPS toggle button */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   HOTKEY_CHECK(RARCH_STATISTICS_TOGGLE, CMD_EVENT_STATISTICS_TOGGLE, true, NULL);

   /* Check if we have pressed the netplay host toggle button */
   HOTKEY_CHECK(RARCH_NETPLAY_HOST_TOGGLE, CMD_EVENT_NETPLAY_HOST_TOGGLE, true, NULL);

   /* Volume stepping + acceleration */
   {
      static unsigned volume_hotkey_delay        = 0;
      static unsigned volume_hotkey_delay_active = 0;
      unsigned volume_hotkey_delay_default       = 6;
      bool volume_hotkey_up                      = BIT256_GET(
            current_bits, RARCH_VOLUME_UP);
      bool volume_hotkey_down                    = BIT256_GET(
            current_bits, RARCH_VOLUME_DOWN);

      if (  (volume_hotkey_up   && !volume_hotkey_down) ||
            (volume_hotkey_down && !volume_hotkey_up))
      {
         if (volume_hotkey_delay > 0)
            volume_hotkey_delay--;
         else
         {
            if (volume_hotkey_up)
               command_event(CMD_EVENT_VOLUME_UP, NULL);
            else if (volume_hotkey_down)
               command_event(CMD_EVENT_VOLUME_DOWN, NULL);

            if (volume_hotkey_delay_active > 0)
               volume_hotkey_delay_active--;
            volume_hotkey_delay = volume_hotkey_delay_active;
         }
      }
      else
      {
         volume_hotkey_delay        = 0;
         volume_hotkey_delay_active = volume_hotkey_delay_default;
      }
   }

   /* Check if we have pressed the audio mute toggle button */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL);

#ifdef HAVE_MENU
   if (menu_st->flags & MENU_ST_FLAG_ALIVE)
   {
      float fastforward_ratio = runloop_get_fastforward_ratio(settings,
            &runloop_st->fastmotion_override.current);

      if (!settings->bools.menu_throttle_framerate && !fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
#endif
   if (pause_nonactive)
      focused                = is_focused;

#ifdef HAVE_SCREENSHOTS
   /* Check if we have pressed the screenshot toggle button */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);
#endif

   /* Check if we have pressed the OSK toggle button */
   HOTKEY_CHECK(RARCH_OSK, CMD_EVENT_OSK_TOGGLE, true, NULL);

   /* Check if we have pressed the recording toggle button */
   HOTKEY_CHECK(RARCH_RECORDING_TOGGLE, CMD_EVENT_RECORDING_TOGGLE, true, NULL);

   /* Check if we have pressed the streaming toggle button */
   HOTKEY_CHECK(RARCH_STREAMING_TOGGLE, CMD_EVENT_STREAMING_TOGGLE, true, NULL);

   /* Check if we have pressed the Run-Ahead toggle button */
   HOTKEY_CHECK(RARCH_RUNAHEAD_TOGGLE, CMD_EVENT_RUNAHEAD_TOGGLE, true, NULL);

   /* Check if we have pressed the AI Service toggle button */
   HOTKEY_CHECK(RARCH_AI_SERVICE, CMD_EVENT_AI_SERVICE_TOGGLE, true, NULL);


#ifdef HAVE_NETWORKING
   /* Check Netplay */
   HOTKEY_CHECK(RARCH_NETPLAY_PING_TOGGLE, CMD_EVENT_NETPLAY_PING_TOGGLE, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_PLAYER_CHAT, CMD_EVENT_NETPLAY_PLAYER_CHAT, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_FADE_CHAT_TOGGLE, CMD_EVENT_NETPLAY_FADE_CHAT_TOGGLE, true, NULL);
#endif

   /* Check if we have pressed the pause button */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool frameadvance_pressed     = false;
      bool trig_frameadvance        = false;
      bool pause_pressed            = BIT256_GET(current_bits, RARCH_PAUSE_TOGGLE);
#ifdef HAVE_CHEEVOS
      /* make sure not to evaluate this before calling menu_driver_iterate
       * as that may change its value */
      cheevos_hardcore_active = rcheevos_hardcore_active();

      if (cheevos_hardcore_active)
      {
         static int unpaused_frames = 0;

         if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
            unpaused_frames         = 0;
         else
         /* Frame advance is not allowed when achievement hardcore is active */
         {
            /* Limit pause to approximately three times per second (depending on core framerate) */
            if (unpaused_frames < 20)
            {
               ++unpaused_frames;
               pause_pressed        = false;
            }
         }
      }
      else
#endif
      {
         frameadvance_pressed = BIT256_GET(current_bits, RARCH_FRAMEADVANCE);
         trig_frameadvance    = frameadvance_pressed && !old_frameadvance;

         /* FRAMEADVANCE will set us into pause mode. */
         pause_pressed       |= (!(runloop_st->flags & RUNLOOP_FLAG_PAUSED))
            && trig_frameadvance;
      }

      /* Check if libretro pause key was pressed. If so, pause or
       * unpause the libretro core. */

      if (focused)
      {
         if (pause_pressed && !old_pause_pressed)
            command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
         else if (!old_focus)
            command_event(CMD_EVENT_UNPAUSE, NULL);
      }
      else if (old_focus)
         command_event(CMD_EVENT_PAUSE, NULL);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
      {
         bool toggle = (!(runloop_st->flags & RUNLOOP_FLAG_IDLE)) ? true : false;

         HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY,
               CMD_EVENT_FULLSCREEN_TOGGLE, true, &toggle);

         /* Check if it's not oneshot */
#ifdef HAVE_REWIND
         if (!(trig_frameadvance || BIT256_GET(current_bits, RARCH_REWIND)))
#else
         if (!trig_frameadvance)
#endif
            focused = false;
      }
   }

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
   /* Copy over the retropad state to a buffer for the translate service
      to send off if it's run. */
   if (settings->bools.ai_service_enable)
   {
      input_st->ai_gamepad_state[0]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_B);
      input_st->ai_gamepad_state[1]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_Y);
      input_st->ai_gamepad_state[2]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_SELECT);
      input_st->ai_gamepad_state[3]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_START);

      input_st->ai_gamepad_state[4]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_UP);
      input_st->ai_gamepad_state[5]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_DOWN);
      input_st->ai_gamepad_state[6]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_LEFT);
      input_st->ai_gamepad_state[7]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_RIGHT);

      input_st->ai_gamepad_state[8]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_A);
      input_st->ai_gamepad_state[9]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_X);
      input_st->ai_gamepad_state[10] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L);
      input_st->ai_gamepad_state[11] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R);

      input_st->ai_gamepad_state[12] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L2);
      input_st->ai_gamepad_state[13] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R2);
      input_st->ai_gamepad_state[14] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L3);
      input_st->ai_gamepad_state[15] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R3);
   }
#endif
#endif

   if (!focused)
   {
      cbs->poll_cb();
      return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }

   /* Apply any pending fastmotion override
    * parameters */
   if (runloop_st->fastmotion_override.pending)
   {
      runloop_apply_fastmotion_override(runloop_st, settings);
      runloop_st->fastmotion_override.pending = false;
   }

   /* Check if we have pressed the fast forward button */
   /* To avoid continuous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between them.
    */
   if (!runloop_st->fastmotion_override.current.inhibit_toggle)
   {
      static bool old_button_state            = false;
      static bool old_hold_button_state       = false;
      bool new_button_state                   = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_KEY);
      bool new_hold_button_state              = BIT256_GET(
            current_bits, RARCH_FAST_FORWARD_HOLD_KEY);
      bool check2                             = new_button_state 
         && !old_button_state;

      if (!check2)
         check2 = old_hold_button_state != new_hold_button_state;

      if (check2)
      {
         if (input_st->flags & INP_FLAG_NONBLOCKING)
         {
            input_st->flags                     &= ~INP_FLAG_NONBLOCKING;
            runloop_st->flags                   &= ~RUNLOOP_FLAG_FASTMOTION;
            runloop_st->fastforward_after_frames = 1;
         }
         else
         {
            input_st->flags                     |=  INP_FLAG_NONBLOCKING;
            runloop_st->flags                   |=  RUNLOOP_FLAG_FASTMOTION;
         }

         driver_set_nonblock_state();

         /* Reset frame time counter when toggling
          * fast-forward off, if required */
         if ( !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
             && settings->bools.frame_time_counter_reset_after_fastforwarding)
            video_st->frame_time_count  = 0;
      }

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;
   }

   /* Display fast-forward notification, unless
    * disabled via override */
   if (!runloop_st->fastmotion_override.current.fastforward ||
       runloop_st->fastmotion_override.current.notification)
   {
      /* > Use widgets, if enabled */
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
      {
         if (settings->bools.notification_show_fast_forward)
         {
            if (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
               video_st->flags |=  VIDEO_FLAG_WIDGETS_FAST_FORWARD;
            else
               video_st->flags &= ~VIDEO_FLAG_WIDGETS_FAST_FORWARD;
         }
         else
            video_st->flags    &= ~VIDEO_FLAG_WIDGETS_FAST_FORWARD;
      }
      else
#endif
      {
         /* > If widgets are disabled, display fast-forward
          *   status via OSD text for 1 frame every frame */
         if (   (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
             && settings->bools.notification_show_fast_forward)
            runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }
#if defined(HAVE_GFX_WIDGETS)
   else
      video_st->flags &= ~VIDEO_FLAG_WIDGETS_FAST_FORWARD;
#endif

   /* Check if we have pressed any of the state slot buttons */
   {
      static bool old_should_slot_increase = false;
      static bool old_should_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_bits, RARCH_STATE_SLOT_MINUS);
      bool check1                          = true;
      bool check2                          = should_slot_increase && !old_should_slot_increase;
      int addition                         = 1;
      int state_slot                       = settings->ints.state_slot;

      if (!check2)
      {
         check2                            = should_slot_decrease && !old_should_slot_decrease;
         check1                            = state_slot > 0;
         addition                          = -1;
      }

      /* Checks if the state increase/decrease keys have been pressed
       * for this frame. */
      if (check2)
      {
         size_t _len;
         char msg[128];
         int cur_state_slot                = state_slot;
         if (check1)
            configuration_set_int(settings, settings->ints.state_slot,
                  cur_state_slot + addition);
         _len = strlcpy(msg, msg_hash_to_str(MSG_STATE_SLOT), sizeof(msg));
         snprintf(msg         + _len,
                  sizeof(msg) - _len,
                  ": %d",
                  settings->ints.state_slot);
         runloop_msg_queue_push(msg, 2, 180, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("[State]: %s\n", msg);
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }

   /* Check if we have pressed any of the savestate buttons */
   HOTKEY_CHECK(RARCH_SAVE_STATE_KEY, CMD_EVENT_SAVE_STATE, true, NULL);
   HOTKEY_CHECK(RARCH_LOAD_STATE_KEY, CMD_EVENT_LOAD_STATE, true, NULL);

#ifdef HAVE_CHEEVOS
   if (!cheevos_hardcore_active)
#endif
   {
      /* Check if rewind toggle was being held. */
      {
#ifdef HAVE_REWIND
         char s[128];
         bool rewinding = false;
         unsigned t     = 0;

         s[0]           = '\0';

         rewinding      = state_manager_check_rewind(
               &runloop_st->rewind_st,
               &runloop_st->current_core,
               BIT256_GET(current_bits, RARCH_REWIND),
               settings->uints.rewind_granularity,
               runloop_st->flags & RUNLOOP_FLAG_PAUSED,
               s, sizeof(s), &t);

#if defined(HAVE_GFX_WIDGETS)
         if (widgets_active)
         {
            if (rewinding)
               video_st->flags |=  VIDEO_FLAG_WIDGETS_REWINDING;
            else
               video_st->flags &= ~VIDEO_FLAG_WIDGETS_REWINDING;
         }
         else
#endif
         {
            if (rewinding)
               runloop_msg_queue_push(s, 0, t, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
#endif
      }

      {
         /* Checks if slowmotion toggle/hold was being pressed and/or held. */
         static bool old_slowmotion_button_state      = false;
         static bool old_slowmotion_hold_button_state = false;
         bool new_slowmotion_button_state             = BIT256_GET(
               current_bits, RARCH_SLOWMOTION_KEY);
         bool new_slowmotion_hold_button_state        = BIT256_GET(
               current_bits, RARCH_SLOWMOTION_HOLD_KEY);

         if (new_slowmotion_button_state && !old_slowmotion_button_state)
         {
            if (!(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION))
               runloop_st->flags |=  RUNLOOP_FLAG_SLOWMOTION;
            else
               runloop_st->flags &= ~RUNLOOP_FLAG_SLOWMOTION;
         }
         else if (old_slowmotion_hold_button_state != new_slowmotion_hold_button_state)
         {
            if (new_slowmotion_hold_button_state)
               runloop_st->flags |=  RUNLOOP_FLAG_SLOWMOTION;
            else
               runloop_st->flags &= ~RUNLOOP_FLAG_SLOWMOTION;
         }

         if (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
         {
            if (settings->uints.video_black_frame_insertion)
               if (!(runloop_st->flags & RUNLOOP_FLAG_IDLE))
                  video_driver_cached_frame();

#if defined(HAVE_GFX_WIDGETS)
            if (!widgets_active)
#endif
            {
#ifdef HAVE_REWIND
               struct state_manager_rewind_state
                  *rewind_st = &runloop_st->rewind_st;
               if (rewind_st->frame_is_reversed)
                  runloop_msg_queue_push(
                        msg_hash_to_str(MSG_SLOW_MOTION_REWIND), 1, 1, false, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               else
#endif
                  runloop_msg_queue_push(
                        msg_hash_to_str(MSG_SLOW_MOTION), 1, 1, false,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }

         old_slowmotion_button_state                  = new_slowmotion_button_state;
         old_slowmotion_hold_button_state             = new_slowmotion_hold_button_state;
      }
   }

   HOTKEY_CHECK(RARCH_VRR_RUNLOOP_TOGGLE, CMD_EVENT_VRR_RUNLOOP_TOGGLE, true, NULL);

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

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   if (settings->bools.video_shader_watch_files)
   {
      static rarch_timer_t timer = {0};
      static bool need_to_apply  = false;

      if (video_shader_check_for_changes())
      {
         need_to_apply = true;

         if (!timer.timer_begin)
         {
            timer.timeout_us  = SHADER_FILE_WATCH_DELAY_MSEC * 1000;
            timer.current     = cpu_features_get_time_usec();
            timer.timeout_end = timer.current + timer.timeout_us;
            timer.timer_begin = true;
            timer.timer_end   = false;
         }
      }

      /* If a file is modified atomically (moved/renamed from a different file),
       * we have no idea how long that might take.
       * If we're trying to re-apply shaders immediately after changes are made
       * to the original file(s), the filesystem might be in an in-between
       * state where the new file hasn't been moved over yet and the original
       * file was already deleted. This leaves us no choice but to wait an
       * arbitrary amount of time and hope for the best.
       */
      if (need_to_apply)
      {
         timer.current        = current_time; 
         timer.timeout_us     = timer.timeout_end - timer.current;

         if (     !timer.timer_end 
               &&  timer.timeout_us <= 0)
         {
            timer.timer_end   = true;
            timer.timer_begin = false;
            timer.timeout_end = 0;
            need_to_apply     = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   if (      settings->uints.video_shader_delay
         && !runloop_st->shader_delay_timer.timer_end)
   {
      if (!runloop_st->shader_delay_timer.timer_begin)
      {
         runloop_st->shader_delay_timer.timeout_us     = settings->uints.video_shader_delay * 1000;
         runloop_st->shader_delay_timer.current        = cpu_features_get_time_usec();
         runloop_st->shader_delay_timer.timeout_end    = runloop_st->shader_delay_timer.current 
                                                       + runloop_st->shader_delay_timer.timeout_us;
         runloop_st->shader_delay_timer.timer_begin    = true;
         runloop_st->shader_delay_timer.timer_end      = false;
      }
      else
      {
         runloop_st->shader_delay_timer.current        = current_time;
         runloop_st->shader_delay_timer.timeout_us     = runloop_st->shader_delay_timer.timeout_end 
                                                       - runloop_st->shader_delay_timer.current;

         if (runloop_st->shader_delay_timer.timeout_us <= 0)
         {
            runloop_st->shader_delay_timer.timer_end   = true;
            runloop_st->shader_delay_timer.timer_begin = false;
            runloop_st->shader_delay_timer.timeout_end = 0;

            {
               const char *preset          = retroarch_get_shader_preset();
               enum rarch_shader_type type = video_shader_parse_type(preset);
               apply_shader(settings, type, preset, false);
            }
         }
      }
   }
#endif

   return RUNLOOP_STATE_ITERATE;
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
int runloop_iterate(void)
{
   int i;
   enum analog_dpad_mode dpad_mode[MAX_USERS];
   input_driver_state_t               *input_st = input_state_get_ptr();
   audio_driver_state_t               *audio_st = audio_state_get_ptr();
   video_driver_state_t               *video_st = video_state_get_ptr();
   recording_state_t              *recording_st = recording_state_get_ptr();
   camera_driver_state_t             *camera_st = camera_state_get_ptr();
#if defined(HAVE_COCOATOUCH)
   uico_driver_state_t  *uico_st                = uico_state_get_ptr();
#endif
   settings_t *settings                         = config_get_ptr();
   runloop_state_t *runloop_st                  = &runloop_state;
   unsigned video_frame_delay                   = settings->uints.video_frame_delay;
   unsigned video_frame_delay_effective         = video_st->frame_delay_effective;
   bool vrr_runloop_enable                      = settings->bools.vrr_runloop_enable;
   unsigned max_users                           = settings->uints.input_max_users;
   retro_time_t current_time                    = cpu_features_get_time_usec();
#ifdef HAVE_MENU
#ifdef HAVE_NETWORKING
   bool menu_pause_libretro                     = settings->bools.menu_pause_libretro &&
      netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
   bool menu_pause_libretro                     = settings->bools.menu_pause_libretro;
#endif
   bool core_paused                             = (runloop_st->flags &
RUNLOOP_FLAG_PAUSED) || (menu_pause_libretro && (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE));
#else
   bool core_paused                             = (runloop_st->flags & RUNLOOP_FLAG_PAUSED);
#endif
   float slowmotion_ratio                       = settings->floats.slowmotion_ratio;
#ifdef HAVE_CHEEVOS
   bool cheevos_enable                          = settings->bools.cheevos_enable;
#endif
   bool audio_sync                              = settings->bools.audio_sync;
#ifdef HAVE_DISCORD
   discord_state_t *discord_st                  = discord_state_get_ptr();

   if (discord_st->inited)
   {
      Discord_RunCallbacks();
#ifdef DISCORD_DISABLE_IO_THREAD
      Discord_UpdateConnection();
#endif
   }
#endif

   if (runloop_st->frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */
      retro_usec_t runloop_last_frame_time = runloop_st->frame_time_last;
      retro_time_t current                 = current_time;
      bool is_locked_fps                   = (
               (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
            || (input_st->flags & INP_FLAG_NONBLOCKING))
            | !!recording_st->data;
      retro_time_t delta                   = (!runloop_last_frame_time || is_locked_fps)
         ? runloop_st->frame_time.reference
         : (current - runloop_last_frame_time);

      if (is_locked_fps)
         runloop_st->frame_time_last  = 0;
      else
      {
         runloop_st->frame_time_last  = current;

         if (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
            delta /= slowmotion_ratio;
      }

      if (!core_paused)
         runloop_st->frame_time.callback(delta);
   }

   /* Update audio buffer occupancy if buffer status
    * callback is in use by the core */
   if (runloop_st->audio_buffer_status.callback)
   {
      bool audio_buf_active        = false;
      unsigned audio_buf_occupancy = 0;
      bool audio_buf_underrun      = false;

      if (!(    (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
            || !(audio_st->flags & AUDIO_FLAG_ACTIVE)
            || !(audio_st->output_samples_buf))
          && audio_st->current_audio->write_avail
          && audio_st->context_audio_data
          && audio_st->buffer_size)
      {
         size_t audio_buf_avail;

         if ((audio_buf_avail = audio_st->current_audio->write_avail(
               audio_st->context_audio_data)) > audio_st->buffer_size)
            audio_buf_avail = audio_st->buffer_size;

         audio_buf_occupancy = (unsigned)(100 - (audio_buf_avail * 100) /
               audio_st->buffer_size);

         /* Elsewhere, we standardise on a 'low water mark'
          * of 25% of the total audio buffer size - use
          * the same metric here (can be made more sophisticated
          * if required - i.e. determine buffer occupancy in
          * terms of usec, and weigh this against the expected
          * frame time) */
         audio_buf_underrun  = audio_buf_occupancy < 25;

         audio_buf_active    = true;
      }

      if (!core_paused)
         runloop_st->audio_buffer_status.callback(
               audio_buf_active, audio_buf_occupancy, audio_buf_underrun);
   }

   switch ((enum runloop_state_enum)runloop_check_state(
            global_get_ptr()->error_on_init,
            settings, current_time))
   {
      case RUNLOOP_STATE_QUIT:
         runloop_st->frame_limit_last_time = 0.0;
         runloop_st->flags                &= ~RUNLOOP_FLAG_CORE_RUNNING;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
#if defined(HAVE_COCOATOUCH)
         if (!uico_st->is_on_foreground)
#endif
            retro_sleep(10);
         return 1;
      case RUNLOOP_STATE_END:
#ifdef HAVE_NETWORKING
#ifdef HAVE_MENU
         /* FIXME: This is an ugly way to tell Netplay this... */
         if (menu_pause_libretro &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL)
            )
            netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
#endif
         goto end;
      case RUNLOOP_STATE_MENU_ITERATE:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
         return 0;
      case RUNLOOP_STATE_ITERATE:
	 runloop_st->flags       |= RUNLOOP_FLAG_CORE_RUNNING;
         break;
   }

#ifdef HAVE_THREADS
   if (runloop_st->flags & RUNLOOP_FLAG_AUTOSAVE)
      autosave_lock();
#endif

#ifdef HAVE_BSV_MOVIE
   /* Used for rewinding while playback/record. */
   if (input_st->bsv_movie_state_handle)
      input_st->bsv_movie_state_handle->frame_pos[input_st->bsv_movie_state_handle->frame_ptr]
         = intfstream_tell(input_st->bsv_movie_state_handle->file);
#endif

   if (     camera_st->cb.caps
         && camera_st->driver
         && camera_st->driver->poll
         && camera_st->data)
      camera_st->driver->poll(camera_st->data,
            camera_st->cb.frame_raw_framebuffer,
            camera_st->cb.frame_opengl_texture);

   /* Update binds for analog dpad modes. */
   for (i = 0; i < (int)max_users; i++)
   {
      dpad_mode[i] = (enum analog_dpad_mode)
            settings->uints.input_analog_dpad_mode[i];

      switch (dpad_mode[i])
      {
         case ANALOG_DPAD_LSTICK:
         case ANALOG_DPAD_RSTICK:
            {
               unsigned mapped_port = settings->uints.input_remap_ports[i];
               if (input_st->analog_requested[mapped_port])
                  dpad_mode[i] = ANALOG_DPAD_NONE;
            }
            break;
         case ANALOG_DPAD_LSTICK_FORCED:
            dpad_mode[i] = ANALOG_DPAD_LSTICK;
            break;
         case ANALOG_DPAD_RSTICK_FORCED:
            dpad_mode[i] = ANALOG_DPAD_RSTICK;
            break;
         default:
            break;
      }

      /* Push analog to D-Pad mappings to binds. */
      if (dpad_mode[i] != ANALOG_DPAD_NONE)
      {
         unsigned k;
         unsigned joy_idx                    = settings->uints.input_joypad_index[i];
         struct retro_keybind *general_binds = input_config_binds[joy_idx];
         struct retro_keybind *auto_binds    = input_autoconf_binds[joy_idx];
         unsigned x_plus                     = RARCH_ANALOG_RIGHT_X_PLUS;
         unsigned y_plus                     = RARCH_ANALOG_RIGHT_Y_PLUS;
         unsigned x_minus                    = RARCH_ANALOG_RIGHT_X_MINUS;
         unsigned y_minus                    = RARCH_ANALOG_RIGHT_Y_MINUS;

         if (dpad_mode[i] == ANALOG_DPAD_LSTICK)
         {
            x_plus            =  RARCH_ANALOG_LEFT_X_PLUS;
            y_plus            =  RARCH_ANALOG_LEFT_Y_PLUS;
            x_minus           =  RARCH_ANALOG_LEFT_X_MINUS;
            y_minus           =  RARCH_ANALOG_LEFT_Y_MINUS;
         }

         for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
         {
            (auto_binds)[k].orig_joyaxis    = (auto_binds)[k].joyaxis;
            (general_binds)[k].orig_joyaxis = (general_binds)[k].joyaxis;
         }

         if (!INHERIT_JOYAXIS(auto_binds))
         {
            unsigned j = x_plus + 3;
            /* Inherit joyaxis from analogs. */
            for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
               (auto_binds)[k].joyaxis = (auto_binds)[j--].joyaxis;
         }

         if (!INHERIT_JOYAXIS(general_binds))
         {
            unsigned j = x_plus + 3;
            /* Inherit joyaxis from analogs. */
            for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++)
               (general_binds)[k].joyaxis = (general_binds)[j--].joyaxis;
         }
      }
   }

   if (!(input_st->flags & INP_FLAG_NONBLOCKING))
   {
      if (settings->bools.video_frame_delay_auto)
      {
         static bool slowmotion_prev  = false;
         static unsigned skip_frames  = 0;
         float refresh_rate           = settings->floats.video_refresh_rate;
         unsigned video_swap_interval = runloop_get_video_swap_interval(
               settings->uints.video_swap_interval);
         unsigned video_bfi           = settings->uints.video_black_frame_insertion;
         unsigned frame_time_interval = 8;
         bool frame_time_update       =
               /* Skip some starting frames for stabilization */
               video_st->frame_count > frame_time_interval &&
               video_st->frame_count % frame_time_interval == 0;

         /* A few frames need to get ignored after slowmotion is disabled */
         if (!(runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION) && slowmotion_prev)
            skip_frames = frame_time_interval * 2;

         if (skip_frames)
            skip_frames--;

         slowmotion_prev = runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION;
         /* Always skip when slowmotion is active */
         if (slowmotion_prev)
            skip_frames = 1;

         if (skip_frames)
            frame_time_update = false;

         /* Black frame insertion + swap interval multiplier */
         refresh_rate = (refresh_rate / (video_bfi + 1.0f) / video_swap_interval);

         /* Set target moderately as half frame time with 0 delay */
         if (video_frame_delay == 0)
            video_frame_delay = 1 / refresh_rate * 1000 / 2;

         if (video_st->frame_delay_target != video_frame_delay)
         {
            video_st->frame_delay_target = video_frame_delay_effective = video_frame_delay;
            RARCH_LOG("[Video]: Frame delay reset to %d ms.\n", video_frame_delay);
         }

         if (video_frame_delay_effective > 0 && frame_time_update)
         {
            video_frame_delay_auto_t vfda = {0};
            vfda.frame_time_interval      = frame_time_interval;
            vfda.refresh_rate             = refresh_rate;

            video_frame_delay_auto(video_st, &vfda);
            if (vfda.decrease > 0)
            {
               video_frame_delay_effective -= vfda.decrease;
               RARCH_LOG("[Video]: Frame delay decrease by %d ms to %d ms due to frame time: %d > %d.\n",
                     vfda.decrease, video_frame_delay_effective, vfda.time, vfda.target);
            }
         }
      }
      else
         video_st->frame_delay_target = video_frame_delay_effective = video_frame_delay;

      video_st->frame_delay_effective = video_frame_delay_effective;

      if (video_frame_delay_effective > 0)
         retro_sleep(video_frame_delay_effective);
   }

   {
#ifdef HAVE_RUNAHEAD
      bool run_ahead_enabled            = settings->bools.run_ahead_enabled;
      unsigned run_ahead_num_frames     = settings->uints.run_ahead_frames;
      bool run_ahead_hide_warnings      = settings->bools.run_ahead_hide_warnings;
      bool run_ahead_secondary_instance = settings->bools.run_ahead_secondary_instance;
      /* Run Ahead Feature replaces the call to core_run in this loop */
      bool want_runahead                = run_ahead_enabled
            && (run_ahead_num_frames > 0) 
            && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE);
#ifdef HAVE_NETWORKING
      want_runahead                     = want_runahead 
            && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif

      if (want_runahead)
         do_runahead(
               runloop_st,
               run_ahead_num_frames,
               run_ahead_hide_warnings,
               run_ahead_secondary_instance);
      else
#endif
         core_run();
   }

   /* Increment runtime tick counter after each call to
    * core_run() or run_ahead() */
   runloop_st->core_runtime_usec += runloop_core_runtime_tick(
         runloop_st,
         slowmotion_ratio,
         current_time);

#ifdef HAVE_CHEEVOS
   if (cheevos_enable)
      rcheevos_test();
#endif
#ifdef HAVE_CHEATS
   cheat_manager_apply_retro_cheats();
#endif
#ifdef HAVE_PRESENCE
   presence_update(PRESENCE_GAME);
#endif

   /* Restores analog D-pad binds temporarily overridden. */
   for (i = 0; i < max_users; i++)
   {
      if (dpad_mode[i] != ANALOG_DPAD_NONE)
      {
         int j;
         unsigned joy_idx                    = settings->uints.input_joypad_index[i];
         struct retro_keybind *general_binds = input_config_binds[joy_idx];
         struct retro_keybind *auto_binds    = input_autoconf_binds[joy_idx];

         for (j = RETRO_DEVICE_ID_JOYPAD_UP; j <= RETRO_DEVICE_ID_JOYPAD_RIGHT; j++)
         {
            (auto_binds)[j].joyaxis    = (auto_binds)[j].orig_joyaxis;
            (general_binds)[j].joyaxis = (general_binds)[j].orig_joyaxis;
         }
      }
   }

#ifdef HAVE_BSV_MOVIE
   if (input_st->bsv_movie_state_handle)
   {
      input_st->bsv_movie_state_handle->frame_ptr    =
         (input_st->bsv_movie_state_handle->frame_ptr + 1)
         & input_st->bsv_movie_state_handle->frame_mask;

      input_st->bsv_movie_state_handle->first_rewind =
         !input_st->bsv_movie_state_handle->did_rewind;
      input_st->bsv_movie_state_handle->did_rewind   = false;
   }
#endif

#ifdef HAVE_THREADS
   if (runloop_st->flags & RUNLOOP_FLAG_AUTOSAVE)
      autosave_unlock();
#endif

end:
   if (vrr_runloop_enable)
   {
      /* Sync on video only, block audio later. */
      if (runloop_st->fastforward_after_frames && audio_sync)
      {
         if (runloop_st->fastforward_after_frames == 1)
         {
            /* Nonblocking audio */
            if (    (audio_st->flags & AUDIO_FLAG_ACTIVE)
                 && (audio_st->context_audio_data))
               audio_st->current_audio->set_nonblock_state(
                     audio_st->context_audio_data, true);
            audio_st->chunk_size =
               audio_st->chunk_nonblock_size;
         }

         runloop_st->fastforward_after_frames++;

         if (runloop_st->fastforward_after_frames == 6)
         {
            /* Blocking audio */
            if (     (audio_st->flags & AUDIO_FLAG_ACTIVE)
                  && (audio_st->context_audio_data))
               audio_st->current_audio->set_nonblock_state(
                     audio_st->context_audio_data,
                     audio_sync ? false : true);

            audio_st->chunk_size = audio_st->chunk_block_size;
            runloop_st->fastforward_after_frames = 0;
         }
      }

      if (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
         runloop_st->frame_limit_minimum_time = 
            runloop_set_frame_limit(&video_st->av_info,
                  runloop_get_fastforward_ratio(settings,
                     &runloop_st->fastmotion_override.current));
      else
         runloop_st->frame_limit_minimum_time = 
            runloop_set_frame_limit(&video_st->av_info,
                  1.0f);
   }

   /* if there's a fast forward limit, inject sleeps to keep from going too fast. */
   if (runloop_st->frame_limit_minimum_time)
   {
      const retro_time_t end_frame_time = cpu_features_get_time_usec();
      const retro_time_t to_sleep_ms = (
            (  runloop_st->frame_limit_last_time 
             + runloop_st->frame_limit_minimum_time)
            - end_frame_time) / 1000;

      if (to_sleep_ms > 0)
      {
         unsigned               sleep_ms = (unsigned)to_sleep_ms;

         /* Combat jitter a bit. */
         runloop_st->frame_limit_last_time += 
            runloop_st->frame_limit_minimum_time;

         if (sleep_ms > 0)
         {
#if defined(HAVE_COCOATOUCH)
            if (!uico_state_get_ptr()->is_on_foreground)
#endif
               retro_sleep(sleep_ms);
         }

         return 1;
      }

      runloop_st->frame_limit_last_time = end_frame_time;
   }

   return 0;
}

void runloop_msg_queue_deinit(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   RUNLOOP_MSG_QUEUE_LOCK(runloop_st);

   msg_queue_deinitialize(&runloop_st->msg_queue);

   RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
#ifdef HAVE_THREADS
   slock_free(runloop_st->msg_queue_lock);
   runloop_st->msg_queue_lock = NULL;
#endif

   runloop_st->msg_queue_size = 0;
}

void runloop_msg_queue_init(void)
{
   runloop_state_t *runloop_st = &runloop_state;

   runloop_msg_queue_deinit();
   msg_queue_initialize(&runloop_st->msg_queue, 8);

#ifdef HAVE_THREADS
   runloop_st->msg_queue_lock   = slock_new();
#endif
}

void runloop_task_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
#if defined(HAVE_GFX_WIDGETS)
#ifdef HAVE_MENU
   struct menu_state *menu_st     = menu_state_get_ptr();
#endif
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st      = access_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   bool accessibility_enable      = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   runloop_state_t *runloop_st    = &runloop_state;
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   bool widgets_active            = p_dispwidget->active;

   if (widgets_active && task->title && !task->mute)
   {
      RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
      ui_companion_driver_msg_queue_push(msg,
            prio, task ? duration : duration * 60 / 1000, flush);
#ifdef HAVE_ACCESSIBILITY
      if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
         accessibility_speak_priority(
               accessibility_enable,
               accessibility_narrator_speech_speed,
               (char*)msg, 0);
#endif
      gfx_widgets_msg_queue_push(
            task,
            msg,
            duration,
            NULL,
            (enum message_queue_icon)MESSAGE_QUEUE_CATEGORY_INFO,
            (enum message_queue_category)MESSAGE_QUEUE_ICON_DEFAULT,
            prio,
            flush,
#ifdef HAVE_MENU
            menu_st->flags & MENU_ST_FLAG_ALIVE
#else
            false
#endif
            );
      RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
   }
   else
#endif
      runloop_msg_queue_push(msg, prio, duration, flush, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

bool retroarch_get_current_savestate_path(char *path, size_t len)
{
   runloop_state_t *runloop_st = &runloop_state;
   settings_t *settings        = config_get_ptr();
   int state_slot              = settings ? settings->ints.state_slot : 0;
   const char *name_savestate  = NULL;

   if (!path)
      return false;

   name_savestate              = runloop_st->name.savestate;
   if (string_is_empty(name_savestate))
      return false;

   if (state_slot > 0)
      snprintf(path, len, "%s%d",  name_savestate, state_slot);
   else if (state_slot < 0)
      fill_pathname_join_delim(path, name_savestate, "auto", '.', len);
   else
      strlcpy(path, name_savestate, len);

   return true;
}

bool retroarch_get_entry_state_path(char *path, size_t len, unsigned slot)
{
   size_t _len;
   runloop_state_t *runloop_st = &runloop_state;
   const char *name_savestate  = NULL;

   if (!path || !slot)
      return false;

   name_savestate              = runloop_st->name.savestate;
   if (string_is_empty(name_savestate))
      return false;

   _len = strlcpy(path, name_savestate, len);
   snprintf(path + _len, len - _len, "%d.entry", slot);

   return true;
}

void runloop_set_current_core_type(
      enum rarch_core_type type, bool explicitly_set)
{
   runloop_state_t *runloop_st                = &runloop_state;

   if (runloop_st->flags & RUNLOOP_FLAG_HAS_SET_CORE)
      return;

   if (explicitly_set)
   {
      runloop_st->flags                      |= RUNLOOP_FLAG_HAS_SET_CORE;
      runloop_st->explicit_current_core_type  = type;
   }
   runloop_st->current_core_type              = type;
}

bool core_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs  = (struct retro_callbacks*)data;
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   cbs->frame_cb                = video_driver_frame;
   cbs->sample_cb               = audio_driver_sample;
   cbs->sample_batch_cb         = audio_driver_sample_batch;
   cbs->state_cb                = state_cb;
   cbs->poll_cb                 = input_driver_poll;

   return true;
}

#ifdef HAVE_NETWORKING
/**
 * core_set_netplay_callbacks:
 *
 * Set the I/O callbacks to use netplay's interceding callback system. Should
 * only be called while initializing netplay.
 **/
bool core_set_netplay_callbacks(void)
{
   runloop_state_t *runloop_st        = &runloop_state;

   /* Force normal poll type for netplay. */
   runloop_st->current_core.poll_type = POLL_TYPE_NORMAL;

   /* And use netplay's interceding callbacks */
   runloop_st->current_core.retro_set_video_refresh(video_frame_net);
   runloop_st->current_core.retro_set_audio_sample(audio_sample_net);
   runloop_st->current_core.retro_set_audio_sample_batch(audio_sample_batch_net);
   runloop_st->current_core.retro_set_input_state(input_state_net);

   return true;
}

/**
 * core_unset_netplay_callbacks
 *
 * Unset the I/O callbacks from having used netplay's interceding callback
 * system. Should only be called while uninitializing netplay.
 */
bool core_unset_netplay_callbacks(void)
{
   struct retro_callbacks cbs;
   runloop_state_t *runloop_st  = &runloop_state;

   if (!core_set_default_callbacks(&cbs))
      return false;

   runloop_st->current_core.retro_set_video_refresh(cbs.frame_cb);
   runloop_st->current_core.retro_set_audio_sample(cbs.sample_cb);
   runloop_st->current_core.retro_set_audio_sample_batch(cbs.sample_batch_cb);
   runloop_st->current_core.retro_set_input_state(cbs.state_cb);

   return true;
}
#endif

bool core_set_cheat(retro_ctx_cheat_info_t *info)
{
   runloop_state_t *runloop_st       = &runloop_state;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   settings_t *settings              = config_get_ptr();
   bool run_ahead_enabled            = false;
   unsigned run_ahead_frames         = 0;
   bool run_ahead_secondary_instance = false;
   bool want_runahead                = false;

   if (settings)
   {
      run_ahead_enabled              = settings->bools.run_ahead_enabled;
      run_ahead_frames               = settings->uints.run_ahead_frames;
      run_ahead_secondary_instance   = settings->bools.run_ahead_secondary_instance;
      want_runahead                  = run_ahead_enabled
            && (run_ahead_frames > 0) 
            && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE);
#ifdef HAVE_NETWORKING
      if (want_runahead)
         want_runahead               = !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
   }
#endif

   runloop_st->current_core.retro_cheat_set(info->index, info->enabled, info->code);

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   if (     (want_runahead)
         && (run_ahead_secondary_instance)
         && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE)
         && (secondary_core_ensure_exists(settings))
         && (runloop_st->secondary_core.retro_cheat_set))
      runloop_st->secondary_core.retro_cheat_set(
            info->index, info->enabled, info->code);
#endif

   return true;
}

bool core_reset_cheat(void)
{
   runloop_state_t *runloop_st       = &runloop_state;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   settings_t *settings              = config_get_ptr();
   bool run_ahead_enabled            = false;
   unsigned run_ahead_frames         = 0;
   bool run_ahead_secondary_instance = false;
   bool want_runahead                = false;

   if (settings)
   {
      run_ahead_enabled              = settings->bools.run_ahead_enabled;
      run_ahead_frames               = settings->uints.run_ahead_frames;
      run_ahead_secondary_instance   = settings->bools.run_ahead_secondary_instance;
      want_runahead                  = run_ahead_enabled 
         && (run_ahead_frames > 0) 
         && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE);
#ifdef HAVE_NETWORKING
      if (want_runahead)
         want_runahead               = !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
   }
#endif

   runloop_st->current_core.retro_cheat_reset();

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   if (   (want_runahead)
       && (run_ahead_secondary_instance)
       && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE)
       && (secondary_core_ensure_exists(settings))
       && (runloop_st->secondary_core.retro_cheat_reset))
      runloop_st->secondary_core.retro_cheat_reset();
#endif

   return true;
}

bool core_set_poll_type(unsigned type)
{
   runloop_state_t *runloop_st        = &runloop_state;
   runloop_st->current_core.poll_type = type;
   return true;
}

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   runloop_state_t *runloop_st    = &runloop_state;
   input_driver_state_t *input_st = input_state_get_ptr();
   if (!pad)
      return false;

   /* We are potentially 'connecting' a entirely different
    * type of virtual input device, which may or may not
    * support analog inputs. We therefore have to reset
    * the 'analog input requested' flag for this port - but
    * since port mapping is arbitrary/mutable, it is easiest
    * to simply reset the flags for all ports.
    * Correct values will be registered at the next call
    * of 'input_state()' */
   memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));

#ifdef HAVE_RUNAHEAD
   remember_controller_port_device(pad->port, pad->device);
#endif

   runloop_st->current_core.retro_set_controller_port_device(pad->port, pad->device);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   runloop_state_t *runloop_st    = &runloop_state;
   if (!info)
      return false;
   info->size  = runloop_st->current_core.retro_get_memory_size(info->id);
   info->data  = runloop_st->current_core.retro_get_memory_data(info->id);
   return true;
}

bool core_load_game(retro_ctx_load_content_info_t *load_info)
{
   bool             game_loaded = false;
   runloop_state_t *runloop_st  = &runloop_state;
   uint8_t flags                = content_get_flags();

   video_driver_set_cached_frame_ptr(NULL);

#ifdef HAVE_RUNAHEAD
   set_load_content_info(runloop_st, load_info);
   runloop_clear_controller_port_map();
#endif

   set_save_state_in_background(false);

   if (load_info && load_info->special)
      game_loaded = runloop_st->current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      game_loaded = runloop_st->current_core.retro_load_game(load_info->info);
   else if (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
      game_loaded = runloop_st->current_core.retro_load_game(NULL);

   runloop_st->current_core.game_loaded = game_loaded;

   /* If 'game_loaded' is true at this point, then
    * core is actually running; register that any
    * changes to global remap-related parameters
    * should be reset once core is deinitialised */
   if (game_loaded)
      input_remapping_enable_global_config_restore();

   return game_loaded;
}

bool core_get_system_info(struct retro_system_info *system)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!system)
      return false;
   runloop_st->current_core.retro_get_system_info(system);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!info || !runloop_st->current_core.retro_unserialize(info->data_const, info->size))
      return false;

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_unserialize_special(retro_ctx_serialize_info_t *info)
{
   bool ret;
   runloop_state_t *runloop_st = &runloop_state;

   if (!info)
      return false;

   runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   ret = runloop_st->current_core.retro_unserialize(info->data_const, info->size);
   runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

#ifdef HAVE_NETWORKING
   if (ret)
      netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return ret;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!info || !runloop_st->current_core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_special(retro_ctx_serialize_info_t *info)
{
   bool ret;
   runloop_state_t *runloop_st = &runloop_state;

   if (!info)
      return false;

   runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   ret                = runloop_st->current_core.retro_serialize(info->data, info->size);
   runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

   return ret;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!info)
      return false;
   info->size = runloop_st->current_core.retro_serialize_size();
   return true;
}

bool core_serialize_size_special(retro_ctx_size_info_t *info)
{
   runloop_state_t *runloop_st = &runloop_state;

   if (!info)
      return false;

   runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   info->size         = runloop_st->current_core.retro_serialize_size();
   runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

   return true;
}

uint64_t core_serialization_quirks(void)
{
   runloop_state_t *runloop_st  = &runloop_state;
   return runloop_st->current_core.serialization_quirks_v;
}

bool core_reset(void)
{
   runloop_state_t *runloop_st  = &runloop_state;

   video_driver_set_cached_frame_ptr(NULL);

   runloop_st->current_core.retro_reset();
   return true;
}

bool core_run(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   struct retro_core_t *
      current_core             = &runloop_st->current_core;
   const enum poll_type_override_t
      core_poll_type_override  = runloop_st->core_poll_type_override;
   unsigned new_poll_type      = (core_poll_type_override != POLL_TYPE_OVERRIDE_DONTCARE)
      ? (core_poll_type_override - 1)
      : current_core->poll_type;
   bool early_polling          = new_poll_type == POLL_TYPE_EARLY;
   bool late_polling           = new_poll_type == POLL_TYPE_LATE;
#ifdef HAVE_NETWORKING
   bool netplay_preframe       = netplay_driver_ctl(
         RARCH_NETPLAY_CTL_PRE_FRAME, NULL);

   if (!netplay_preframe)
   {
      /* Paused due to netplay. We must poll and display something so that a
       * netplay peer pausing doesn't just hang. */
      input_driver_poll();
      video_driver_cached_frame();
      return true;
   }
#endif

   if (early_polling)
      input_driver_poll();
   else if (late_polling)
      current_core->input_polled = false;

   current_core->retro_run();

   if (late_polling && !current_core->input_polled)
      input_driver_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

   return true;
}

bool core_has_set_input_descriptor(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->current_core.has_set_input_descriptors;
}

char* crt_switch_core_name(void)
{
   return (char*)runloop_state.system.info.library_name;
}

void runloop_path_set_basename(const char *path)
{
   runloop_state_t *runloop_st = &runloop_state;
   char *dst                   = NULL;

   path_set(RARCH_PATH_CONTENT,  path);
   path_set(RARCH_PATH_BASENAME, path);

#ifdef HAVE_COMPRESSION
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths.
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable
    * directory then and the name of srm and states are meaningful.
    *
    */
   path_basedir_wrapper(runloop_st->runtime_content_path_basename);
   if (!string_is_empty(runloop_st->runtime_content_path_basename))
      fill_pathname_dir(runloop_st->runtime_content_path_basename, path, "", sizeof(runloop_st->runtime_content_path_basename));
#endif

   if ((dst = strrchr(runloop_st->runtime_content_path_basename, '.')))
      *dst = '\0';
}

void runloop_path_set_names(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
   {
      size_t len = strlcpy(runloop_st->name.savefile,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.savefile));
      runloop_st->name.savefile[len  ] = '.';
      runloop_st->name.savefile[len+1] = 's';
      runloop_st->name.savefile[len+2] = 'r';
      runloop_st->name.savefile[len+3] = 'm';
      runloop_st->name.savefile[len+4] = '\0';
   }

   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
   {
      size_t len                        = strlcpy(
            runloop_st->name.savestate,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.savestate));
      runloop_st->name.savestate[len  ] = '.';
      runloop_st->name.savestate[len+1] = 's';
      runloop_st->name.savestate[len+2] = 't';
      runloop_st->name.savestate[len+3] = 'a';
      runloop_st->name.savestate[len+4] = 't';
      runloop_st->name.savestate[len+5] = 'e';
      runloop_st->name.savestate[len+6] = '\0';
   }

#ifdef HAVE_CHEATS
   if (!string_is_empty(runloop_st->runtime_content_path_basename))
   {
      size_t len                        = strlcpy(
            runloop_st->name.cheatfile,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.cheatfile));
      runloop_st->name.cheatfile[len  ] = '.';
      runloop_st->name.cheatfile[len+1] = 'c';
      runloop_st->name.cheatfile[len+2] = 'h';
      runloop_st->name.cheatfile[len+3] = 't';
      runloop_st->name.cheatfile[len+4] = '\0';
   }
#endif
}
