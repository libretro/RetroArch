/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-Andr� Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <errno.h>
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
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
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

#ifdef HAVE_MIST
#include "steam/steam.h"
#endif

#include "config.def.h"

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

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#include "input/input_remapping.h"

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#include "cheevos/cheevos_menu.h"
#endif

#ifdef HAVE_TRANSLATE
#include <encodings/base64.h>
#include <formats/rbmp.h>
#include <formats/rpng.h>
#include <formats/rjson.h>
#include "translation_defines.h"
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
#include "config.features.h"
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

#include "gfx/video_display_server.h"
#ifdef HAVE_BLUETOOTH
#include "bluetooth/bluetooth_driver.h"
#endif
#include "misc/cpufreq/cpufreq.h"
#include "midi_driver.h"
#include "core.h"
#include "configuration.h"
#include "list_special.h"
#ifdef HAVE_CHEATS
#include "cheat_manager.h"
#endif
#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"

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

#define _PSUPP(var, name, desc) printf("  %s:\n\t\t%s: %s\n", name, desc, var ? "yes" : "no")

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   retroarch_fail(1, "validate_cpu_features()"); \
} while (0)

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

#ifdef HAVE_CONFIGFILE
#define CONFIG_FILE_ARG "c:"
#else
#define CONFIG_FILE_ARG
#endif

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_ARG "P:R:M:"
#else
#define BSV_MOVIE_ARG
#endif

#define _PSUPP_BUF(buf, var, name, desc) \
   strlcat(buf, "  ", sizeof(buf)); \
   strlcat(buf, name, sizeof(buf)); \
   strlcat(buf, ":\n\t\t", sizeof(buf)); \
   strlcat(buf, desc, sizeof(buf)); \
   strlcat(buf, ": ", sizeof(buf)); \
   strlcat(buf, var ? "yes\n" : "no\n", sizeof(buf))

/* Griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

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
   RA_OPT_MAX_FRAMES_SCREENSHOT_PATH,
   RA_OPT_SET_SHADER,
   RA_OPT_ACCESSIBILITY,
   RA_OPT_LOAD_MENU_ON_ERROR
};

/* DRIVERS */
#ifdef HAVE_BLUETOOTH
extern const bluetooth_driver_t *bluetooth_drivers[];
#endif

/* MAIN GLOBAL VARIABLES */
struct rarch_state
{
   char *connect_host; /* Netplay hostname passed from CLI */

   struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_t rarch_tls;               /* unsigned alignment */
#endif
   unsigned perf_ptr_rarch;

   char launch_arguments[4096];
   char path_default_shader_preset[PATH_MAX_LENGTH];
   char path_content[PATH_MAX_LENGTH];
   char path_libretro[PATH_MAX_LENGTH];
   char path_config_file[PATH_MAX_LENGTH];
   char path_config_append_file[256];
   char path_core_options_file[PATH_MAX_LENGTH];
   char dir_system[PATH_MAX_LENGTH];
   char dir_savefile[PATH_MAX_LENGTH];
   char dir_savestate[PATH_MAX_LENGTH];
   bool has_set_username;
   bool has_set_verbosity;
   bool has_set_libretro;
   bool has_set_libretro_directory;
   bool has_set_save_path;
   bool has_set_state_path;
#ifdef HAVE_PATCH
   bool has_set_ups_pref;
   bool has_set_bps_pref;
   bool has_set_ips_pref;
#endif
   bool has_set_log_to_file;
   bool rarch_ups_pref;
   bool rarch_bps_pref;
   bool rarch_ips_pref;

#ifdef HAVE_CONFIGFILE
   bool rarch_block_config_read;
#endif
};

/* Forward declarations */
#ifdef HAVE_LIBNX
void libnx_apply_overclock(void);
#endif

static struct rarch_state rarch_st        = {0};

#ifdef HAVE_THREAD_STORAGE
static const void *MAGIC_POINTER          = (void*)(uintptr_t)0x0DEFACED;
#endif

static access_state_t access_state_st     = {0};
static struct global global_driver_st     = {0}; /* retro_time_t alignment */

access_state_t *access_state_get_ptr(void)
{
   return &access_state_st;
}

/* GLOBAL POINTER GETTERS */
global_t *global_get_ptr(void)
{
   return &global_driver_st;
}

struct retro_perf_counter **retro_get_perf_counter_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_counters_rarch;
}

unsigned retro_get_perf_count_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_ptr_rarch;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   if (
            !runloop_st->perfcnt_enable
         || perf->registered
         || p_rarch->perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   p_rarch->perf_counters_rarch[p_rarch->perf_ptr_rarch++] = perf;
   perf->registered = true;
}

struct string_list *dir_list_new_special(const char *input_dir,
      enum dir_list_type type, const char *filter,
      bool show_hidden_files)
{
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   char ext_shaders[255];
#endif
   char ext_name[255];
   const char *exts                  = NULL;
   bool recursive                    = false;

   switch (type)
   {
      case DIR_LIST_AUTOCONFIG:
         exts = filter;
         break;
      case DIR_LIST_CORES:
         ext_name[0]         = '\0';

         if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
            return NULL;

         exts = ext_name;
         break;
      case DIR_LIST_RECURSIVE:
         recursive = true;
         /* fall-through */
      case DIR_LIST_CORE_INFO:
         {
            core_info_list_t *list = NULL;
            core_info_get_list(&list);

            if (list)
               exts = list->all_ext;
         }
         break;
      case DIR_LIST_SHADERS:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         {
            union string_list_elem_attr attr;
            struct string_list str_list;

            if (!string_list_initialize(&str_list))
               return NULL;

            ext_shaders[0]                   = '\0';

            attr.i = 0;

            if (video_shader_is_supported(RARCH_SHADER_CG))
            {
               string_list_append(&str_list, "cgp", attr);
               string_list_append(&str_list, "cg", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_GLSL))
            {
               string_list_append(&str_list, "glslp", attr);
               string_list_append(&str_list, "glsl", attr);
            }

            if (video_shader_is_supported(RARCH_SHADER_SLANG))
            {
               string_list_append(&str_list, "slangp", attr);
               string_list_append(&str_list, "slang", attr);
            }

            string_list_join_concat(ext_shaders, sizeof(ext_shaders), &str_list, "|");
            string_list_deinitialize(&str_list);
            exts = ext_shaders;
         }
         break;
#else
         return NULL;
#endif
      case DIR_LIST_COLLECTIONS:
         exts = "lpl";
         break;
      case DIR_LIST_DATABASES:
         exts = "rdb";
         break;
      case DIR_LIST_PLAIN:
         exts = filter;
         break;
      case DIR_LIST_NONE:
      default:
         return NULL;
   }

   return dir_list_new(input_dir, exts, false,
         show_hidden_files,
         type == DIR_LIST_CORE_INFO, recursive);
}

