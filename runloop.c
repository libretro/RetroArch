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

#include <libretro.h>
#ifdef HAVE_VULKAN
#include <libretro_vulkan.h>
#endif

#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#include <features/features_cpu.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <lists/dir_list.h>

#ifdef EMSCRIPTEN
#include <emscripten/emscripten.h>
#endif

#ifdef HAVE_LIBNX
#include <switch.h>
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

#ifdef HAVE_RUNAHEAD
#include "runahead.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_cbs.h"
#include "menu/menu_driver.h"
#include "menu/menu_input.h"
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "menu/menu_shader.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#include "input/input_keymaps.h"
#include "input/input_remapping.h"

#ifdef HAVE_MICROPHONE
#include "audio/microphone_driver.h"
#endif

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

#if TARGET_OS_IPHONE
#include "JITSupport.h"
#endif

#if HAVE_GAME_AI
#include "ai/game_ai.h"
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
   if (!current_core->x) { RARCH_ERR("Failed to load symbol: \"%s\"\n", #x); retroarch_fail(1, "runloop_init_libretro_symbols()"); } \
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

bool state_manager_frame_is_reversed(void)
{
#ifdef HAVE_REWIND
   return (runloop_state.rewind_st.flags & STATE_MGR_REWIND_ST_FLAG_FRAME_IS_REVERSED) > 0;
#else
   return false;
#endif
}

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
   for (i = 0; i < (int)num; i++)
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

static void runloop_perf_log(void)
{
   if (!runloop_state.perfcnt_enable)
      return;

   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   runloop_log_counters(runloop_state.perf_counters_libretro,
         runloop_state.perf_ptr_libretro);
}

static bool runloop_environ_cb_get_system_info(unsigned cmd, void *data)
{
   runloop_state_t *runloop_st    = &runloop_state;
   rarch_system_info_t *sys_info  = &runloop_st->system;

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
         unsigned log_level      = config_get_ptr()->uints.libretro_log_level;

         runloop_st->subsystem_current_count = 0;

         RARCH_LOG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_DBG("Subsystem ID: %d\nSpecial game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  i,
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );
            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_DBG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         size = i;

         if (log_level == RETRO_LOG_DEBUG)
         {
            RARCH_DBG("Subsystems: %d\n", i);
            if (size > SUBSYSTEM_MAX_SUBSYSTEMS)
               RARCH_WARN("Subsystems exceed subsystem max, clamping to %d\n", SUBSYSTEM_MAX_SUBSYSTEMS);
         }

         if (sys_info)
         {
            for (i = 0; i < size && i < SUBSYSTEM_MAX_SUBSYSTEMS; i++)
            {
               struct retro_subsystem_info *subsys_info         = &runloop_st->subsystem_data[i];
               struct retro_subsystem_rom_info *subsys_rom_info = runloop_st->subsystem_data_roms[i];
               /* Nasty, but have to do it like this since
                * the pointers are const char *
                * (if we don't free them, we get a memory leak) */
               if (!string_is_empty(subsys_info->desc))
                  free((char*)subsys_info->desc);
               if (!string_is_empty(subsys_info->ident))
                  free((char*)subsys_info->ident);
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
                     free((char*)
                           subsys_rom_info[j].desc);
                  if (!string_is_empty(
                           subsys_rom_info[j].valid_extensions))
                     free((char*)
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

static dylib_t load_dynamic_core(const char *path, char *s,
      size_t len)
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
   path_resolve_realpath(s, len, resolve_symlinks);
   return dylib_load(path);
}

static dylib_t libretro_get_system_info_lib(const char *path,
      struct retro_system_info *sysinfo, bool *load_no_content)
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

   proc(sysinfo);

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
      char log[256]             = {0};
      unsigned hours            = 0;
      unsigned minutes          = 0;
      unsigned seconds          = 0;

      runtime_log_convert_usec2hms(
            runloop_st->core_runtime_usec,
            &hours, &minutes, &seconds);

      /* TODO/FIXME - localize */
      snprintf(log, sizeof(log),
            "[Core]: Content ran for a total of:"
            " %02u hours, %02u minutes, %02u seconds.",
            hours, minutes, seconds);
      RARCH_LOG("%s\n", log);
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
   /* Does this need to treat the microphone driver the same way? */
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
   if (!driver_switch_enable)
   {
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
            if (     !string_is_equal(video_ident, "gl")
                  && !string_is_equal(video_ident, "glcore"))
               return false;
            break;
         case RETRO_HW_CONTEXT_D3D10:
            if (!string_is_equal(video_ident, "d3d10"))
               return false;
            break;
         case RETRO_HW_CONTEXT_D3D11:
            if (!string_is_equal(video_ident, "d3d11"))
               return false;
            break;
         case RETRO_HW_CONTEXT_D3D12:
            if (!string_is_equal(video_ident, "d3d12"))
               return false;
            break;
         default:
            break;
      }
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

#if defined(HAVE_D3D11)
      case RETRO_HW_CONTEXT_D3D11:
         RARCH_LOG("Requesting D3D11 context.\n");
         break;
#endif
#ifdef HAVE_D3D10
      case RETRO_HW_CONTEXT_D3D10:
         RARCH_LOG("Requesting D3D10 context.\n");
         break;
#endif
#ifdef HAVE_D3D12
      case RETRO_HW_CONTEXT_D3D12:
         RARCH_LOG("Requesting D3D12 context.\n");
         break;
#endif
#if defined(HAVE_D3D9)
      case RETRO_HW_CONTEXT_D3D9:
         RARCH_LOG("Requesting D3D9 context.\n");
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
   unsigned libretro_log_level = config_get_ptr()->uints.libretro_log_level;

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

         desc->core.select = top_addr
            & ~mmap_inflate(mmap_add_bits_down(desc->core.len - 1),
               desc->core.disconnect);
      }

      if (desc->core.len == 0)
         desc->core.len = mmap_add_bits_down(
               mmap_reduce(top_addr & ~desc->core.select,
                  desc->core.disconnect)) + 1;

      if (desc->core.start & ~desc->core.select)
         return false;

      highest_reachable = mmap_inflate(desc->core.len - 1,
            desc->core.disconnect);

      /* Disconnect unselected bits that are too high to ever
       * index into the core's buffer. Higher addresses will
       * repeat / mirror the buffer as long as they match select */
      while (mmap_highest_bit(top_addr
               & ~desc->core.select
               & ~desc->core.disconnect) >
                mmap_highest_bit(highest_reachable))
         desc->core.disconnect |= mmap_highest_bit(top_addr
               & ~desc->core.select
               & ~desc->core.disconnect);
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

      /* We only need to save configuration settings
       * for the current core
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
   char config_directory[DIR_MAX_LENGTH];
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

static bool validate_folder_options(char *s, size_t len, bool mkdir)
{
   const char *game_path       = path_get(RARCH_PATH_BASENAME);

   if (!string_is_empty(game_path))
   {
      char folder_name[DIR_MAX_LENGTH];
      runloop_state_t *runloop_st = &runloop_state;
      const char *core_name       = runloop_st->system.info.library_name;
      fill_pathname_parent_dir_name(folder_name,
            game_path, sizeof(folder_name));
      return validate_per_core_options(s, len, mkdir,
            core_name, folder_name);
   }
   return false;
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
      bool game_specific_options,
      bool per_core_options,
      const char *path_core_options,
      char *s,  size_t len,
      char *s2, size_t len2)
{
   runloop_state_t *runloop_st    = &runloop_state;

   /* Check whether game-specific options exist */
   if (   game_specific_options
       && validate_per_core_options(s, len, false,
         runloop_st->system.info.library_name,
         path_basename_nocompression(path_get(RARCH_PATH_BASENAME)))
       && path_is_valid(s))
   {
      RARCH_LOG("[Core]: %s \"%s\".\n",
            msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
            s);
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, s);
      runloop_st->flags &= ~RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
   }
   /* Check whether folder-specific options exist */
   else if (   game_specific_options
            && validate_folder_options(s, len, false)
            && path_is_valid(s))
   {
      RARCH_LOG("[Core]: %s \"%s\".\n",
            msg_hash_to_str(MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT),
            s);
      /* Notify system that we have a valid core options
       * override */
      path_set(RARCH_PATH_CORE_OPTIONS, s);
      runloop_st->flags &= ~RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
   }
   else
   {
      char global_options_path[PATH_MAX_LENGTH];
      char per_core_options_path[PATH_MAX_LENGTH];
      bool per_core_options_exist   = false;

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
      if (     !per_core_options
            || !per_core_options_exist)
      {
         if (!string_is_empty(path_core_options))
            strlcpy(global_options_path,
                  path_core_options, sizeof(global_options_path));
         else if (!path_is_empty(RARCH_PATH_CONFIG))
            fill_pathname_resolve_relative(
                  global_options_path, path_get(RARCH_PATH_CONFIG),
                  FILE_PATH_CORE_OPTIONS_CONFIG, sizeof(global_options_path));
      }

      /* Allocate correct path/src_path strings */
      if (per_core_options)
      {
         strlcpy(s, per_core_options_path, len);
         if (!per_core_options_exist)
            strlcpy(s2, global_options_path, len2);
      }
      else
         strlcpy(s, global_options_path, len);

      /* Notify system that we *do not* have a valid core options
       * options override */
      runloop_st->flags &= ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                           | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
   }
}

static core_option_manager_t *runloop_init_core_options(
      bool categories_enabled,
      bool game_specific_options,
      bool global_core_options,
      const char *path_core_options,
      const struct retro_core_options_v2 *options_v2)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];
   /* Ensure these are NULL-terminated */
   options_path[0]     = '\0';
   src_options_path[0] = '\0';
   /* Get core options file path */
   runloop_init_core_options_path(
         game_specific_options,
         global_core_options,
         path_core_options,
         options_path, sizeof(options_path),
         src_options_path, sizeof(src_options_path));
   if (!string_is_empty(options_path))
      return core_option_manager_new(options_path,
            src_options_path, options_v2,
            categories_enabled);
   return NULL;
}