struct string_list *string_list_new_special(enum string_list_type type,
      void *data, unsigned *len, size_t *list_size)
{
   union string_list_elem_attr attr;
   unsigned i;
   struct string_list *s = string_list_new();

   if (!s || !len)
      goto error;

   attr.i = 0;
   *len   = 0;

   switch (type)
   {
      case STRING_LIST_MENU_DRIVERS:
#ifdef HAVE_MENU
         for (i = 0; menu_ctx_drivers[i]; i++)
         {
            const char *opt  = menu_ctx_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            /* Don't allow the user to set menu driver to "null" using the UI.
             * Can prevent the user from locking him/herself out of the program. */
            if (string_is_not_equal(opt, "null"))
               string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_CAMERA_DRIVERS:
         for (i = 0; camera_drivers[i]; i++)
         {
            const char *opt  = camera_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_BLUETOOTH_DRIVERS:
#ifdef HAVE_BLUETOOTH
         for (i = 0; bluetooth_drivers[i]; i++)
         {
            const char *opt  = bluetooth_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_WIFI_DRIVERS:
#ifdef HAVE_WIFI
         for (i = 0; wifi_drivers[i]; i++)
         {
            const char *opt  = wifi_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
      case STRING_LIST_LOCATION_DRIVERS:
         for (i = 0; location_drivers[i]; i++)
         {
            const char *opt  = location_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_AUDIO_DRIVERS:
         for (i = 0; audio_drivers[i]; i++)
         {
            const char *opt  = audio_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_AUDIO_RESAMPLER_DRIVERS:
         for (i = 0; audio_resampler_driver_find_handle(i); i++)
         {
            const char *opt  = audio_resampler_driver_find_ident(i);
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_VIDEO_DRIVERS:
         for (i = 0; video_drivers[i]; i++)
         {
            const char *opt  = video_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            /* Don't allow the user to set video driver to "null" using the UI.
             * Can prevent the user from locking him/herself out of the program. */
            if (string_is_not_equal(opt, "null"))
               string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_DRIVERS:
         for (i = 0; input_drivers[i]; i++)
         {
            const char *opt  = input_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            /* Don't allow the user to set input driver to "null" using the UI.
             * Can prevent the user from locking him/herself out of the program. */
            if (string_is_not_equal(opt, "null"))
               string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_INPUT_HID_DRIVERS:
#ifdef HAVE_HID
         for (i = 0; hid_drivers[i]; i++)
         {
            const char *opt  = hid_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            /* Don't allow the user to set input HID driver to "null" using the UI.
             * Can prevent the user from locking him/herself out of the program. */
            if (string_is_not_equal(opt, "null"))
               string_list_append(s, opt, attr);
         }
#endif
         break;
      case STRING_LIST_INPUT_JOYPAD_DRIVERS:
         for (i = 0; joypad_drivers[i]; i++)
         {
            const char *opt  = joypad_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            /* Don't allow the user to set input joypad driver to "null" using the UI.
             * Can prevent the user from locking him/herself out of the program. */
            if (string_is_not_equal(opt, "null"))
               string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_RECORD_DRIVERS:
         for (i = 0; record_drivers[i]; i++)
         {
            const char *opt  = record_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
      case STRING_LIST_MIDI_DRIVERS:
         for (i = 0; midi_driver_find_handle(i); i++)
         {
            const char *opt  = midi_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#ifdef HAVE_LAKKA
      case STRING_LIST_TIMEZONES:
         {
            const char *opt  = DEFAULT_TIMEZONE;
            *len            += strlen(opt) + 1;
            string_list_append(s, opt, attr);

            FILE *zones_file = popen("grep -v ^# /usr/share/zoneinfo/zone.tab | "
                                     "cut -f3 | "
                                     "sort", "r");

            if (zones_file)
            {
               char zone_desc[TIMEZONE_LENGTH];
               while (fgets(zone_desc, TIMEZONE_LENGTH, zones_file))
               {
                  size_t zone_desc_len = strlen(zone_desc);

                  if (zone_desc_len > 0)
                     if (zone_desc[--zone_desc_len] == '\n')
                        zone_desc[zone_desc_len] = '\0';

                  if (zone_desc && zone_desc[0] != '\0')
                  {
                     const char *opt  = zone_desc;
                     *len            += strlen(opt) + 1;
                     string_list_append(s, opt, attr);
                  }
               }
               pclose(zones_file);
            }
         }
         break;
#endif
      case STRING_LIST_NONE:
      default:
         goto error;
   }

   return s;

error:
   string_list_free(s);
   s    = NULL;
   return NULL;
}

const char *char_list_new_special(enum string_list_type type, void *data)
{
   unsigned len = 0;
   size_t list_size;
   struct string_list *s = string_list_new_special(type, data, &len, &list_size);
   char         *options = (len > 0) ? (char*)calloc(len, sizeof(char)): NULL;

   if (options && s)
      string_list_join_concat(options, len, s, "|");

   string_list_free(s);
   s = NULL;

   return options;
}

void retroarch_path_set_redirect(settings_t *settings)
{
   char content_dir_name[PATH_MAX_LENGTH];
   char new_savefile_dir[PATH_MAX_LENGTH];
   char new_savestate_dir[PATH_MAX_LENGTH];
   struct rarch_state *p_rarch                 = &rarch_st;
   const char *old_savefile_dir                = p_rarch->dir_savefile;
   const char *old_savestate_dir               = p_rarch->dir_savestate;
   runloop_state_t *runloop_st                 = runloop_state_get_ptr();
   struct retro_system_info *system            = &runloop_st->system.info;
   bool sort_savefiles_enable                  = settings->bools.sort_savefiles_enable;
   bool sort_savefiles_by_content_enable       = settings->bools.sort_savefiles_by_content_enable;
   bool sort_savestates_enable                 = settings->bools.sort_savestates_enable;
   bool sort_savestates_by_content_enable      = settings->bools.sort_savestates_by_content_enable;
   bool savefiles_in_content_dir               = settings->bools.savefiles_in_content_dir;
   bool savestates_in_content_dir              = settings->bools.savestates_in_content_dir;

   content_dir_name[0]  = '\0';

   /* Initialize current save directories
    * with the values from the config. */
   strlcpy(new_savefile_dir,  old_savefile_dir,  sizeof(new_savefile_dir));
   strlcpy(new_savestate_dir, old_savestate_dir, sizeof(new_savestate_dir));

   /* Get content directory name, if per-content-directory
    * saves/states are enabled */
   if ((sort_savefiles_by_content_enable ||
         sort_savestates_by_content_enable) &&
       !string_is_empty(runloop_st->runtime_content_path_basename))
      fill_pathname_parent_dir_name(content_dir_name,
            runloop_st->runtime_content_path_basename,
            sizeof(content_dir_name));

   if (system && !string_is_empty(system->library_name))
   {
#ifdef HAVE_MENU
      if (!string_is_equal(system->library_name,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_CORE)))
#endif
      {
         /* Per-core and/or per-content-directory saves */
         if ((sort_savefiles_enable || sort_savefiles_by_content_enable)
               && !string_is_empty(old_savefile_dir))
         {
            /* Append content directory name to save location */
            if (sort_savefiles_by_content_enable)
               fill_pathname_join(
                     new_savefile_dir,
                     old_savefile_dir,
                     content_dir_name,
                     sizeof(new_savefile_dir));

            /* Append library_name to the save location */
            if (sort_savefiles_enable)
               fill_pathname_join(
                     new_savefile_dir,
                     new_savefile_dir,
                     system->library_name,
                     sizeof(new_savefile_dir));

            /* If path doesn't exist, try to create it,
             * if everything fails revert to the original path. */
            if (!path_is_directory(new_savefile_dir))
               if (!path_mkdir(new_savefile_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
                        old_savefile_dir);

                  strlcpy(new_savefile_dir, old_savefile_dir, sizeof(new_savefile_dir));
               }
         }

         /* Per-core and/or per-content-directory savestates */
         if ((sort_savestates_enable || sort_savestates_by_content_enable)
               && !string_is_empty(old_savestate_dir))
         {
            /* Append content directory name to savestate location */
            if (sort_savestates_by_content_enable)
               fill_pathname_join(
                     new_savestate_dir,
                     old_savestate_dir,
                     content_dir_name,
                     sizeof(new_savestate_dir));

            /* Append library_name to the savestate location */
            if (sort_savestates_enable)
            {
               fill_pathname_join(
                     new_savestate_dir,
                     new_savestate_dir,
                     system->library_name,
                     sizeof(new_savestate_dir));
            }

            /* If path doesn't exist, try to create it.
             * If everything fails, revert to the original path. */
            if (!path_is_directory(new_savestate_dir))
               if (!path_mkdir(new_savestate_dir))
               {
                  RARCH_LOG("%s %s\n",
                        msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
                        old_savestate_dir);
                  strlcpy(new_savestate_dir,
                        old_savestate_dir,
                        sizeof(new_savestate_dir));
               }
         }
      }
   }

   /* Set savefile directory if empty to content directory */
   if (string_is_empty(new_savefile_dir) || savefiles_in_content_dir)
   {
      strlcpy(new_savefile_dir,
            runloop_st->runtime_content_path_basename,
            sizeof(new_savefile_dir));
      path_basedir(new_savefile_dir);

      if (string_is_empty(new_savefile_dir))
         RARCH_LOG("Cannot resolve save file path.\n");
      else if (sort_savefiles_enable || sort_savefiles_by_content_enable)
         RARCH_LOG("Saving files in content directory is set. This overrides other save file directory settings.\n");
   }

   /* Set savestate directory if empty based on content directory */
   if (string_is_empty(new_savestate_dir) || savestates_in_content_dir)
   {
      strlcpy(new_savestate_dir,
            runloop_st->runtime_content_path_basename,
            sizeof(new_savestate_dir));
      path_basedir(new_savestate_dir);

      if (string_is_empty(new_savestate_dir))
         RARCH_LOG("Cannot resolve save state file path.\n");
      else if (sort_savestates_enable || sort_savestates_by_content_enable)
         RARCH_LOG("Saving save states in content directory is set. This overrides other save state file directory settings.\n");
   }

#ifdef HAVE_NETWORKING
   /* Special save directory for netplay clients. */
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
         !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL))
   {
      fill_pathname_join(new_savefile_dir, new_savefile_dir, ".netplay",
         sizeof(new_savefile_dir));

      if (!path_is_directory(new_savefile_dir) &&
            !path_mkdir(new_savefile_dir))
         path_basedir(new_savefile_dir);
   }
#endif

   if (system && !string_is_empty(system->library_name))
   {
      bool savefile_is_dir  = path_is_directory(new_savefile_dir);
      bool savestate_is_dir = path_is_directory(new_savestate_dir);
      if (savefile_is_dir)
         strlcpy(runloop_st->name.savefile, new_savefile_dir,
               sizeof(runloop_st->name.savefile));
      else
         savefile_is_dir    = path_is_directory(runloop_st->name.savefile);

      if (savestate_is_dir)
         strlcpy(runloop_st->name.savestate, new_savestate_dir,
               sizeof(runloop_st->name.savestate));
      else
         savestate_is_dir   = path_is_directory(runloop_st->name.savestate);

      if (savefile_is_dir)
      {
         fill_pathname_dir(runloop_st->name.savefile,
               !string_is_empty(runloop_st->runtime_content_path_basename)
               ? runloop_st->runtime_content_path_basename
               : system->library_name,
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
               : system->library_name,
               FILE_PATH_STATE_EXTENSION,
               sizeof(runloop_st->name.savestate));
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
               : system->library_name,
               FILE_PATH_CHT_EXTENSION,
               sizeof(runloop_st->name.cheatfile));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               runloop_st->name.cheatfile);
      }
#endif
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE,  new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
}

void path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   char str[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   bool is_dir                         = false;
   struct string_list subsystem_paths  = {0};
   runloop_state_t         *runloop_st = runloop_state_get_ptr();
   const char *savestate_dir           = runloop_st->savestate_dir;

   /* First content file is the significant one. */
   runloop_path_set_basename(argv[0]);

   string_list_initialize(&subsystem_paths);

   runloop_st->subsystem_fullpaths     = string_list_new();
   retro_assert(runloop_st->subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
   {
      string_list_append(runloop_st->subsystem_fullpaths, argv[i], attr);
      strlcpy(str, argv[i], sizeof(str));
      path_remove_extension(str);
      string_list_append(&subsystem_paths, path_basename(str), attr);
   }

   str[0] = '\0';
   string_list_join_concat(str, sizeof(str), &subsystem_paths, " + ");
   string_list_deinitialize(&subsystem_paths);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */
   is_dir = path_is_directory(savestate_dir);

   if (is_dir)
      strlcpy(runloop_st->name.savestate, savestate_dir,
            sizeof(runloop_st->name.savestate));
   else
      is_dir   = path_is_directory(runloop_st->name.savestate);

   if (is_dir)
   {
      fill_pathname_dir(runloop_st->name.savestate,
            str,
            ".state",
            sizeof(runloop_st->name.savestate));
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
            runloop_st->name.savestate);
   }
}

char *path_get_ptr(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return p_rarch->path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return p_rarch->path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return runloop_st->runtime_content_path_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return p_rarch->path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return runloop_st->subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return p_rarch->path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return p_rarch->path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return p_rarch->path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

const char *path_get(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return p_rarch->path_content;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return p_rarch->path_default_shader_preset;
      case RARCH_PATH_BASENAME:
         return runloop_st->runtime_content_path_basename;
      case RARCH_PATH_CORE_OPTIONS:
         if (!path_is_empty(RARCH_PATH_CORE_OPTIONS))
            return p_rarch->path_core_options_file;
         break;
      case RARCH_PATH_SUBSYSTEM:
         return runloop_st->subsystem_path;
      case RARCH_PATH_CONFIG:
         if (!path_is_empty(RARCH_PATH_CONFIG))
            return p_rarch->path_config_file;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (!path_is_empty(RARCH_PATH_CONFIG_APPEND))
            return p_rarch->path_config_append_file;
         break;
      case RARCH_PATH_CORE:
         return p_rarch->path_libretro;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return NULL;
}

size_t path_get_realsize(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return sizeof(p_rarch->path_content);
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return sizeof(p_rarch->path_default_shader_preset);
      case RARCH_PATH_BASENAME:
         return sizeof(runloop_st->runtime_content_path_basename);
      case RARCH_PATH_CORE_OPTIONS:
         return sizeof(p_rarch->path_core_options_file);
      case RARCH_PATH_SUBSYSTEM:
         return sizeof(runloop_st->subsystem_path);
      case RARCH_PATH_CONFIG:
         return sizeof(p_rarch->path_config_file);
      case RARCH_PATH_CONFIG_APPEND:
         return sizeof(p_rarch->path_config_append_file);
      case RARCH_PATH_CORE:
         return sizeof(p_rarch->path_libretro);
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return 0;
}

bool path_set(enum rarch_path_type type, const char *path)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   if (!path)
      return false;

   switch (type)
   {
      case RARCH_PATH_BASENAME:
         strlcpy(runloop_st->runtime_content_path_basename, path,
               sizeof(runloop_st->runtime_content_path_basename));
         break;
      case RARCH_PATH_NAMES:
         runloop_path_set_basename(path);
         runloop_path_set_names();
         retroarch_path_set_redirect(config_get_ptr());
         break;
      case RARCH_PATH_CORE:
         strlcpy(p_rarch->path_libretro, path,
               sizeof(p_rarch->path_libretro));
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         strlcpy(p_rarch->path_default_shader_preset, path,
               sizeof(p_rarch->path_default_shader_preset));
         break;
      case RARCH_PATH_CONFIG_APPEND:
         strlcpy(p_rarch->path_config_append_file, path,
               sizeof(p_rarch->path_config_append_file));
         break;
      case RARCH_PATH_CONFIG:
         strlcpy(p_rarch->path_config_file, path,
               sizeof(p_rarch->path_config_file));
         break;
      case RARCH_PATH_SUBSYSTEM:
         strlcpy(runloop_st->subsystem_path, path,
               sizeof(runloop_st->subsystem_path));
         break;
      case RARCH_PATH_CORE_OPTIONS:
         strlcpy(p_rarch->path_core_options_file, path,
               sizeof(p_rarch->path_core_options_file));
         break;
      case RARCH_PATH_CONTENT:
         strlcpy(p_rarch->path_content, path,
               sizeof(p_rarch->path_content));
         break;
      case RARCH_PATH_NONE:
         break;
   }

   return true;
}

bool path_is_empty(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         if (string_is_empty(p_rarch->path_default_shader_preset))
            return true;
         break;
      case RARCH_PATH_SUBSYSTEM:
         if (string_is_empty(runloop_st->subsystem_path))
            return true;
         break;
      case RARCH_PATH_CONFIG:
         if (string_is_empty(p_rarch->path_config_file))
            return true;
         break;
      case RARCH_PATH_CORE_OPTIONS:
         if (string_is_empty(p_rarch->path_core_options_file))
            return true;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (string_is_empty(p_rarch->path_config_append_file))
            return true;
         break;
      case RARCH_PATH_CONTENT:
         if (string_is_empty(p_rarch->path_content))
            return true;
         break;
      case RARCH_PATH_CORE:
         if (string_is_empty(p_rarch->path_libretro))
            return true;
         break;
      case RARCH_PATH_BASENAME:
         if (string_is_empty(runloop_st->runtime_content_path_basename))
            return true;
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }

   return false;
}

void path_clear(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_PATH_SUBSYSTEM:
         *runloop_st->subsystem_path = '\0';
         break;
      case RARCH_PATH_CORE:
         *p_rarch->path_libretro = '\0';
         break;
      case RARCH_PATH_CONFIG:
         *p_rarch->path_config_file = '\0';
         break;
      case RARCH_PATH_CONTENT:
         *p_rarch->path_content = '\0';
         break;
      case RARCH_PATH_BASENAME:
         *runloop_st->runtime_content_path_basename = '\0';
         break;
      case RARCH_PATH_CORE_OPTIONS:
         *p_rarch->path_core_options_file = '\0';
         break;
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         *p_rarch->path_default_shader_preset = '\0';
         break;
      case RARCH_PATH_CONFIG_APPEND:
         *p_rarch->path_config_append_file = '\0';
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
   }
}

static void path_clear_all(void)
{
   path_clear(RARCH_PATH_CONTENT);
   path_clear(RARCH_PATH_CONFIG);
   path_clear(RARCH_PATH_CONFIG_APPEND);
   path_clear(RARCH_PATH_CORE_OPTIONS);
   path_clear(RARCH_PATH_BASENAME);
}

static void ram_state_to_file(void)
{
   char state_path[PATH_MAX_LENGTH];

   if (!content_ram_state_pending())
      return;

   state_path[0] = '\0';

   if (retroarch_get_current_savestate_path(state_path, sizeof(state_path)))
      command_event(CMD_EVENT_RAM_STATE_TO_FILE, state_path);
}

enum rarch_content_type path_is_media_type(const char *path)
{
   char ext_lower[128];
   strlcpy(ext_lower, path_get_extension(path), sizeof(ext_lower));

   string_to_lower(ext_lower);

   /* hack, to detect livestreams so the ffmpeg core can be started */
   if (string_starts_with_size(path, "udp://",   STRLEN_CONST("udp://"))   ||
       string_starts_with_size(path, "http://",  STRLEN_CONST("http://"))  ||
       string_starts_with_size(path, "https://", STRLEN_CONST("https://")) ||
       string_starts_with_size(path, "tcp://",   STRLEN_CONST("tcp://"))   ||
       string_starts_with_size(path, "rtmp://",  STRLEN_CONST("rtmp://"))  ||
       string_starts_with_size(path, "rtp://",   STRLEN_CONST("rtp://")))
      return RARCH_CONTENT_MOVIE;

   switch (msg_hash_to_file_type(msg_hash_calculate(ext_lower)))
   {
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
      case FILE_TYPE_OGM:
      case FILE_TYPE_MKV:
      case FILE_TYPE_AVI:
      case FILE_TYPE_MP4:
      case FILE_TYPE_FLV:
      case FILE_TYPE_WEBM:
      case FILE_TYPE_3GP:
      case FILE_TYPE_3G2:
      case FILE_TYPE_F4F:
      case FILE_TYPE_F4V:
      case FILE_TYPE_MOV:
      case FILE_TYPE_WMV:
      case FILE_TYPE_MPG:
      case FILE_TYPE_MPEG:
      case FILE_TYPE_VOB:
      case FILE_TYPE_ASF:
      case FILE_TYPE_DIVX:
      case FILE_TYPE_M2P:
      case FILE_TYPE_M2TS:
      case FILE_TYPE_PS:
      case FILE_TYPE_TS:
      case FILE_TYPE_MXF:
         return RARCH_CONTENT_MOVIE;
      case FILE_TYPE_WMA:
      case FILE_TYPE_OGG:
      case FILE_TYPE_MP3:
      case FILE_TYPE_M4A:
      case FILE_TYPE_FLAC:
      case FILE_TYPE_WAV:
         return RARCH_CONTENT_MUSIC;
#endif
#ifdef HAVE_IMAGEVIEWER
      case FILE_TYPE_JPEG:
      case FILE_TYPE_PNG:
      case FILE_TYPE_TGA:
      case FILE_TYPE_BMP:
         return RARCH_CONTENT_IMAGE;
#endif
#ifdef HAVE_IBXM
      case FILE_TYPE_MOD:
      case FILE_TYPE_S3M:
      case FILE_TYPE_XM:
         return RARCH_CONTENT_MUSIC;
#endif

      case FILE_TYPE_NONE:
      default:
         break;
   }

   return RARCH_CONTENT_NONE;
}

static void path_deinit_subsystem(runloop_state_t *runloop_st)
{
   if (runloop_st->subsystem_fullpaths)
      string_list_free(runloop_st->subsystem_fullpaths);
   runloop_st->subsystem_fullpaths = NULL;
}

/* get size functions */

size_t dir_get_size(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return sizeof(p_rarch->dir_system);
      case RARCH_DIR_SAVESTATE:
         return sizeof(p_rarch->dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return sizeof(runloop_st->savestate_dir);
      case RARCH_DIR_SAVEFILE:
         return sizeof(p_rarch->dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return sizeof(runloop_st->savefile_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return 0;
}

/* clear functions */

void dir_clear(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         *p_rarch->dir_savefile = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVEFILE:
         *runloop_st->savefile_dir = '\0';
         break;
      case RARCH_DIR_SAVESTATE:
         *p_rarch->dir_savestate = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         *runloop_st->savestate_dir = '\0';
         break;
      case RARCH_DIR_SYSTEM:
         *p_rarch->dir_system = '\0';
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

static void dir_clear_all(void)
{
   dir_clear(RARCH_DIR_SYSTEM);
   dir_clear(RARCH_DIR_SAVEFILE);
   dir_clear(RARCH_DIR_SAVESTATE);
}

/* get ptr functions */

char *dir_get_ptr(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return p_rarch->dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return runloop_st->savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return p_rarch->dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return runloop_st->savestate_dir;
      case RARCH_DIR_SYSTEM:
         return p_rarch->dir_system;
      case RARCH_DIR_NONE:
         break;
   }

   return NULL;
}

void dir_set(enum rarch_dir_type type, const char *path)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   switch (type)
   {
      case RARCH_DIR_CURRENT_SAVEFILE:
         strlcpy(runloop_st->savefile_dir, path,
               sizeof(runloop_st->savefile_dir));
         break;
      case RARCH_DIR_SAVEFILE:
         strlcpy(p_rarch->dir_savefile, path,
               sizeof(p_rarch->dir_savefile));
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         strlcpy(runloop_st->savestate_dir, path,
               sizeof(runloop_st->savestate_dir));
         break;
      case RARCH_DIR_SAVESTATE:
         strlcpy(p_rarch->dir_savestate, path,
               sizeof(p_rarch->dir_savestate));
         break;
      case RARCH_DIR_SYSTEM:
         strlcpy(p_rarch->dir_system, path,
               sizeof(p_rarch->dir_system));
         break;
      case RARCH_DIR_NONE:
         break;
   }
}

void dir_check_defaults(const char *custom_ini_path)
{
   size_t i;

   /* Early return for people with a custom folder setup
    * so it doesn't create unnecessary directories */
   if (!string_is_empty(custom_ini_path) &&
       path_is_valid(custom_ini_path))
      return;

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      char new_path[PATH_MAX_LENGTH];

      if (string_is_empty(dir_path))
         continue;

      new_path[0] = '\0';
      fill_pathname_expand_special(new_path,
            dir_path, sizeof(new_path));

      if (!path_is_directory(new_path))
         path_mkdir(new_path);
   }
}

#ifdef HAVE_ACCESSIBILITY
bool is_accessibility_enabled(bool accessibility_enable, bool accessibility_enabled)
{
   return accessibility_enabled || accessibility_enable;
}
#endif

/**
 * command_event:
 * @cmd                  : Event command index.
 *
 * Performs program event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event(enum event_command cmd, void *data)
{
   bool boolean                    = false;
#if defined(HAVE_DISCORD) || defined(HAVE_NETWORKING)
   struct rarch_state *p_rarch     = &rarch_st;
#endif
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   uico_driver_state_t *uico_st    = uico_state_get_ptr();
#if defined(HAVE_ACCESSIBILITY) || defined(HAVE_TRANSLATE)
   access_state_t *access_st       = access_state_get_ptr();
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st      = menu_state_get_ptr();
#endif
   video_driver_state_t *video_st  = video_state_get_ptr();
   settings_t *settings            = config_get_ptr();
   recording_state_t *recording_st = recording_state_get_ptr();

   switch (cmd)
   {
      case CMD_EVENT_SAVE_FILES:
         event_save_files(runloop_st->use_sram);
         break;
      case CMD_EVENT_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         input_overlay_deinit();
#endif
#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
         /* Because the overlay is a display widget,
          * it's going to be written
          * over the menu, so we unset it here. */
         if (dispwidget_get_ptr()->ai_service_overlay_state != 0)
            gfx_widgets_ai_service_overlay_unload();
#endif
         break;
      case CMD_EVENT_OVERLAY_INIT:
#ifdef HAVE_OVERLAY
         input_overlay_init();
#endif
         break;
      case CMD_EVENT_CHEAT_INDEX_PLUS:
#ifdef HAVE_CHEATS
         cheat_manager_index_next();
#endif
         break;
      case CMD_EVENT_CHEAT_INDEX_MINUS:
#ifdef HAVE_CHEATS
         cheat_manager_index_prev();
#endif
         break;
      case CMD_EVENT_CHEAT_TOGGLE:
#ifdef HAVE_CHEATS
         cheat_manager_toggle();
#endif
         break;
      case CMD_EVENT_SHADER_NEXT:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_MENU
         dir_check_shader(menu_st->driver_data, settings,
               &video_st->dir_shader_list, true, false);
#else
         dir_check_shader(NULL, settings,
               &video_st->dir_shader_list, true, false);
#endif
#endif
         break;
      case CMD_EVENT_SHADER_PREV:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_MENU
         dir_check_shader(menu_st->driver_data, settings,
               &video_st->dir_shader_list, false, true);
#else
         dir_check_shader(NULL, settings,
               &video_st->dir_shader_list, false, true);
#endif
#endif
         break;
      case CMD_EVENT_BSV_RECORDING_TOGGLE:
         {
#ifdef HAVE_BSV_MOVIE
            input_driver_state_t *input_st = input_state_get_ptr();
            if (!recording_st->enable)
               command_event(CMD_EVENT_RECORD_INIT, NULL);
            else
               command_event(CMD_EVENT_RECORD_DEINIT, NULL);
            bsv_movie_check(input_st, settings);
#endif
         }
         break;
      case CMD_EVENT_AI_SERVICE_TOGGLE:
         {
#ifdef HAVE_TRANSLATE
            bool ai_service_pause     = settings->bools.ai_service_pause;

            if (!settings->bools.ai_service_enable)
               break;

            if (ai_service_pause)
            {
               /* pause on call, unpause on second press. */
               if (!runloop_st->paused)
               {
                  command_event(CMD_EVENT_PAUSE, NULL);
                  command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
               }
               else
               {
#ifdef HAVE_ACCESSIBILITY
                  bool accessibility_enable = settings->bools.accessibility_enable;
                  unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
                  if (is_accessibility_enabled(
                           accessibility_enable,
                           access_st->enabled))
                     accessibility_speak_priority(
                           accessibility_enable,
                           accessibility_narrator_speech_speed,
                           (char*)msg_hash_to_str(MSG_UNPAUSED), 10);
#endif
                  command_event(CMD_EVENT_UNPAUSE, NULL);
               }
            }
            else
            {
               /* Don't pause - useful for Text-To-Speech since
                * the audio can't currently play while paused.
                * Also useful for cases when users don't want the
                * core's sound to stop while translating.
                *
                * Also, this mode is required for "auto" translation
                * packages, since you don't want to pause for that.
                */
               if (access_st->ai_service_auto == 2)
               {
                  /* Auto mode was turned on, but we pressed the
                   * toggle button, so turn it off now. */
                  access_st->ai_service_auto = 0;
#ifdef HAVE_MENU_WIDGETS
                  gfx_widgets_ai_service_overlay_unload();
#endif
               }
               else
                  command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
            }
#endif
            break;
         }
      case CMD_EVENT_STREAMING_TOGGLE:
         if (recording_st->streaming_enable)
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         else
         {
            streaming_set_state(true);
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         }
         break;
      case CMD_EVENT_RUNAHEAD_TOGGLE:
         {
            char msg[256];
            msg[0] = '\0';

            if (!core_info_current_supports_runahead())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD),
                     1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               break;
            }

            settings->bools.run_ahead_enabled =
               !(settings->bools.run_ahead_enabled);

            if (!settings->bools.run_ahead_enabled)
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_DISABLED),
                     1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else if (!settings->bools.run_ahead_secondary_instance)
            {
               snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_RUNAHEAD_ENABLED),
                     settings->uints.run_ahead_frames);

               runloop_msg_queue_push(
                     msg, 1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else
            {
               snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE),
                     settings->uints.run_ahead_frames);

               runloop_msg_queue_push(
                     msg, 1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }
         break;
      case CMD_EVENT_RECORDING_TOGGLE:
         if (recording_st->enable)
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         else
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         break;
      case CMD_EVENT_OSK_TOGGLE:
         {
            input_driver_state_t *input_st   = input_state_get_ptr();
            if (input_st->keyboard_linefeed_enable)
               input_st->keyboard_linefeed_enable = false;
            else
               input_st->keyboard_linefeed_enable = true;
         }
         break;
      case CMD_EVENT_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;
            char desc[64] = {0};

            command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

            if (video_driver_get_video_output_size(&width, &height, desc, sizeof(desc)))
            {
               char msg[128] = {0};

               video_driver_set_video_mode(width, height, true);

               if (width == 0 || height == 0)
                  snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SCREEN_RESOLUTION_DEFAULT));
               else
               {
                  if (!string_is_empty(desc))
                     snprintf(msg, sizeof(msg),
                        msg_hash_to_str(MSG_SCREEN_RESOLUTION_DESC),
                        width, height, desc);
                  else
                     snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_SCREEN_RESOLUTION_NO_DESC),
                        width, height);
               }

               runloop_msg_queue_push(msg, 1, 100, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }
#endif
         break;
      case CMD_EVENT_LOAD_CORE_PERSIST:
         {
            rarch_system_info_t *system_info = &runloop_st->system;
            struct retro_system_info *system = &system_info->info;
            const char *core_path            = path_get(RARCH_PATH_CORE);

#if defined(HAVE_DYNAMIC)
            if (string_is_empty(core_path))
               return false;
#endif

            if (!libretro_get_system_info(
                     core_path,
                     system,
                     &system_info->load_no_content))
               return false;

            if (!core_info_load(core_path))
            {
#ifdef HAVE_DYNAMIC
               return false;
#endif
            }
         }
         break;
      case CMD_EVENT_LOAD_CORE:
         runloop_st->subsystem_current_count = 0;
         content_clear_subsystem();
#ifdef HAVE_DYNAMIC
         if (!(command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL)))
            return false;
#else
         command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
         command_event(CMD_EVENT_QUIT, NULL);
#endif
         break;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
      case CMD_EVENT_LOAD_SECOND_CORE:
         if (!runloop_st->core_running ||
             !runloop_st->runahead_secondary_core_available)
            return false;
         if (runloop_st->secondary_lib_handle)
            return true;
         if (!secondary_core_ensure_exists(settings))
         {
            runloop_secondary_core_destroy();
            runloop_st->runahead_secondary_core_available = false;
            return false;
         }
         return true;
#endif
      case CMD_EVENT_LOAD_STATE:
         {
#ifdef HAVE_BSV_MOVIE
            /* Immutable - disallow savestate load when
             * we absolutely cannot change game state. */
            input_driver_state_t *input_st   = input_state_get_ptr();
            if (input_st->bsv_movie_state_handle)
               return false;
#endif

#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
               return false;
            }
#endif
            if (!command_event_main_state(cmd))
               return false;
         }
         break;
      case CMD_EVENT_UNDO_LOAD_STATE:
      case CMD_EVENT_UNDO_SAVE_STATE:
      case CMD_EVENT_LOAD_STATE_FROM_RAM:
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_RAM_STATE_TO_FILE:
         if (!content_ram_state_to_file((char *) data))
            return false;
         break;
      case CMD_EVENT_RESIZE_WINDOWED_SCALE:
         if
            (!command_event_resize_windowed_scale
             (settings,
              runloop_st->pending_windowed_scale))
            return false;
         break;
      case CMD_EVENT_MENU_TOGGLE:
#ifdef HAVE_MENU
         if (menu_st->alive)
            retroarch_menu_running_finished(false);
         else
            retroarch_menu_running();
#endif
         break;
      case CMD_EVENT_RESET:
         RARCH_LOG("[Core]: %s.\n", msg_hash_to_str(MSG_RESET));
         runloop_msg_queue_push(msg_hash_to_str(MSG_RESET), 1, 120, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

         core_reset();
#ifdef HAVE_CHEEVOS
#ifdef HAVE_GFX_WIDGETS
         rcheevos_reset_game(dispwidget_get_ptr()->active);
#else
         rcheevos_reset_game(false);
#endif
#endif
#if HAVE_NETWORKING
         netplay_driver_ctl(RARCH_NETPLAY_CTL_RESET, NULL);
#endif
         /* Recalibrate frame delay target */
         if (settings->bools.video_frame_delay_auto)
            video_st->frame_delay_target = 0;

         return false;
      case CMD_EVENT_SAVE_STATE:
      case CMD_EVENT_SAVE_STATE_TO_RAM:
         {
            int state_slot            = settings->ints.state_slot;

            if (settings->bools.savestate_auto_index)
            {
               int new_state_slot = state_slot + 1;
               configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
            }
         }
         if (!command_event_main_state(cmd))
            return false;
         break;
      case CMD_EVENT_SAVE_STATE_DECREMENT:
         {
            int state_slot            = settings->ints.state_slot;

            /* Slot -1 is (auto) slot. */
            if (state_slot >= 0)
            {
               int new_state_slot = state_slot - 1;
               configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
            }
         }
         break;
      case CMD_EVENT_SAVE_STATE_INCREMENT:
         {
            int new_state_slot        = settings->ints.state_slot + 1;
            configuration_set_int(settings, settings->ints.state_slot, new_state_slot);
         }
         break;
      case CMD_EVENT_TAKE_SCREENSHOT:
#ifdef HAVE_SCREENSHOTS
         {
            const char *dir_screenshot = settings->paths.directory_screenshot;
            if (!take_screenshot(dir_screenshot,
                     path_get(RARCH_PATH_BASENAME), false,
                     video_driver_cached_frame_has_valid_framebuffer(), false, true))
               return false;
         }
#endif
         break;
      case CMD_EVENT_UNLOAD_CORE:
         {
            bool load_dummy_core            = data ? *(bool*)data : true;
            bool contentless                = false;
            bool is_inited                  = false;
            content_ctx_info_t content_info = {0};
            global_t   *global              = global_get_ptr();
            rarch_system_info_t *sys_info   = &runloop_st->system;

            content_get_status(&contentless, &is_inited);

            runloop_st->core_running        = false;

            /* The platform that uses ram_state_save calls it when the content
             * ends and writes it to a file */
            ram_state_to_file();

            /* Save last selected disk index, if required */
            if (sys_info)
               disk_control_save_image_index(&sys_info->disk_control);

            runloop_runtime_log_deinit(runloop_st,
                  settings->bools.content_runtime_log,
                  settings->bools.content_runtime_log_aggregate,
                  settings->paths.directory_runtime_log,
                  settings->paths.directory_playlist);
            command_event_save_auto_state(
                  settings->bools.savestate_auto_save,
                  runloop_st->current_core_type);

            if (     runloop_st->remaps_core_active
                  || runloop_st->remaps_content_dir_active
                  || runloop_st->remaps_game_active
                  || !string_is_empty(runloop_st->name.remapfile)
               )
            {
               input_remapping_deinit(settings->bools.remap_save_on_exit);
               input_remapping_set_defaults(true);
            }
            else
               input_remapping_restore_global_config(true);

#ifdef HAVE_CONFIGFILE
            if (runloop_st->overrides_active)
            {
               /* Reload the original config */
               config_unload_override();
               runloop_st->overrides_active = false;

               if (!settings->bools.video_fullscreen)
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  video_driver_show_mouse();
                  if (input_driver_ungrab_mouse())
                     input_st->grab_mouse_state = false;
               }
            }
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            runloop_st->runtime_shader_preset_path[0] = '\0';
#endif

            video_driver_restore_cached(settings);

            if (is_inited && load_dummy_core)
            {
#ifdef HAVE_MENU
               if (  (settings->uints.quit_on_close_content == QUIT_ON_CLOSE_CONTENT_CLI && global->launched_from_cli)
                     || settings->uints.quit_on_close_content == QUIT_ON_CLOSE_CONTENT_ENABLED
                  )
                  command_event(CMD_EVENT_QUIT, NULL);
#endif
               if (!task_push_start_dummy_core(&content_info))
                  return false;
            }
#ifdef HAVE_PRESENCE
            {
               presence_userdata_t userdata;
               userdata.status = PRESENCE_NETPLAY_NETPLAY_STOPPED;
               command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
               userdata.status = PRESENCE_MENU;
               command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
            }
#endif
#ifdef HAVE_DYNAMIC
            path_clear(RARCH_PATH_CORE);
            runloop_system_info_free();
#endif
            {
               audio_driver_state_t
                  *audio_st                  = audio_state_get_ptr();
               audio_st->callback.callback   = NULL;
               audio_st->callback.set_state  = NULL;
            }
            if (is_inited)
            {
               runloop_st->subsystem_current_count = 0;
               content_clear_subsystem();
            }
         }
         break;
      case CMD_EVENT_CLOSE_CONTENT:
#ifdef HAVE_MENU
         /* Closing content via hotkey requires toggling menu
          * and resetting the position later on to prevent
          * going to empty Quick Menu */
         if (!menu_state_get_ptr()->alive)
         {
            menu_state_get_ptr()->pending_close_content = true;
            command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         }
#else
         command_event(CMD_EVENT_QUIT, NULL);
#endif
         break;
      case CMD_EVENT_QUIT:
         if (!retroarch_main_quit())
            return false;
         break;
      case CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE:
#ifdef HAVE_CHEEVOS
         rcheevos_toggle_hardcore_paused();
#endif
         break;
      case CMD_EVENT_REINIT_FROM_TOGGLE:
         video_st->force_fullscreen = false;
         /* this fallthrough is on purpose, it should do
            a CMD_EVENT_REINIT too */
      case CMD_EVENT_REINIT:
         command_event_reinit(
               data ? *(const int*)data : DRIVERS_CMD_ALL);
         break;
      case CMD_EVENT_CHEATS_APPLY:
#ifdef HAVE_CHEATS
         cheat_manager_apply_cheats();
#endif
         break;
      case CMD_EVENT_REWIND_DEINIT:
#ifdef HAVE_REWIND
         {
            bool core_type_is_dummy   = runloop_st->current_core_type == CORE_TYPE_DUMMY;

            if (core_type_is_dummy)
               return false;

            state_manager_event_deinit(&runloop_st->rewind_st,
                  &runloop_st->current_core);
         }
#endif
         break;
      case CMD_EVENT_REWIND_INIT:
#ifdef HAVE_REWIND
         {
            bool rewind_enable        = settings->bools.rewind_enable;
            size_t rewind_buf_size    = settings->sizes.rewind_buffer_size;
            bool core_type_is_dummy   = runloop_st->current_core_type == CORE_TYPE_DUMMY;

            if (core_type_is_dummy)
               return false;
#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active())
               return false;
#endif
            if (rewind_enable)
            {
#ifdef HAVE_NETWORKING
               /* Only enable state manager if netplay is not underway
                  TODO/FIXME: Add a setting for these tweaks */
               if (!netplay_driver_ctl(
                        RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
#endif
               {
                  state_manager_event_init(&runloop_st->rewind_st,
                        (unsigned)rewind_buf_size);
               }
            }
         }
#endif
         break;
      case CMD_EVENT_REWIND_TOGGLE:
#ifdef HAVE_REWIND
         {
            bool rewind_enable        = settings->bools.rewind_enable;
            if (rewind_enable)
               command_event(CMD_EVENT_REWIND_INIT, NULL);
            else
               command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         }
#endif
         break;
      case CMD_EVENT_AUTOSAVE_INIT:
#ifdef HAVE_THREADS
         if (runloop_st->use_sram)
            autosave_deinit();
         {
#ifdef HAVE_NETWORKING
            unsigned autosave_interval =
               settings->uints.autosave_interval;
            /* Only enable state manager if netplay is not underway
               TODO/FIXME: Add a setting for these tweaks */
            if (      (autosave_interval != 0)
                  && !netplay_driver_ctl(
                     RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
#endif
               runloop_st->autosave = autosave_init();
         }
#endif
         break;
      case CMD_EVENT_AUDIO_STOP:
         midi_driver_set_all_sounds_off();
         if (!audio_driver_stop())
            return false;
         break;
      case CMD_EVENT_AUDIO_START:
         if (!audio_driver_start(runloop_st->shutdown_initiated))
            return false;
         break;
      case CMD_EVENT_AUDIO_MUTE_TOGGLE:
         {
            audio_driver_state_t
               *audio_st                       = audio_state_get_ptr();
            bool audio_mute_enable             =
               *(audio_get_bool_ptr(AUDIO_ACTION_MUTE_ENABLE));
            const char *msg                    = !audio_mute_enable ?
               msg_hash_to_str(MSG_AUDIO_MUTED):
               msg_hash_to_str(MSG_AUDIO_UNMUTED);

            audio_st->mute_enable  =
               !audio_st->mute_enable;

#if defined(HAVE_GFX_WIDGETS)
            if (dispwidget_get_ptr()->active)
               gfx_widget_volume_update_and_show(
                     settings->floats.audio_volume,
                     audio_st->mute_enable);
            else
#endif
               runloop_msg_queue_push(msg, 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_SEND_DEBUG_INFO:
         break;
      case CMD_EVENT_FPS_TOGGLE:
         settings->bools.video_fps_show = !(settings->bools.video_fps_show);
         break;
      case CMD_EVENT_STATISTICS_TOGGLE:
         settings->bools.video_statistics_show = !(settings->bools.video_statistics_show);
         break;
      case CMD_EVENT_OVERLAY_NEXT:
         /* Switch to the next available overlay screen. */
#ifdef HAVE_OVERLAY
         {
            bool *check_rotation           = (bool*)data;
            video_driver_state_t
               *video_st                   = video_state_get_ptr();
            input_driver_state_t *input_st = input_state_get_ptr();
            bool inp_overlay_auto_rotate   = settings->bools.input_overlay_auto_rotate;
            float input_overlay_opacity    = settings->floats.input_overlay_opacity;
            if (!input_st->overlay_ptr)
               return false;

            input_st->overlay_ptr->index   = input_st->overlay_ptr->next_index;
            input_st->overlay_ptr->active  = &input_st->overlay_ptr->overlays[
               input_st->overlay_ptr->index];

            input_overlay_load_active(input_st->overlay_visibility,
                  input_st->overlay_ptr, input_overlay_opacity);

            input_st->overlay_ptr->blocked    = true;
            input_st->overlay_ptr->next_index = (unsigned)((input_st->overlay_ptr->index + 1) % input_st->overlay_ptr->size);

            /* Check orientation, if required */
            if (inp_overlay_auto_rotate)
               if (check_rotation)
                  if (*check_rotation)
                     input_overlay_auto_rotate_(
                           video_st->width,
                           video_st->height,
                           settings->bools.input_overlay_enable,
                           input_st->overlay_ptr);
         }
#endif
         break;
      case CMD_EVENT_DSP_FILTER_INIT:
#ifdef HAVE_DSP_FILTER
         {
            const char *path_audio_dsp_plugin = settings->paths.path_audio_dsp_plugin;
            audio_driver_dsp_filter_free();
            if (string_is_empty(path_audio_dsp_plugin))
               break;
            if (!audio_driver_dsp_filter_init(path_audio_dsp_plugin))
            {
               RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n",
                     path_audio_dsp_plugin);
            }
         }
#endif
         break;
      case CMD_EVENT_RECORD_DEINIT:
         recording_st->enable = false;
         streaming_set_state(false);
         if (!recording_deinit())
            return false;
         break;
      case CMD_EVENT_RECORD_INIT:
         recording_st->enable = true;
         if (!recording_init())
         {
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
            return false;
         }
         break;
      case CMD_EVENT_HISTORY_DEINIT:
         if (g_defaults.content_history)
         {
            playlist_write_file(g_defaults.content_history);
            playlist_free(g_defaults.content_history);
         }
         g_defaults.content_history = NULL;

         if (g_defaults.music_history)
         {
            playlist_write_file(g_defaults.music_history);
            playlist_free(g_defaults.music_history);
         }
         g_defaults.music_history = NULL;

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
         if (g_defaults.video_history)
         {
            playlist_write_file(g_defaults.video_history);
            playlist_free(g_defaults.video_history);
         }
         g_defaults.video_history = NULL;
#endif

#ifdef HAVE_IMAGEVIEWER
         if (g_defaults.image_history)
         {
            playlist_write_file(g_defaults.image_history);
            playlist_free(g_defaults.image_history);
         }
         g_defaults.image_history = NULL;
#endif
         break;
      case CMD_EVENT_HISTORY_INIT:
         {
            playlist_config_t playlist_config;
            bool history_list_enable               = settings->bools.history_list_enable;
            const char *path_content_history       = settings->paths.path_content_history;
            const char *path_content_music_history = settings->paths.path_content_music_history;
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            const char *path_content_video_history = settings->paths.path_content_video_history;
#endif
#ifdef HAVE_IMAGEVIEWER
            const char *path_content_image_history = settings->paths.path_content_image_history;
#endif
            playlist_config.capacity               = settings->uints.content_history_size;
            playlist_config.old_format             = settings->bools.playlist_use_old_format;
            playlist_config.compress               = settings->bools.playlist_compression;
            playlist_config.fuzzy_archive_match    = settings->bools.playlist_fuzzy_archive_match;
            /* don't use relative paths for content, music, video, and image histories */
            playlist_config_set_base_content_directory(&playlist_config, NULL);

            command_event(CMD_EVENT_HISTORY_DEINIT, NULL);

            if (!history_list_enable)
               return false;

            /* Note: Sorting is disabled by default for
             * all content history playlists */
            RARCH_LOG("[Playlist]: %s: \"%s\".\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_history);
            playlist_config_set_path(&playlist_config, path_content_history);
            g_defaults.content_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.content_history, PLAYLIST_SORT_MODE_OFF);

            RARCH_LOG("[Playlist]: %s: \"%s\".\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_music_history);
            playlist_config_set_path(&playlist_config, path_content_music_history);
            g_defaults.music_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.music_history, PLAYLIST_SORT_MODE_OFF);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            RARCH_LOG("[Playlist]: %s: \"%s\".\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_video_history);
            playlist_config_set_path(&playlist_config, path_content_video_history);
            g_defaults.video_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.video_history, PLAYLIST_SORT_MODE_OFF);
#endif

#ifdef HAVE_IMAGEVIEWER
            RARCH_LOG("[Playlist]: %s: \"%s\".\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_image_history);
            playlist_config_set_path(&playlist_config, path_content_image_history);
            g_defaults.image_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.image_history, PLAYLIST_SORT_MODE_OFF);
#endif
         }
         break;
      case CMD_EVENT_CORE_INFO_DEINIT:
         core_info_deinit_list();
         core_info_free_current_core();
         break;
      case CMD_EVENT_CORE_INFO_INIT:
         {
            char ext_name[255];
            const char *dir_libretro       = settings->paths.directory_libretro;
            const char *path_libretro_info = settings->paths.path_libretro_info;
            bool show_hidden_files         = settings->bools.show_hidden_files;
            bool core_info_cache_enable    = settings->bools.core_info_cache_enable;

            ext_name[0]                    = '\0';

            command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);

            if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
               return false;

            if (!string_is_empty(dir_libretro))
            {
               bool cache_supported = false;

               core_info_init_list(path_libretro_info,
                     dir_libretro,
                     ext_name,
                     show_hidden_files,
                     core_info_cache_enable,
                     &cache_supported);

               /* If core info cache is enabled but cache
                * functionality is unsupported (i.e. because
                * the core info directory is on read-only
                * storage), force-disable the setting to
                * avoid repeated failures */
               if (core_info_cache_enable && !cache_supported)
                  configuration_set_bool(settings,
                        settings->bools.core_info_cache_enable, false);
            }
         }
         break;
      case CMD_EVENT_CORE_DEINIT:
         {
            struct retro_hw_render_callback *hwr = NULL;
            video_driver_state_t
               *video_st                         = video_state_get_ptr();
            rarch_system_info_t *sys_info        = &runloop_st->system;

            /* The platform that uses ram_state_save calls it when the content
             * ends and writes it to a file */
            ram_state_to_file();

            /* Save last selected disk index, if required */
            if (sys_info)
               disk_control_save_image_index(&sys_info->disk_control);

            runloop_runtime_log_deinit(runloop_st,
                  settings->bools.content_runtime_log,
                  settings->bools.content_runtime_log_aggregate,
                  settings->paths.directory_runtime_log,
                  settings->paths.directory_playlist);
            content_reset_savestate_backups();
            hwr = VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
#ifdef HAVE_CHEEVOS
            rcheevos_unload();
#endif
            runloop_event_deinit_core();

#ifdef HAVE_RUNAHEAD
            /* If 'runahead_available' is false, then
             * runahead is enabled by the user but an
             * error occurred while the core was running
             * (typically a save state issue). In this
             * case we have to 'manually' reset the runahead
             * runtime variables, otherwise runahead will
             * remain disabled until the user restarts
             * RetroArch */
            if (!runloop_st->runahead_available)
               runloop_runahead_clear_variables(runloop_st);
#endif

            if (hwr)
               memset(hwr, 0, sizeof(*hwr));

            break;
         }
      case CMD_EVENT_CORE_INIT:
         {
            enum rarch_core_type *type     = (enum rarch_core_type*)data;
            rarch_system_info_t *sys_info  = &runloop_st->system;
            input_driver_state_t *input_st = input_state_get_ptr();
            audio_driver_state_t *audio_st = audio_state_get_ptr();

            content_reset_savestate_backups();

            /* Ensure that disk control interface is reset */
            if (sys_info)
               disk_control_set_ext_callback(&sys_info->disk_control, NULL);

            /* Ensure that audio callback interface is reset */
            audio_st->callback.callback  = NULL;
            audio_st->callback.set_state = NULL;

            if (!type || !runloop_event_init_core(settings, input_st, *type))
            {
               /* If core failed to initialise, audio callback
                * interface may be assigned invalid function
                * pointers -> ensure it is reset */
               audio_st->callback.callback  = NULL;
               audio_st->callback.set_state = NULL;
               return false;
            }
         }
         break;
      case CMD_EVENT_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case CMD_EVENT_VIDEO_SET_BLOCKING_STATE:
         {
            bool adaptive_vsync       = settings->bools.video_adaptive_vsync;
            unsigned swap_interval    = runloop_get_video_swap_interval(
                  settings->uints.video_swap_interval);
            video_driver_state_t
               *video_st              = video_state_get_ptr();

            if (video_st->current_video->set_nonblock_state)
               video_st->current_video->set_nonblock_state(
                     video_st->data, false,
                     video_driver_test_all_flags(
                        GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
                     adaptive_vsync, swap_interval);
         }
         break;
      case CMD_EVENT_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio();
         break;
      case CMD_EVENT_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         {
            overlay_layout_desc_t layout_desc;
            video_driver_state_t
               *video_st                        = video_state_get_ptr();
            input_driver_state_t *input_st      = input_state_get_ptr();

            layout_desc.scale_landscape         = settings->floats.input_overlay_scale_landscape;
            layout_desc.aspect_adjust_landscape = settings->floats.input_overlay_aspect_adjust_landscape;
            layout_desc.x_separation_landscape  = settings->floats.input_overlay_x_separation_landscape;
            layout_desc.y_separation_landscape  = settings->floats.input_overlay_y_separation_landscape;
            layout_desc.x_offset_landscape      = settings->floats.input_overlay_x_offset_landscape;
            layout_desc.y_offset_landscape      = settings->floats.input_overlay_y_offset_landscape;
            layout_desc.scale_portrait          = settings->floats.input_overlay_scale_portrait;
            layout_desc.aspect_adjust_portrait  = settings->floats.input_overlay_aspect_adjust_portrait;
            layout_desc.x_separation_portrait   = settings->floats.input_overlay_x_separation_portrait;
            layout_desc.y_separation_portrait   = settings->floats.input_overlay_y_separation_portrait;
            layout_desc.x_offset_portrait       = settings->floats.input_overlay_x_offset_portrait;
            layout_desc.y_offset_portrait       = settings->floats.input_overlay_y_offset_portrait;
            layout_desc.touch_scale             = (float)settings->uints.input_touch_scale;
            layout_desc.auto_scale              = settings->bools.input_overlay_auto_scale;

            input_overlay_set_scale_factor(input_st->overlay_ptr,
                  &layout_desc,
                  video_st->width,
                  video_st->height);
         }
#endif
         break;
      case CMD_EVENT_OVERLAY_SET_ALPHA_MOD:
         /* Sets a modulating factor for alpha channel. Default is 1.0.
          * The alpha factor is applied for all overlays. */
#ifdef HAVE_OVERLAY
         {
            float input_overlay_opacity    = settings->floats.input_overlay_opacity;
            input_driver_state_t *input_st = input_state_get_ptr();

            input_overlay_set_alpha_mod(input_st->overlay_visibility,
                  input_st->overlay_ptr, input_overlay_opacity);
         }
#endif
         break;
      case CMD_EVENT_AUDIO_REINIT:
         driver_uninit(DRIVER_AUDIO_MASK);
         drivers_init(settings, DRIVER_AUDIO_MASK, verbosity_is_enabled());
#if defined(HAVE_AUDIOMIXER)
         audio_driver_load_system_sounds();
#endif
         break;
      case CMD_EVENT_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         if (settings->bools.config_save_on_exit)
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_SHUTTING_DOWN), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         }
#ifdef HAVE_LAKKA
         system("(sleep 1 && shutdown -P now) & disown");
#else
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -P now");
#endif /* HAVE_LAKKA */
#endif
         break;
      case CMD_EVENT_REBOOT:
#if defined(__linux__) && !defined(ANDROID)
         if (settings->bools.config_save_on_exit)
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_REBOOTING), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         }
#ifdef HAVE_LAKKA
         system("(sleep 1 && shutdown -r now) & disown");
#else
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -r now");
#endif /* HAVE_LAKKA */
#endif
         break;
      case CMD_EVENT_RESUME:
#ifdef HAVE_MENU
         retroarch_menu_running_finished(false);
#endif
         if (uico_st->is_on_foreground)
         {
#ifdef HAVE_QT
            bool desktop_menu_enable = settings->bools.desktop_menu_enable;
            bool ui_companion_toggle = settings->bools.ui_companion_toggle;
#else
            bool desktop_menu_enable = false;
            bool ui_companion_toggle = false;
#endif
            ui_companion_driver_toggle(desktop_menu_enable,
                  ui_companion_toggle, false);
         }
         break;
      case CMD_EVENT_ADD_TO_FAVORITES:
         {
            struct string_list *str_list = (struct string_list*)data;

            /* Check whether favourties playlist is at capacity */
            if (playlist_size(g_defaults.content_favorites) >=
                  playlist_capacity(g_defaults.content_favorites))
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_ADD_TO_FAVORITES_FAILED), 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
               return false;
            }

            if (str_list)
            {
               if (str_list->size >= 6)
               {
                  struct playlist_entry entry     = {0};
                  bool playlist_sort_alphabetical = settings->bools.playlist_sort_alphabetical;

                  entry.path      = str_list->elems[0].data; /* content_path */
                  entry.label     = str_list->elems[1].data; /* content_label */
                  entry.core_path = str_list->elems[2].data; /* core_path */
                  entry.core_name = str_list->elems[3].data; /* core_name */
                  entry.crc32     = str_list->elems[4].data; /* crc32 */
                  entry.db_name   = str_list->elems[5].data; /* db_name */

                  /* Write playlist entry */
                  if (playlist_push(g_defaults.content_favorites, &entry))
                  {
                     enum playlist_sort_mode current_sort_mode =
                        playlist_get_sort_mode(g_defaults.content_favorites);

                     /* New addition - need to resort if option is enabled */
                     if ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
                           (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
                        playlist_qsort(g_defaults.content_favorites);

                     playlist_write_file(g_defaults.content_favorites);
                     runloop_msg_queue_push(msg_hash_to_str(MSG_ADDED_TO_FAVORITES), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
               }
            }

            break;
         }
      case CMD_EVENT_RESET_CORE_ASSOCIATION:
         {
            const char *core_name          = "DETECT";
            const char *core_path          = "DETECT";
            size_t *playlist_index         = (size_t*)data;
            struct playlist_entry entry    = {0};

            /* the update function reads our entry as const,
             * so these casts are safe */
            entry.core_path                = (char*)core_path;
            entry.core_name                = (char*)core_name;

            command_playlist_update_write(
                  NULL, *playlist_index, &entry);

            runloop_msg_queue_push(msg_hash_to_str(MSG_RESET_CORE_ASSOCIATION), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            break;

         }
      case CMD_EVENT_RESTART_RETROARCH:
         if (!frontend_driver_set_fork(FRONTEND_FORK_RESTART))
            return false;
#ifndef HAVE_DYNAMIC
         command_event(CMD_EVENT_QUIT, NULL);
#endif
         break;
      case CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG:
         config_set_defaults(global_get_ptr());
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG:
#if !defined(HAVE_DYNAMIC)
         config_save_file_salamander();
#endif
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_NONE);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_CORE);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_CONTENT_DIR);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME:
#ifdef HAVE_CONFIGFILE
         command_event_save_current_config(OVERRIDE_GAME);