static core_option_manager_t *runloop_init_core_variables(
      bool game_specific_options,
      bool global_core_options,
      const char *path_core_options,
      const struct retro_variable *vars)
{
   char options_path[PATH_MAX_LENGTH];
   char src_options_path[PATH_MAX_LENGTH];

   /* Ensure these are NULL-terminated */
   options_path[0]     = '\0';
   src_options_path[0] = '\0';

   /* Get core options file path */
   runloop_init_core_options_path(
         game_specific_options,
         global_core_options,
         path_core_options,
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
   enum message_queue_category category;
   /* Get duration in frames */
   double fps               = (av_info && (av_info->timing.fps > 0)) ? av_info->timing.fps : 60.0;
   unsigned duration_frames = (unsigned)((fps * (float)msg->duration / 1000.0f) + 0.5f);

   /* Assign category */
   switch (msg->level)
   {
      case RETRO_LOG_WARN:
         category  = MESSAGE_QUEUE_CATEGORY_WARNING;
         break;
      case RETRO_LOG_ERROR:
         category  = MESSAGE_QUEUE_CATEGORY_ERROR;
         break;
      case RETRO_LOG_INFO:
      case RETRO_LOG_DEBUG:
      default:
         category  = MESSAGE_QUEUE_CATEGORY_INFO;
         break;
   }

   /* Note: Do not flush the message queue here - a core
    * may need to send multiple notifications simultaneously */
   runloop_msg_queue_push(msg->msg, strlen(msg->msg),
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
   settings_t         *settings           = config_get_ptr();
   rarch_system_info_t *sys_info          = &runloop_st->system;
   bool ignore_environment_cb             = (runloop_st->flags &
      RUNLOOP_FLAG_IGNORE_ENVIRONMENT_CB) ? true : false;

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
            *(bool*)data             = !video_crop_overscan;
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
            struct retro_variable *var = (struct retro_variable*)data;
            size_t opt_idx;

            if (!var)
               return true;

            var->value = NULL;

            if (!runloop_st->core_options)
            {
               RARCH_ERR("[Environ]: GET_VARIABLE: %s - %s.\n",
                     var->key, "Not implemented");
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

            if (!var->value)
            {
               RARCH_ERR("[Environ]: GET_VARIABLE: %s - %s.\n",
                     var->key, "Invalid value");
               return true;
            }

            RARCH_DBG("[Environ]: GET_VARIABLE: %s = \"%s\"\n",
                  var->key, var->value);
         }
         break;

      case RETRO_ENVIRONMENT_SET_VARIABLE:
         {
            size_t opt_idx, val_idx;
            const struct retro_variable *var = (const struct retro_variable*)data;

            /* If core passes NULL to the callback, return
             * value indicates whether callback is supported */
            if (!var)
               return true;

            if (     string_is_empty(var->key)
                  || string_is_empty(var->value))
               return false;

            if (!runloop_st->core_options)
            {
               RARCH_ERR("[Environ]: SET_VARIABLE: %s - %s.\n",
                     var->key, "Not implemented");
               return false;
            }

            /* Check whether key is valid */
            if (!core_option_manager_get_idx(runloop_st->core_options,
                  var->key, &opt_idx))
            {
               RARCH_ERR("[Environ]: SET_VARIABLE: %s - %s.\n",
                     var->key, "Invalid key");
               return false;
            }

            /* Check whether value is valid */
            if (!core_option_manager_get_val_idx(runloop_st->core_options,
                  opt_idx, var->value, &val_idx))
            {
               RARCH_ERR("[Environ]: SET_VARIABLE: %s - %s: %s\n",
                     var->key, "Invalid value", var->value);
               return false;
            }

            /* Update option value if core-requested value
             * is not currently set */
            if (val_idx != runloop_st->core_options->opts[opt_idx].index)
               core_option_manager_set_val(runloop_st->core_options,
                     opt_idx, val_idx, true);

            RARCH_DBG("[Environ]: SET_VARIABLE: %s = \"%s\"\n",
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
                     (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE) ? true : false,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags           &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options     = NULL;
            }

            if ((new_vars = runloop_init_core_variables(
                        settings->bools.game_specific_options,
                        !settings->bools.global_core_options,
                        settings->paths.path_core_options,
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
                     (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE) ? true : false,
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
               core_option_manager_t *new_vars = runloop_init_core_options(
                     settings->bools.core_option_category_enable,
                     settings->bools.game_specific_options,
                     !settings->bools.global_core_options,
                     settings->paths.path_core_options,
                     options_v2);

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
                     (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE) ? true : false,
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
               core_option_manager_t *new_vars = runloop_init_core_options(
                     settings->bools.core_option_category_enable,
                     settings->bools.game_specific_options,
                     !settings->bools.global_core_options,
                     settings->paths.path_core_options,
                     options_v2);

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
                     (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE) ? true : false,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->flags                &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
               runloop_st->core_options          = NULL;
            }

            if (options_v2)
            {
               new_vars = runloop_init_core_options(
                     settings->bools.core_option_category_enable,
                     settings->bools.game_specific_options,
                     !settings->bools.global_core_options,
                     settings->paths.path_core_options,
                     options_v2);

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
                     (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE)
                     ? true : false,
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
               new_vars = runloop_init_core_options(
                     categories_enabled,
                     settings->bools.game_specific_options,
                     !settings->bools.global_core_options,
                     settings->paths.path_core_options,
                     options_v2);

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

            if (   update_display_callback
                && update_display_callback->callback)
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
            runloop_msg_queue_push(msg->msg, strlen(msg->msg), 3, msg->frames,
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
            switch (msg->level)
            {
               case RETRO_LOG_DEBUG:
                  RARCH_DBG("[Environ]: SET_MESSAGE_EXT: %s\n", msg->msg);
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
                  if (   !runloop_st->core_status_msg.set
                      || (runloop_st->core_status_msg.priority <= msg->priority))
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
         unsigned rotation_v[4]  = {0, 90, 180, 270};
         bool video_allow_rotate = settings->bools.video_allow_rotate;

         RARCH_DBG("[Environ]: SET_ROTATION: \"%u\" (%u deg).\n", rotation, rotation_v[rotation % 4]);

         if (sys_info)
            sys_info->core_requested_rotation = rotation;

         if (!video_allow_rotate)
            return false;

         if (sys_info)
            sys_info->rotation = rotation;

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
         if (sys_info)
         {
            sys_info->performance_level = *(const unsigned*)data;
            RARCH_LOG("[Environ]: PERFORMANCE_LEVEL: %u.\n",
                  sys_info->performance_level);
         }
         break;

      case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
         {
            const char *dir_system          = settings->paths.directory_system;
            bool systemfiles_in_content_dir = settings->bools.systemfiles_in_content_dir;

            if (     string_is_empty(dir_system)
                  || systemfiles_in_content_dir)
            {
               const char *fullpath = path_get(RARCH_PATH_CONTENT);

               if (!string_is_empty(fullpath))
               {
                  size_t _len;
                  char tmp_path[PATH_MAX_LENGTH];

                  if (string_is_empty(dir_system))
                     RARCH_WARN("[Environ]: SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                                fullpath);

                  _len = fill_pathname_basedir(tmp_path, fullpath, sizeof(tmp_path));
                  /* Removes trailing slash (unless root dir) */
                  if (string_count_occurrences_single_character(tmp_path, PATH_DEFAULT_SLASH_C()) > 1
                        && tmp_path[_len - 1] == PATH_DEFAULT_SLASH_C())
                           tmp_path[_len - 1] = '\0';

                  dir_set(RARCH_DIR_SYSTEM, tmp_path);
                  *(const char**)data = dir_get_ptr(RARCH_DIR_SYSTEM);
               }
               else /* If content path is empty, fall back to global system dir path */
                  *(const char**)data = dir_system;

               RARCH_LOG("[Environ]: SYSTEM_DIRECTORY: \"%s\".\n",
                     *(const char**)data);
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
         *(const char**)data = runloop_st->savefile_dir;
         RARCH_LOG("[Environ]: SAVE_DIRECTORY: \"%s\".\n",
               runloop_st->savefile_dir);
         break;

      case RETRO_ENVIRONMENT_GET_USERNAME:
         *(const char**)data = *settings->paths.username
            ? settings->paths.username : NULL;
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
         if (sys_info)
         {
            unsigned retro_id;
            const struct retro_input_descriptor *desc = NULL;
            memset((void*)&sys_info->input_desc_btn, 0,
                  sizeof(sys_info->input_desc_btn));

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
                     sys_info->input_desc_btn[retro_port]
                        [retro_id] = desc->description;
                     break;
                  case RETRO_DEVICE_ANALOG:
                     switch (retro_id)
                     {
                        case RETRO_DEVICE_ID_ANALOG_X:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_X_PLUS]  = desc->description;
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_X_MINUS] = desc->description;
                                 break;
                              case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_X_PLUS] = desc->description;
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_X_MINUS] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_ANALOG_Y:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_LEFT:
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_Y_PLUS] = desc->description;
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_LEFT_Y_MINUS] = desc->description;
                                 break;
                              case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_Y_PLUS] = desc->description;
                                 sys_info->input_desc_btn[retro_port]
                                    [RARCH_ANALOG_RIGHT_Y_MINUS] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_JOYPAD_R2:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_BUTTON:
                                 sys_info->input_desc_btn[retro_port]
                                    [retro_id] = desc->description;
                                 break;
                           }
                           break;
                        case RETRO_DEVICE_ID_JOYPAD_L2:
                           switch (desc->index)
                           {
                              case RETRO_DEVICE_INDEX_ANALOG_BUTTON:
                                 sys_info->input_desc_btn[retro_port]
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
               unsigned log_level = settings->uints.libretro_log_level;

               if (log_level == RETRO_LOG_DEBUG)
               {
                  unsigned input_driver_max_users = settings->uints.input_max_users;

                  for (p = 0; p < input_driver_max_users; p++)
                  {
                     unsigned mapped_port = settings->uints.input_remap_ports[p];

                     RARCH_DBG("   %s %u:\n", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT), p + 1);

                     for (retro_id = 0; retro_id < RARCH_FIRST_CUSTOM_BIND; retro_id++)
                     {
                        unsigned bind_index     = input_config_bind_order[retro_id];
                        const char *description = sys_info->input_desc_btn[mapped_port][bind_index];

                        if (!description)
                           continue;

                        RARCH_DBG("      \"%s\" => \"%s\"\n",
                              msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B + bind_index),
                              description);
                     }
                  }
               }
            }

            runloop_st->current_core.flags |=
               RETRO_CORE_FLAG_HAS_SET_INPUT_DESCRIPTORS;
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
          * then it is assumed that Game Focus mode is desired */
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

            if (sys_info)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_INTERFACE.\n");
               disk_control_set_callback(&sys_info->disk_control, control_cb);
            }
         }
         break;

      case RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE:
         {
            const struct retro_disk_control_ext_callback *control_cb =
                  (const struct retro_disk_control_ext_callback*)data;

            if (sys_info)
            {
               RARCH_LOG("[Environ]: SET_DISK_CONTROL_EXT_INTERFACE.\n");
               disk_control_set_ext_callback(&sys_info->disk_control, control_cb);
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
         else if (string_is_equal(video_driver_name, "d3d11"))
         {
             *cb = RETRO_HW_CONTEXT_D3D11;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_D3D11.\n");
         }
         else if (string_is_equal(video_driver_name, "d3d12"))
         {
             *cb = RETRO_HW_CONTEXT_D3D12;
             RARCH_LOG("[Environ]: GET_PREFERRED_HW_RENDER - Context callback set to RETRO_HW_CONTEXT_D3D12.\n");
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
         RARCH_DBG("Reached end of SET_HW_RENDER.\n");
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
         recording_state_t *rec_st   = recording_state_get_ptr();
         audio_driver_state_t
            *audio_st                = audio_state_get_ptr();
         const struct
            retro_audio_callback *cb = (const struct retro_audio_callback*)data;
         RARCH_LOG("[Environ]: SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (rec_st->data) /* A/V sync is a must. */
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
                      (runloop_st->audio_latency > audio_latency_default)
                     ? runloop_st->audio_latency : audio_latency_default;
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
            recording_state_t *rec_st      = recording_state_get_ptr();
            video_driver_state_t *video_st = video_state_get_ptr();
            bool video_fullscreen          = settings->bools.video_fullscreen;
            int reinit_flags               = DRIVERS_CMD_ALL &
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK | DRIVER_MENU_MASK);

            RARCH_LOG("[Environ]: Setting audio latency to %u ms.\n", audio_latency_new);

            command_event(CMD_EVENT_REINIT, &reinit_flags);
            video_driver_set_aspect_ratio();

            /* Cannot continue recording with different
             * parameters.
             * Take the easiest route out and just restart
             * the recording. */

            if (rec_st->data)
            {
               const char *_msg = msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT);
               runloop_msg_queue_push(_msg, strlen(_msg), 2, 180, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               if (rec_st->streaming_enable)
               {
                  command_event(CMD_EVENT_STREAMING_TOGGLE, NULL);
                  command_event(CMD_EVENT_STREAMING_TOGGLE, NULL);
               }
               else
               {
                  command_event(CMD_EVENT_RECORD_DEINIT, NULL);
                  command_event(CMD_EVENT_RECORD_INIT, NULL);
               }
            }

            /* Hide mouse cursor in fullscreen mode */
            if (video_fullscreen)
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, false);
            }
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
         location_driver_state_t *loc_st = location_state_get_ptr();

         RARCH_LOG("[Environ]: GET_LOCATION_INTERFACE.\n");
         cb->start                       = driver_location_start;
         cb->stop                        = driver_location_stop;
         cb->get_position                = driver_location_get_position;
         cb->set_interval                = driver_location_set_interval;

         if (sys_info)
            sys_info->location_cb        = *cb;

         loc_st->active                  = true;
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

         *dir = *dir_core_assets ? dir_core_assets : NULL;
         RARCH_LOG("[Environ]: CORE_ASSETS_DIRECTORY: \"%s\".\n",
               dir_core_assets);
         break;
      }

      case RETRO_ENVIRONMENT_GET_PLAYLIST_DIRECTORY:
      {
         const char **dir            = (const char**)data;
         const char *dir_playlist    = settings->paths.directory_playlist;

         *dir = *dir_playlist ? dir_playlist : NULL;
         RARCH_LOG("[Environ]: PLAYLIST_DIRECTORY: \"%s\".\n",
               dir_playlist);
         break;
      }

      case RETRO_ENVIRONMENT_GET_FILE_BROWSER_START_DIRECTORY:
      {
         const char **dir            = (const char**)data;
         const char *dir_content     = settings->paths.directory_menu_content;

         *dir = *dir_content ? dir_content : NULL;
         RARCH_LOG("[Environ]: FILE_BROWSER_START_DIRECTORY: \"%s\".\n",
               dir_content);
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
            recording_state_t *rec_st             = recording_state_get_ptr();
            float refresh_rate                    = (*info)->timing.fps;
            unsigned crt_switch_resolution        = settings->uints.crt_switch_resolution;
            bool video_fullscreen                 = settings->bools.video_fullscreen;
            bool video_frame_delay_auto           = settings->bools.video_frame_delay_auto;
            bool video_switch_refresh_rate        = false;
            bool no_video_reinit                  = true;

            /* Refresh rate switch for regular displays */
            if (video_display_server_has_resolution_list())
               video_switch_refresh_rate_maybe(&refresh_rate, &video_switch_refresh_rate);

            /* Recalibrate frame delay target when video reinits
             * and pause frame delay when video does not reinit */
            if (video_frame_delay_auto)
            {
               if (no_video_reinit && !video_switch_refresh_rate)
                  video_st->frame_delay_pause  = true;
               else
                  video_st->frame_delay_target = 0;
            }

            no_video_reinit                       = (
                     (crt_switch_resolution     == 0)
                  && (video_switch_refresh_rate == false)
                  && data
                  && ((*info)->geometry.max_width  == av_info->geometry.max_width)
                  && ((*info)->geometry.max_height == av_info->geometry.max_height));

            /* First set new refresh rate and display rate, then after REINIT do
             * another display rate change to make sure the change stays */
            if (     video_switch_refresh_rate
                  && video_display_server_set_refresh_rate(refresh_rate))
               video_monitor_set_refresh_rate(refresh_rate);

            /* When not doing video reinit, we also must not do input and menu
             * reinit, otherwise the input driver crashes and the menu gets
             * corrupted. */
            if (no_video_reinit)
               reinit_flags =
                  DRIVERS_CMD_ALL &
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK | DRIVER_MENU_MASK);
            /* no need to reinit camera or microphone here */
            reinit_flags &= ~(DRIVER_CAMERA_MASK | DRIVER_MICROPHONE_MASK);

            RARCH_LOG("[Environ]: SET_SYSTEM_AV_INFO: %ux%u, Aspect: %.3f, FPS: %.2f, Sample rate: %.2f Hz.\n",
                  (*info)->geometry.base_width, (*info)->geometry.base_height,
                  (*info)->geometry.aspect_ratio,
                  (*info)->timing.fps,
                  (*info)->timing.sample_rate);

            memcpy(av_info, *info, sizeof(*av_info));

            command_event(CMD_EVENT_REINIT, &reinit_flags);

            if (no_video_reinit)
               video_driver_set_aspect_ratio();

            if (video_switch_refresh_rate)
               video_display_server_set_refresh_rate(refresh_rate);

            /* Cannot continue recording with different parameters.
             * Take the easiest route out and just restart
             * the recording. */
            if (rec_st->data)
            {
               const char *_msg = msg_hash_to_str(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT);
               runloop_msg_queue_push(_msg, strlen(_msg), 2, 180, false, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               if (rec_st->streaming_enable)
               {
                  command_event(CMD_EVENT_STREAMING_TOGGLE, NULL);
                  command_event(CMD_EVENT_STREAMING_TOGGLE, NULL);
               }
               else
               {
                  command_event(CMD_EVENT_RECORD_DEINIT, NULL);
                  command_event(CMD_EVENT_RECORD_INIT, NULL);
               }
            }

            /* Hide mouse cursor in fullscreen after
             * a RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO call. */
            if (video_fullscreen)
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, false);
            }

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

         RARCH_DBG("[Environ]: SET_SUBSYSTEM_INFO.\n");

         for (i = 0; info[i].ident; i++)
         {
            unsigned j;

            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_DBG("Special game type: %s\n  Ident: %s\n  ID: %u\n  Content:\n",
                  info[i].desc,
                  info[i].ident,
                  info[i].id
                  );

            for (j = 0; j < info[i].num_roms; j++)
            {
               RARCH_DBG("    %s (%s)\n",
                     info[i].roms[j].desc, info[i].roms[j].required ?
                     "required" : "optional");
            }
         }

         if (sys_info)
         {
            struct retro_subsystem_info *info_ptr = NULL;
            free(sys_info->subsystem.data);
            sys_info->subsystem.data = NULL;
            sys_info->subsystem.size = 0;

            info_ptr = (struct retro_subsystem_info*)
                  malloc(i * sizeof(*info_ptr));

            if (!info_ptr)
               return false;

            sys_info->subsystem.data = info_ptr;

            memcpy(sys_info->subsystem.data, info,
                  i * sizeof(*sys_info->subsystem.data));
            sys_info->subsystem.size                 = i;
            runloop_st->current_core.flags          |=
                  RETRO_CORE_FLAG_HAS_SET_SUBSYSTEMS;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
      {
         unsigned i, j;
         const struct retro_controller_info *info
                                 = (const struct retro_controller_info*)data;
         unsigned log_level      = settings->uints.libretro_log_level;

         RARCH_LOG("[Environ]: SET_CONTROLLER_INFO.\n");

         for (i = 0; info[i].types; i++)
         {
            if (log_level != RETRO_LOG_DEBUG)
               continue;

            RARCH_DBG("   %s %u:\n", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT), i + 1);

            for (j = 0; j < info[i].num_types; j++)
               if (info[i].types[j].desc)
                  RARCH_DBG("      \"%s\" (%u)\n",
                        info[i].types[j].desc,
                     info[i].types[j].id);
         }

         if (sys_info)
         {
            struct retro_controller_info *info_ptr = NULL;

            free(sys_info->ports.data);
            sys_info->ports.data = NULL;
            sys_info->ports.size = 0;

            if (!(info_ptr = (struct retro_controller_info*)
                     calloc(i, sizeof(*info_ptr))))
               return false;

            sys_info->ports.data = info_ptr;
            memcpy(sys_info->ports.data, info,
                  i * sizeof(*sys_info->ports.data));
            sys_info->ports.size = i;
         }
         break;
      }

      case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
      {
         if (sys_info)
         {
            unsigned i;
            const struct retro_memory_map *mmaps   =
                  (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;
            unsigned int log_level                 = settings->uints.libretro_log_level;

            RARCH_LOG("[Environ]: SET_MEMORY_MAPS.\n");

            free((void*)sys_info->mmaps.descriptors);
            sys_info->mmaps.descriptors     = 0;
            sys_info->mmaps.num_descriptors = 0;

            if (!(descriptors = (rarch_memory_descriptor_t*)calloc(mmaps->num_descriptors,
                  sizeof(*descriptors))))
               return false;

            sys_info->mmaps.descriptors     = descriptors;
            sys_info->mmaps.num_descriptors = mmaps->num_descriptors;

            for (i = 0; i < mmaps->num_descriptors; i++)
               sys_info->mmaps.descriptors[i].core = mmaps->descriptors[i];

            mmap_preprocess_descriptors(descriptors, mmaps->num_descriptors);

#ifdef HAVE_CHEEVOS
            rcheevos_refresh_memory();
#endif
#ifdef HAVE_CHEATS
            if (cheat_manager_state.memory_initialized)
            {
               cheat_manager_initialize_memory(NULL, 0, true);
               cheat_manager_apply_retro_cheats();
            }
#endif

            if (log_level != RETRO_LOG_DEBUG)
               break;

            if (sizeof(void *) == 8)
               RARCH_DBG("           ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_DBG("           ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

            for (i = 0; i < sys_info->mmaps.num_descriptors; i++)
            {
               char flags[7];
               const rarch_memory_descriptor_t *desc =
                  &sys_info->mmaps.descriptors[i];

               flags[0]    = 'M';
               if (     (desc->core.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
                  flags[1] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
                  flags[1] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
                  flags[1] = '2';
               else
                  flags[1] = '1';

               flags[2] = 'A';
               if (     (desc->core.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
                  flags[3] = '8';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
                  flags[3] = '4';
               else if ((desc->core.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
                  flags[3] = '2';
               else
                  flags[3] = '1';

               flags[4] = (desc->core.flags & RETRO_MEMDESC_BIGENDIAN) ? 'B' : 'b';
               flags[5] = (desc->core.flags & RETRO_MEMDESC_CONST)     ? 'C' : 'c';
               flags[6] = 0;

               RARCH_DBG("           %03u %s %p %08X %08X %08X %08X %08X %s\n",
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
         if (     (geom->base_width   != in_geom->base_width)
               || (geom->base_height  != in_geom->base_height)
               || (geom->aspect_ratio != in_geom->aspect_ratio))
         {
            bool video_frame_delay_auto = settings->bools.video_frame_delay_auto;

            geom->base_width            = in_geom->base_width;
            geom->base_height           = in_geom->base_height;
            geom->aspect_ratio          = in_geom->aspect_ratio;

            RARCH_LOG("[Environ]: SET_GEOMETRY: %ux%u, Aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            video_driver_set_aspect_ratio();

            /* Ignore frame delay target temporarily */
            if (video_frame_delay_auto)
               video_st->frame_delay_pause = true;

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
         video_driver_state_t *video_st = video_state_get_ptr();
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
            sys_info->supports_vfs                     = true;
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
         struct retro_led_interface *ledintf = (struct retro_led_interface *)data;

         if (ledintf)
            ledintf->set_led_state = led_driver_set_led;

         RARCH_LOG("[Environ]: GET_LED_INTERFACE.\n");
         break;
      }

      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         enum retro_av_enable_flags result = (enum retro_av_enable_flags)0;
         video_driver_state_t *video_st    = video_state_get_ptr();
         audio_driver_state_t *audio_st    = audio_state_get_ptr();

         if (    !(audio_st->flags & AUDIO_FLAG_SUSPENDED)
               && (audio_st->flags & AUDIO_FLAG_ACTIVE))
            result |= RETRO_AV_ENABLE_AUDIO;

         if (      (video_st->flags & VIDEO_FLAG_ACTIVE)
               && !(video_st->current_video->frame == video_null.frame))
            result |= RETRO_AV_ENABLE_VIDEO;

#ifdef HAVE_RUNAHEAD
         if (audio_st->flags & AUDIO_FLAG_HARD_DISABLE)
            result |= RETRO_AV_ENABLE_HARD_DISABLE_AUDIO;
#endif

#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_REPLAYING, NULL))
            result &= ~(RETRO_AV_ENABLE_VIDEO|RETRO_AV_ENABLE_AUDIO);
#endif

#if defined(HAVE_RUNAHEAD) || defined(HAVE_NETWORKING)
         /* Deprecated.
            Use RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT instead. */
         /* TODO/FIXME: Get rid of this ugly hack. */
         if (runloop_st->flags & RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE)
            result |= RETRO_AV_ENABLE_FAST_SAVESTATES;
#endif
         if (data)
         {
            enum retro_av_enable_flags* result_p = (enum retro_av_enable_flags*)data;
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
                     &&  secondary_core_ensure_exists(runloop_st, settings))
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
         video_driver_state_t *video_st = video_state_get_ptr();
         audio_driver_state_t *audio_st = audio_state_get_ptr();
         struct retro_throttle_state *throttle_state
                          = (struct retro_throttle_state *)data;

         bool menu_opened = false;
         bool core_paused = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
         bool no_audio    = ((audio_st->flags & AUDIO_FLAG_SUSPENDED)
                         || !(audio_st->flags & AUDIO_FLAG_ACTIVE));
         float core_fps   = (float)video_st->av_info.timing.fps;

#ifdef HAVE_REWIND
         if (runloop_st->rewind_st.flags
               & STATE_MGR_REWIND_ST_FLAG_FRAME_IS_REVERSED)
         {
            throttle_state->mode = RETRO_THROTTLE_REWINDING;
            throttle_state->rate = 0.0f;
            break; /* ignore vsync */
         }
#endif

#ifdef HAVE_MENU
         menu_opened = (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) ? true : false;
         if (menu_opened)
         {
            bool menu_pause_libretro = settings->bools.menu_pause_libretro;
#ifdef HAVE_NETWORKING
            core_paused = menu_pause_libretro
               && netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            core_paused = menu_pause_libretro;
#endif
         }
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
            float slowmotion_ratio = settings->floats.slowmotion_ratio;
            throttle_state->mode   = RETRO_THROTTLE_SLOW_MOTION;
            throttle_state->rate  /= (slowmotion_ratio > 0.0f
                  ? slowmotion_ratio : 1.0f);
         }

         /* VSync overrides the mode if the rate is limited by the display. */
         if (      menu_opened /* Menu currently always runs with vsync on. */
               || (settings->bools.video_vsync
               && (!(runloop_st->flags & RUNLOOP_FLAG_FORCE_NONBLOCK))
               && !(input_state_get_ptr()->flags & INP_FLAG_NONBLOCKING)))
         {
            float refresh_rate = video_driver_get_refresh_rate();
            if (refresh_rate == 0.0f)
               refresh_rate = settings->floats.video_refresh_rate;
            if (    (refresh_rate < throttle_state->rate)
                  || !throttle_state->rate)
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

            if (     p_content
                  && p_content->content_list
                  && p_content->content_list->game_info_ext)
               *game_info_ext = p_content->content_list->game_info_ext;
            else
            {
               RARCH_ERR("[Environ]: Failed to retrieve extended game info.\n");
               *game_info_ext = NULL;
               return false;
            }
         }
         break;
      case RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE:
#ifdef HAVE_MICROPHONE
         {
            struct retro_microphone_interface* microphone = (struct retro_microphone_interface *)data;
            microphone_driver_state_t *mic_st             = microphone_state_get_ptr();
            const microphone_driver_t *driver             = mic_st->driver;

            RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE.\n");

            if (!microphone)
               return false;
            /* User didn't provide a pointer for a response, what can we do? */

            if (microphone->interface_version != RETRO_MICROPHONE_INTERFACE_VERSION)
            {
               RARCH_ERR("[Environ]: Core requested unexpected microphone interface version %u, only %u is available\n",
                  microphone->interface_version,
                  RETRO_MICROPHONE_INTERFACE_VERSION);

               return false;
            }

            /* Initialize the interface... */
            memset(microphone, 0, sizeof(*microphone));

            /* If the null driver is active... */
            if (driver == &microphone_null)
            {
               RARCH_ERR("[Environ]: Cannot initialize microphone interface, active driver is null\n");
               return false;
            }

            /* If microphone support is off... */
            if (!settings->bools.microphone_enable)
            {
               RARCH_WARN("[Environ]: Will not initialize microphone interface, support is turned off\n");
               return false;
            }

            /* The core might request a mic before the mic driver is initialized,
             * so we still have to see if the frontend intends to init a mic driver. */
            if (!driver && string_is_equal(settings->arrays.microphone_driver, "null"))
            { /* If we're going to load the null driver... */
               RARCH_ERR("[Environ]: Cannot initialize microphone interface, configured driver is null\n");
               return false;
            }

            microphone->interface_version = RETRO_MICROPHONE_INTERFACE_VERSION;
            microphone->open_mic          = microphone_driver_open_mic;
            microphone->close_mic         = microphone_driver_close_mic;
            microphone->get_params        = microphone_driver_get_effective_params;
            microphone->set_mic_state     = microphone_driver_set_mic_state;
            microphone->get_mic_state     = microphone_driver_get_mic_state;
            microphone->read_mic          = microphone_driver_read;
         }
#else
         {
            struct retro_microphone_interface* microphone = (struct retro_microphone_interface *)data;
            RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE.\n");

            if (microphone)
               microphone->interface_version = 0;

            RARCH_ERR("[Environ]: Core requested microphone interface, but this build does not include support\n");

            return false;
         }
#endif
         break;
      case RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT:
         {
            struct retro_hw_render_context_negotiation_interface *iface =
                  (struct retro_hw_render_context_negotiation_interface*)data;

#ifdef HAVE_VULKAN
            if (iface->interface_type == RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
               iface->interface_version = RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION;
            else
#endif
            {
               iface->interface_version = 0;
            }
         }
         break;

      case RETRO_ENVIRONMENT_GET_JIT_CAPABLE:
         {
#if TARGET_OS_IPHONE
            *(bool*)data             = jit_available();
#else
            *(bool*)data             = true;
#endif
         }
         break;

      case RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE:
#ifdef HAVE_NETWORKING
         RARCH_LOG("[Environ]: RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE.\n");
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_SET_CORE_PACKET_INTERFACE, data))
         {
            RARCH_ERR("[Environ] RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE set too late\n");
            return false;
         }
         break;
#else
         return false;
#endif

      case RETRO_ENVIRONMENT_GET_DEVICE_POWER:
         {
            struct retro_device_power *status = (struct retro_device_power *)data;
            frontend_ctx_driver_t *frontend = frontend_get_ptr();
            int seconds = 0;
            int percent = 0;

            /* If the frontend driver is unavailable... */
            if (!frontend)
               return false;

            /* If the core just wants to query support for this environment call... */
            if (!status)
               return frontend->get_powerstate != NULL;

            /* If the frontend driver doesn't support reporting the powerstate... */
            if (frontend->get_powerstate == NULL)
               return false;

            switch (frontend->get_powerstate(&seconds, &percent))
            {
               case FRONTEND_POWERSTATE_ON_POWER_SOURCE: /* on battery power */
                  status->state = RETRO_POWERSTATE_DISCHARGING;
                  status->percent = (int8_t)percent;
                  status->seconds = seconds == 0 ? RETRO_POWERSTATE_NO_ESTIMATE : seconds;
                  break;
               case FRONTEND_POWERSTATE_CHARGING /* battery available, charging */:
                  status->state = RETRO_POWERSTATE_CHARGING;
                  status->percent = (int8_t)percent;
                  status->seconds = seconds == 0 ? RETRO_POWERSTATE_NO_ESTIMATE : seconds;
                  break;
               case FRONTEND_POWERSTATE_CHARGED: /* on AC, battery is full */
                  status->state = RETRO_POWERSTATE_CHARGED;
                  status->percent = (int8_t)percent;
                  status->seconds = RETRO_POWERSTATE_NO_ESTIMATE;
                  break;
               case FRONTEND_POWERSTATE_NO_SOURCE: /* on AC, no battery available */
                  status->state = RETRO_POWERSTATE_PLUGGED_IN;
                  status->percent = RETRO_POWERSTATE_NO_ESTIMATE;
                  status->seconds = RETRO_POWERSTATE_NO_ESTIMATE;
                  break;
               default:
                  /* The frontend driver supports power status queries,
                   * but it still gave us bad information for whatever reason. */
                  return false;
                  break;
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
      struct retro_system_info *sysinfo,
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

   memcpy(sysinfo, &dummy_info, sizeof(*sysinfo));

   runloop_st->current_library_name[0]     = '\0';
   runloop_st->current_library_version[0]  = '\0';
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

   sysinfo->library_name     = runloop_st->current_library_name;
   sysinfo->library_version  = runloop_st->current_library_version;
   sysinfo->valid_extensions = runloop_st->current_valid_extensions;

#ifdef HAVE_DYNAMIC
   dylib_close(lib);
#endif
   return true;
}

bool runloop_init_libretro_symbols(
      void *data,
      enum rarch_core_type type,
      struct retro_core_t *current_core,
      const char *lib_path,
      void *_lib_handle_p)
{
#ifdef HAVE_DYNAMIC
   /* the library handle for use with the SYMBOL macro */
   dylib_t lib_handle_local;
   runloop_state_t *runloop_st = (runloop_state_t*)data;
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
                  const char *_msg = msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE);
                  RARCH_ERR("%s: \"%s\"\nError(s): %s\n", _msg, path, dylib_error());
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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

static void runloop_frame_time_free(runloop_state_t *runloop_st)
{
   memset(&runloop_st->frame_time, 0,
         sizeof(struct retro_frame_time_callback));
   runloop_st->frame_time_last    = 0;
   runloop_st->max_frames         = 0;
}

static void runloop_audio_buffer_status_free(runloop_state_t *runloop_st)
{
   memset(&runloop_st->audio_buffer_status, 0,
         sizeof(struct retro_audio_buffer_status_callback));
   runloop_st->audio_latency = 0;
}

static void runloop_fastmotion_override_free(runloop_state_t *runloop_st,
      float fastforward_ratio)
{
   video_driver_state_t
      *video_st            = video_state_get_ptr();
   bool reset_frame_limit  = runloop_st->fastmotion_override.current.fastforward
         && (runloop_st->fastmotion_override.current.ratio >= 0.0f)
         && (runloop_st->fastmotion_override.current.ratio != fastforward_ratio);

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
      runloop_set_frame_limit(&video_st->av_info, fastforward_ratio);
}

void runloop_state_free(runloop_state_t *runloop_st)
{
   runloop_frame_time_free(runloop_st);
   runloop_audio_buffer_status_free(runloop_st);
   input_game_focus_free();
   runloop_fastmotion_override_free(runloop_st, config_get_ptr()->floats.fastforward_ratio);

   /* Only a single core options callback is used at present */
   runloop_st->core_options_callback.update_display = NULL;

   runloop_st->video_swap_interval_auto             = 1;
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
   runloop_state_t *runloop_st      = &runloop_state;
   input_driver_state_t *input_st   = input_state_get_ptr();
   audio_driver_state_t *audio_st   = audio_state_get_ptr();
   camera_driver_state_t *camera_st = camera_state_get_ptr();
   location_driver_state_t *loc_st  = location_state_get_ptr();
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
            (runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE) ? true : false,
            path_get(RARCH_PATH_CORE_OPTIONS),
            runloop_st->core_options);
      runloop_st->flags                    &=
                  ~(RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE
                  | RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE);
      runloop_st->core_options              = NULL;
   }
   runloop_system_info_free();
   audio_st->callback.callback              = NULL;
   audio_st->callback.set_state             = NULL;
   runloop_state_free(runloop_st);
   camera_st->active                        = false;
   loc_st->active                           = false;

   /* Core has finished utilising the input driver;
    * reset 'analog input requested' flags */
   memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));

   /* Performance counters no longer valid. */
   runloop_st->perf_ptr_libretro  = 0;
   memset(runloop_st->perf_counters_libretro, 0,
         sizeof(runloop_st->perf_counters_libretro));
}


static retro_time_t runloop_core_runtime_tick(
      runloop_state_t *runloop_st,
      float slowmotion_ratio,
      retro_time_t current_time)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   retro_time_t frame_time              =
      (1.0 / video_st->av_info.timing.fps) * 1000000;
   bool runloop_slowmotion              = (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION) ? true : false;
   bool runloop_fastmotion              = (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION) ? true : false;

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
   runloop_state_t *runloop_st    = &runloop_state;
   video_driver_state_t *video_st = video_state_get_ptr();

   video_driver_free_hw_context();

   video_st->frame_cache_data     = NULL;

   if ((runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED))
   {
      RARCH_LOG("[Core]: Unloading game..\n");
      runloop_st->current_core.retro_unload_game();
      runloop_st->core_poll_type_override  = POLL_TYPE_OVERRIDE_DONTCARE;
      runloop_st->current_core.flags      &= ~RETRO_CORE_FLAG_GAME_LOADED;
   }

   audio_driver_stop();

#ifdef HAVE_MICROPHONE
   microphone_driver_stop();
#endif

   return true;
}

static void runloop_apply_fastmotion_override(runloop_state_t *runloop_st,
      bool frame_time_counter_reset_after_fastforwarding,
      float fastforward_ratio_default,
      bool audio_fastforward_mute)
{
   float fastforward_ratio_current;
   video_driver_state_t *video_st                     = video_state_get_ptr();
   audio_driver_state_t *audio_st                     = audio_state_get_ptr();
   float fastforward_ratio_last                       =
                     (runloop_st->fastmotion_override.current.fastforward
                  && (runloop_st->fastmotion_override.current.ratio >= 0.0f)) ?
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

      if (audio_fastforward_mute && (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
         audio_st->flags |=  AUDIO_FLAG_MUTED;
      else
         audio_st->flags &= ~AUDIO_FLAG_MUTED;

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
   fastforward_ratio_current = (runloop_st->fastmotion_override.current.fastforward
         && (runloop_st->fastmotion_override.current.ratio >= 0.0f)) ?
               runloop_st->fastmotion_override.current.ratio :
                     fastforward_ratio_default;

   if (fastforward_ratio_current != fastforward_ratio_last)
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

   video_st->frame_cache_data  = NULL;

   if (runloop_st->current_core.flags & RETRO_CORE_FLAG_INITED)
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
            settings->bools.frame_time_counter_reset_after_fastforwarding,
            settings->floats.fastforward_ratio,
            settings->bools.audio_fastforward_mute
            );
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
      input_remapping_restore_global_config(true, false);

   RARCH_LOG("[Core]: Unloading core symbols..\n");
   uninit_libretro_symbols(&runloop_st->current_core);
   runloop_st->current_core.flags &= ~RETRO_CORE_FLAG_SYMBOLS_INITED;

   /* Restore original refresh rate, if it has been changed
    * automatically in SET_SYSTEM_AV_INFO */
   if (video_st->video_refresh_rate_original)
      video_display_server_restore_refresh_rate();

   /* Recalibrate frame delay target */
   if (settings->bools.video_frame_delay_auto)
      video_st->frame_delay_target = 0;

   driver_uninit(DRIVERS_CMD_ALL, 0);

#ifdef HAVE_CONFIGFILE
   if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
   {
      /* Reload the original config */
      config_unload_override();
   }
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   runloop_st->runtime_shader_preset_path[0] = '\0';
#endif
#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_SET_CORE_PACKET_INTERFACE, NULL);
#endif
}

static bool runloop_path_init_subsystem(runloop_state_t *runloop_st)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   rarch_system_info_t           *sys_info = &runloop_st->system;
   bool subsystem_path_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   const char                *savefile_dir = runloop_st->savefile_dir;

   if (!sys_info || subsystem_path_empty)
      return false;

   /* For subsystems, we know exactly which RAM types are supported. */
   /* We'll handle this error gracefully later. */
   if ((info = libretro_find_subsystem_info(
         sys_info->subsystem.data,
         sys_info->subsystem.size,
         path_get(RARCH_PATH_SUBSYSTEM))))
   {
      unsigned num_content = MIN(info->num_roms,
            subsystem_path_empty
            ? 0
            : (unsigned)runloop_st->subsystem_fullpaths->size);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            char ext[32];
            union string_list_elem_attr attr;
            char savename[NAME_MAX_LENGTH];
            char path[PATH_MAX_LENGTH];
            size_t _len = 0;
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];
            ext[  _len]  = '.';
            ext[++_len]  = '\0';
            strlcpy(ext + _len, mem->extension, sizeof(ext) - _len);
            fill_pathname(savename,
                  runloop_st->subsystem_fullpaths->elems[i].data, "",
                  sizeof(savename));

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
      fill_pathname(runloop_st->name.savefile,
            runloop_st->runtime_content_path_basename,
            ".srm",
            sizeof(runloop_st->name.savefile));

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

static void runloop_path_init_savefile_internal(runloop_state_t *runloop_st)
{
   path_deinit_savefile();
   path_init_savefile_new();
   if (!runloop_path_init_subsystem(runloop_st))
      path_init_savefile_rtc(runloop_st->name.savefile);
}

static void runloop_path_init_savefile(runloop_state_t *runloop_st)
{
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

static bool event_init_content(
      runloop_state_t *runloop_st,
      settings_t *settings,
      input_driver_state_t *input_st)
{
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

   if (flags & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
      runloop_path_init_savefile_internal(runloop_st);

   runloop_path_fill_names();

   if (!content_init())
      return false;

   command_event_set_savestate_auto_index(settings);
   command_event_set_replay_auto_index(settings);

   runloop_path_init_savefile(runloop_st);

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
   if (     !cheevos_enable
         || !cheevos_hardcore_mode_enable)
#endif
   {
#ifdef HAVE_BSV_MOVIE
     /* ignore entry state if we're doing bsv playback (we do want it
        for bsv recording though) */
     if (!(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_PLAYBACK))
#endif
      {
         if (      runloop_st->entry_state_slot > -1
               && !command_event_load_entry_state(settings))
         {
           /* loading the state failed, reset entry slot */
            runloop_st->entry_state_slot = -1;
         }
      }
#ifdef HAVE_BSV_MOVIE
     /* ignore autoload state if we're doing bsv playback or recording */
     if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_START_RECORDING | BSV_FLAG_MOVIE_START_PLAYBACK)))
#endif
      {
        if (runloop_st->entry_state_slot < 0 && settings->bools.savestate_auto_load)
          command_event_load_auto_state();
      }
   }

#ifdef HAVE_BSV_MOVIE
   movie_stop(input_st);
   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_RECORDING)
   {
     configuration_set_uint(settings, settings->uints.rewind_granularity, 1);
#ifndef HAVE_THREADS
     /* Hack: the regular scheduler doesn't do the right thing here at
        least in emscripten builds.  I would expect that the check in
        task_movie.c:343 should defer recording until the movie task
        is done, but maybe that task isn't enqueued again yet when the
        movie-record task is checked?  Or the finder call in
        content_load_state_in_progress is not correct?  Either way,
        the load happens after the recording starts rather than the
        right way around.
     */
     task_queue_wait(NULL,NULL);
#endif
     movie_start_record(input_st, input_st->bsv_movie_state.movie_start_path);
   }
   else if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_PLAYBACK)
   {
     configuration_set_uint(settings, settings->uints.rewind_granularity, 1);
     movie_start_playback(input_st, input_st->bsv_movie_state.movie_start_path);
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

void runloop_set_frame_limit(
      const struct retro_system_av_info *av_info,
      float fastforward_ratio)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (fastforward_ratio < 1.0f)
      runloop_st->frame_limit_minimum_time = 0.0f;
   else
      runloop_st->frame_limit_minimum_time = (retro_time_t)
         roundf(1000000.0f /
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
      unsigned black_frame_insertion,
      unsigned shader_subframes,
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
    *   set swap interval to 1
    * > If BFI is active set swap interval to 1
    * > If Shader Subframes active, set swap interval to 1 */
   if (   (vrr_runloop_enable)
       || (core_hz    > timing_hz)
       || (core_hz   <= 0.0f)
       || (timing_hz <= 0.0f)
       || (black_frame_insertion)
       || (shader_subframes > 1))
   {
      runloop_st->video_swap_interval_auto = 1;
      return;
   }

   /* Check whether display refresh rate is an integer
    * multiple of core fps (within timing skew tolerance) */
   swap_ratio   = timing_hz / core_hz;
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

/*
   Returns rotation requested by the core regardless of if it has been
   applied with the final video rotation
*/
unsigned int retroarch_get_core_requested_rotation(void)
{
   return runloop_state.system.core_requested_rotation;
}

/*
   Returns final rotation including both user chosen video rotation
   and core requested rotation if allowed by video_allow_rotate
*/
unsigned int retroarch_get_rotation(void)
{
   return config_get_ptr()->uints.video_rotation + runloop_state.system.rotation;
}

static void retro_run_null(void) { } /* Stub function callback impl. */

static bool core_verify_api_version(runloop_state_t *runloop_st)
{
   unsigned api_version        = runloop_st->current_core.retro_api_version();
   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("[Core]: %s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
   RARCH_LOG("[Core]: %s: %u, %s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version,
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION
         );
   return true;
}

static int16_t core_input_state_poll_late(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   runloop_state_t     *runloop_st       = &runloop_state;
   if (!(runloop_st->current_core.flags & RETRO_CORE_FLAG_INPUT_POLLED))
      input_driver_poll();
   runloop_st->current_core.flags       |= RETRO_CORE_FLAG_INPUT_POLLED;

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
static void core_init_libretro_cbs(runloop_state_t *runloop_st,
      struct retro_callbacks *cbs)
{
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   runloop_st->current_core.retro_set_video_refresh(video_driver_frame);
   runloop_st->current_core.retro_set_audio_sample(audio_driver_sample);
   runloop_st->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   runloop_st->current_core.retro_set_input_state(state_cb);
   runloop_st->current_core.retro_set_input_poll(core_input_state_poll_maybe);

   runloop_st->input_poll_callback_original    = core_input_state_poll_maybe;

   core_set_default_callbacks(cbs);

#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      core_set_netplay_callbacks();
#endif
}


static bool runloop_event_load_core(runloop_state_t *runloop_st,
      unsigned poll_type_behavior)
{
   video_driver_state_t *video_st     = video_state_get_ptr();
   runloop_st->current_core.poll_type = poll_type_behavior;

   if (!core_verify_api_version(runloop_st))
      return false;
   core_init_libretro_cbs(runloop_st, &runloop_st->retro_ctx);

   runloop_st->current_core.retro_get_system_av_info(&video_st->av_info);

   RARCH_LOG("[Core]: Geometry: %ux%u, Aspect: %.3f, FPS: %.2f, Sample rate: %.2f Hz.\n",
         video_st->av_info.geometry.base_width, video_st->av_info.geometry.base_height,
         video_st->av_info.geometry.aspect_ratio,
         video_st->av_info.timing.fps,
         video_st->av_info.timing.sample_rate);

   return true;
}

bool runloop_event_init_core(
      settings_t *settings,
      void *input_data,
      enum rarch_core_type type,
      const char *old_savefile_dir,
      const char *old_savestate_dir)
{
   size_t _len;
   runloop_state_t *runloop_st     = &runloop_state;
   input_driver_state_t *input_st  = (input_driver_state_t*)input_data;
   video_driver_state_t *video_st  = video_state_get_ptr();
#ifdef HAVE_CONFIGFILE
   bool auto_overrides_enable      = settings->bools.auto_overrides_enable;
   bool auto_remaps_enable         = false;
   const char *dir_input_remapping = NULL;
#endif
   bool initial_disk_change_enable = true;
   bool show_set_initial_disk_msg  = false;
   unsigned poll_type_behavior     = 0;
   float fastforward_ratio         = 0.0f;
   rarch_system_info_t *sys_info   = &runloop_st->system;

#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
   {
      /* We need this in order for core_info_current_supports_netplay
         to work correctly at init_netplay,
         called later at event_init_content. */
      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
      command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
   }
#endif

   /* Load symbols */
   if (!runloop_init_libretro_symbols(runloop_st,
            type, &runloop_st->current_core, NULL, NULL))
      return false;
#ifdef HAVE_RUNAHEAD
   /* Remember last core type created, so creating a
    * secondary core will know what core type to use. */
   runloop_st->last_core_type              = type;
#endif
   if (!runloop_st->current_core.retro_run)
      runloop_st->current_core.retro_run   = retro_run_null;
   runloop_st->current_core.flags         |= RETRO_CORE_FLAG_SYMBOLS_INITED;
   runloop_st->current_core.retro_get_system_info(&sys_info->info);

   if (!sys_info->info.library_name)
      sys_info->info.library_name = msg_hash_to_str(MSG_UNKNOWN);
   if (!sys_info->info.library_version)
      sys_info->info.library_version = "v0";

   _len = strlcpy(
         video_st->title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         sizeof(video_st->title_buf));

   if (!string_is_empty(sys_info->info.library_name))
   {
      video_st->title_buf[  _len] = ' ';
      video_st->title_buf[++_len] = '\0';
      _len += strlcpy(video_st->title_buf + _len,
            sys_info->info.library_name,
            sizeof(video_st->title_buf)   - _len);
   }

   if (!string_is_empty(sys_info->info.library_version))
   {
      video_st->title_buf[  _len] = ' ';
      video_st->title_buf[++_len] = '\0';
      strlcpy(video_st->title_buf        + _len,
            sys_info->info.library_version,
            sizeof(video_st->title_buf)  - _len);
   }

   if (!sys_info->info.valid_extensions)
   strlcpy(sys_info->valid_extensions, DEFAULT_EXT,
         sizeof(sys_info->valid_extensions));

#ifdef HAVE_CONFIGFILE
   if (auto_overrides_enable)
      config_load_override(&runloop_st->system);
#endif

   /* Cannot access these settings-related parameters
    * until *after* config overrides have been loaded */
#ifdef HAVE_CONFIGFILE
   auto_remaps_enable         = settings->bools.auto_remaps_enable;
   dir_input_remapping        = settings->paths.directory_input_remapping;
#endif
   initial_disk_change_enable = settings->bools.initial_disk_change_enable;
   show_set_initial_disk_msg  = settings->bools.notification_show_set_initial_disk;
   poll_type_behavior         = settings->uints.input_poll_type_behavior;
   fastforward_ratio          = runloop_get_fastforward_ratio(
         settings, &runloop_st->fastmotion_override.current);

#ifdef HAVE_CHEEVOS
   /* Assume the core supports achievements unless it tells us otherwise */
   rcheevos_set_support_cheevos(true);
#endif

   /* Load auto-shaders on the next occasion */
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   video_st->flags |= VIDEO_FLAG_SHADER_PRESETS_NEED_RELOAD;
   runloop_st->shader_delay_timer.timer_begin = false; /* not initialized */
   runloop_st->shader_delay_timer.timer_end   = false; /* not expired */
#endif

   /* Reset video format to libretro's default */
   video_st->pix_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

   /* Set save redirection paths */
   runloop_path_set_redirect(settings, old_savefile_dir, old_savestate_dir);

   /* Set core environment */
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

   video_st->frame_cache_data              = NULL;

   runloop_st->current_core.retro_init();
   runloop_st->current_core.flags         |= RETRO_CORE_FLAG_INITED;

   /* Attempt to set initial disk index */
   if (initial_disk_change_enable)
      disk_control_set_initial_index(
         &sys_info->disk_control,
         path_get(RARCH_PATH_CONTENT),
         runloop_st->savefile_dir);

   if (!event_init_content(runloop_st, settings, input_st))
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
      return false;
   }

   /* Verify that initial disk index was set correctly */
   disk_control_verify_initial_index(&sys_info->disk_control,
         show_set_initial_disk_msg, initial_disk_change_enable);

   if (!runloop_event_load_core(runloop_st, poll_type_behavior))
      return false;

   runloop_set_frame_limit(&video_st->av_info, fastforward_ratio);
   runloop_st->frame_limit_last_time    = cpu_features_get_time_usec();

   runloop_runtime_log_init(runloop_st);
   return true;
}

void runloop_pause_checks(void)
{
#ifdef HAVE_PRESENCE
   presence_userdata_t userdata;
#endif
   video_driver_state_t *video_st = video_state_get_ptr();
   settings_t *settings           = config_get_ptr();
   float video_refresh_rate       = settings->floats.video_refresh_rate;
   float fastforward_ratio        = settings->floats.fastforward_ratio;
   runloop_state_t *runloop_st    = &runloop_state;
   bool is_paused                 = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
   bool is_idle                   = (runloop_st->flags & RUNLOOP_FLAG_IDLE)   ? true : false;
#if defined(HAVE_GFX_WIDGETS)
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
      {
         const char *_msg = msg_hash_to_str(MSG_PAUSED);
         runloop_msg_queue_push(_msg, strlen(_msg), 1, 1, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }

      if (!is_idle)
         video_driver_cached_frame();

      midi_driver_set_all_sounds_off();

#ifdef HAVE_PRESENCE
      userdata.status = PRESENCE_GAME_PAUSED;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
#endif

#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_MENU);
#endif

      /* Limit paused frames to video refresh. */
      runloop_st->frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f /
            ((video_st->video_refresh_rate_original)
               ? video_st->video_refresh_rate_original
               : video_refresh_rate));
   }
   else
   {
#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_CORE);
#endif

      /* Restore frame limit. */
      runloop_set_frame_limit(&video_st->av_info, fastforward_ratio);
   }

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (p_dispwidget->ai_service_overlay_state == 1)
      gfx_widgets_ai_service_overlay_unload();
#endif

   /* Signal/reset paused rewind to take the initial step */
   runloop_st->run_frames_and_pause = -1;

   /* Ignore frame delay target temporarily */
   video_st->frame_delay_pause      = true;
}

struct string_list *path_get_subsystem_list(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->subsystem_fullpaths;
}

void runloop_path_fill_names(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
#ifdef HAVE_BSV_MOVIE
   input_driver_state_t *input_st = input_state_get_ptr();
#endif

   runloop_path_init_savefile_internal(runloop_st);

#ifdef HAVE_BSV_MOVIE
   strlcpy(input_st->bsv_movie_state.movie_auto_path,
         runloop_st->name.replay,
         sizeof(input_st->bsv_movie_state.movie_auto_path));
#endif

   if (string_is_empty(runloop_st->runtime_content_path_basename))
      return;

   if (string_is_empty(runloop_st->name.ups))
    {
      size_t _len = strlcpy(runloop_st->name.ups,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.ups));
      strlcpy(runloop_st->name.ups       + _len,
            ".ups",
            sizeof(runloop_st->name.ups) - _len);
   }

   if (string_is_empty(runloop_st->name.bps))
   {
      size_t _len = strlcpy(runloop_st->name.bps,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.bps));
      strlcpy(runloop_st->name.bps       + _len,
            ".bps",
            sizeof(runloop_st->name.bps) - _len);
   }

   if (string_is_empty(runloop_st->name.ips))
   {
      size_t _len = strlcpy(runloop_st->name.ips,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.ips));
      strlcpy(runloop_st->name.ips       + _len,
            ".ips",
            sizeof(runloop_st->name.ips) - _len);
   }

   if (string_is_empty(runloop_st->name.xdelta))
   {
      size_t _len = strlcpy(runloop_st->name.xdelta,
            runloop_st->runtime_content_path_basename,
            sizeof(runloop_st->name.xdelta));
      strlcpy(runloop_st->name.xdelta       + _len,
            ".xdelta",
            sizeof(runloop_st->name.xdelta) - _len);
   }
}


/* Creates folder and core options stub file for subsequent runs */
bool core_options_create_override(bool game_specific)
{
   char options_path[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st = &runloop_state;
   const char *_msg            = NULL;
   config_file_t *conf         = NULL;

   options_path[0]             = '\0';

   if (game_specific)
   {
      /* Get options file path (game-specific) */
      if (!validate_per_core_options(options_path,
               sizeof(options_path),
               true,
               runloop_st->system.info.library_name,
               path_basename_nocompression(path_get(RARCH_PATH_BASENAME))))
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

   RARCH_LOG("[Core]: Core options file created successfully: \"%s\".\n", options_path);
   _msg = msg_hash_to_str(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY);
   runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   path_set(RARCH_PATH_CORE_OPTIONS, options_path);
   if (game_specific)
   {
      runloop_st->flags |=  RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
      runloop_st->flags &= ~RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
   }
   else
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE;
      runloop_st->flags |=  RUNLOOP_FLAG_FOLDER_OPTIONS_ACTIVE;
   }

   config_file_free(conf);
   return true;

error:
   _msg = msg_hash_to_str(MSG_ERROR_SAVING_CORE_OPTIONS_FILE);
   runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

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
   if (          !coreopts
         || (    (!(runloop_st->flags & RUNLOOP_FLAG_GAME_OPTIONS_ACTIVE))
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
   if (   game_specific
       && validate_folder_options(
          new_options_path,
          sizeof(new_options_path), false)
       && path_is_valid(new_options_path))
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
   if (   folder_options_active
       || path_is_valid(new_options_path))
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


   {
      const char *_msg = msg_hash_to_str(MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

   if (conf)
      config_file_free(conf);

   return true;

error:
   {
      const char *_msg = msg_hash_to_str(MSG_ERROR_REMOVING_CORE_OPTIONS_FILE);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }

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

   {
      const char *_msg = msg_hash_to_str(MSG_CORE_OPTIONS_RESET);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

void core_options_flush(void)
{
   size_t _len;
   char msg[128];
   runloop_state_t *runloop_st     = &runloop_state;
   core_option_manager_t *coreopts = runloop_st->core_options;
   const char *path_core_options   = path_get(RARCH_PATH_CORE_OPTIONS);
   const char *core_options_file   = NULL;
   bool success                    = false;

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
            runloop_st->core_options->conf->flags |= CONF_FILE_FLG_MODIFIED;

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
      _len = strlcpy(msg, msg_hash_to_str(MSG_CORE_OPTIONS_FLUSHED),
            sizeof(msg));
      RARCH_LOG(
            "[Core]: Saved core options to \"%s\".\n",
            path_core_options ? path_core_options : "UNKNOWN");
   }
   else
   {
      /* Log result */
      _len = strlcpy(msg, msg_hash_to_str(MSG_CORE_OPTIONS_FLUSH_FAILED),
            sizeof(msg));
      RARCH_LOG(
            "[Core]: Failed to save core options to \"%s\".\n",
            path_core_options ? path_core_options : "UNKNOWN");
   }

   _len += snprintf(msg + _len, sizeof(msg) - _len, " \"%s\"",
         core_options_file);

   runloop_msg_queue_push(msg, _len, 1, 100, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void runloop_msg_queue_push(
      const char *msg,
      size_t len,
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
            len,
            roundf((float)duration / 60.0f * 1000.0f),
            title,
            icon,
            category,
            prio,
            flush,
#ifdef HAVE_MENU
            (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE) ? true : false
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
   bool runloop_idle             = (runloop_st->flags & RUNLOOP_FLAG_IDLE) ? true : false;
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

static void runloop_pause_toggle(
      bool *runloop_paused_hotkey,
      bool pause_pressed, bool old_pause_pressed,
      bool focused, bool old_focus)
{
   runloop_state_t *runloop_st         = &runloop_state;

   if (focused)
   {
      if (pause_pressed && !old_pause_pressed)
      {
         /* Keep track of hotkey triggered pause to
          * distinguish it from menu triggered pause */
         *runloop_paused_hotkey = !(runloop_st->flags & RUNLOOP_FLAG_PAUSED);
         command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
      }
      else if (!old_focus)
         command_event(CMD_EVENT_UNPAUSE, NULL);
   }
   else if (old_focus)
      command_event(CMD_EVENT_PAUSE, NULL);
}

static INLINE bool runloop_is_libretro_running(runloop_state_t* runloop_st, bool menu_pause_libretro)
{
   return ((runloop_st->flags & RUNLOOP_FLAG_IS_INITED))
      &&  !(runloop_st->flags & RUNLOOP_FLAG_PAUSED)
      &&  (!menu_pause_libretro
      &&    runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING);
}

static enum runloop_state_enum runloop_check_state(
      input_driver_state_t *input_st,
      audio_driver_state_t *audio_st,
      video_driver_state_t *video_st,
      uico_driver_state_t   *uico_st,
      bool error_on_init,
      settings_t *settings,
      retro_time_t current_time,
      bool netplay_allow_pause,
      bool netplay_allow_timeskip)
{
   input_bits_t current_bits;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
   gfx_display_t            *p_disp    = disp_get_ptr();
   runloop_state_t *runloop_st         = &runloop_state;
   static bool old_focus               = true;
   static bool runloop_paused_hotkey   = false;
   struct retro_callbacks *cbs         = &runloop_st->retro_ctx;
   bool is_focused                     = false;
   bool is_alive                       = false;
   uint64_t frame_count                = 0;
   bool focused                        = true;
   bool rarch_is_initialized           = (runloop_st->flags & RUNLOOP_FLAG_IS_INITED) ? true : false;
   bool runloop_paused                 = (runloop_st->flags & RUNLOOP_FLAG_PAUSED)    ? true : false;
   bool pause_nonactive                = settings->bools.pause_nonactive;
   unsigned quit_gamepad_combo         = settings->uints.input_quit_gamepad_combo;
   bool menu_pause_libretro            = settings->bools.menu_pause_libretro;
#ifdef HAVE_MENU
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
   bool menu_driver_binding_state      = (menu_st->flags & MENU_ST_FLAG_IS_BINDING) ? true : false;
   bool menu_was_alive                 = (menu_st->flags & MENU_ST_FLAG_ALIVE)      ? true : false;
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
         ((menu_toggle_gamepad_combo != INPUT_COMBO_NONE)
          && input_driver_button_combo(
             menu_toggle_gamepad_combo,
             current_time,
             &last_input)))
      BIT256_SET(current_bits, RARCH_MENU_TOGGLE);

   if (menu_st->input_driver_flushing_input > 0)
   {
      bool input_active = bits_any_set(current_bits.data, ARRAY_SIZE(current_bits.data));
      /* Don't count 'enable_hotkey' as active input */
      if (      input_active
            &&  BIT256_GET(current_bits, RARCH_ENABLE_HOTKEY)
            && !BIT256_GET(current_bits, RARCH_MENU_TOGGLE))
         input_active = false;

      if (!input_active)
         menu_st->input_driver_flushing_input--;

      if (input_active || (menu_st->input_driver_flushing_input > 0))
      {
         BIT256_CLEAR_ALL(current_bits);
         if (      runloop_paused
               && !runloop_paused_hotkey
               &&  menu_pause_libretro)
            BIT256_SET(current_bits, RARCH_PAUSE_TOGGLE);
         else if (runloop_paused_hotkey)
         {
            /* Restore pause if pause is triggered with both hotkey and menu,
             * and restore cached video frame to continue properly to
             * paused state from non-paused menu */
            if (menu_pause_libretro)
               command_event(CMD_EVENT_PAUSE, NULL);
            else
               video_driver_cached_frame();
         }
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

   /* Check fullscreen hotkey */
   HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY, CMD_EVENT_FULLSCREEN_TOGGLE, true, NULL);

   /* Check mouse grab hotkey */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE, CMD_EVENT_GRAB_MOUSE_TOGGLE, true, NULL);

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
      static unsigned last_width                     = 0;
      static unsigned last_height                    = 0;
      unsigned video_driver_width                    = video_st->width;
      unsigned video_driver_height                   = video_st->height;
      bool check_next_rotation                       = true;
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
               input_overlay_unload();
            else
               input_overlay_init();

            last_controller_connected = controller_connected;
         }
      }

      /* Check next overlay hotkey */
      HOTKEY_CHECK(RARCH_OVERLAY_NEXT, CMD_EVENT_OVERLAY_NEXT, true, &check_next_rotation);

      /* Check whether video aspect has changed */
      if (   (video_driver_width  != last_width)
          || (video_driver_height != last_height))
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

      /* Check OSK hotkey */
      HOTKEY_CHECK(RARCH_OSK, CMD_EVENT_OSK_TOGGLE, true, NULL);
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
      if (   (video_driver_width  != last_width)
          || (video_driver_height != last_height))
      {
         /* Update set aspect ratio so the full matches the current video width & height */
         command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

         last_width  = video_driver_width;
         last_height = video_driver_height;
      }
   }

   /* Check quit hotkey */
   {
      bool trig_quit_key, quit_press_twice;
      static bool quit_key     = false;
      static bool old_quit_key = false;
      static bool runloop_exec = false;
      quit_key                 = BIT256_GET(
            current_bits, RARCH_QUIT_KEY);
      trig_quit_key            = quit_key && !old_quit_key;
      /* Check for quit gamepad combo */
      if (    !trig_quit_key
          && ((quit_gamepad_combo != INPUT_COMBO_NONE)
          && input_driver_button_combo(
             quit_gamepad_combo,
             current_time,
             &current_bits)))
        trig_quit_key = true;
      old_quit_key             = quit_key;
      quit_press_twice         = settings->bools.quit_press_twice;

      /* Check double press if enabled */
      if (     trig_quit_key
            && quit_press_twice)
      {
         static retro_time_t quit_key_time   = 0;
         retro_time_t cur_time               = current_time;
         trig_quit_key                       = (cur_time - quit_key_time < QUIT_DELAY_USEC);
         quit_key_time                       = cur_time;

         if (!trig_quit_key)
         {
            const char *_msg = msg_hash_to_str(MSG_PRESS_AGAIN_TO_QUIT);
            float target_hz  = 0.0;

            runloop_environment_cb(
                  RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &target_hz);

            runloop_msg_queue_push(_msg, strlen(_msg), 1, QUIT_DELAY_USEC * target_hz / 1000000,
                  true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }

      if (RUNLOOP_TIME_TO_EXIT(trig_quit_key))
      {
         bool quit_runloop           = false;
#ifdef HAVE_SCREENSHOTS
         unsigned runloop_max_frames = runloop_st->max_frames;

         if (     (runloop_max_frames != 0)
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
                     screenshot_path,
                     false,
                     video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID),
                     fullpath,
                     false))
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

#ifdef HAVE_MENU
   /* Check menu hotkey */
   {
      static bool old_pressed = false;
      char *menu_driver       = settings->arrays.menu_driver;
      bool pressed            = BIT256_GET(current_bits, RARCH_MENU_TOGGLE)
            && !string_is_equal(menu_driver, "null");
      bool core_type_is_dummy = runloop_st->current_core_type == CORE_TYPE_DUMMY;

      if (    (pressed && !old_pressed)
            || core_type_is_dummy)
      {
         if (menu_st->flags & MENU_ST_FLAG_ALIVE)
         {
            if (rarch_is_initialized && !core_type_is_dummy)
               retroarch_menu_running_finished(false);
         }
         else
            retroarch_menu_running();
      }

      old_pressed             = pressed;
   }
#endif

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
      bool rarch_force_fullscreen = (video_st->flags &
         VIDEO_FLAG_FORCE_FULLSCREEN) ? true : false;
      bool video_is_fullscreen    = settings->bools.video_fullscreen
                                 || rarch_force_fullscreen;

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
   if (menu_st->flags & MENU_ST_FLAG_ALIVE)
   {
      enum menu_action action;
      static input_bits_t old_input = {{0}};
      static enum menu_action
         old_action                 = MENU_ACTION_CANCEL;
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
      if (!netplay_allow_pause)
         focused = true;
      else
#endif
      {
         if (pause_nonactive)
            focused = is_focused && (!(uico_st->flags & UICO_ST_FLAG_IS_ON_FOREGROUND));
         else
            focused = (!(uico_st->flags & UICO_ST_FLAG_IS_ON_FOREGROUND));
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
            if (     menu_st->prev_action == action
                  && menu_st->noop_press_time < 200000) /* 250ms */
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
      if (     (screensaver_timeout > 0)
            && (menu_st->flags   & MENU_ST_FLAG_SCREENSAVER_SUPPORTED)
            && (!(menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE))
            && ((menu_st->current_time_us - menu_st->input_last_time_us)
             > ((retro_time_t)screensaver_timeout * 1000000)))
      {
         menu_st->flags             |= MENU_ST_FLAG_SCREENSAVER_ACTIVE;
         if (menu_st->driver_ctx->environ_cb)
            menu_st->driver_ctx->environ_cb(MENU_ENVIRON_ENABLE_SCREENSAVER,
                     NULL, menu_st->userdata);
      }

      /* Iterate the menu driver for one frame. */

      /* If the user had requested that the Quick Menu
       * be spawned during the previous frame, do this now
       * and exit the function to go to the next frame. */
      if (menu_st->flags & MENU_ST_FLAG_PENDING_QUICK_MENU)
      {
         /* We are going to push a new menu; ensure
          * that the current one is cached for animation
          * purposes */
         if (menu_st->driver_ctx && menu_st->driver_ctx->list_cache)
            menu_st->driver_ctx->list_cache(menu_st->userdata,
                  MENU_LIST_PLAIN, MENU_ACTION_NOOP);

         p_disp->flags   |= GFX_DISP_FLAG_MSG_FORCE;

         generic_action_ok_displaylist_push("", NULL,
               "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);

         menu_st->selection_ptr      = 0;
         menu_st->flags             &= ~MENU_ST_FLAG_PENDING_QUICK_MENU;
      }
      else if (!menu_driver_iterate(menu_st, p_disp, anim_get_ptr(),
               settings, action, current_time))
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
#ifdef HAVE_NETWORKING
         const bool libretro_running = runloop_is_libretro_running(runloop_st,
               menu_pause_libretro && netplay_allow_pause);
#else
         const bool libretro_running = runloop_is_libretro_running(runloop_st,
               menu_pause_libretro);
#endif

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

               if (uico_st->flags & UICO_ST_FLAG_IS_ON_FOREGROUND)
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
                        (runloop_st->flags & RUNLOOP_FLAG_IDLE) ? true : false);
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

         if (settings->bools.audio_enable_menu && !libretro_running)
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

   /* Check Game Focus hotkey */
   {
      enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_TOGGLE;
      HOTKEY_CHECK(RARCH_GAME_FOCUS_TOGGLE, CMD_EVENT_GAME_FOCUS_TOGGLE, true, &game_focus_cmd);
   }

   /* Check UI companion hotkey */
   HOTKEY_CHECK(RARCH_UI_COMPANION_TOGGLE, CMD_EVENT_UI_COMPANION_TOGGLE, true, NULL);

   /* Check close content hotkey */
   HOTKEY_CHECK(RARCH_CLOSE_CONTENT_KEY, CMD_EVENT_CLOSE_CONTENT, true, NULL);

   /* Check FPS hotkey */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   /* Check statistics hotkey */
   HOTKEY_CHECK(RARCH_STATISTICS_TOGGLE, CMD_EVENT_STATISTICS_TOGGLE, true, NULL);

   /* Check netplay host hotkey */
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

      if (     (volume_hotkey_up   && !volume_hotkey_down)
            || (volume_hotkey_down && !volume_hotkey_up))
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

   /* Check audio mute hotkey */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL);

#ifdef HAVE_SCREENSHOTS
   /* Check screenshot hotkey */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);
#endif

#ifdef HAVE_CHEEVOS
   /* Make sure not to evaluate this before calling menu_driver_iterate
    * as that may change its value */
   cheevos_hardcore_active = rcheevos_hardcore_active();

   if (!cheevos_hardcore_active)
#endif
   {
      /* Check rewind hotkey */
      /* > Must do this before MENU_ITERATE to not lose rewind steps
       *   while menu is active when menu pause is disabled */
      {
#ifdef HAVE_REWIND
         char s[128];
         bool rewinding      = false;
         static bool old_rewind_pressed = false;
         bool rewind_pressed = BIT256_GET(current_bits, RARCH_REWIND);
         unsigned t          = 0;

         s[0]                = '\0';

#ifdef HAVE_MENU
         /* Don't allow rewinding while menu is active */
         if (menu_st->flags & MENU_ST_FLAG_ALIVE)
            rewind_pressed   = false;
#endif

         /* Prevent rewind hold while paused to rewind only one frame */
         if (     runloop_paused
               && rewind_pressed
               && old_rewind_pressed
               && !runloop_st->run_frames_and_pause)
         {
            cbs->poll_cb();
            return RUNLOOP_STATE_PAUSE;
         }

         rewinding           = state_manager_check_rewind(
               &runloop_st->rewind_st,
               &runloop_st->current_core,
               rewind_pressed,
               settings->uints.rewind_granularity,
               runloop_paused
#ifdef HAVE_MENU
                     || (  (menu_st->flags & MENU_ST_FLAG_ALIVE)
                        && menu_pause_libretro)
#endif
               ,
               s, sizeof(s), &t);

         if (rewind_pressed != old_rewind_pressed)
         {
            if (settings->bools.audio_rewind_mute && rewind_pressed)
               audio_st->flags |=  AUDIO_FLAG_MUTED;
            else
               audio_st->flags &= ~AUDIO_FLAG_MUTED;
         }

         old_rewind_pressed = rewind_pressed;

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
               runloop_msg_queue_push(s, strlen(s), 0, t, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }

         if (     rewinding
               && runloop_paused
#ifdef HAVE_MENU
               && !(menu_st->flags & MENU_ST_FLAG_ALIVE)
#endif
            )
         {
            cbs->poll_cb();
            /* Run a few frames on first press after pausing to
             * prevent going forwards for the first frame */
            if (runloop_st->run_frames_and_pause == -1)
            {
               runloop_st->flags               &= ~RUNLOOP_FLAG_PAUSED;
               runloop_st->run_frames_and_pause = 3;
            }
            return RUNLOOP_STATE_ITERATE;
         }
#endif
      }
   }

   /* Check pause hotkey in menu */
#ifdef HAVE_MENU
   if (menu_st->flags & MENU_ST_FLAG_ALIVE)
   {
      static bool old_pause_pressed = false;
      bool pause_pressed            = BIT256_GET(current_bits, RARCH_PAUSE_TOGGLE);

      /* Decide pause hotkey */
      runloop_pause_toggle(&runloop_paused_hotkey,
            pause_pressed, old_pause_pressed,
            focused, old_focus);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
   }
#endif

#ifdef HAVE_MENU
   /* Stop checking the rest of the hotkeys if menu is alive */
   if (menu_st->flags & MENU_ST_FLAG_ALIVE)
      return RUNLOOP_STATE_MENU;
#endif

#ifdef HAVE_NETWORKING
   if (netplay_allow_pause)
#endif
   if (pause_nonactive)
      focused                = is_focused;

   /* Check pause hotkey */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      static bool pauseframeadvance = false;
      bool frameadvance_pressed     = false;
      bool frameadvance_trigger     = false;
      bool pause_pressed            = BIT256_GET(current_bits, RARCH_PAUSE_TOGGLE);
      bool pause_on_disconnect      = settings->bools.pause_on_disconnect;

      /* Reset frameadvance pause when triggering pause */
      if (pause_pressed)
         pauseframeadvance          = false;

      /* Allow unpausing with Start */
      if (runloop_paused && pause_on_disconnect)
         pause_pressed             |= BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_START);

#ifdef HAVE_CHEEVOS
      if (cheevos_hardcore_active)
      {
         if (!(runloop_st->flags & RUNLOOP_FLAG_PAUSED))
         {
            /* In hardcore mode, the user is only allowed to pause infrequently. */
            if ((pause_pressed && !old_pause_pressed) ||
               (!focused && old_focus && pause_nonactive))
            {
               /* If the user is trying to pause, check to see if it's allowed. */
               if (!rcheevos_is_pause_allowed())
               {
                  pause_pressed = false;
                  if (pause_nonactive)
                     focused = true;
               }
            }
         }
      }
      else /* frame advance not allowed in hardcore */
#endif
      {
         frameadvance_pressed = BIT256_GET(current_bits, RARCH_FRAMEADVANCE);
         frameadvance_trigger = frameadvance_pressed && !old_frameadvance;

         /* FRAMEADVANCE will set us into special pause mode. */
         if (frameadvance_trigger)
         {
            pauseframeadvance = true;
            if (!(runloop_st->flags & RUNLOOP_FLAG_PAUSED))
               pause_pressed = true;
         }
      }

      /* Decide pause hotkey */
      runloop_pause_toggle(&runloop_paused_hotkey,
            pause_pressed, old_pause_pressed,
            focused, old_focus);

      old_focus           = focused;
      old_pause_pressed   = pause_pressed;
      old_frameadvance    = frameadvance_pressed;

      if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
      {
#ifdef HAVE_REWIND
         /* Frame advance must also trigger rewind save */
         if (frameadvance_trigger && runloop_paused)
            state_manager_check_rewind(
               &runloop_st->rewind_st,
               &runloop_st->current_core,
               false,
               settings->uints.rewind_granularity,
               false,
               NULL, 0, NULL);
#endif

         /* Check if it's not oneshot */
#ifdef HAVE_REWIND
         if (!(frameadvance_trigger || BIT256_GET(current_bits, RARCH_REWIND)))
#else
         if (!frameadvance_trigger)
#endif
            focused = false;
#ifdef HAVE_CHEEVOS
         else if (!cheevos_hardcore_active)
#else
         else
#endif
            runloop_paused = false;

         /* Drop to RUNLOOP_STATE_POLLED_AND_SLEEP if frameadvance is triggered */
         if (pauseframeadvance)
            runloop_paused = false;
      }
   }

   /* Check recording hotkey */
   HOTKEY_CHECK(RARCH_RECORDING_TOGGLE, CMD_EVENT_RECORDING_TOGGLE, true, NULL);

   /* Check streaming hotkey */
   HOTKEY_CHECK(RARCH_STREAMING_TOGGLE, CMD_EVENT_STREAMING_TOGGLE, true, NULL);

   /* Check Run-Ahead hotkey */
   HOTKEY_CHECK(RARCH_RUNAHEAD_TOGGLE, CMD_EVENT_RUNAHEAD_TOGGLE, true, NULL);

   /* Check Preemptive Frames hotkey */
   HOTKEY_CHECK(RARCH_PREEMPT_TOGGLE, CMD_EVENT_PREEMPT_TOGGLE, true, NULL);

   /* Check AI Service hotkey */
   HOTKEY_CHECK(RARCH_AI_SERVICE, CMD_EVENT_AI_SERVICE_TOGGLE, true, NULL);

#ifdef HAVE_NETWORKING
   /* Check netplay hotkeys */
   HOTKEY_CHECK(RARCH_NETPLAY_PING_TOGGLE, CMD_EVENT_NETPLAY_PING_TOGGLE, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_PLAYER_CHAT, CMD_EVENT_NETPLAY_PLAYER_CHAT, true, NULL);
   HOTKEY_CHECK(RARCH_NETPLAY_FADE_CHAT_TOGGLE, CMD_EVENT_NETPLAY_FADE_CHAT_TOGGLE, true, NULL);
#endif

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

   if (!focused && !runloop_paused)
   {
      cbs->poll_cb();
      return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }

   /* Apply any pending fastmotion override parameters */
   if (runloop_st->fastmotion_override.pending)
   {
      runloop_apply_fastmotion_override(runloop_st,
            settings->bools.frame_time_counter_reset_after_fastforwarding,
            settings->floats.fastforward_ratio,
            settings->bools.audio_fastforward_mute);
      runloop_st->fastmotion_override.pending = false;
   }

   /* Check fastmotion hotkeys */
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
      bool check2                             = new_button_state && !old_button_state;

      if (!check2)
         check2 = old_hold_button_state != new_hold_button_state;

      /* Don't allow fastmotion while paused */
      if (check2 && runloop_paused)
      {
         new_button_state      = false;
         new_hold_button_state = false;
         input_st->flags      |= INP_FLAG_NONBLOCKING;
      }

#ifdef HAVE_NETWORKING
      if (check2 && !netplay_allow_timeskip)
         check2 = false;
#endif

      if (check2)
      {
         bool audio_fastforward_mute = settings->bools.audio_fastforward_mute;
         bool frame_time_counter_reset_after_ffwd = settings->bools.frame_time_counter_reset_after_fastforwarding;
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
            command_event(CMD_EVENT_SET_FRAME_LIMIT, NULL);
         }

         if (audio_fastforward_mute && (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
            audio_st->flags |=  AUDIO_FLAG_MUTED;
         else
            audio_st->flags &= ~AUDIO_FLAG_MUTED;

         driver_set_nonblock_state();

         /* Reset frame time counter when toggling
          * fast-forward off, if required */
         if ( !(runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
             && frame_time_counter_reset_after_ffwd)
            video_st->frame_time_count  = 0;
      }

      old_button_state                  = new_button_state;
      old_hold_button_state             = new_hold_button_state;
   }

   /* Display fast-forward notification, unless
    * disabled via override */
   if (  !runloop_st->fastmotion_override.current.fastforward
       || runloop_st->fastmotion_override.current.notification)
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
         {
            const char *_msg = msg_hash_to_str(MSG_FAST_FORWARD);
            runloop_msg_queue_push(_msg, strlen(_msg), 1, 1, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
      }
   }
#if defined(HAVE_GFX_WIDGETS)
   else
      video_st->flags &= ~VIDEO_FLAG_WIDGETS_FAST_FORWARD;
#endif

#ifdef HAVE_CHEEVOS
   if (!cheevos_hardcore_active)
#endif
   {
      {
         /* Check slowmotion hotkeys */
         static bool old_slowmotion_button_state      = false;
         static bool old_slowmotion_hold_button_state = false;
         bool new_slowmotion_button_state             = BIT256_GET(
               current_bits, RARCH_SLOWMOTION_KEY);
         bool new_slowmotion_hold_button_state        = BIT256_GET(
               current_bits, RARCH_SLOWMOTION_HOLD_KEY);

         /* Don't allow slowmotion while paused */
         if (runloop_paused)
         {
            new_slowmotion_button_state      = false;
            new_slowmotion_hold_button_state = false;
         }

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

#ifdef HAVE_NETWORKING
         if ((runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
               && !netplay_allow_timeskip)
            runloop_st->flags &= ~RUNLOOP_FLAG_SLOWMOTION;
#endif

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
               if (rewind_st->flags
                     & STATE_MGR_REWIND_ST_FLAG_FRAME_IS_REVERSED)
               {
                  const char *_msg = msg_hash_to_str(MSG_SLOW_MOTION_REWIND);
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 1, false, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
               else
#endif
               {
                  const char *_msg = msg_hash_to_str(MSG_SLOW_MOTION);
                  runloop_msg_queue_push(_msg, strlen(_msg), 1, 1, false, NULL,
                        MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               }
            }
         }

         old_slowmotion_button_state                  = new_slowmotion_button_state;
         old_slowmotion_hold_button_state             = new_slowmotion_hold_button_state;
      }
   }

   /* Check save state slot hotkeys */
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
         check1                            = state_slot > -1;
         addition                          = -1;

         /* Wrap-around to 999 */
         if (check2 && !check1 && state_slot + addition < -1)
         {
            state_slot = 1000;
            check1     = true;
         }
      }
      /* Wrap-around to -1 (Auto) */
      else if (state_slot + addition > 999)
         state_slot = -2;

      if (check2)
      {
         size_t _len;
         char msg[128];
         int cur_state_slot                = state_slot + addition;

         if (check1)
            configuration_set_int(settings, settings->ints.state_slot,
                  cur_state_slot);
         _len  = strlcpy(msg, msg_hash_to_str(MSG_STATE_SLOT), sizeof(msg));
         _len += snprintf(msg + _len, sizeof(msg) - _len,
                  ": %d", settings->ints.state_slot);

         if (cur_state_slot < 0)
            _len += strlcpy(msg + _len, " (Auto)", sizeof(msg) - _len);

#ifdef HAVE_GFX_WIDGETS
         if (dispwidget_get_ptr()->active)
            gfx_widget_set_generic_message(msg, 1000);
         else
#endif
            runloop_msg_queue_push(msg, _len, 2, 60, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         RARCH_LOG("[State]: %s\n", msg);
      }

      old_should_slot_increase = should_slot_increase;
      old_should_slot_decrease = should_slot_decrease;
   }
   /* Check replay slot hotkeys */
   {
      static bool old_should_replay_slot_increase = false;
      static bool old_should_replay_slot_decrease = false;
      bool should_slot_increase            = BIT256_GET(
            current_bits, RARCH_REPLAY_SLOT_PLUS);
      bool should_slot_decrease            = BIT256_GET(
            current_bits, RARCH_REPLAY_SLOT_MINUS);
      bool check1                          = true;
      bool check2                          = should_slot_increase && !old_should_replay_slot_increase;
      int addition                         = 1;
      int replay_slot                      = settings->ints.replay_slot;

      if (!check2)
      {
         check2                            = should_slot_decrease && !old_should_replay_slot_decrease;
         check1                            = replay_slot > -1;
         addition                          = -1;

         /* Wrap-around to 999 */
         if (check2 && !check1 && replay_slot + addition < -1)
         {
            replay_slot = 1000;
            check1      = true;
         }
      }
      /* Wrap-around to -1 (Auto) */
      else if (replay_slot + addition > 999)
         replay_slot    = -2;

      if (check2)
      {
         size_t _len;
         char msg[128];
         int cur_replay_slot                = replay_slot + addition;

         if (check1)
            configuration_set_int(settings, settings->ints.replay_slot,
                  cur_replay_slot);
         _len  = strlcpy(msg, msg_hash_to_str(MSG_REPLAY_SLOT), sizeof(msg));
         _len += snprintf(msg + _len, sizeof(msg) - _len,
                  ": %d", settings->ints.replay_slot);

         if (cur_replay_slot < 0)
            _len += strlcpy(msg + _len, " (Auto)", sizeof(msg) - _len);

#ifdef HAVE_GFX_WIDGETS
         if (dispwidget_get_ptr()->active)
            gfx_widget_set_generic_message(msg, 1000);
         else
#endif
            runloop_msg_queue_push(msg, _len, 2, 60, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         RARCH_LOG("[Replay]: %s\n", msg);
      }

      old_should_replay_slot_increase = should_slot_increase;
      old_should_replay_slot_decrease = should_slot_decrease;
   }

   /* Check save state hotkeys */
   HOTKEY_CHECK(RARCH_SAVE_STATE_KEY, CMD_EVENT_SAVE_STATE, true, NULL);
   HOTKEY_CHECK(RARCH_LOAD_STATE_KEY, CMD_EVENT_LOAD_STATE, true, NULL);

   /* Check reset hotkey */
   HOTKEY_CHECK(RARCH_RESET, CMD_EVENT_RESET, true, NULL);

   /* Check VRR runloop hotkey */
   HOTKEY_CHECK(RARCH_VRR_RUNLOOP_TOGGLE, CMD_EVENT_VRR_RUNLOOP_TOGGLE, true, NULL);

   /* Check bsv movie hotkeys */
   HOTKEY_CHECK(RARCH_PLAY_REPLAY_KEY, CMD_EVENT_PLAY_REPLAY, true, NULL);
   HOTKEY_CHECK(RARCH_RECORD_REPLAY_KEY, CMD_EVENT_RECORD_REPLAY, true, NULL);
   HOTKEY_CHECK(RARCH_HALT_REPLAY_KEY, CMD_EVENT_HALT_REPLAY, true, NULL);

   /* Check Disc Control hotkeys */
   HOTKEY_CHECK3(
         RARCH_DISK_EJECT_TOGGLE, CMD_EVENT_DISK_EJECT_TOGGLE,
         RARCH_DISK_NEXT,         CMD_EVENT_DISK_NEXT,
         RARCH_DISK_PREV,         CMD_EVENT_DISK_PREV);

   /* Check cheat hotkeys */
   HOTKEY_CHECK3(
         RARCH_CHEAT_INDEX_PLUS,  CMD_EVENT_CHEAT_INDEX_PLUS,
         RARCH_CHEAT_INDEX_MINUS, CMD_EVENT_CHEAT_INDEX_MINUS,
         RARCH_CHEAT_TOGGLE,      CMD_EVENT_CHEAT_TOGGLE);

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   /* Check shader hotkeys */
   HOTKEY_CHECK3(
         RARCH_SHADER_NEXT,   CMD_EVENT_SHADER_NEXT,
         RARCH_SHADER_PREV,   CMD_EVENT_SHADER_PREV,
         RARCH_SHADER_TOGGLE, CMD_EVENT_SHADER_TOGGLE);

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
               const char *preset          = video_shader_get_current_shader_preset();
               enum rarch_shader_type type = video_shader_parse_type(preset);
               video_shader_apply_shader(settings, type, preset, false);
            }
         }
      }
   }
#endif

   if (runloop_paused)
   {
      cbs->poll_cb();
      return RUNLOOP_STATE_PAUSE;
   }
#if HAVE_MENU
   if (menu_was_alive)
      return RUNLOOP_STATE_MENU;
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
   input_driver_state_t         *input_st = input_state_get_ptr();
   audio_driver_state_t         *audio_st = audio_state_get_ptr();
   video_driver_state_t         *video_st = video_state_get_ptr();
   recording_state_t              *rec_st = recording_state_get_ptr();
   camera_driver_state_t       *camera_st = camera_state_get_ptr();
   uico_driver_state_t           *uico_st = uico_state_get_ptr();
   settings_t *settings                   = config_get_ptr();
   runloop_state_t *runloop_st            = &runloop_state;
   bool vrr_runloop_enable                = settings->bools.vrr_runloop_enable;
   retro_time_t current_time              = cpu_features_get_time_usec();
#ifdef HAVE_MENU
#ifdef HAVE_NETWORKING
   bool netplay_is_enabled                = netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
   bool netplay_allow_timeskip            = netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_TIMESKIP, NULL);
   bool netplay_allow_pause               = netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
   bool menu_pause_libretro               = settings->bools.menu_pause_libretro && netplay_allow_pause;
#else
   bool netplay_allow_timeskip            = false;
   bool netplay_allow_pause               = false;
   bool menu_pause_libretro               = settings->bools.menu_pause_libretro;
#endif
   bool core_paused                       =
            (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
         || (menu_pause_libretro && (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE));
#else
   bool core_paused                       = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
#endif
   float slowmotion_ratio                 = settings->floats.slowmotion_ratio;
#ifdef HAVE_CHEEVOS
   bool cheevos_enable                    = settings->bools.cheevos_enable;
#endif
   bool audio_sync                        = settings->bools.audio_sync;
#ifdef HAVE_DISCORD
   discord_state_t *discord_st            = discord_state_get_ptr();

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
             | !!rec_st->data;
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
            input_st, audio_st, video_st,
            uico_st,
            ((global_get_ptr()->flags & GLOB_FLG_ERR_ON_INIT) > 0),
            settings, current_time, netplay_allow_pause,
            netplay_allow_timeskip))
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
         if (!(uico_st->flags & UICO_ST_FLAG_IS_ON_FOREGROUND))
#endif
            retro_sleep(10);
         return 1;
      case RUNLOOP_STATE_PAUSE:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
         video_driver_cached_frame();
         goto end;
      case RUNLOOP_STATE_MENU:
#if defined(HAVE_MENU) && defined(HAVE_NETWORKING)
         /* FIXME: This is an ugly way to tell Netplay this... */
         if (menu_pause_libretro && netplay_is_enabled)
            netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif

#ifdef HAVE_CHEEVOS
         if (cheevos_enable)
         {
            if (runloop_is_libretro_running(runloop_st, menu_pause_libretro))
               rcheevos_test();
            else
               rcheevos_idle();
         }
#endif

#ifdef HAVE_MENU
         /* Rely on vsync throttling unless VRR is enabled and menu throttle is disabled. */
         if (vrr_runloop_enable && !settings->bools.menu_throttle_framerate)
            return 0;
         else if (settings->bools.video_vsync)
            goto end;

         /* Otherwise run menu in video refresh rate speed. */
         if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
            runloop_st->frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f /
                     ((video_st->video_refresh_rate_original)
                     ? video_st->video_refresh_rate_original
                     : settings->floats.video_refresh_rate));
         else
            runloop_set_frame_limit(&video_st->av_info, settings->floats.fastforward_ratio);
#endif
         goto end;
      case RUNLOOP_STATE_ITERATE:
         runloop_st->flags       |= RUNLOOP_FLAG_CORE_RUNNING;
         break;
   }

#ifdef HAVE_THREADS
   if (runloop_st->flags & RUNLOOP_FLAG_AUTOSAVE)
      autosave_lock();
#endif

#ifdef HAVE_BSV_MOVIE
   bsv_movie_next_frame(input_st);
#endif

   if (     settings->bools.camera_allow
         && camera_st->cb.caps
         && camera_st->driver
         && camera_st->driver->poll
         && camera_st->data)
      camera_st->driver->poll(camera_st->data,
            camera_st->cb.frame_raw_framebuffer,
            camera_st->cb.frame_opengl_texture);

   /* Measure the time between core_run() and video_driver_frame() */
   runloop_st->core_run_time = cpu_features_get_time_usec();

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
      want_runahead                     = want_runahead && !netplay_is_enabled;
#endif

      if (want_runahead)
         runahead_run(
               runloop_st,
               run_ahead_num_frames,
               run_ahead_hide_warnings,
               run_ahead_secondary_instance);
      else if (runloop_st->preempt_data)
         preempt_run(runloop_st->preempt_data, runloop_st);
      else
#endif
         core_run();
   }

   /* Increment runtime tick counter after each call to
    * core_run() or run_ahead() */
   runloop_st->core_runtime_usec += runloop_core_runtime_tick(
         runloop_st, slowmotion_ratio, current_time);

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
#ifdef HAVE_BSV_MOVIE
   bsv_movie_finish_rewind(input_st);
   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_END)
   {
      movie_stop_playback(input_st);
      command_event(CMD_EVENT_PAUSE, NULL);
   }
   if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_END)
   {
      movie_stop_playback(input_st);
      command_event(CMD_EVENT_PAUSE, NULL);
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
         runloop_set_frame_limit(&video_st->av_info,
               runloop_get_fastforward_ratio(settings,
                  &runloop_st->fastmotion_override.current));
      else
         runloop_set_frame_limit(&video_st->av_info, 1.0f);
   }

   /* if there's a fast forward limit, inject sleeps to keep from going too fast. */
   if (   (runloop_st->frame_limit_minimum_time)
          && (   (vrr_runloop_enable)
              || (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION)
#ifdef HAVE_MENU
              || (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE
                  && !(settings->bools.video_vsync))
#endif
              || (runloop_st->flags & RUNLOOP_FLAG_PAUSED)))
   {
      const retro_time_t end_frame_time  = cpu_features_get_time_usec();
      const retro_time_t to_sleep_ms     = (
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
            if (!(uico_state_get_ptr()->flags & UICO_ST_FLAG_IS_ON_FOREGROUND))
#endif
               retro_sleep(sleep_ms);
         }

         return 1;
      }

      runloop_st->frame_limit_last_time = end_frame_time;
   }

   /* Frame delay */
   if (     !(input_st->flags & INP_FLAG_NONBLOCKING)
         || (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION))
      video_frame_delay(video_st, settings);

   /* Set paused state after x frames */
   if (runloop_st->run_frames_and_pause > 0)
   {
      runloop_st->run_frames_and_pause--;
      if (!runloop_st->run_frames_and_pause)
         runloop_st->flags |= RUNLOOP_FLAG_PAUSED;
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

void runloop_task_msg_queue_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
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

   if (widgets_active && task->title && (!((task->flags & RETRO_TASK_FLG_MUTE) > 0)))
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
            strlen(msg),
            duration,
            NULL,
            (enum message_queue_icon)MESSAGE_QUEUE_CATEGORY_INFO,
            (enum message_queue_category)MESSAGE_QUEUE_ICON_DEFAULT,
            prio,
            flush,
#ifdef HAVE_MENU
            (menu_st->flags & MENU_ST_FLAG_ALIVE) ? true : false
#else
            false
#endif
            );
      RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
   }
   else
#endif
      runloop_msg_queue_push(msg, strlen(msg), prio, duration, flush, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

bool runloop_get_current_savestate_path(char *s, size_t len)
{
   settings_t *settings        = config_get_ptr();
   int state_slot              = settings ? settings->ints.state_slot : 0;
   return runloop_get_savestate_path(s, len, state_slot);
}

bool runloop_get_savestate_path(char *s, size_t len, int state_slot)
{
   runloop_state_t *runloop_st = &runloop_state;
   const char *name_savestate  = NULL;

   if (!s)
      return false;

   name_savestate              = runloop_st->name.savestate;
   if (string_is_empty(name_savestate))
      return false;

   if (state_slot < 0)
      fill_pathname_join_delim(s, name_savestate, "auto", '.', len);
   else
   {
      size_t _len = strlcpy(s, name_savestate, len);
      if (state_slot > 0)
         snprintf(s + _len, len - _len, "%d", state_slot);
   }

   return true;
}

bool runloop_get_replay_path(char *s, size_t len, unsigned slot)
{
   size_t _len;
   runloop_state_t *runloop_st = &runloop_state;
   const char *name_replay     = NULL;

   if (!s)
      return false;

   name_replay = runloop_st->name.replay;
   if (string_is_empty(name_replay))
      return false;

   _len = strlcpy(s, name_replay, len);
   if (slot >= 0)
      snprintf(s + _len, len - _len, "%d",  slot);

   return true;
}


bool runloop_get_entry_state_path(char *s, size_t len, int slot)
{
   size_t _len;
   runloop_state_t *runloop_st = &runloop_state;
   const char *name_savestate  = NULL;

   if (!s)
      return false;

   name_savestate              = runloop_st->name.savestate;
   if (string_is_empty(name_savestate))
      return false;

   _len = strlcpy(s, name_savestate, len);
   snprintf(s + _len, len - _len, "%d.entry", slot);

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

   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_USE_CORE_PACKET_INTERFACE, NULL))
   {
      /* Force normal poll type for netplay. */
      runloop_st->current_core.poll_type = POLL_TYPE_NORMAL;

      /* And use netplay's interceding callbacks */
      runloop_st->current_core.retro_set_video_refresh(video_frame_net);
      runloop_st->current_core.retro_set_audio_sample(audio_sample_net);
      runloop_st->current_core.retro_set_audio_sample_batch(audio_sample_batch_net);
      runloop_st->current_core.retro_set_input_state(input_state_net);
   }

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
         && (secondary_core_ensure_exists(runloop_st, settings))
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
       && (secondary_core_ensure_exists(runloop_st, settings))
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

#if defined(HAVE_RUNAHEAD)
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   runahead_remember_controller_port_device(runloop_st, pad->port, pad->device);
#endif
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
   bool             game_loaded   = false;
   video_driver_state_t *video_st = video_state_get_ptr();
   runloop_state_t *runloop_st    = &runloop_state;

   video_st->frame_cache_data     = NULL;

#ifdef HAVE_RUNAHEAD
   runahead_set_load_content_info(runloop_st, load_info);
#if defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB)
   runahead_clear_controller_port_map(runloop_st);
#endif
#endif

   set_save_state_in_background(false);

   if (load_info && load_info->special)
      game_loaded = runloop_st->current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      game_loaded = runloop_st->current_core.retro_load_game(load_info->info);
   else if (content_get_flags() & CONTENT_ST_FLAG_CORE_DOES_NOT_NEED_CONTENT)
      game_loaded = runloop_st->current_core.retro_load_game(NULL);

   if (game_loaded)
   {
      /* If 'game_loaded' is true at this point, then
       * core is actually running; register that any
       * changes to global remap-related parameters
       * should be reset once core is deinitialised */
      input_state_get_ptr()->flags   |=  INP_FLAG_REMAPPING_CACHE_ACTIVE;
      runloop_st->current_core.flags |=  RETRO_CORE_FLAG_GAME_LOADED;

#ifdef HAVE_GAME_AI
      /* load models */
      game_ai_load(load_info->info->path, runloop_st->current_core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM), runloop_st->current_core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM), libretro_log_cb);