#endif
         break;
      case CMD_EVENT_MENU_SAVE_CONFIG:
#ifdef HAVE_CONFIGFILE
         if (!command_event_save_core_config(
                  settings->paths.directory_menu_config,
                  path_get(RARCH_PATH_CONFIG)))
            return false;
#endif
         break;
      case CMD_EVENT_SHADER_PRESET_LOADED:
         ui_companion_event_command(cmd);
         break;
      case CMD_EVENT_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         menu_shader_manager_apply_changes(menu_shader_get(),
               settings->paths.directory_video_shader,
               settings->paths.directory_menu_config
               );
#endif
#endif
         ui_companion_event_command(cmd);
         break;
      case CMD_EVENT_PAUSE_TOGGLE:
         {
#ifdef HAVE_ACCESSIBILITY
            bool accessibility_enable                    = settings->bools.accessibility_enable;
            unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif

#ifdef HAVE_NETWORKING
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
            break;
#endif

            boolean                                      = runloop_st->paused;
            boolean                                      = !boolean;

#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled(
                  accessibility_enable,
                  access_st->enabled))
            {
               if (boolean)
                  accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     (char*)msg_hash_to_str(MSG_PAUSED), 10);
               else
                  accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     (char*)msg_hash_to_str(MSG_UNPAUSED), 10);
            }
#endif

            runloop_st->paused   = boolean;
            runloop_pause_checks();
         }
         break;
      case CMD_EVENT_UNPAUSE:
#ifdef HAVE_NETWORKING
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
            break;
#endif

         boolean                 = false;
         runloop_st->paused      = boolean;
         runloop_pause_checks();
         break;
      case CMD_EVENT_PAUSE:
#ifdef HAVE_NETWORKING
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
            break;
#endif

         boolean                 = true;
         runloop_st->paused      = boolean;
         runloop_pause_checks();
         break;
      case CMD_EVENT_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (menu_st->alive)
         {
#ifdef HAVE_NETWORKING
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
#endif
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_STOP, NULL);
            else
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
         else
         {
#ifdef HAVE_NETWORKING
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
#endif
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
#endif
         break;
#ifdef HAVE_NETWORKING
      case CMD_EVENT_NETPLAY_PING_TOGGLE:
         settings->bools.netplay_ping_show =
            !settings->bools.netplay_ping_show;
         break;
      case CMD_EVENT_NETPLAY_GAME_WATCH:
         netplay_driver_ctl(RARCH_NETPLAY_CTL_GAME_WATCH, NULL);
         break;
      case CMD_EVENT_NETPLAY_PLAYER_CHAT:
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PLAYER_CHAT, NULL);
         break;
      case CMD_EVENT_NETPLAY_FADE_CHAT_TOGGLE:
         settings->bools.netplay_fade_chat =
            !settings->bools.netplay_fade_chat;
         break;
      case CMD_EVENT_NETPLAY_DEINIT:
         deinit_netplay();
         break;
      case CMD_EVENT_NETWORK_INIT:
         network_init();
         break;
         /* init netplay manually */
      case CMD_EVENT_NETPLAY_INIT:
         {
            char tmp_netplay_server[256];
            char tmp_netplay_session[sizeof(tmp_netplay_server)];
            char *netplay_server  = NULL;
            char *netplay_session = NULL;
            unsigned netplay_port = 0;

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            tmp_netplay_server[0]  = '\0';
            tmp_netplay_session[0] = '\0';
            if (netplay_decode_hostname(p_rarch->connect_host,
               tmp_netplay_server, &netplay_port, tmp_netplay_session,
               sizeof(tmp_netplay_server)))
            {
               netplay_server  = tmp_netplay_server;
               netplay_session = tmp_netplay_session;
            }
            if (p_rarch->connect_host)
            {
               free(p_rarch->connect_host);
               p_rarch->connect_host = NULL;
            }

            if (string_is_empty(netplay_server))
               netplay_server = settings->paths.netplay_server;
            if (!netplay_port)
               netplay_port   = settings->uints.netplay_port;

            if (!init_netplay(netplay_server, netplay_port, netplay_session))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               return false;
            }

            /* Disable rewind & SRAM autosave if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st,
                  &runloop_st->current_core);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* Initialize netplay via lobby when content is loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT:
         {
            char netplay_server[256];
            char netplay_session[sizeof(netplay_server)];
            unsigned netplay_port = 0;

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            netplay_server[0]  = '\0';
            netplay_session[0] = '\0';
            netplay_decode_hostname((char*) data, netplay_server,
               &netplay_port, netplay_session, sizeof(netplay_server));

            if (!netplay_port)
               netplay_port = settings->uints.netplay_port;

            RARCH_LOG("[Netplay]: Connecting to %s|%d (direct)\n",
               netplay_server, netplay_port);

            if (!init_netplay(netplay_server, netplay_port, netplay_session))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               return false;
            }

            /* Disable rewind if it was enabled
               TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st,
                  &runloop_st->current_core);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* init netplay via lobby when content is not loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED:
         {
            char netplay_server[256];
            char netplay_session[sizeof(netplay_server)];
            unsigned netplay_port = 0;

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            netplay_server[0]  = '\0';
            netplay_session[0] = '\0';
            netplay_decode_hostname((char*) data, netplay_server,
               &netplay_port, netplay_session, sizeof(netplay_server));

            if (!netplay_port)
               netplay_port = settings->uints.netplay_port;

            RARCH_LOG("[Netplay]: Connecting to %s|%d (deferred)\n",
               netplay_server, netplay_port);

            if (!init_netplay_deferred(netplay_server, netplay_port, netplay_session))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               return false;
            }

            /* Disable rewind if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st,
                  &runloop_st->current_core);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
      case CMD_EVENT_NETPLAY_ENABLE_HOST:
         {
            if (!task_push_netplay_content_reload(NULL))
            {
#ifdef HAVE_DYNAMIC
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);

               runloop_msg_queue_push(
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
                  1, 480, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#else
               runloop_msg_queue_push(
                  msg_hash_to_str(MSG_NETPLAY_NEED_CONTENT_LOADED),
                  1, 480, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#endif

               return false;
            }
         }
         break;
      case CMD_EVENT_NETPLAY_DISCONNECT:
         {
            bool rewind_enable         = settings->bools.rewind_enable;
            unsigned autosave_interval = settings->uints.autosave_interval;

            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISCONNECT, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);

#ifdef HAVE_REWIND
            /* Re-enable rewind if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
            if (rewind_enable)
               command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
            if (autosave_interval != 0)
               command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
         }
         break;
      case CMD_EVENT_NETPLAY_HOST_TOGGLE:
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
         {
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) ||
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
               command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         }
         else
            command_event(CMD_EVENT_NETPLAY_ENABLE_HOST, NULL);

         break;
#else
      case CMD_EVENT_NETPLAY_DEINIT:
      case CMD_EVENT_NETWORK_INIT:
      case CMD_EVENT_NETPLAY_INIT:
      case CMD_EVENT_NETPLAY_INIT_DIRECT:
      case CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED:
      case CMD_EVENT_NETPLAY_HOST_TOGGLE:
      case CMD_EVENT_NETPLAY_DISCONNECT:
      case CMD_EVENT_NETPLAY_ENABLE_HOST:
      case CMD_EVENT_NETPLAY_PING_TOGGLE:
      case CMD_EVENT_NETPLAY_GAME_WATCH:
      case CMD_EVENT_NETPLAY_PLAYER_CHAT:
      case CMD_EVENT_NETPLAY_FADE_CHAT_TOGGLE:
         return false;
#endif
      case CMD_EVENT_FULLSCREEN_TOGGLE:
         {
            audio_driver_state_t
               *audio_st              = audio_state_get_ptr();
            input_driver_state_t
               *input_st              = input_state_get_ptr();
            bool *userdata            = (bool*)data;
            bool video_fullscreen     = settings->bools.video_fullscreen;
            bool ra_is_forced_fs      = video_st->force_fullscreen;
            bool new_fullscreen_state = !video_fullscreen && !ra_is_forced_fs;

            if (!video_driver_has_windowed())
               return false;

            audio_st->suspended                      = true;
            video_st->is_switching_display_mode      = true;

            /* we toggled manually, write the new value to settings */
            configuration_set_bool(settings, settings->bools.video_fullscreen,
                  new_fullscreen_state);
            /* Need to grab this setting's value again */
            video_fullscreen = new_fullscreen_state;

            /* we toggled manually, the CLI arg is irrelevant now */
            if (ra_is_forced_fs)
               video_st->force_fullscreen = false;

            /* If we go fullscreen we drop all drivers and
             * reinitialize to be safe. */
            command_event(CMD_EVENT_REINIT, NULL);
            if (video_fullscreen)
            {
               video_driver_hide_mouse();
               if (!settings->bools.video_windowed_fullscreen)
                  if (input_driver_grab_mouse())
                     input_st->grab_mouse_state = true;
            }
            else
            {
               video_driver_show_mouse();
               if (!settings->bools.video_windowed_fullscreen)
                  if (input_driver_ungrab_mouse())
                     input_st->grab_mouse_state = false;
            }

            video_st->is_switching_display_mode      = false;
            audio_st->suspended                      = false;

            if (userdata && *userdata == true)
               video_driver_cached_frame();
         }
         break;
      case CMD_EVENT_DISK_APPEND_IMAGE:
         {
            const char *path              = (const char*)data;
            rarch_system_info_t *sys_info = &runloop_st->system;

            if (string_is_empty(path) || !sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
#if defined(HAVE_MENU)
               bool refresh               = false;
               /* Get initial disk eject state */
               bool initial_disk_ejected  = disk_control_get_eject_state(&sys_info->disk_control);
#endif
               /* Append disk image */
               bool success               =
                  command_event_disk_control_append_image(path);

#if defined(HAVE_MENU)
               /* Appending a disk image may or may not affect
                * the disk tray eject status. If status has changed,
                * must refresh the disk options menu */
               if (initial_disk_ejected != disk_control_get_eject_state(
				       &sys_info->disk_control))
               {
                  menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
                  menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
               }
#endif
               return success;
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_EJECT_TOGGLE:
         {
            rarch_system_info_t *sys_info = &runloop_st->system;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool *show_msg = (bool*)data;
               bool eject     = !disk_control_get_eject_state(&sys_info->disk_control);
               bool verbose   = true;
               bool refresh   = false;

               if (show_msg)
                  verbose     = *show_msg;

               disk_control_set_eject_state(
                     &sys_info->disk_control, eject, verbose);

#if defined(HAVE_MENU)
               /* It is necessary to refresh the disk options
                * menu when toggling the tray state */
               menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
               menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
#endif
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_NEXT:
         {
            rarch_system_info_t *sys_info = &runloop_st->system;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool *show_msg = (bool*)data;
               bool verbose   = true;

               if (show_msg)
                  verbose     = *show_msg;

               disk_control_set_index_next(&sys_info->disk_control, verbose);
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_PREV:
         {
            rarch_system_info_t *sys_info = &runloop_st->system;

            if (!sys_info)
               return false;

            if (disk_control_enabled(&sys_info->disk_control))
            {
               bool *show_msg = (bool*)data;
               bool verbose   = true;

               if (show_msg)
                  verbose     = *show_msg;

               disk_control_set_index_prev(&sys_info->disk_control, verbose);
            }
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_DISK_INDEX:
         {
            rarch_system_info_t *sys_info = &runloop_st->system;
            unsigned *index               = (unsigned*)data;

            if (!sys_info || !index)
               return false;

            /* Note: Menu itself provides visual feedback - no
             * need to print info message to screen */
            if (disk_control_enabled(&sys_info->disk_control))
               disk_control_set_index(&sys_info->disk_control, *index, false);
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                     1, 120, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
      case CMD_EVENT_RUMBLE_STOP:
         {
            unsigned i;

            for (i = 0; i < MAX_USERS; i++)
            {
               unsigned joy_idx = settings->uints.input_joypad_index[i];
               input_driver_set_rumble(i, joy_idx, RETRO_RUMBLE_STRONG, 0);
               input_driver_set_rumble(i, joy_idx, RETRO_RUMBLE_WEAK, 0);
            }
         }
         break;
      case CMD_EVENT_GRAB_MOUSE_TOGGLE:
         {
            bool ret              = false;
            input_driver_state_t
               *input_st          = input_state_get_ptr();
            bool grab_mouse_state = !input_st->grab_mouse_state;

            if (grab_mouse_state)
            {
               if ((ret = input_driver_grab_mouse()))
                  input_st->grab_mouse_state = true;
            }
            else
            {
               if ((ret = input_driver_ungrab_mouse()))
                  input_st->grab_mouse_state = false;
            }

            if (!ret)
               return false;

            RARCH_LOG("[Input]: %s => %s\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "ON" : "OFF");

            if (grab_mouse_state)
               video_driver_hide_mouse();
            else
               video_driver_show_mouse();
         }
         break;
      case CMD_EVENT_UI_COMPANION_TOGGLE:
         {
#ifdef HAVE_QT
            bool desktop_menu_enable = settings->bools.desktop_menu_enable;
            bool ui_companion_toggle = settings->bools.ui_companion_toggle;
#else
            bool desktop_menu_enable = false;
            bool ui_companion_toggle = false;
#endif
            ui_companion_driver_toggle(desktop_menu_enable,
                  ui_companion_toggle, true);
         }
         break;
      case CMD_EVENT_GAME_FOCUS_TOGGLE:
         {
            bool video_fullscreen                         =
               settings->bools.video_fullscreen || video_st->force_fullscreen;
            enum input_game_focus_cmd_type game_focus_cmd = GAME_FOCUS_CMD_TOGGLE;
            input_driver_state_t
               *input_st                                  = input_state_get_ptr();
            bool current_enable_state                     = input_st->game_focus_state.enabled;
            bool apply_update                             = false;
            bool show_message                             = false;

            if (data)
               game_focus_cmd = *((enum input_game_focus_cmd_type*)data);

            switch (game_focus_cmd)
            {
               case GAME_FOCUS_CMD_OFF:
                  /* Force game focus off */
                  input_st->game_focus_state.enabled = false;
                  if (input_st->game_focus_state.enabled != current_enable_state)
                  {
                     apply_update = true;
                     show_message = true;
                  }
                  break;
               case GAME_FOCUS_CMD_ON:
                  /* Force game focus on */
                  input_st->game_focus_state.enabled = true;
                  if (input_st->game_focus_state.enabled != current_enable_state)
                  {
                     apply_update = true;
                     show_message = true;
                  }
                  break;
               case GAME_FOCUS_CMD_TOGGLE:
                  /* Invert current game focus state */
                  input_st->game_focus_state.enabled = !input_st->game_focus_state.enabled;
#ifdef HAVE_MENU
                  /* If menu is currently active, disable
                   * 'toggle on' functionality */
                  if (menu_st->alive)
                     input_st->game_focus_state.enabled = false;
#endif
                  if (input_st->game_focus_state.enabled != current_enable_state)
                  {
                     apply_update = true;
                     show_message = true;
                  }
                  break;
               case GAME_FOCUS_CMD_REAPPLY:
                  /* Reapply current game focus state */
                  apply_update = true;
                  show_message = false;
                  break;
               default:
                  break;
            }

            if (apply_update)
            {
               input_driver_state_t
                  *input_st          = input_state_get_ptr();

               if (input_st->game_focus_state.enabled)
               {
                  if (input_driver_grab_mouse())
                     input_st->grab_mouse_state = true;
                  video_driver_hide_mouse();
               }
               /* Ungrab only if windowed and auto mouse grab is disabled */
               else if (!video_fullscreen &&
                     !settings->bools.input_auto_mouse_grab)
               {
                  if (input_driver_ungrab_mouse())
                     input_st->grab_mouse_state = false;
                  video_driver_show_mouse();
               }

               input_st->block_hotkey =
                  input_st->game_focus_state.enabled;
               input_st->keyboard_mapping_blocked  =
                  input_st->game_focus_state.enabled;

               if (show_message)
                  runloop_msg_queue_push(
                        input_st->game_focus_state.enabled ?
                        msg_hash_to_str(MSG_GAME_FOCUS_ON) :
                        msg_hash_to_str(MSG_GAME_FOCUS_OFF),
                        1, 60, true,
                        NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                        MESSAGE_QUEUE_CATEGORY_INFO);

               RARCH_LOG("[Input]: %s => %s\n",
                     "Game Focus",
                     input_st->game_focus_state.enabled ? "ON" : "OFF");
            }
         }
         break;
      case CMD_EVENT_VOLUME_UP:
         {
            audio_driver_state_t
               *audio_st              = audio_state_get_ptr();
            command_event_set_volume(settings, 0.5f,
#if defined(HAVE_GFX_WIDGETS)
                  dispwidget_get_ptr()->active,
#else
                  false,
#endif
                  audio_st->mute_enable);
         }
         break;
      case CMD_EVENT_VOLUME_DOWN:
         command_event_set_volume(settings, -0.5f,
#if defined(HAVE_GFX_WIDGETS)
               dispwidget_get_ptr()->active,
#else
               false,
#endif
               audio_state_get_ptr()->mute_enable
               );
         break;
      case CMD_EVENT_MIXER_VOLUME_UP:
         command_event_set_mixer_volume(settings, 0.5f);
         break;
      case CMD_EVENT_MIXER_VOLUME_DOWN:
         command_event_set_mixer_volume(settings, -0.5f);
         break;
      case CMD_EVENT_SET_FRAME_LIMIT:
         {
            video_driver_state_t
               *video_st                        = video_state_get_ptr();
            runloop_st->frame_limit_minimum_time=
               runloop_set_frame_limit(&video_st->av_info,
                     runloop_get_fastforward_ratio(
                        settings,
                        &runloop_st->fastmotion_override.current));
         }
         break;
      case CMD_EVENT_DISCORD_INIT:
#ifdef HAVE_DISCORD
         {
            bool discord_enable         = settings ? settings->bools.discord_enable : false;
            const char *discord_app_id  = settings ? settings->arrays.discord_app_id : NULL;
            discord_state_t *discord_st = discord_state_get_ptr();
            if (!settings)
               return false;
            if (!discord_enable)
               return false;
            if (discord_st->ready)
               return true;
            discord_init(discord_app_id, p_rarch->launch_arguments);
         }
#endif
         break;
      case CMD_EVENT_PRESENCE_UPDATE:
         {
#ifdef HAVE_PRESENCE
            presence_userdata_t *userdata = NULL;
            if (!data)
               return false;

            userdata = (presence_userdata_t*)data;
            presence_update(userdata->status);
#endif
         }
         break;

      case CMD_EVENT_AI_SERVICE_CALL:
         {
#ifdef HAVE_TRANSLATE
#ifdef HAVE_ACCESSIBILITY
            bool accessibility_enable = settings->bools.accessibility_enable;
            unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
            unsigned ai_service_mode  = settings->uints.ai_service_mode;

#ifdef HAVE_AUDIOMIXER
            if (ai_service_mode == 1 && audio_driver_is_ai_service_speech_running())
            {
               audio_driver_mixer_stop_stream(10);
               audio_driver_mixer_remove_stream(10);
#ifdef HAVE_ACCESSIBILITY
               if (is_accessibility_enabled(
                        accessibility_enable,
                        access_st->enabled))
                  accessibility_speak_priority(
                        accessibility_enable,
                        accessibility_narrator_speech_speed,
                        "stopped.", 10);
#endif
            }
            else
#endif
#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled(
                     accessibility_enable,
                     access_st->enabled)
                  && ai_service_mode == 2
                  && is_narrator_running(accessibility_enable))
               accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     "stopped.", 10);
            else
#endif
            {
               bool paused = runloop_st->paused;
               if (data)
                  paused = *((bool*)data);

               if (      access_st->ai_service_auto == 0
                     && !settings->bools.ai_service_pause)
                  access_st->ai_service_auto = 1;

               run_translation_service(settings, paused);
            }
#endif
            break;
         }
      case CMD_EVENT_CONTROLLER_INIT:
         {
            rarch_system_info_t *info = &runloop_st->system;
            if (info)
               command_event_init_controllers(info, settings,
                     settings->uints.input_max_users);
         }
         break;
      case CMD_EVENT_VRR_RUNLOOP_TOGGLE:
         settings->bools.vrr_runloop_enable = !(settings->bools.vrr_runloop_enable);
         runloop_msg_queue_push(
               msg_hash_to_str(
                     settings->bools.vrr_runloop_enable ? MSG_VRR_RUNLOOP_ENABLED
                                                        : MSG_VRR_RUNLOOP_DISABLED),
               1, 100, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         break;
      case CMD_EVENT_NONE:
         return false;
   }

   return true;
}

/* FRONTEND */

void retroarch_override_setting_set(
      enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state            *p_rarch = &rarch_st;
#ifdef HAVE_NETWORKING
   net_driver_state_t *net_st  = networking_state_get_ptr();
#endif

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
               runloop_state_t *runloop_st = runloop_state_get_ptr();
               BIT256_SET(runloop_st->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         p_rarch->has_set_verbosity = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->has_set_libretro = true;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->has_set_libretro_directory = true;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->has_set_save_path = true;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->has_set_state_path = true;
         break;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         net_st->has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         net_st->has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         net_st->has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         net_st->has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         net_st->has_set_netplay_check_frames = true;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_ups_pref = true;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_bps_pref = true;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_ips_pref = true;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->has_set_log_to_file = true;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
}

void retroarch_override_setting_unset(
      enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_NETWORKING
   net_driver_state_t *net_st  = networking_state_get_ptr();
#endif

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
               runloop_state_t *runloop_st = runloop_state_get_ptr();
               BIT256_CLEAR(runloop_st->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         p_rarch->has_set_verbosity = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->has_set_libretro = false;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->has_set_libretro_directory = false;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->has_set_save_path = false;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->has_set_state_path = false;
         break;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         net_st->has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         net_st->has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         net_st->has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         net_st->has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         net_st->has_set_netplay_check_frames = false;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_ups_pref = false;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_bps_pref = false;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->has_set_ips_pref = false;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->has_set_log_to_file = false;
         break;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }
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
            retroarch_override_setting_unset(
                  RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &j);
      }
      else
         retroarch_override_setting_unset(
               (enum rarch_override_setting)(i), NULL);
   }
}

static void global_free(struct rarch_state *p_rarch)
{
   global_t            *global = NULL;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   content_deinit();

   path_deinit_subsystem(runloop_st);
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);

   retro_main_log_file_deinit();

   runloop_st->is_sram_load_disabled                  = false;
   runloop_st->is_sram_save_disabled                  = false;
   runloop_st->use_sram                               = false;
#ifdef HAVE_PATCH
   p_rarch->rarch_bps_pref                            = false;
   p_rarch->rarch_ips_pref                            = false;
   p_rarch->rarch_ups_pref                            = false;
   runloop_st->patch_blocked                          = false;
#endif
#ifdef HAVE_CONFIGFILE
   p_rarch->rarch_block_config_read                   = false;
   runloop_st->overrides_active                       = false;
   runloop_st->remaps_core_active                     = false;
   runloop_st->remaps_game_active                     = false;
   runloop_st->remaps_content_dir_active              = false;
#endif

   runloop_st->current_core.has_set_input_descriptors = false;
   runloop_st->current_core.has_set_subsystems        = false;

   global                                             = global_get_ptr();
   path_clear_all();
   dir_clear_all();

   if (!string_is_empty(runloop_st->name.remapfile))
      free(runloop_st->name.remapfile);
   runloop_st->name.remapfile = NULL;
   *runloop_st->name.ups                 = '\0';
   *runloop_st->name.bps                 = '\0';
   *runloop_st->name.ips                 = '\0';
   *runloop_st->name.savefile            = '\0';
   *runloop_st->name.savestate           = '\0';
   *runloop_st->name.cheatfile           = '\0';
   *runloop_st->name.label               = '\0';

   if (global)
      memset(global, 0, sizeof(struct global));
   retroarch_override_setting_free_state();
}

#if defined(HAVE_SDL) || defined(HAVE_SDL2) || defined(HAVE_SDL_DINGUX)
static void sdl_exit(void)
{
   /* Quit any SDL subsystems, then quit
    * SDL itself */
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   if (sdl_subsystem_flags != 0)
   {
      SDL_QuitSubSystem(sdl_subsystem_flags);
      SDL_Quit();
   }
}
#endif

/**
 * main_exit:
 *
 * Cleanly exit RetroArch.
 *
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 **/
void main_exit(void *args)
{
   struct rarch_state *p_rarch  = &rarch_st;
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state  *menu_st  = menu_state_get_ptr();
#endif
   settings_t     *settings     = config_get_ptr();
   bool     config_save_on_exit = settings->bools.config_save_on_exit;

   video_driver_restore_cached(settings);

   if (config_save_on_exit)
      command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(HAVE_GFX_WIDGETS)
   /* Do not want display widgets to live any more. */
   dispwidget_get_ptr()->persisting = false;
#endif
#ifdef HAVE_MENU
   /* Do not want menu context to live any more. */
   if (menu_st)
      menu_st->data_own = false;
#endif
   retroarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   if (runloop_st->perfcnt_enable)
   {
      RARCH_LOG("[PERF]: Performance counters (RetroArch):\n");
      runloop_log_counters(p_rarch->perf_counters_rarch, p_rarch->perf_ptr_rarch);
   }

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   frontend_driver_deinit(args);
   frontend_driver_exitspawn(
         path_get_ptr(RARCH_PATH_CORE),
         path_get_realsize(RARCH_PATH_CORE),
         p_rarch->launch_arguments);

   p_rarch->has_set_username        = false;
   runloop_st->is_inited            = false;
   global_get_ptr()->error_on_init  = false;
#ifdef HAVE_CONFIGFILE
   p_rarch->rarch_block_config_read = false;
#endif

   runloop_msg_queue_deinit();
   driver_uninit(DRIVERS_CMD_ALL);

   retro_main_log_file_deinit();

   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);
   task_queue_deinit();

   rarch_config_deinit();

   ui_companion_driver_deinit();

   frontend_driver_shutdown(false);

   retroarch_deinit_drivers(&runloop_st->retro_ctx);
   uico_state_get_ptr()->drv = NULL;
   frontend_driver_free();

   rtime_deinit();

#if defined(ANDROID)
   play_feature_delivery_deinit();
#endif

#if defined(HAVE_MIST)
   steam_deinit();
#endif

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   CoUninitialize();
#endif

#if defined(HAVE_SDL) || defined(HAVE_SDL2) || defined(HAVE_SDL_DINGUX)
   sdl_exit();
#endif
}

/**
 * main_entry:
 *
 * Main function of RetroArch.
 *
 * If HAVE_MAIN is not defined, will contain main loop and will not
 * be exited from until we exit the program. Otherwise, will
 * just do initialization.
 *
 * Returns: varies per platform.
 **/
int rarch_main(int argc, char *argv[], void *data)
{
   struct rarch_state *p_rarch         = &rarch_st;
   runloop_state_t *runloop_st         = runloop_state_get_ptr();
   video_driver_state_t *video_st      = video_state_get_ptr();
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   video_st->shader_presets_need_reload                          = true;
#endif
#ifdef HAVE_RUNAHEAD
   video_st->runahead_is_active                                  = true;
   runloop_st->runahead_available                                = true;
   runloop_st->runahead_secondary_core_available                 = true;
   runloop_st->runahead_force_input_dirty                        = true;
#endif
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
   if (FAILED(CoInitialize(NULL)))
   {
      RARCH_ERR("FATAL: Failed to initialize the COM interface\n");
      return 1;
   }
#endif

   rtime_init();

#if defined(ANDROID)
   play_feature_delivery_init();
#endif

#if defined(HAVE_MIST)
   steam_init();
#endif

   libretro_free_system_info(&runloop_st->system.info);
   command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
   rarch_favorites_deinit();

   rarch_config_init();

   retroarch_deinit_drivers(&runloop_st->retro_ctx);
   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);

   frontend_driver_init_first(data);

   if (runloop_st->is_inited)
      driver_uninit(DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_create(&p_rarch->rarch_tls);
   sthread_tls_set(&p_rarch->rarch_tls, MAGIC_POINTER);
#endif
   video_st->active              = true;
   audio_state_get_ptr()->active = true;

   {
      unsigned i;
      for (i = 0; i < MAX_USERS; i++)
         input_config_set_device(i, RETRO_DEVICE_JOYPAD);
   }

   runloop_msg_queue_init();

   if (frontend_state_get_ptr()->current_frontend_ctx)
   {
      content_ctx_info_t info;

      info.argc            = argc;
      info.argv            = argv;
      info.args            = data;
      info.environ_get     = frontend_state_get_ptr()->current_frontend_ctx->environment_get;

      if (!task_push_load_content_from_cli(
               NULL,
               NULL,
               &info,
               CORE_TYPE_PLAIN,
               NULL,
               NULL))
         return 1;
   }

   ui_companion_driver_init_first();
#if !defined(HAVE_MAIN) || defined(HAVE_QT)
   for (;;)
   {
      int ret;
      bool app_exit     = false;
#ifdef HAVE_QT
      ui_companion_qt.application->process_events();
#endif
      ret = runloop_iterate();

      task_queue_check();

#ifdef HAVE_MIST
   steam_poll();
#endif

#ifdef HAVE_QT
      app_exit = ui_companion_qt.application->exiting;
#endif

      if (ret == -1 || app_exit)
      {
#ifdef HAVE_QT
         ui_companion_qt.application->quit();
#endif
         break;
      }
   }

   main_exit(data);
#endif

   return 0;
}

#if defined(EMSCRIPTEN)
#include "gfx/common/gl_common.h"

void RWebAudioRecalibrateTime(void);

void emscripten_mainloop(void)
{
   int ret;
   static unsigned emscripten_frame_count = 0;
   video_driver_state_t *video_st         = video_state_get_ptr();
   settings_t        *settings            = config_get_ptr();
   input_driver_state_t *input_st         = input_state_get_ptr();
   bool black_frame_insertion             = settings->uints.video_black_frame_insertion;
   bool input_driver_nonblock_state       = input_st ? input_st->nonblocking_flag : false;
   runloop_state_t *runloop_st            = runloop_state_get_ptr();
   bool runloop_is_slowmotion             = runloop_st->slowmotion;
   bool runloop_is_paused                 = runloop_st->paused;

   RWebAudioRecalibrateTime();

   emscripten_frame_count++;

   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
             black_frame_insertion
         && !input_driver_nonblock_state
         && !runloop_is_slowmotion
         && !runloop_is_paused)
   {
      if ((emscripten_frame_count % (black_frame_insertion+1)) != 0)
      {
         gl_clear();
         if (video_st->current_video_context.swap_buffers)
            video_st->current_video_context.swap_buffers(
                  video_st->context_data);
         return;
      }
   }

   ret = runloop_iterate();

   task_queue_check();

   if (ret != -1)
      return;

   main_exit(NULL);
   emscripten_force_exit(0);
}
#endif

#ifndef HAVE_MAIN
#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif

/* DYNAMIC LIBRETRO CORE  */

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info, unsigned num_info,
      const char *ident)
{
   unsigned i;
   for (i = 0; i < num_info; i++)
   {
      if (     string_is_equal(info[i].ident, ident)
            || string_is_equal(info[i].desc,  ident)
         )
         return &info[i];
   }

   return NULL;
}

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
libretro_find_controller_description(
      const struct retro_controller_info *info, unsigned id)
{
   unsigned i;

   for (i = 0; i < info->num_types; i++)
   {
      if (info->types[i].id != id)
         continue;

      return &info->types[i];
   }

   return NULL;
}

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info)
{
   if (!info)
      return;

   free((void*)info->library_name);
   free((void*)info->library_version);
   free((void*)info->valid_extensions);
   memset(info, 0, sizeof(*info));
}