#endif
      return true;
   }

   runloop_st->current_core.flags &= ~RETRO_CORE_FLAG_GAME_LOADED;
   return false;
}

bool core_get_system_info(struct retro_system_info *sysinfo)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!sysinfo)
      return false;
   runloop_st->current_core.retro_get_system_info(sysinfo);
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
#if HAVE_RUNAHEAD
   command_event(CMD_EVENT_PREEMPT_RESET_BUFFER, NULL);
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
   ret                = runloop_st->current_core.retro_serialize(
                        info->data, info->size);
   runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

   return ret;
}

size_t core_serialize_size(void)
{
   runloop_state_t *runloop_st  = &runloop_state;
   return runloop_st->current_core.retro_serialize_size();
}

size_t core_serialize_size_special(void)
{
   size_t val;
   runloop_state_t *runloop_st = &runloop_state;
   runloop_st->flags |=  RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;
   val                = runloop_st->current_core.retro_serialize_size();
   runloop_st->flags &= ~RUNLOOP_FLAG_REQUEST_SPECIAL_SAVESTATE;

   return val;
}

uint64_t core_serialization_quirks(void)
{
   runloop_state_t *runloop_st  = &runloop_state;
   return runloop_st->current_core.serialization_quirks_v;
}

void core_reset(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
   video_driver_state_t *video_st = video_state_get_ptr();
   video_st->frame_cache_data     = NULL;
   runloop_st->current_core.retro_reset();
}