static void retroarch_print_features(void)
{
   char buf[2048];
   buf[0] = '\0';
   frontend_driver_attach_console();

   strlcpy(buf, "\nFeatures:\n", sizeof(buf));

   _PSUPP_BUF(buf, SUPPORTS_LIBRETRODB,      "LibretroDB",      "LibretroDB support");
   _PSUPP_BUF(buf, SUPPORTS_COMMAND,         "Command",         "Command interface support");
   _PSUPP_BUF(buf, SUPPORTS_NETWORK_COMMAND, "Network Command", "Network Command interface "
         "support");
   _PSUPP_BUF(buf, SUPPORTS_SDL,             "SDL",             "SDL input/audio/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_SDL2,            "SDL2",            "SDL2 input/audio/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_X11,             "X11",             "X11 input/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_WAYLAND,         "wayland",         "Wayland input/video drivers");
   _PSUPP_BUF(buf, SUPPORTS_THREAD,          "Threads",         "Threading support");
   _PSUPP_BUF(buf, SUPPORTS_VULKAN,          "Vulkan",          "Vulkan video driver");
   _PSUPP_BUF(buf, SUPPORTS_METAL,           "Metal",           "Metal video driver");
   _PSUPP_BUF(buf, SUPPORTS_OPENGL,          "OpenGL",          "OpenGL   video driver support");
   _PSUPP_BUF(buf, SUPPORTS_OPENGLES,        "OpenGL ES",       "OpenGLES video driver support");
   _PSUPP_BUF(buf, SUPPORTS_XVIDEO,          "XVideo",          "Video driver");
   _PSUPP_BUF(buf, SUPPORTS_UDEV,            "UDEV",            "UDEV/EVDEV input driver support");
   _PSUPP_BUF(buf, SUPPORTS_EGL,             "EGL",             "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_KMS,             "KMS",             "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_VG,              "OpenVG",          "Video context driver");
   _PSUPP_BUF(buf, SUPPORTS_COREAUDIO,       "CoreAudio",       "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_COREAUDIO3,      "CoreAudioV3",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_ALSA,            "ALSA",            "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_OSS,             "OSS",             "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_JACK,            "Jack",            "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_RSOUND,          "RSound",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_ROAR,            "RoarAudio",       "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_PULSE,           "PulseAudio",      "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_DSOUND,          "DirectSound",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_WASAPI,          "WASAPI",     "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_XAUDIO,          "XAudio2",         "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_AL,              "OpenAL",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_SL,              "OpenSL",          "Audio driver");
   _PSUPP_BUF(buf, SUPPORTS_7ZIP,            "7zip",            "7zip extraction support");
   _PSUPP_BUF(buf, SUPPORTS_ZLIB,            "zlib",            ".zip extraction support");
   _PSUPP_BUF(buf, SUPPORTS_DYLIB,           "External",        "External filter and plugin support");
   _PSUPP_BUF(buf, SUPPORTS_CG,              "Cg",              "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_GLSL,            "GLSL",            "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_HLSL,            "HLSL",            "Fragment/vertex shader driver");
   _PSUPP_BUF(buf, SUPPORTS_SDL_IMAGE,       "SDL_image",       "SDL_image image loading");
   _PSUPP_BUF(buf, SUPPORTS_RPNG,            "rpng",            "PNG image loading/encoding");
   _PSUPP_BUF(buf, SUPPORTS_RJPEG,            "rjpeg",           "JPEG image loading");
   _PSUPP_BUF(buf, SUPPORTS_DYNAMIC,         "Dynamic",         "Dynamic run-time loading of "
                                              "libretro library");
   _PSUPP_BUF(buf, SUPPORTS_FFMPEG,          "FFmpeg",          "On-the-fly recording of gameplay "
                                              "with libavcodec");
   _PSUPP_BUF(buf, SUPPORTS_FREETYPE,        "FreeType",        "TTF font rendering driver");
   _PSUPP_BUF(buf, SUPPORTS_CORETEXT,        "CoreText",        "TTF font rendering driver ");
   _PSUPP_BUF(buf, SUPPORTS_NETPLAY,         "Netplay",         "Peer-to-peer netplay");
   _PSUPP_BUF(buf, SUPPORTS_LIBUSB,          "Libusb",          "Libusb support");
   _PSUPP_BUF(buf, SUPPORTS_COCOA,           "Cocoa",           "Cocoa UI companion support "
                                              "(for OSX and/or iOS)");
   _PSUPP_BUF(buf, SUPPORTS_QT,              "Qt",              "Qt UI companion support");
   _PSUPP_BUF(buf, SUPPORTS_V4L2,            "Video4Linux2",    "Camera driver");

   puts(buf);
}

static void retroarch_print_version(void)
{
   char str[255];
   frontend_driver_attach_console();
   str[0] = '\0';

   fprintf(stdout, "%s: %s -- v%s",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND),
         PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   printf(" -- %s --\n", retroarch_git_version);
#else
   printf("\n");
#endif
   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
   strlcat(str, " Built: " __DATE__, sizeof(str));
   fprintf(stdout, "%s\n", str);
}

/**
 * retroarch_print_help:
 *
 * Prints help message explaining the program's commandline switches.
 **/
static void retroarch_print_help(const char *arg0)
{
   char buf[2048];
   buf[0] = '\0';

   frontend_driver_attach_console();
   fputs("\n", stdout);
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");
   fputs("\n", stdout);

   fprintf(stdout, "Usage: %s [OPTIONS]... [FILE]\n\n", arg0);

   strlcat(buf, "  -h, --help                     "
         "Show this help message.\n", sizeof(buf));
   strlcat(buf, "  -v, --verbose                  "
         "Verbose logging.\n", sizeof(buf));
   strlcat(buf, "      --log-file=FILE            "
         "Log messages to FILE.\n", sizeof(buf));
   strlcat(buf, "      --version                  "
         "Show version.\n", sizeof(buf));
   strlcat(buf, "      --features                 "
         "Print available features compiled into program.\n", sizeof(buf));

#ifdef HAVE_MENU
   strlcat(buf, "      --menu                     "
         "Do not require content or libretro core to be loaded,\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  starts directly in menu. If no arguments are passed to\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  the program, it is equivalent to using --menu as only argument.\n", sizeof(buf));
#endif

#ifdef HAVE_CONFIGFILE
   strlcat(buf, "  -c, --config=FILE              "
         "Path for config file.\n", sizeof(buf));
#ifdef _WIN32
   strlcat(buf, "                                 "
         "  Defaults to retroarch.cfg in same directory as retroarch.exe.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  If a default config is not found, the program will attempt to create one.\n", sizeof(buf));
#else
   strlcat(buf, "                                 "
         "  By default looks for config in\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  $XDG_CONFIG_HOME/retroarch/retroarch.cfg,\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  $HOME/.config/retroarch/retroarch.cfg, and\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  $HOME/.retroarch.cfg.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  If a default config is not found, the program will attempt to create one\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  based on the skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg).\n", sizeof(buf));
#endif
   strlcat(buf, "      --appendconfig=FILE        "
         "Extra config files are loaded in, and take priority over\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  config selected in -c (or default). Multiple configs are\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  delimited by '|'.\n", sizeof(buf));
#endif

   fputs(buf, stdout);
   buf[0] = '\0';

#ifdef HAVE_DYNAMIC
   strlcat(buf, "  -L, --libretro=FILE            "
         "Path to libretro implementation. Overrides any config setting.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  FILE may be one of the following:\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  1. The full path to a core shared object library: path/to/<core_name>_libretro.<lib_ext>\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  2. A core shared object library 'file name' (*): <core_name>_libretro.<lib_ext>\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  3. A core 'short name' (*): <core_name>_libretro OR <core_name>\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  (*) If 'file name' or 'short name' do not correspond to an existing full file path,\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  the configured frontend 'cores' directory will be searched for a match.\n", sizeof(buf));
#endif

   strlcat(buf, "      --subsystem=NAME           "
         "Use a subsystem of the libretro core. Multiple content\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  files are loaded as multiple arguments. If a content\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  file is skipped, use a blank (\"\") command line argument.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  Content must be loaded in an order which depends on the\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  particular subsystem used. See verbose log output to learn\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  how a particular subsystem wants content to be loaded.\n", sizeof(buf));

   strlcat(buf, "  -f, --fullscreen               "
         "Start the program in fullscreen regardless of config setting.\n", sizeof(buf));
   strlcat(buf, "      --set-shader=PATH          "
         "Path to a shader (preset) that will be loaded each time content is loaded.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  Effectively overrides automatic shader presets.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  An empty argument \"\" will disable automatic shader presets.\n", sizeof(buf));

   fputs(buf, stdout);
   buf[0] = '\0';

   printf(      "  -N, --nodevice=PORT            "
         "Disconnects controller device connected to PORT (1 to %d).\n", MAX_USERS);
   printf(      "  -A, --dualanalog=PORT          "
         "Connect a DualAnalog controller to PORT (1 to %d).\n", MAX_USERS);
   printf(      "  -d, --device=PORT:ID           "
         "Connect a generic device into PORT of the device (1 to %d).\n", MAX_USERS);
   strlcat(buf, "                                 "
         "  Format is PORT:ID, where ID is a number corresponding to the particular device.\n", sizeof(buf));

   strlcat(buf, "  -M, --sram-mode=MODE           "
         "SRAM handling mode. MODE can be:\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  'noload-nosave', 'noload-save', 'load-nosave' or 'load-save'.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  Note: 'noload-save' implies that save files *WILL BE OVERWRITTEN*.\n", sizeof(buf));

#ifdef HAVE_NETWORKING
   strlcat(buf, "  -H, --host                     "
         "Host netplay as user 1.\n", sizeof(buf));
   strlcat(buf, "  -C, --connect=HOST             "
         "Connect to netplay server as user 2.\n", sizeof(buf));
   strlcat(buf, "      --port=PORT                "
         "Port used to netplay. Default is 55435.\n", sizeof(buf));
   strlcat(buf, "      --nick=NICK                "
         "Picks a username (for use with netplay). Not mandatory.\n", sizeof(buf));
   /*strlcat(buf, "      --stateless                "
         "Use \"stateless\" mode for netplay (requires a very fast network).\n", sizeof(buf));*/
   strlcat(buf, "      --check-frames=NUMBER      "
         "Check frames when using netplay.\n", sizeof(buf));
#ifdef HAVE_NETWORK_CMD
   strlcat(buf, "      --command                  "
         "Sends a command over UDP to an already running program process.\n", sizeof(buf));
   strlcat(buf, "                                 "
         "  Available commands are listed if command is invalid.\n", sizeof(buf));
#endif
#endif

#ifdef HAVE_BSV_MOVIE
   strlcat(buf, "  -P, --bsvplay=FILE             "
         "Playback a BSV movie file.\n", sizeof(buf));
   strlcat(buf, "  -R, --bsvrecord=FILE           "
         "Start recording a BSV movie file from the beginning.\n", sizeof(buf));
   strlcat(buf, "      --eof-exit                 "
         "Exit upon reaching the end of the BSV movie file.\n", sizeof(buf));
#endif

   strlcat(buf, "  -r, --record=FILE              "
         "Path to record video file. Using mkv extension is recommended.\n", sizeof(buf));
   strlcat(buf, "      --recordconfig             "
         "Path to settings used during recording.\n", sizeof(buf));
   strlcat(buf, "      --size=WIDTHxHEIGHT        "
         "Overrides output video size when recording.\n", sizeof(buf));

   fputs(buf, stdout);
   buf[0] = '\0';

   strlcat(buf, "  -D, --detach                   "
         "Detach program from the running console. Not relevant for all platforms.\n", sizeof(buf));
   strlcat(buf, "      --max-frames=NUMBER        "
         "Runs for the specified number of frames, then exits.\n", sizeof(buf));

#ifdef HAVE_PATCH
   strlcat(buf, "  -U, --ups=FILE                 "
         "Specifies path for UPS patch that will be applied to content.\n", sizeof(buf));
   strlcat(buf, "      --bps=FILE                 "
         "Specifies path for BPS patch that will be applied to content.\n", sizeof(buf));
   strlcat(buf, "      --ips=FILE                 "
         "Specifies path for IPS patch that will be applied to content.\n", sizeof(buf));
   strlcat(buf, "      --no-patch                 "
         "Disables all forms of content patching.\n", sizeof(buf));
#endif

#ifdef HAVE_SCREENSHOTS
   strlcat(buf, "      --max-frames-ss            "
         "Takes a screenshot at the end of max-frames.\n", sizeof(buf));
   strlcat(buf, "      --max-frames-ss-path=FILE  "
         "Path to save the screenshot to at the end of max-frames.\n", sizeof(buf));
#endif

#ifdef HAVE_ACCESSIBILITY
   strlcat(buf, "      --accessibility            "
         "Enables accessibilty for blind users using text-to-speech.\n", sizeof(buf));
#endif

   strlcat(buf, "      --load-menu-on-error       "
         "Open menu instead of quitting if specified core or content fails to load.\n", sizeof(buf));
   strlcat(buf, "  -e, --entryslot=NUMBER         "
         "Slot from which to load an entry state.\n", sizeof(buf));

   strlcat(buf, "  -s, --save=PATH                "
         "Path for save files (*.srm). (DEPRECATED, use --appendconfig and savefile_directory)\n", sizeof(buf));
   strlcat(buf, "  -S, --savestate=PATH           "
         "Path for the save state files (*.state). (DEPRECATED, use --appendconfig and savestate_directory)\n", sizeof(buf));

   fputs(buf, stdout);
   fputs("\n", stdout);
}

#ifdef HAVE_DYNAMIC
static void retroarch_parse_input_libretro_path(const char *path)
{
   settings_t *settings   = config_get_ptr();
   int path_stats         = 0;
   const char *path_ext   = NULL;
   core_info_t *core_info = NULL;
   const char *core_path  = NULL;
   bool core_path_matched = false;
   char tmp_path[PATH_MAX_LENGTH];

   if (string_is_empty(path))
      goto end;

   /* Check if path refers to a built-in core */
   if (string_ends_with_size(path, "builtin",
            strlen(path), STRLEN_CONST("builtin")))
   {
      RARCH_LOG("--libretro argument \"%s\" is a built-in core. Ignoring.\n",
            path);
      return;
   }

   path_stats = path_stat(path);

   /* Check if path is a directory */
   if ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
   {
      path_clear(RARCH_PATH_CORE);

      configuration_set_string(settings,
            settings->paths.directory_libretro, path);

      retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
      retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY, NULL);

      RARCH_WARN("Using old --libretro behavior. "
            "Setting libretro_directory to \"%s\" instead.\n",
            path);
      return;
   }

   /* Check if path is a valid file */
   if ((path_stats & RETRO_VFS_STAT_IS_VALID) != 0)
   {
      core_path = path;
      goto end;
   }

   /* If path refers to a core file that does not exist,
    * check for its presence in the user-defined cores
    * directory */
   path_ext = path_get_extension(path);

   if (!string_is_empty(path_ext))
   {
      char core_ext[255];

      core_ext[0] = '\0';

      if (string_is_empty(settings->paths.directory_libretro) ||
          !frontend_driver_get_core_extension(core_ext,
               sizeof(core_ext)) ||
          !string_is_equal(path_ext, core_ext))
         goto end;

      fill_pathname_join(tmp_path, settings->paths.directory_libretro,
            path, sizeof(tmp_path));

      if (string_is_empty(tmp_path))
         goto end;

      path_stats = path_stat(tmp_path);

      if ((path_stats & RETRO_VFS_STAT_IS_VALID) != 0 &&
          (path_stats & RETRO_VFS_STAT_IS_DIRECTORY) == 0)
      {
         core_path         = tmp_path;
         core_path_matched = true;
         goto end;
      }
   }
   else
   {
      /* If path has no extension and contains no path
       * delimiters, check if it is a core 'name', matching
       * an existing file in the cores directory */
      if (find_last_slash(path))
         goto end;

      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

      strlcpy(tmp_path, path, sizeof(tmp_path));

      if (!string_ends_with_size(tmp_path, "_libretro",
            strlen(tmp_path), STRLEN_CONST("_libretro")))
         strlcat(tmp_path, "_libretro", sizeof(tmp_path));

      if (!core_info_find(tmp_path, &core_info) ||
          string_is_empty(core_info->path))
         goto end;

      core_path         = core_info->path;
      core_path_matched = true;
   }

end:
   if (!string_is_empty(core_path))
   {
      path_set(RARCH_PATH_CORE, core_path);
      retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);

      /* We requested an explicit core, so use PLAIN core type. */
      runloop_set_current_core_type(CORE_TYPE_PLAIN, false);

      if (core_path_matched)
         RARCH_LOG("--libretro argument \"%s\" matches core file \"%s\".\n",
               path, core_path);
   }
   else
      RARCH_WARN("--libretro argument \"%s\" is not a file, core name"
            " or directory. Ignoring.\n",
            path ? path : "");
}
#endif

/**
 * retroarch_parse_input_and_config:
 * @argc                 : Count of (commandline) arguments.
 * @argv                 : (Commandline) arguments.
 *
 * Parses (commandline) arguments passed to program and loads the config file,
 * with command line options overriding the config file.
 *
 **/
static bool retroarch_parse_input_and_config(
      struct rarch_state *p_rarch,
      global_t *global,
      int argc, char *argv[])
{
   unsigned i;
   static bool           first_run = true;
   bool verbosity_enabled          = false;
   const char           *optstring = NULL;
   bool              explicit_menu = false;
   bool                 cli_active = false;
   bool               cli_core_set = false;
   bool            cli_content_set = false;
   recording_state_t *recording_st = recording_state_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   runloop_state_t     *runloop_st = runloop_state_get_ptr();
   settings_t          *settings   = config_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st       = access_state_get_ptr();
#endif

   const struct option opts[]      = {
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
#ifdef HAVE_CONFIGFILE
      { "config",             1, NULL, 'c' },
      { "appendconfig",       1, NULL, RA_OPT_APPENDCONFIG },
#endif
      { "nodevice",           1, NULL, 'N' },
      { "dualanalog",         1, NULL, 'A' },
      { "device",             1, NULL, 'd' },
      { "savestate",          1, NULL, 'S' },
      { "set-shader",         1, NULL, RA_OPT_SET_SHADER },
#ifdef HAVE_BSV_MOVIE
      { "bsvplay",            1, NULL, 'P' },
      { "bsvrecord",          1, NULL, 'R' },
#endif
      { "sram-mode",          1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",               0, NULL, 'H' },
      { "connect",            1, NULL, 'C' },
      { "stateless",          0, NULL, RA_OPT_STATELESS },
      { "check-frames",       1, NULL, RA_OPT_CHECK_FRAMES },
      { "port",               1, NULL, RA_OPT_PORT },
#ifdef HAVE_NETWORK_CMD
      { "command",            1, NULL, RA_OPT_COMMAND },
#endif
#endif
      { "nick",               1, NULL, RA_OPT_NICK },
#ifdef HAVE_PATCH
      { "ups",                1, NULL, 'U' },
      { "bps",                1, NULL, RA_OPT_BPS },
      { "ips",                1, NULL, RA_OPT_IPS },
      { "no-patch",           0, NULL, RA_OPT_NO_PATCH },
#endif
      { "detach",             0, NULL, 'D' },
      { "features",           0, NULL, RA_OPT_FEATURES },
      { "subsystem",          1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",         1, NULL, RA_OPT_MAX_FRAMES },
      { "max-frames-ss",      0, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT },
      { "max-frames-ss-path", 1, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT_PATH },
      { "eof-exit",           0, NULL, RA_OPT_EOF_EXIT },
      { "version",            0, NULL, RA_OPT_VERSION },
      { "log-file",           1, NULL, RA_OPT_LOG_FILE },
      { "accessibility",      0, NULL, RA_OPT_ACCESSIBILITY},
      { "load-menu-on-error", 0, NULL, RA_OPT_LOAD_MENU_ON_ERROR },
      { "entryslot",          1, NULL, 'e' },
      { NULL, 0, NULL, 0 }
   };

   if (first_run)
   {
      /* Copy the args into a buffer so launch arguments can be reused */
      for (i = 0; i < (unsigned)argc; i++)
      {
         strlcat(p_rarch->launch_arguments,
               argv[i], sizeof(p_rarch->launch_arguments));
         strlcat(p_rarch->launch_arguments, " ",
               sizeof(p_rarch->launch_arguments));
      }
      string_trim_whitespace_left(p_rarch->launch_arguments);
      string_trim_whitespace_right(p_rarch->launch_arguments);

      first_run  = false;

      /* Command line interface is only considered
       * to be 'active' (i.e. used by a third party)
       * if this is the first run (subsequent runs
       * are triggered by RetroArch itself) */
      cli_active = true;
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

   if (!runloop_st->has_set_core)
      runloop_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   p_rarch->has_set_username             = false;
#ifdef HAVE_PATCH
   p_rarch->rarch_ups_pref               = false;
   p_rarch->rarch_ips_pref               = false;
   p_rarch->rarch_bps_pref               = false;
   *runloop_st->name.ups                 = '\0';
   *runloop_st->name.bps                 = '\0';
   *runloop_st->name.ips                 = '\0';
#endif
#ifdef HAVE_CONFIGFILE
   runloop_st->overrides_active          = false;
#endif
   global->cli_load_menu_on_error        = false;

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:U:DN:d:e:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG CONFIG_FILE_ARG;

#if defined(WEBOS)
   argv      = &(argv[1]);
   argc      = argc - 1;
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

#ifdef HAVE_CONFIGFILE
            case 'c':
               path_set(RARCH_PATH_CONFIG, optarg);
               break;
            case RA_OPT_APPENDCONFIG:
               path_set(RARCH_PATH_CONFIG_APPEND, optarg);
               break;
#endif

            case 's':
               strlcpy(runloop_st->name.savefile, optarg,
                     sizeof(runloop_st->name.savefile));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL);
               break;

            case 'S':
               strlcpy(runloop_st->name.savestate, optarg,
                     sizeof(runloop_st->name.savestate));
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_STATE_PATH, NULL);
               break;
            case 'v':
               verbosity_enable();
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
               break;
            case RA_OPT_LOG_FILE:
               /* Enable 'log to file' */
               configuration_set_bool(settings,
                     settings->bools.log_to_file, true);

               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL);

               /* Cache log file path override */
               rarch_log_file_set_override(optarg);
               break;

            /* Must handle '?' otherwise you get an infinite loop */
            case '?':
               frontend_driver_attach_console();
#ifdef _WIN32
               fprintf(stderr, "\n%s: unrecognized option '%s'\n", argv[0], argv[optind]);
#endif
               fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
               exit(EXIT_FAILURE);
               break;
            /* All other arguments are handled in the second pass */
         }
      }
   }
   verbosity_enabled = verbosity_is_enabled();
   /* Enable logging to file if verbosity and log-file arguments were passed.
    * RARCH_OVERRIDE_SETTING_LOG_TO_FILE is set by the RA_OPT_LOG_FILE case above
    * The parameters passed to rarch_log_file_init are hardcoded as the config
    * has not yet been initialized at this point. */
   if (verbosity_enabled && retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL))
      rarch_log_file_init(true, false, NULL);

   /* Flush out some states that could have been set
    * by core environment variables. */
   runloop_st->current_core.has_set_input_descriptors = false;
   runloop_st->current_core.has_set_subsystems        = false;

   /* Load the config file now that we know what it is */
#ifdef HAVE_CONFIGFILE
   if (!p_rarch->rarch_block_config_read)
#endif
   {
      /* If this is a static build, load salamander
       * config file first (sets RARCH_PATH_CORE) */
#if !defined(HAVE_DYNAMIC)
      config_load_file_salamander();
#endif
      config_load(global_get_ptr());
   }

   verbosity_enabled = verbosity_is_enabled();
   /* Init logging after config load only if not overridden by command line argument.
    * This handles when logging is set in the config but not via the --log-file option. */
   if (verbosity_enabled && !retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LOG_TO_FILE, NULL))
      rarch_log_file_init(
            settings->bools.log_to_file,
            settings->bools.log_to_file_timestamp,
            settings->paths.log_dir);

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
                  new_port = port - 1;

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
               video_st->force_fullscreen = true;
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
                  input_config_set_device(new_port, RETRO_DEVICE_NONE);
                  retroarch_override_setting_set(
                        RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE, &new_port);
               }
               break;

            case 'r':
               strlcpy(recording_st->path, optarg,
                     sizeof(recording_st->path));
               if (recording_st->enable)
                  recording_st->enable = true;
               break;

            case RA_OPT_SET_SHADER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
               /* disable auto-shaders */
               if (string_is_empty(optarg))
               {
                  video_st->cli_shader_disable = true;
                  break;
               }

               /* rebase on shader directory */
               if (path_is_absolute(optarg))
                  strlcpy(video_st->cli_shader_path, optarg,
                        sizeof(video_st->cli_shader_path));
               else
                  fill_pathname_join(video_st->cli_shader_path,
                        settings->paths.directory_video_shader,
                        optarg, sizeof(video_st->cli_shader_path));
#endif
               break;

#ifdef HAVE_DYNAMIC
            case 'L':
               retroarch_parse_input_libretro_path(optarg);
               break;
#endif
            case 'P':
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  strlcpy(input_st->bsv_movie_state.movie_start_path, optarg,
                        sizeof(input_st->bsv_movie_state.movie_start_path));

                  input_st->bsv_movie_state.movie_start_playback  = true;
                  input_st->bsv_movie_state.movie_start_recording = false;
               }
#endif
               break;
            case 'R':
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  strlcpy(input_st->bsv_movie_state.movie_start_path, optarg,
                        sizeof(input_st->bsv_movie_state.movie_start_path));

                  input_st->bsv_movie_state.movie_start_playback  = false;
                  input_st->bsv_movie_state.movie_start_recording = true;
               }
#endif
               break;

            case 'M':
               if (string_is_equal(optarg, "noload-nosave"))
               {
                  runloop_st->is_sram_load_disabled = true;
                  runloop_st->is_sram_save_disabled = true;
               }
               else if (string_is_equal(optarg, "noload-save"))
                  runloop_st->is_sram_load_disabled = true;
               else if (string_is_equal(optarg, "load-nosave"))
                  runloop_st->is_sram_save_disabled = true;
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
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
               p_rarch->connect_host = strdup(optarg);
               break;

            case RA_OPT_STATELESS:
               configuration_set_bool(settings,
                     settings->bools.netplay_stateless_mode, true);

               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE, NULL);
               break;

            case RA_OPT_CHECK_FRAMES:
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);

               configuration_set_int(settings,
                     settings->ints.netplay_check_frames,
                     (int)strtoul(optarg, NULL, 0));
               break;

            case RA_OPT_PORT:
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT, NULL);
               configuration_set_uint(settings,
                     settings->uints.netplay_port,
                     (int)strtoul(optarg, NULL, 0));
               break;

#ifdef HAVE_NETWORK_CMD
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
#ifdef HAVE_PATCH
               strlcpy(runloop_st->name.bps, optarg,
                     sizeof(runloop_st->name.bps));
               p_rarch->rarch_bps_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
#endif
               break;

            case 'U':
#ifdef HAVE_PATCH
               strlcpy(runloop_st->name.ups, optarg,
                     sizeof(runloop_st->name.ups));
               p_rarch->rarch_ups_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
#endif
               break;

            case RA_OPT_IPS:
#ifdef HAVE_PATCH
               strlcpy(runloop_st->name.ips, optarg,
                     sizeof(runloop_st->name.ips));
               p_rarch->rarch_ips_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
#endif
               break;

            case RA_OPT_NO_PATCH:
#ifdef HAVE_PATCH
               runloop_st->patch_blocked = true;
#endif
               break;

            case 'D':
               frontend_driver_detach_console();
               break;

            case RA_OPT_MENU:
               explicit_menu = true;
               break;

            case RA_OPT_NICK:
               p_rarch->has_set_username = true;

               configuration_set_string(settings,
                     settings->paths.username, optarg);
               break;

            case RA_OPT_SIZE:
               if (sscanf(optarg, "%ux%u",
                        &recording_st->width,
                        &recording_st->height) != 2)
               {
                  RARCH_ERR("Wrong format for --size.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(1, "retroarch_parse_input()");
               }
               break;

            case RA_OPT_RECORDCONFIG:
               strlcpy(recording_st->config, optarg,
                     sizeof(recording_st->config));
               break;

            case RA_OPT_MAX_FRAMES:
               runloop_st->max_frames  = (unsigned)strtoul(optarg, NULL, 10);
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT:
#ifdef HAVE_SCREENSHOTS
               runloop_st->max_frames_screenshot = true;
#endif
               break;

            case RA_OPT_MAX_FRAMES_SCREENSHOT_PATH:
#ifdef HAVE_SCREENSHOTS
               strlcpy(runloop_st->max_frames_screenshot_path,
                     optarg,
                     sizeof(runloop_st->max_frames_screenshot_path));
#endif
               break;

            case RA_OPT_SUBSYSTEM:
               path_set(RARCH_PATH_SUBSYSTEM, optarg);
               break;

            case RA_OPT_FEATURES:
               retroarch_print_features();
               exit(0);

            case RA_OPT_EOF_EXIT:
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  input_st->bsv_movie_state.eof_exit = true;
               }
#endif
               break;

            case RA_OPT_VERSION:
               retroarch_print_version();
               exit(0);

            case 'h':
#ifdef HAVE_CONFIGFILE
            case 'c':
            case RA_OPT_APPENDCONFIG:
#endif
            case 's':
            case 'S':
            case 'v':
            case RA_OPT_LOG_FILE:
               break; /* Handled in the first pass */

            case '?':
               retroarch_print_help(argv[0]);
               retroarch_fail(1, "retroarch_parse_input()");
            case RA_OPT_ACCESSIBILITY:
#ifdef HAVE_ACCESSIBILITY
               access_st->enabled = true;
#endif
               break;
            case RA_OPT_LOAD_MENU_ON_ERROR:
               global->cli_load_menu_on_error = true;
               break;
            case 'e':
               {
                  unsigned entry_state_slot = (unsigned)strtoul(optarg, NULL, 0);

                  if (entry_state_slot)
                     runloop_st->entry_state_slot = entry_state_slot;
                  else
                     RARCH_WARN("--entryslot argument \"%s\" is not a valid "
                        "entry state slot index. Ignoring.\n", optarg);
               }
               break;
            default:
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
               retroarch_fail(1, "retroarch_parse_input()");
         }
      }
   }

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
         runloop_set_current_core_type(CORE_TYPE_DUMMY, false);
      }
#endif
   }

   if (optind < argc)
   {
      bool subsystem_path_is_empty = path_is_empty(RARCH_PATH_SUBSYSTEM);

      /* We requested explicit ROM, so use PLAIN core type. */
      runloop_set_current_core_type(CORE_TYPE_PLAIN, false);

      if (subsystem_path_is_empty)
         path_set(RARCH_PATH_NAMES, (const char*)argv[optind]);
      else
         path_set_special(argv + optind, argc - optind);

      /* Register that content has been set via the
       * command line interface */
      cli_content_set = true;
   }
   else if (runloop_st->entry_state_slot)
   {
      runloop_st->entry_state_slot = 0;
      RARCH_WARN("Trying to load entry state without content. Ignoring.\n");
   }

   /* Check whether a core has been set via the
    * command line interface */
   cli_core_set = (runloop_st->current_core_type != CORE_TYPE_DUMMY);

   /* Update global 'content launched from command
    * line' status flag */
   global->launched_from_cli = cli_active && (cli_core_set || cli_content_set);

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(runloop_st->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, runloop_st->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(runloop_st->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, runloop_st->name.savestate);

   return verbosity_enabled;
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
#if defined(DEBUG) && defined(HAVE_DRMINGW)
   char log_file_name[128];
#endif
   bool verbosity_enabled       = false;
   bool           init_failed   = false;
   struct rarch_state *p_rarch  = &rarch_st;
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   input_driver_state_t
      *input_st                 = input_state_get_ptr();
   video_driver_state_t*video_st= video_state_get_ptr();
   settings_t *settings         = config_get_ptr();
   recording_state_t
	   *recording_st             = recording_state_get_ptr();
   global_t            *global  = global_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st    = access_state_get_ptr();
   bool accessibility_enable    = false;
   unsigned accessibility_narrator_speech_speed = 0;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st   = menu_state_get_ptr();
#endif

   input_st->osk_idx            = OSK_LOWERCASE_LATIN;
   video_st->active             = true;
   audio_state_get_ptr()->active= true;

   if (setjmp(global->error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN),
            global->error_string);
      goto error;
   }

   global->error_on_init = true;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   verbosity_enabled = retroarch_parse_input_and_config(p_rarch,
         global_get_ptr(), argc, argv);

#ifdef HAVE_ACCESSIBILITY
   accessibility_enable                = settings->bools.accessibility_enable;
   accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   /* State that the narrator is on, and also include the first menu
      item we're on at startup. */
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
      accessibility_speak_priority(
            accessibility_enable,
            accessibility_narrator_speech_speed,
            "RetroArch accessibility on.  Main Menu Load Core.",
            10);
#endif

   if (verbosity_enabled)
   {
      {
         char str_output[256];
         const char *cpu_model  = NULL;
         str_output[0] = '\0';

         cpu_model     = frontend_driver_get_cpu_model_name();

         strlcpy(str_output,
               "=== Build =======================================\n",
               sizeof(str_output));

         if (!string_is_empty(cpu_model))
         {
            strlcat(str_output, FILE_PATH_LOG_INFO " CPU Model Name: ", sizeof(str_output));
            strlcat(str_output, cpu_model, sizeof(str_output));
            strlcat(str_output, "\n", sizeof(str_output));
         }

         RARCH_LOG_OUTPUT("%s", str_output);
      }
      {
         char str_output[256];
         char str[128];
         str[0]        = '\0';

         retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));

#ifdef HAVE_GIT_VERSION
         snprintf(str_output, sizeof(str_output),
               "%s: %s" "\n" FILE_PATH_LOG_INFO " Built: " __DATE__ "\n" FILE_PATH_LOG_INFO " Version: " PACKAGE_VERSION "\n" FILE_PATH_LOG_INFO " Git: %s" "\n" FILE_PATH_LOG_INFO " =================================================\n",
               msg_hash_to_str(MSG_CAPABILITIES),
               str,
               retroarch_git_version
               );
#else
         snprintf(str_output, sizeof(str_output),
               "%s: %s" "\n" FILE_PATH_LOG_INFO " Built: " __DATE__ "\n" FILE_PATH_LOG_INFO " Version: " PACKAGE_VERSION "\n" FILE_PATH_LOG_INFO " =================================================\n",
               msg_hash_to_str(MSG_CAPABILITIES),
               str);
#endif
         RARCH_LOG_OUTPUT("%s", str_output);
      }
   }

#if defined(DEBUG) && defined(HAVE_DRMINGW)
   RARCH_LOG_OUTPUT("Initializing Dr.MingW Exception handler\n");
   fill_str_dated_filename(log_file_name, "crash",
         "log", sizeof(log_file_name));
   ExcHndlInit();
   ExcHndlSetLogFileNameA(log_file_name);
#endif

   retroarch_validate_cpu_features();
   retroarch_init_task_queue();

   {
      const char    *fullpath  = path_get(RARCH_PATH_CONTENT);

      if (!string_is_empty(fullpath))
      {
         enum rarch_content_type cont_type = path_is_media_type(fullpath);
#ifdef HAVE_IMAGEVIEWER
         bool builtin_imageviewer          = settings->bools.multimedia_builtin_imageviewer_enable;
#endif
         bool builtin_mediaplayer          = settings->bools.multimedia_builtin_mediaplayer_enable;

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
                  runloop_set_current_core_type(CORE_TYPE_MPV, false);
#elif defined(HAVE_FFMPEG)
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  runloop_set_current_core_type(CORE_TYPE_FFMPEG, false);
#endif
               }
               break;
#ifdef HAVE_IMAGEVIEWER
            case RARCH_CONTENT_IMAGE:
               if (builtin_imageviewer)
               {
                  retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
                  runloop_set_current_core_type(CORE_TYPE_IMAGEVIEWER, false);
               }
               break;
#endif
            default:
               break;
         }
      }
   }

   /* Pre-initialize all drivers
    * Attempts to find a default driver for
    * all driver types.
    */
   if (!(audio_driver_find_driver(settings,
         "audio driver", verbosity_enabled)))
      retroarch_fail(1, "audio_driver_find()");
   if (!video_driver_find_driver(settings,
         "video driver", verbosity_enabled))
      retroarch_fail(1, "video_driver_find_driver()");
   if (!input_driver_find_driver(
            settings,
            "input driver", verbosity_enabled))
      retroarch_fail(1, "input_driver_find_driver()");

   if (!camera_driver_find_driver("camera driver", verbosity_enabled))
      retroarch_fail(1, "find_camera_driver()");

#ifdef HAVE_BLUETOOTH
   bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_FIND_DRIVER, NULL);
#endif
#ifdef HAVE_WIFI
   wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);
#endif
   location_driver_find_driver(settings,
         "location driver", verbosity_enabled);
#ifdef HAVE_MENU
   {
      if (!(menu_st->driver_ctx = menu_driver_find_driver(settings,
                  "menu driver", verbosity_enabled)))
         retroarch_fail(1, "menu_driver_find_driver()");
   }
#endif
   /* Enforce stored brightness if needed */
   if (frontend_driver_can_set_screen_brightness())
      frontend_driver_set_screen_brightness(settings->uints.screen_brightness);

   /* Attempt to initialize core */
   if (runloop_st->has_set_core)
   {
      runloop_st->has_set_core = false;
      if (!command_event(CMD_EVENT_CORE_INIT,
               &runloop_st->explicit_current_core_type))
         init_failed = true;
   }
   else if (!command_event(CMD_EVENT_CORE_INIT,
            &runloop_st->current_core_type))
      init_failed = true;

   /* Handle core initialization failure */
   if (init_failed)
   {
#ifdef HAVE_DYNAMIC
      /* Check if menu was active prior to core initialization */
      if (   !global->launched_from_cli
          ||  global->cli_load_menu_on_error
#ifdef HAVE_MENU
          ||  menu_st->alive
#endif
         )
#endif
      {
         /* Before initialising the dummy core, ensure
          * that we:
          * - Unload any active input remaps
          * - Disable any active config overrides */
         if (     runloop_st->remaps_core_active
               || runloop_st->remaps_content_dir_active
               || runloop_st->remaps_game_active
               || !string_is_empty(runloop_st->name.remapfile)
            )
         {
            input_remapping_deinit(false);
            input_remapping_set_defaults(true);
         }
         else
            input_remapping_restore_global_config(true);

#ifdef HAVE_CONFIGFILE
         if (runloop_st->overrides_active)
         {
            /* Reload the original config */
            config_unload_override();
            runloop_st->overrides_active = false;
         }
#endif

#ifdef HAVE_DYNAMIC
         /* Ensure that currently loaded core is properly
          * deinitialised */
         if (runloop_st->current_core_type != CORE_TYPE_DUMMY)
            command_event(CMD_EVENT_CORE_DEINIT, NULL);
#endif
         /* Attempt initializing dummy core */
         runloop_st->current_core_type = CORE_TYPE_DUMMY;
         if (!command_event(CMD_EVENT_CORE_INIT, &runloop_st->current_core_type))
            goto error;
      }
#ifdef HAVE_DYNAMIC
      else /* Fall back to regular error handling */
         goto error;
#endif
   }

#ifdef HAVE_CHEATS
   cheat_manager_state_free();
   command_event_init_cheats(
         settings->bools.apply_cheats_after_load,
         settings->paths.path_cheat_database,
#ifdef HAVE_BSV_MOVIE
         input_st->bsv_movie_state_handle
#else
         NULL
#endif
         );
#endif
   drivers_init(settings, DRIVERS_CMD_ALL, verbosity_enabled);
#ifdef HAVE_COMMAND
   input_driver_deinit_command(input_st);
   input_driver_init_command(input_st, settings);