void core_run(void)
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
      return;
   }
#endif

   if (early_polling)
      input_driver_poll();
   else if (late_polling)
      current_core->flags &= ~RETRO_CORE_FLAG_INPUT_POLLED;

   current_core->retro_run();

#ifdef HAVE_GAME_AI
   {
      settings_t *settings           = config_get_ptr();
      video_driver_state_t *video_st = video_state_get_ptr();
      game_ai_think(
            settings->bools.game_ai_override_p1,
            settings->bools.game_ai_override_p2,
            settings->bools.game_ai_show_debug,
            video_st->frame_cache_data,
            video_st->frame_cache_width,
            video_st->frame_cache_height,
            video_st->frame_cache_pitch,
            video_st->pix_fmt);
   }
#endif

   if (      late_polling
         && (!(current_core->flags & RETRO_CORE_FLAG_INPUT_POLLED)))
      input_driver_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif
}

bool core_has_set_input_descriptor(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return ((runloop_st->current_core.flags &
            RETRO_CORE_FLAG_HAS_SET_INPUT_DESCRIPTORS) > 0);
}

void runloop_path_set_basename(const char *path)
{
   runloop_state_t *runloop_st = &runloop_state;
   char *dst                   = NULL;

   path_set(RARCH_PATH_CONTENT,  path);
   strlcpy(runloop_st->runtime_content_path_basename, path,
         sizeof(runloop_st->runtime_content_path_basename));

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
      fill_pathname_dir(runloop_st->runtime_content_path_basename, path,
            "", sizeof(runloop_st->runtime_content_path_basename));
#endif

   if ((dst = strrchr(runloop_st->runtime_content_path_basename, '.')))
      *dst = '\0';
}

void runloop_path_set_names(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
      fill_pathname(runloop_st->name.savefile,
             runloop_st->runtime_content_path_basename,
             ".srm",
             sizeof(runloop_st->name.savefile));

   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
      fill_pathname(runloop_st->name.savestate,
            runloop_st->runtime_content_path_basename,
            ".state",
            sizeof(runloop_st->name.savestate));

#ifdef HAVE_BSV_MOVIE
   if (!retroarch_override_setting_is_set(
            RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
      fill_pathname(
            runloop_st->name.replay,
            runloop_st->runtime_content_path_basename,
            ".replay",
            sizeof(runloop_st->name.replay));
#endif

#ifdef HAVE_CHEATS
   if (!string_is_empty(runloop_st->runtime_content_path_basename))
      fill_pathname(
            runloop_st->name.cheatfile,
            runloop_st->runtime_content_path_basename,
            ".cht",
            sizeof(runloop_st->name.cheatfile));
#endif
}

void runloop_path_set_redirect(settings_t *settings,
      const char *old_savefile_dir,
      const char *old_savestate_dir)
{
   char content_dir_name[DIR_MAX_LENGTH];
   char new_savefile_dir[DIR_MAX_LENGTH];
   char new_savestate_dir[DIR_MAX_LENGTH];
   char intermediate_savefile_dir[DIR_MAX_LENGTH];
   char intermediate_savestate_dir[DIR_MAX_LENGTH];
   runloop_state_t *runloop_st            = &runloop_state;
   struct retro_system_info *sysinfo      = &runloop_st->system.info;
   bool sort_savefiles_enable             = settings->bools.sort_savefiles_enable;
   bool sort_savefiles_by_content_enable  = settings->bools.sort_savefiles_by_content_enable;
   bool sort_savestates_enable            = settings->bools.sort_savestates_enable;
   bool sort_savestates_by_content_enable = settings->bools.sort_savestates_by_content_enable;
   bool savefiles_in_content_dir          = settings->bools.savefiles_in_content_dir;
   bool savestates_in_content_dir         = settings->bools.savestates_in_content_dir;

   content_dir_name[0] = '\0';

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(intermediate_savefile_dir, old_savefile_dir, sizeof(intermediate_savefile_dir));
   strlcpy(intermediate_savestate_dir, old_savestate_dir, sizeof(intermediate_savestate_dir));

   /* Get content directory name, if per-content-directory
    * saves/states are enabled */
   if ((   sort_savefiles_by_content_enable
        || sort_savestates_by_content_enable)
       && !string_is_empty(runloop_st->runtime_content_path_basename))
      fill_pathname_parent_dir_name(content_dir_name,
            runloop_st->runtime_content_path_basename,
            sizeof(content_dir_name));

   /* Set savefile directory if empty to content directory */
   if (     string_is_empty(intermediate_savefile_dir)
         || savefiles_in_content_dir)
   {
      fill_pathname_basedir(
            intermediate_savefile_dir,
            runloop_st->runtime_content_path_basename,
            sizeof(intermediate_savefile_dir));

      if (string_is_empty(intermediate_savefile_dir))
         RARCH_LOG("Cannot resolve save file path.\n");
   }

   /* Set savestate directory if empty based on content directory */
   if (   string_is_empty(intermediate_savestate_dir)
       || savestates_in_content_dir)
   {
      fill_pathname_basedir(intermediate_savestate_dir,
            runloop_st->runtime_content_path_basename,
            sizeof(intermediate_savestate_dir));

      if (string_is_empty(intermediate_savestate_dir))
         RARCH_LOG("Cannot resolve save state file path.\n");
   }

   strlcpy(new_savefile_dir, intermediate_savefile_dir,
         sizeof(new_savefile_dir));
   strlcpy(new_savestate_dir, intermediate_savestate_dir,
         sizeof(new_savestate_dir));

   if (sysinfo && !string_is_empty(sysinfo->library_name))
   {
#ifdef HAVE_MENU
      if (!string_is_equal(sysinfo->library_name,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
#endif
      {
         /* Per-core and/or per-content-directory saves */
         if ((sort_savefiles_enable
              || sort_savefiles_by_content_enable)
             && !string_is_empty(new_savefile_dir))
         {
            /* Append content directory name to save location */
            if (sort_savefiles_by_content_enable)
               fill_pathname_join_special(new_savefile_dir,
                  intermediate_savefile_dir,
                  content_dir_name,
                  sizeof(new_savefile_dir));

            /* Append library_name to the save location */
            if (sort_savefiles_enable)
               fill_pathname_join(new_savefile_dir,
                  new_savefile_dir,
                  sysinfo->library_name,
                  sizeof(new_savefile_dir));

            /* If path doesn't exist, try to create it,
             * if everything fails revert to the original path. */
            if (!path_is_directory(new_savefile_dir))
               if (!path_mkdir(new_savefile_dir))
               {
                  RARCH_LOG("%s %s\n",
                            msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                            intermediate_savefile_dir);
                  strlcpy(new_savefile_dir,
                        intermediate_savefile_dir,
                        sizeof(new_savefile_dir));
               }
         }

         /* Per-core and/or per-content-directory savestates */
         if ((sort_savestates_enable || sort_savestates_by_content_enable)
             && !string_is_empty(new_savestate_dir))
         {
            /* Append content directory name to savestate location */
            if (sort_savestates_by_content_enable)
               fill_pathname_join_special(
                  new_savestate_dir,
                  intermediate_savestate_dir,
                  content_dir_name,
                  sizeof(new_savestate_dir));

            /* Append library_name to the savestate location */
            if (sort_savestates_enable)
               fill_pathname_join(
                  new_savestate_dir,
                  new_savestate_dir,
                  sysinfo->library_name,
                  sizeof(new_savestate_dir));

            /* If path doesn't exist, try to create it.
             * If everything fails, revert to the original path. */
            if (!path_is_directory(new_savestate_dir))
               if (!path_mkdir(new_savestate_dir))
               {
                  RARCH_LOG("%s %s\n",
                            msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                            intermediate_savestate_dir);
                  strlcpy(new_savestate_dir,
                          intermediate_savestate_dir,
                          sizeof(new_savestate_dir));
               }
         }
      }
   }


#ifdef HAVE_NETWORKING
   /* Special save directory for netplay clients. */
   if (      netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL)
         && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL)
         && !netplay_driver_ctl(RARCH_NETPLAY_CTL_USE_CORE_PACKET_INTERFACE, NULL))
   {
      fill_pathname_join(new_savefile_dir,
            new_savefile_dir, ".netplay",
            sizeof(new_savefile_dir));

      if (     !path_is_directory(new_savefile_dir)
            && !path_mkdir(new_savefile_dir))
         path_basedir(new_savefile_dir);
   }
#endif

   if (sysinfo && !string_is_empty(sysinfo->library_name))
   {
      bool savefile_is_dir  = path_is_directory(new_savefile_dir);
      bool savestate_is_dir = path_is_directory(new_savestate_dir);
      if (savefile_is_dir)
         strlcpy(runloop_st->name.savefile, new_savefile_dir,
                 sizeof(runloop_st->name.savefile));
      else
         savefile_is_dir = path_is_directory(runloop_st->name.savefile);

      if (savestate_is_dir)
      {
         strlcpy(runloop_st->name.savestate, new_savestate_dir,
                 sizeof(runloop_st->name.savestate));
         strlcpy(runloop_st->name.replay, new_savestate_dir,
                 sizeof(runloop_st->name.replay));
      }
      else
         savestate_is_dir = path_is_directory(runloop_st->name.savestate);

      if (savefile_is_dir)
      {
         fill_pathname_dir(runloop_st->name.savefile,
                           !string_is_empty(runloop_st->runtime_content_path_basename)
                           ? runloop_st->runtime_content_path_basename
                           : sysinfo->library_name,
                           FILE_PATH_SRM_EXTENSION,
                           sizeof(runloop_st->name.savefile));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
                   msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
                   runloop_st->name.savefile);
      }

      if (savestate_is_dir)
      {
         fill_pathname_dir(runloop_st->name.savestate,
                           !string_is_empty(runloop_st->runtime_content_path_basename)
                           ? runloop_st->runtime_content_path_basename
                           : sysinfo->library_name,
                           FILE_PATH_STATE_EXTENSION,
                           sizeof(runloop_st->name.savestate));
         fill_pathname_dir(runloop_st->name.replay,
                           !string_is_empty(runloop_st->runtime_content_path_basename)
                           ? runloop_st->runtime_content_path_basename
                           : sysinfo->library_name,
                           FILE_PATH_BSV_EXTENSION,
                           sizeof(runloop_st->name.replay));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
                   msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
                   runloop_st->name.savestate);
      }

#ifdef HAVE_CHEATS
      if (path_is_directory(runloop_st->name.cheatfile))
      {
         fill_pathname_dir(runloop_st->name.cheatfile,
               !string_is_empty(runloop_st->runtime_content_path_basename)
               ? runloop_st->runtime_content_path_basename
               : sysinfo->library_name,
               FILE_PATH_CHT_EXTENSION,
               sizeof(runloop_st->name.cheatfile));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               runloop_st->name.cheatfile);
      }
#endif
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE, new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
}