#endif
#ifdef HAVE_NETWORKGAMEPAD
   if (input_st->remote)
      input_remote_free(input_st->remote,
            settings->uints.input_max_users);
   input_st->remote    = NULL;
   if (settings->bools.network_remote_enable)
      input_st->remote = input_driver_init_remote(
            settings,
            settings->uints.input_max_users);
#endif
   input_mapper_reset(&input_st->mapper);
#ifdef HAVE_REWIND
   command_event(CMD_EVENT_REWIND_INIT, NULL);
#endif
   command_event(CMD_EVENT_CONTROLLER_INIT, NULL);
   if (!string_is_empty(recording_st->path))
      command_event(CMD_EVENT_RECORD_INIT, NULL);

   runloop_path_init_savefile();

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   global->error_on_init            = false;
   runloop_st->is_inited            = true;

#ifdef HAVE_DISCORD
   {
      discord_state_t *discord_st = discord_state_get_ptr();

	   if (command_event(CMD_EVENT_DISCORD_INIT, NULL))
		   discord_st->inited = true;
   }
#endif

#ifdef HAVE_PRESENCE
   {
      presence_userdata_t userdata;
      userdata.status = PRESENCE_MENU;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
   }
#endif

#if defined(HAVE_AUDIOMIXER)
   audio_driver_load_system_sounds();
#endif

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   runloop_st->is_inited         = false;

   return false;
}

#if 0
static bool retroarch_is_on_main_thread(shtread_tls_t *tls)
{
#ifdef HAVE_THREAD_STORAGE
   return sthread_tls_get(tls) == MAGIC_POINTER;
#else
   return true;
#endif
}
#endif

void retroarch_init_task_queue(void)
{
#ifdef HAVE_THREADS
   settings_t *settings        = config_get_ptr();
   bool threaded_enable        = settings->bools.threaded_data_runloop_enable;
#else
   bool threaded_enable        = false;
#endif

   task_queue_deinit();
   task_queue_init(threaded_enable, runloop_task_msg_queue_push);
}

bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   struct rarch_state     *p_rarch = &rarch_st;
   runloop_state_t     *runloop_st = runloop_state_get_ptr();

   switch(state)
   {
      case RARCH_CTL_HAS_SET_SUBSYSTEMS:
         return runloop_st->current_core.has_set_subsystems;
      case RARCH_CTL_CORE_IS_RUNNING:
         return runloop_st->core_running;
#ifdef HAVE_BSV_MOVIE
      case RARCH_CTL_BSV_MOVIE_IS_INITED:
         return (input_state_get_ptr()->bsv_movie_state_handle != NULL);
#endif
#ifdef HAVE_PATCH
      case RARCH_CTL_IS_BPS_PREF:
         return p_rarch->rarch_bps_pref;
      case RARCH_CTL_UNSET_BPS_PREF:
         p_rarch->rarch_bps_pref = false;
         break;
      case RARCH_CTL_IS_UPS_PREF:
         return p_rarch->rarch_ups_pref;
      case RARCH_CTL_UNSET_UPS_PREF:
         p_rarch->rarch_ups_pref = false;
         break;
      case RARCH_CTL_IS_IPS_PREF:
         return p_rarch->rarch_ips_pref;
      case RARCH_CTL_UNSET_IPS_PREF:
         p_rarch->rarch_ips_pref = false;
         break;
#endif
      case RARCH_CTL_IS_DUMMY_CORE:
         return runloop_st->current_core_type == CORE_TYPE_DUMMY;
      case RARCH_CTL_IS_CORE_LOADED:
         {
            const char *core_path = (const char*)data;
            const char *core_file = path_basename_nocompression(core_path);
            if (!string_is_empty(core_file))
            {
               /* Get loaded core file name */
               const char *loaded_core_file = path_basename_nocompression(
                     path_get(RARCH_PATH_CORE));
               /* Check whether specified core and currently
                * loaded core are the same */
               if (!string_is_empty(loaded_core_file))
                  if (string_is_equal(core_file, loaded_core_file))
                     return true;
            }
         }
         return false;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
      case RARCH_CTL_IS_SECOND_CORE_AVAILABLE:
         return
                  runloop_st->core_running
               && runloop_st->runahead_secondary_core_available;
      case RARCH_CTL_IS_SECOND_CORE_LOADED:
         return
                   runloop_st->core_running
               && (runloop_st->secondary_lib_handle != NULL);
#endif
      case RARCH_CTL_HAS_SET_USERNAME:
         return p_rarch->has_set_username;
      case RARCH_CTL_IS_INITED:
         return runloop_st->is_inited;
      case RARCH_CTL_MAIN_DEINIT:
         {
            input_driver_state_t *input_st = input_state_get_ptr();
            if (!runloop_st->is_inited)
               return false;
            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
#ifdef HAVE_COMMAND
            input_driver_deinit_command(input_st);
#endif
#ifdef HAVE_NETWORKGAMEPAD
            if (input_st->remote)
               input_remote_free(input_st->remote,
                     config_get_ptr()->uints.input_max_users);
            input_st->remote = NULL;
#endif
            input_mapper_reset(&input_st->mapper);

#ifdef HAVE_THREADS
            if (runloop_st->use_sram)
               autosave_deinit();
#endif

            command_event(CMD_EVENT_RECORD_DEINIT, NULL);

            command_event(CMD_EVENT_SAVE_FILES, NULL);

#ifdef HAVE_REWIND
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
#endif
#ifdef HAVE_CHEATS
            cheat_manager_state_free();
#endif
#ifdef HAVE_BSV_MOVIE
            bsv_movie_deinit(input_st);
#endif

            command_event(CMD_EVENT_CORE_DEINIT, NULL);

            content_deinit();

            path_deinit_subsystem(runloop_st);
            path_deinit_savefile();

            runloop_st->is_inited         = false;

#ifdef HAVE_THREAD_STORAGE
            sthread_tls_delete(&p_rarch->rarch_tls);
#endif
         }
         break;
#ifdef HAVE_CONFIGFILE
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         p_rarch->rarch_block_config_read = true;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         p_rarch->rarch_block_config_read = false;
         break;
#endif
      case RARCH_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            if (runloop_st->core_options)
               *idx = (unsigned)runloop_st->core_options->size;
            else
               *idx = 0;
         }
         break;
      case RARCH_CTL_HAS_CORE_OPTIONS:
         return (runloop_st->core_options != NULL);
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts || !runloop_st->core_options)
               return false;
            *coreopts = runloop_st->core_options;
         }
         break;
      case RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY:
         if (runloop_st->core_options &&
             runloop_st->core_options_callback.update_display)
         {
            /* Note: The update_display() callback may read
             * core option values via RETRO_ENVIRONMENT_GET_VARIABLE.
             * This will reset the 'options updated' flag.
             * We therefore have to cache the current 'options updated'
             * state and restore it after the update_display() function
             * returns */
            bool values_updated  = runloop_st->core_options->updated;
            bool display_updated = runloop_st->core_options_callback.update_display();

            runloop_st->core_options->updated = values_updated;
            return display_updated;
         }
         return false;
#ifdef HAVE_CONFIGFILE
      case RARCH_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_st->overrides_active;
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         /* Only one type of remap can be active
          * at any one time */
         runloop_st->remaps_core_active        = true;
         runloop_st->remaps_content_dir_active = false;
         runloop_st->remaps_game_active        = false;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return runloop_st->remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_st->remaps_core_active        = false;
         runloop_st->remaps_content_dir_active = false;
         runloop_st->remaps_game_active        = true;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return runloop_st->remaps_game_active;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_st->remaps_core_active        = false;
         runloop_st->remaps_content_dir_active = true;
         runloop_st->remaps_game_active        = false;
         break;
      case RARCH_CTL_IS_REMAPS_CONTENT_DIR_ACTIVE:
         return runloop_st->remaps_content_dir_active;
#endif
      case RARCH_CTL_SET_MISSING_BIOS:
         runloop_st->missing_bios = true;
         break;
      case RARCH_CTL_UNSET_MISSING_BIOS:
         runloop_st->missing_bios = false;
         break;
      case RARCH_CTL_IS_MISSING_BIOS:
         return runloop_st->missing_bios;
      case RARCH_CTL_IS_GAME_OPTIONS_ACTIVE:
         return runloop_st->game_options_active;
      case RARCH_CTL_IS_FOLDER_OPTIONS_ACTIVE:
         return runloop_st->folder_options_active;
      case RARCH_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_st->perfcnt_enable;
         }
         break;
      case RARCH_CTL_SET_PERFCNT_ENABLE:
         runloop_st->perfcnt_enable = true;
         break;
      case RARCH_CTL_UNSET_PERFCNT_ENABLE:
         runloop_st->perfcnt_enable = false;
         break;
      case RARCH_CTL_IS_PERFCNT_ENABLE:
         return runloop_st->perfcnt_enable;
      case RARCH_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_st->pending_windowed_scale = *idx;
         }
         break;
      case RARCH_CTL_STATE_FREE:
         {
            input_driver_state_t *input_st = input_state_get_ptr();
            runloop_st->perfcnt_enable   = false;
            runloop_st->idle             = false;
            runloop_st->paused           = false;
            runloop_st->slowmotion       = false;
#ifdef HAVE_CONFIGFILE
            runloop_st->overrides_active = false;
#endif
            runloop_st->autosave         = false;
            runloop_frame_time_free();
            runloop_audio_buffer_status_free();
            input_game_focus_free();
            runloop_fastmotion_override_free();
            runloop_core_options_cb_free();
            runloop_st->video_swap_interval_auto = 1;
            memset(&input_st->analog_requested, 0,
                  sizeof(input_st->analog_requested));
         }
         break;
      case RARCH_CTL_IS_IDLE:
         return runloop_st->idle;
      case RARCH_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_st->idle = *ptr;
         }
         break;
      case RARCH_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_st->paused = *ptr;
         }
         break;
      case RARCH_CTL_IS_PAUSED:
         return runloop_st->paused;
      case RARCH_CTL_SET_SHUTDOWN:
         runloop_st->shutdown_initiated = true;
         break;
      case RARCH_CTL_CORE_OPTION_PREV:
         /*
          * Get previous value for core option specified by @idx.
          * Options wrap around.
          */
         {
            unsigned *idx = (unsigned*)data;
            if (!idx || !runloop_st->core_options)
               return false;
            core_option_manager_adjust_val(runloop_st->core_options,
                  *idx, -1, true);
         }
         break;
      case RARCH_CTL_CORE_OPTION_NEXT:
         /*
          * Get next value for core option specified by @idx.
          * Options wrap around.
          */
         {
            unsigned* idx = (unsigned*)data;
            if (!idx || !runloop_st->core_options)
               return false;
            core_option_manager_adjust_val(runloop_st->core_options,
                  *idx, 1, true);
         }
         break;


      case RARCH_CTL_NONE:
      default:
         return false;
   }

   return true;
}

bool retroarch_override_setting_is_set(
      enum rarch_override_setting enum_idx, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_NETWORKING
   net_driver_state_t *net_st  = networking_state_get_ptr();
#endif

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
               runloop_state_t *runloop_st = runloop_state_get_ptr();
               return BIT256_GET(runloop_st->has_set_libretro_device, bit);
            }
         }
         break;
      case RARCH_OVERRIDE_SETTING_VERBOSITY:
         return p_rarch->has_set_verbosity;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return p_rarch->has_set_libretro;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return p_rarch->has_set_libretro_directory;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return p_rarch->has_set_save_path;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return p_rarch->has_set_state_path;
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return net_st->has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return net_st->has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return net_st->has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return net_st->has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return net_st->has_set_netplay_check_frames;
#endif
#ifdef HAVE_PATCH
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return p_rarch->has_set_ups_pref;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return p_rarch->has_set_bps_pref;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return p_rarch->has_set_ips_pref;
#endif
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         return p_rarch->has_set_log_to_file;
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *s, size_t len)
{
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu     = cpu_features_get();

            if (cpu & RETRO_SIMD_MMX)
               strlcat(s, " MMX", len);
            if (cpu & RETRO_SIMD_MMXEXT)
               strlcat(s, " MMXEXT", len);
            if (cpu & RETRO_SIMD_SSE)
               strlcat(s, " SSE", len);
            if (cpu & RETRO_SIMD_SSE2)
               strlcat(s, " SSE2", len);
            if (cpu & RETRO_SIMD_SSE3)
               strlcat(s, " SSE3", len);
            if (cpu & RETRO_SIMD_SSSE3)
               strlcat(s, " SSSE3", len);
            if (cpu & RETRO_SIMD_SSE4)
               strlcat(s, " SSE4", len);
            if (cpu & RETRO_SIMD_SSE42)
               strlcat(s, " SSE4.2", len);
            if (cpu & RETRO_SIMD_AES)
               strlcat(s, " AES", len);
            if (cpu & RETRO_SIMD_AVX)
               strlcat(s, " AVX", len);
            if (cpu & RETRO_SIMD_AVX2)
               strlcat(s, " AVX2", len);
            if (cpu & RETRO_SIMD_NEON)
               strlcat(s, " NEON", len);
            if (cpu & RETRO_SIMD_VFPV3)
               strlcat(s, " VFPv3", len);
            if (cpu & RETRO_SIMD_VFPV4)
               strlcat(s, " VFPv4", len);
            if (cpu & RETRO_SIMD_VMX)
               strlcat(s, " VMX", len);
            if (cpu & RETRO_SIMD_VMX128)
               strlcat(s, " VMX128", len);
            if (cpu & RETRO_SIMD_VFPU)
               strlcat(s, " VFPU", len);
            if (cpu & RETRO_SIMD_PS)
               strlcat(s, " PS", len);
            if (cpu & RETRO_SIMD_ASIMD)
               strlcat(s, " ASIMD", len);
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

void retroarch_fail(int error_code, const char *error)
{
   global_t *global                = global_get_ptr();
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(global->error_on_init);
   strlcpy(global->error_string,
         error, sizeof(global->error_string));
   longjmp(global->error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
   runloop_state_t *runloop_st   = runloop_state_get_ptr();
   video_driver_state_t*video_st = video_state_get_ptr();
   settings_t *settings          = config_get_ptr();

#ifdef HAVE_PRESENCE
   {
      presence_userdata_t userdata;
      userdata.status = PRESENCE_SHUTDOWN;
      command_event(CMD_EVENT_PRESENCE_UPDATE, &userdata);
   }
#endif
#ifdef HAVE_DISCORD
   {
      discord_state_t *discord_st = discord_state_get_ptr();
      if (discord_st->ready)
      {
         Discord_ClearPresence();
#ifdef DISCORD_DISABLE_IO_THREAD
         Discord_UpdateConnection();
#endif
         Discord_Shutdown();
         discord_st->ready       = false;
      }
      discord_st->inited         = false;
   }
#endif

   /* Restore original refresh rate, if it has been changed
    * automatically in SET_SYSTEM_AV_INFO */
   if (video_st->video_refresh_rate_original)
      video_display_server_restore_refresh_rate();

   if (!runloop_st->shutdown_initiated)
   {
      command_event_save_auto_state(
            settings->bools.savestate_auto_save,
            runloop_st->current_core_type);

      /* If any save states are in progress, wait
       * until all tasks are complete (otherwise
       * save state file may be truncated) */
      content_wait_for_save_state_task();

      if (     runloop_st->remaps_core_active
            || runloop_st->remaps_content_dir_active
            || runloop_st->remaps_game_active
            || !string_is_empty(runloop_st->name.remapfile)
         )
      {
         input_remapping_deinit(settings->bools.remap_save_on_exit);
         input_remapping_set_defaults(true);
      }
      else
         input_remapping_restore_global_config(true);

#ifdef HAVE_CONFIGFILE
      if (runloop_st->overrides_active)
      {
         /* Reload the original config */
         config_unload_override();
         runloop_st->overrides_active = false;
      }
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
      runloop_st->runtime_shader_preset_path[0] = '\0';
#endif
   }

   runloop_st->shutdown_initiated = true;
#ifdef HAVE_MENU
   retroarch_menu_running_finished(true);
#endif

   return true;
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
      {"tr", RETRO_LANGUAGE_TURKISH},
      {"sk", RETRO_LANGUAGE_SLOVAK},
      {"fa", RETRO_LANGUAGE_PERSIAN},
      {"he", RETRO_LANGUAGE_HEBREW},
      {"ast", RETRO_LANGUAGE_ASTURIAN},
      {"fi", RETRO_LANGUAGE_FINNISH},
      {"id", RETRO_LANGUAGE_INDONESIAN},
      {"sv", RETRO_LANGUAGE_SWEDISH},
      {"uk", RETRO_LANGUAGE_UKRAINIAN},
      {"cs", RETRO_LANGUAGE_CZECH},
      {"val", RETRO_LANGUAGE_CATALAN_VALENCIA},
      {"ca", RETRO_LANGUAGE_CATALAN},
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

void rarch_favorites_init(void)
{
   settings_t *settings                = config_get_ptr();
   int content_favorites_size          = settings ? settings->ints.content_favorites_size : 0;
   const char *path_content_favorites  = settings ? settings->paths.path_content_favorites : NULL;
   bool playlist_sort_alphabetical     = settings ? settings->bools.playlist_sort_alphabetical : false;
   playlist_config_t playlist_config;
   enum playlist_sort_mode current_sort_mode;

   playlist_config.capacity            = COLLECTION_SIZE;
   playlist_config.old_format          = settings ? settings->bools.playlist_use_old_format : false;
   playlist_config.compress            = settings ? settings->bools.playlist_compression : false;
   playlist_config.fuzzy_archive_match = settings ? settings->bools.playlist_fuzzy_archive_match : false;
   playlist_config_set_base_content_directory(&playlist_config, NULL);

   if (!settings)
      return;

   if (content_favorites_size >= 0)
      playlist_config.capacity = (size_t)content_favorites_size;

   rarch_favorites_deinit();

   RARCH_LOG("[Playlist]: %s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_FAVORITES_FILE),
         path_content_favorites);
   playlist_config_set_path(&playlist_config, path_content_favorites);
   g_defaults.content_favorites = playlist_init(&playlist_config);

   /* Get current per-playlist sort mode */
   current_sort_mode = playlist_get_sort_mode(g_defaults.content_favorites);

   /* Ensure that playlist is sorted alphabetically,
    * if required */
   if ((playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT)) ||
       (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
      playlist_qsort(g_defaults.content_favorites);
}

void rarch_favorites_deinit(void)
{
   if (!g_defaults.content_favorites)
      return;

   playlist_write_file(g_defaults.content_favorites);
   playlist_free(g_defaults.content_favorites);
   g_defaults.content_favorites = NULL;
}

#ifdef HAVE_ACCESSIBILITY
bool accessibility_speak_priority(
      bool accessibility_enable,
      unsigned accessibility_narrator_speech_speed,
      const char* speak_text, int priority)
{
   access_state_t *access_st   = access_state_get_ptr();
   if (is_accessibility_enabled(
            accessibility_enable,
            access_st->enabled))
   {
      frontend_ctx_driver_t *frontend =
         frontend_state_get_ptr()->current_frontend_ctx;

      RARCH_LOG("Spoke: %s\n", speak_text);

      if (frontend && frontend->accessibility_speak)
         return frontend->accessibility_speak(accessibility_narrator_speech_speed, speak_text,
               priority);

      RARCH_LOG("Platform not supported for accessibility.\n");
      /* The following method is a fallback for other platforms to use the
         AI Service url to do the TTS.  However, since the playback is done
         via the audio mixer, which only processes the audio while the
         core is running, this playback method won't work.  When the audio
         mixer can handle playing streams while the core is paused, then
         we can use this. */
#if 0
#if defined(HAVE_NETWORKING)
      return accessibility_speak_ai_service(speak_text, voice, priority);
#endif
#endif
   }

   return true;
}
#endif