void runloop_path_deinit_subsystem(void)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (runloop_st->subsystem_fullpaths)
      string_list_free(runloop_st->subsystem_fullpaths);
   runloop_st->subsystem_fullpaths = NULL;
}

void runloop_path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   char str[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   bool is_dir                         = false;
   struct string_list subsystem_paths  = {0};
   runloop_state_t         *runloop_st = &runloop_state;
   const char *savestate_dir           = runloop_st->savestate_dir;

   /* First content file is the significant one. */
   runloop_path_set_basename(argv[0]);

   string_list_initialize(&subsystem_paths);

   runloop_st->subsystem_fullpaths     = string_list_new();

   attr.i = 0;

   for (i = 0; i < num_content; i++)
   {
      string_list_append(runloop_st->subsystem_fullpaths, argv[i], attr);
      fill_pathname(str, argv[i], "", sizeof(str));
      string_list_append(&subsystem_paths, path_basename(str), attr);
   }

   str[0] = '\0';
   string_list_join_concat(str, sizeof(str), &subsystem_paths, " + ");
   string_list_deinitialize(&subsystem_paths);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */
   is_dir = path_is_directory(savestate_dir);

   if (is_dir)
   {
      strlcpy(runloop_st->name.savestate, savestate_dir,
              sizeof(runloop_st->name.savestate));
      strlcpy(runloop_st->name.replay, savestate_dir,
              sizeof(runloop_st->name.replay));
   }
   else
      is_dir   = path_is_directory(runloop_st->name.savestate);

   if (is_dir)
   {
      fill_pathname_dir(runloop_st->name.savestate,
            str,
            ".state",
            sizeof(runloop_st->name.savestate));
      fill_pathname_dir(runloop_st->name.replay,
            str,
            ".replay",
            sizeof(runloop_st->name.replay));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
            runloop_st->name.savestate);
   }
}
