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
#include <retro_miscellaneous.h>
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

#ifdef HAVE_MIST
#include "steam/steam.h"
#endif

#include "config.def.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#include "location_driver.h"

#include "runloop.h"
#include "camera/camera_driver.h"
#include "location_driver.h"
#include "record/record_driver.h"

#ifdef HAVE_MICROPHONE
#include "audio/microphone_driver.h"
#endif

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
#ifdef HAVE_CLOUDSYNC
#include "network/cloud_sync_driver.h"
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

#include "gfx/video_driver.h"
#include "gfx/video_display_server.h"
#ifdef HAVE_BLUETOOTH
#include "bluetooth/bluetooth_driver.h"
#endif
#include "misc/cpufreq/cpufreq.h"
#include "led/led_driver.h"
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

/* Griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

#define MIDI_DRIVER_BUF_SIZE 4096
#define MIDI_DRIVER_OFF "OFF"

/* Descriptive names for options without short variant.
 *
 * Please keep the name in sync with the option name.
 * Order does not matter. */
enum
{
   RA_OPT_MENU = 256, /* must be outside the range of a char */
   RA_OPT_CHECK_FRAMES,
   RA_OPT_PORT,
   RA_OPT_SPECTATE,
   RA_OPT_NICK,
   RA_OPT_COMMAND,
   RA_OPT_APPENDCONFIG,
   RA_OPT_BPS,
   RA_OPT_IPS,
   RA_OPT_XDELTA,
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
   RA_OPT_DATABASE_SCAN,
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
   char *connect_mitm_id; /* Netplay MITM address from CLI */

   struct retro_perf_counter *perf_counters_rarch[MAX_COUNTERS];

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_t rarch_tls;               /* unsigned alignment */
#endif
   unsigned perf_ptr_rarch;
   uint16_t flags;

   char launch_arguments[4096];
   char path_default_shader_preset[PATH_MAX_LENGTH];
   char path_content[PATH_MAX_LENGTH];
   char path_libretro[PATH_MAX_LENGTH];
   char path_config_file[PATH_MAX_LENGTH];
   char path_config_append_file[PATH_MAX_LENGTH];
   char path_config_override_file[PATH_MAX_LENGTH];
   char path_core_options_file[PATH_MAX_LENGTH];
   char dir_system[DIR_MAX_LENGTH];
   char dir_savefile[DIR_MAX_LENGTH];
   char dir_savestate[DIR_MAX_LENGTH];
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

static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch) { }
void retro_input_poll_null(void) { }

static location_driver_t location_null = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "null",
};

static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
   &location_null,
   NULL,
};

static location_driver_state_t location_driver_st     = {0};

location_driver_state_t *location_state_get_ptr(void)
{
   return &location_driver_st;
}

const char *config_get_location_driver_options(void)
{
   return char_list_new_special(STRING_LIST_LOCATION_DRIVERS, NULL);
}

static void location_driver_find_driver(
      settings_t *settings,
      location_driver_state_t *location_st,
      const char *prefix,
      bool verbosity_enabled)
{
   int i                        = (int)driver_find_index(
         "location_driver",
         settings->arrays.location_driver);

   if (i >= 0)
      location_st->driver       = (const location_driver_t*)
         location_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.location_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; location_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", location_drivers[d]->ident);

         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      location_st->driver = (const location_driver_t*)location_drivers[0];
   }
}

bool driver_location_start(void)
{
   location_driver_state_t
      *location_st              = &location_driver_st;
   if (     location_st
         && location_st->data
         && location_st->driver
         && location_st->driver->start)
   {
      settings_t *settings = config_get_ptr();
      bool location_allow  = settings->bools.location_allow;
      if (location_allow)
         return location_st->driver->start(location_st->data);

      runloop_msg_queue_push("Location is explicitly disabled.\n",
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return false;
}

void driver_location_stop(void)
{
   location_driver_state_t
      *location_st              = &location_driver_st;
   if (     location_st
         && location_st->driver
         && location_st->driver->stop
         && location_st->data)
      location_st->driver->stop(location_st->data);
}

void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance)
{
   location_driver_state_t
      *location_st              = &location_driver_st;
   if (     location_st
         && location_st->driver
         && location_st->driver->set_interval
         && location_st->data)
      location_st->driver->set_interval(location_st->data,
            interval_msecs, interval_distance);
}

bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy)
{
   location_driver_state_t
      *location_st              = &location_driver_st;
   if (     location_st
         && location_st->driver
         && location_st->driver->get_position
         && location_st->data)
      return location_st->driver->get_position(location_st->data,
            lat, lon, horiz_accuracy, vert_accuracy);

   *lat            = 0.0;
   *lon            = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

static bool init_location(
      void *data,
      location_driver_state_t *location_st,
      settings_t *settings,
      bool verbosity_enabled)
{
   /* Resource leaks will follow if location
      interface is initialized twice. */
   if (!location_st->data)
   {
      rarch_system_info_t *sys_info = (rarch_system_info_t*)data;
      location_driver_find_driver(settings,
            &location_driver_st,
            "location driver", verbosity_enabled);

      if (!(location_st->data = location_st->driver->init()))
      {
         RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
         return false;
      }

      if (sys_info->location_cb.initialized)
         sys_info->location_cb.initialized();
   }

   return true;
}

static void uninit_location(void *data, location_driver_state_t *location_st)
{
   if (location_st->data && location_st->driver)
   {
      rarch_system_info_t *sys_info = (rarch_system_info_t*)data;
      if (sys_info->location_cb.deinitialized)
         sys_info->location_cb.deinitialized();

      if (location_st->driver->free)
         location_st->driver->free(location_st->data);
   }

   location_st->active = false;
   location_st->data   = NULL;
}

static void *rarch_midi_drv_data;
static struct string_list *rarch_midi_drv_inputs;
static struct string_list *rarch_midi_drv_outputs;
static uint8_t *rarch_midi_drv_input_buffer;
static uint8_t *rarch_midi_drv_output_buffer;

static midi_event_t rarch_midi_drv_input_event;  /* ptr alignment */
static midi_event_t rarch_midi_drv_output_event; /* ptr alignment */

static bool rarch_midi_drv_input_enabled;
static bool rarch_midi_drv_output_enabled;
static bool rarch_midi_drv_output_pending;

static void null_midi_free(void *p) { }
static void *null_midi_init(const char *input, const char *output) { return (void*)-1; }
static bool null_midi_get_avail_inputs(struct string_list *inputs) { union string_list_elem_attr attr = {0}; return string_list_append(inputs, "Null", attr); }
static bool null_midi_get_avail_outputs(struct string_list *outputs) { union string_list_elem_attr attr = {0}; return string_list_append(outputs, "Null", attr); }
static bool null_midi_set_input(void *p, const char *input) { return input == NULL || string_is_equal(input, "Null"); }
static bool null_midi_set_output(void *p, const char *output) { return output == NULL || string_is_equal(output, "Null"); }
static bool null_midi_read(void *p, midi_event_t *event) { return false; }
static bool null_midi_write(void *p, const midi_event_t *event) { return true; }
static bool null_midi_flush(void *p) { return true; }

static midi_driver_t midi_null = {
   "null",
   null_midi_get_avail_inputs,
   null_midi_get_avail_outputs,
   null_midi_init,
   null_midi_free,
   null_midi_set_input,
   null_midi_set_output,
   null_midi_read,
   null_midi_write,
   null_midi_flush
};

static midi_driver_t *midi_drv  = &midi_null;

midi_driver_t *midi_drivers[]   = {
#if defined(HAVE_ALSA) && !defined(HAVE_HAKCHI) && !defined(HAVE_SEGAM) && !defined(DINGUX)
   &midi_alsa,
#endif
#ifdef HAVE_WINMM
   &midi_winmm,
#endif
   &midi_null
};

static midi_driver_t *midi_driver_find_driver(const char *ident)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(midi_drivers); ++i)
   {
      if (string_is_equal(midi_drivers[i]->ident, ident))
         return midi_drivers[i];
   }

   RARCH_ERR("[MIDI]: Unknown driver \"%s\", falling back to \"null\" driver.\n", ident);

   return &midi_null;
}

static const void *midi_driver_find_handle(int index)
{
   if (index < 0 || index >= (int)ARRAY_SIZE(midi_drivers))
      return NULL;

   return midi_drivers[index];
}

struct string_list *midi_driver_get_avail_inputs(void)
{
   return rarch_midi_drv_inputs;
}

struct string_list *midi_driver_get_avail_outputs(void)
{
   return rarch_midi_drv_outputs;
}

bool midi_driver_set_all_sounds_off(void)
{
   midi_event_t event;
   uint8_t i;
   uint8_t data[3]     = { 0xB0, 120, 0 };
   bool result         = true;

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
      return false;

#ifdef HAVE_WASAPI
   /* FIXME: Due to some mysterious reason Frame Delay does not
    * work with WASAPI unless MIDI output is active, even when
    * MIDI is not used. Frame Delay also breaks if MIDI sounds
    * are "set off", which happens on menu toggle, therefore
    * skip this if WASAPI is used and Frame Delay is active.. */
   if (string_is_equal(audio_state_get_ptr()->current_audio->ident, "wasapi"))
   {
      if (video_state_get_ptr()->frame_delay_target > 0)
         return false;
   }
#endif

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   for (i = 0; i < 16; ++i)
   {
      data[0] = 0xB0 | i;

      if (!midi_drv->write(rarch_midi_drv_data, &event))
         result = false;
   }

   if (!midi_drv->flush(rarch_midi_drv_data))
      result = false;

   if (!result)
      RARCH_ERR("[MIDI]: All sounds off failed.\n");

   return result;
}

bool midi_driver_set_volume(unsigned volume)
{
   midi_event_t event;
   uint8_t         data[8]     = {
      0xF0, 0x7F, 0x7F, 0x04, 0x01, 0, 0, 0xF7};

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
      return false;

   volume           = (unsigned)(163.83 * volume + 0.5);
   if (volume > 16383)
      volume        = 16383;

   data[5]          = (uint8_t)(volume & 0x7F);
   data[6]          = (uint8_t)(volume >> 7);

   event.data       = data;
   event.data_size  = sizeof(data);
   event.delta_time = 0;

   if (!midi_drv->write(rarch_midi_drv_data, &event))
   {
      RARCH_ERR("[MIDI]: Volume change failed.\n");
      return false;
   }

   return true;
}

static bool midi_driver_init_io_buffers(void)
{
   uint8_t *midi_drv_input_buffer  = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);
   uint8_t *midi_drv_output_buffer = (uint8_t*)malloc(MIDI_DRIVER_BUF_SIZE);

   if (!midi_drv_input_buffer || !midi_drv_output_buffer)
   {
      if (midi_drv_input_buffer)
         free(midi_drv_input_buffer);
      if (midi_drv_output_buffer)
         free(midi_drv_output_buffer);
      return false;
   }

   rarch_midi_drv_input_buffer           = midi_drv_input_buffer;
   rarch_midi_drv_output_buffer          = midi_drv_output_buffer;

   rarch_midi_drv_input_event.data       = midi_drv_input_buffer;
   rarch_midi_drv_input_event.data_size  = 0;

   rarch_midi_drv_output_event.data      = midi_drv_output_buffer;
   rarch_midi_drv_output_event.data_size = 0;

   return true;
}

static void midi_driver_free(void)
{
   if (rarch_midi_drv_data)
   {
      midi_drv->free(rarch_midi_drv_data);
      rarch_midi_drv_data = NULL;
   }

   if (rarch_midi_drv_inputs)
   {
      string_list_free(rarch_midi_drv_inputs);
      rarch_midi_drv_inputs = NULL;
   }

   if (rarch_midi_drv_outputs)
   {
      string_list_free(rarch_midi_drv_outputs);
      rarch_midi_drv_outputs = NULL;
   }

   if (rarch_midi_drv_input_buffer)
   {
      free(rarch_midi_drv_input_buffer);
      rarch_midi_drv_input_buffer = NULL;
   }

   if (rarch_midi_drv_output_buffer)
   {
      free(rarch_midi_drv_output_buffer);
      rarch_midi_drv_output_buffer = NULL;
   }

   rarch_midi_drv_input_enabled  = false;
   rarch_midi_drv_output_enabled = false;
}

static bool midi_driver_init(void *data)
{
   union string_list_elem_attr
      attr                        = {0};
   bool ret                       = true;
   settings_t *settings           = (settings_t*)data;

   rarch_midi_drv_inputs          = string_list_new();
   rarch_midi_drv_outputs         = string_list_new();

   if (!rarch_midi_drv_inputs || !rarch_midi_drv_outputs)
      ret = false;
   else if (!string_list_append(rarch_midi_drv_inputs, MIDI_DRIVER_OFF, attr) ||
            !string_list_append(rarch_midi_drv_outputs, MIDI_DRIVER_OFF, attr))
      ret = false;
   else
   {
      char * input  = NULL;
      char * output = NULL;

      midi_drv      = midi_driver_find_driver(
            settings->arrays.midi_driver);

      if (strcmp(midi_drv->ident, settings->arrays.midi_driver))
      {
         configuration_set_string(settings,
               settings->arrays.midi_driver, midi_drv->ident);
      }

      if (!midi_drv->get_avail_inputs(rarch_midi_drv_inputs))
         ret = false;
      else if (!midi_drv->get_avail_outputs(rarch_midi_drv_outputs))
         ret = false;
      else
      {
         if (string_is_not_equal(settings->arrays.midi_input, MIDI_DRIVER_OFF))
         {
            if (string_list_find_elem(rarch_midi_drv_inputs, settings->arrays.midi_input))
               input = settings->arrays.midi_input;
            else
            {
               RARCH_WARN("[MIDI]: Input device \"%s\" unavailable.\n",
                     settings->arrays.midi_input);
               configuration_set_string(settings,
                     settings->arrays.midi_input, MIDI_DRIVER_OFF);
            }
         }

         if (string_is_not_equal(settings->arrays.midi_output, MIDI_DRIVER_OFF))
         {
            if (string_list_find_elem(rarch_midi_drv_outputs, settings->arrays.midi_output))
               output = settings->arrays.midi_output;
            else
            {
               RARCH_WARN("[MIDI]: Output device \"%s\" unavailable.\n",
                     settings->arrays.midi_output);
               configuration_set_string(settings,
                     settings->arrays.midi_output, MIDI_DRIVER_OFF);
            }
         }

         rarch_midi_drv_data = midi_drv->init(input, output);
         if (!rarch_midi_drv_data)
            ret = false;
         else
         {
            rarch_midi_drv_input_enabled  = (input  != NULL);
            rarch_midi_drv_output_enabled = (output != NULL);

            if (!midi_driver_init_io_buffers())
               ret = false;
            else
            {
               if (input)
                  RARCH_LOG("[MIDI]: Input device: \"%s\".\n", input);

               if (output)
               {
                  RARCH_LOG("[MIDI]: Output device: \"%s\".\n", output);
                  midi_driver_set_volume(settings->uints.midi_volume);
               }
            }
         }
      }
   }

   if (!ret)
   {
      midi_driver_free();
      RARCH_ERR("[MIDI]: Initialization failed.\n");
      return false;
   }
   return true;
}

bool midi_driver_set_input(const char *input)
{
   if (!rarch_midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_input called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(input, MIDI_DRIVER_OFF))
      input = NULL;

   if (!midi_drv->set_input(rarch_midi_drv_data, input))
   {
      if (input)
         RARCH_ERR("[MIDI]: Failed to change input device to \"%s\".\n", input);
      else
         RARCH_ERR("[MIDI]: Failed to disable input.\n");
      return false;
   }

   if (input)
      RARCH_LOG("[MIDI]: Input device changed to \"%s\".\n", input);
   else
      RARCH_LOG("[MIDI]: Input disabled.\n");

   rarch_midi_drv_input_enabled = input != NULL;

   return true;
}

bool midi_driver_set_output(void *settings_data, const char *output)
{
   settings_t        *settings = (settings_t*)settings_data;

   if (!rarch_midi_drv_data)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_set_output called on uninitialized driver.\n");
#endif
      return false;
   }

   if (string_is_equal(output, MIDI_DRIVER_OFF))
      output = NULL;

   if (!midi_drv->set_output(rarch_midi_drv_data, output))
   {
      if (output)
         RARCH_ERR("[MIDI]: Failed to change output device to \"%s\".\n", output);
      else
         RARCH_ERR("[MIDI]: Failed to disable output.\n");
      return false;
   }

   if (output)
   {
      rarch_midi_drv_output_enabled = true;
      RARCH_LOG("[MIDI]: Output device changed to \"%s\".\n", output);

      midi_driver_set_volume(settings->uints.midi_volume);
   }
   else
   {
      rarch_midi_drv_output_enabled = false;
      RARCH_LOG("[MIDI]: Output disabled.\n");
   }

   return true;
}

bool midi_driver_input_enabled(void)
{
   return rarch_midi_drv_input_enabled;
}

bool midi_driver_output_enabled(void)
{
   return rarch_midi_drv_output_enabled;
}

bool midi_driver_read(uint8_t *byte)
{
   static int i = 0;

   if (!rarch_midi_drv_data || !rarch_midi_drv_input_enabled || !byte)
   {
#ifdef DEBUG
      if (!rarch_midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_read called on uninitialized driver.\n");
      else if (!rarch_midi_drv_input_enabled)
         RARCH_ERR("[MIDI]: midi_driver_read called when input is disabled.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_read called with null pointer.\n");
#endif
      return false;
   }

   if (i == (int)rarch_midi_drv_input_event.data_size)
   {
      rarch_midi_drv_input_event.data_size = MIDI_DRIVER_BUF_SIZE;
      if (!midi_drv->read(rarch_midi_drv_data, &rarch_midi_drv_input_event))
      {
         rarch_midi_drv_input_event.data_size = i;
         return false;
      }

      i = 0;

#ifdef DEBUG
      if (rarch_midi_drv_input_event.data_size == 1)
         RARCH_LOG("[MIDI]: In [0x%02X].\n",
               rarch_midi_drv_input_event.data[0]);
      else if (rarch_midi_drv_input_event.data_size == 2)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X].\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data[1]);
      else if (rarch_midi_drv_input_event.data_size == 3)
         RARCH_LOG("[MIDI]: In [0x%02X, 0x%02X, 0x%02X].\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data[1],
               rarch_midi_drv_input_event.data[2]);
      else
         RARCH_LOG("[MIDI]: In [0x%02X, ...], size %u.\n",
               rarch_midi_drv_input_event.data[0],
               rarch_midi_drv_input_event.data_size);
#endif
   }

   *byte = rarch_midi_drv_input_event.data[i++];

   return true;
}

bool midi_driver_write(uint8_t byte, uint32_t delta_time)
{
   static int event_size;

   if (!rarch_midi_drv_data || !rarch_midi_drv_output_enabled)
   {
#ifdef DEBUG
      if (!rarch_midi_drv_data)
         RARCH_ERR("[MIDI]: midi_driver_write called on uninitialized driver.\n");
      else
         RARCH_ERR("[MIDI]: midi_driver_write called when output is disabled.\n");
#endif
      return false;
   }

   if (byte >= 0x80)
   {
      if (     rarch_midi_drv_output_event.data_size
            && rarch_midi_drv_output_event.data[0] == 0xF0)
      {
         if (byte == 0xF7)
            event_size = (int)rarch_midi_drv_output_event.data_size + 1;
         else
         {
            if (!midi_drv->write(rarch_midi_drv_data,
                     &rarch_midi_drv_output_event))
               return false;

#ifdef DEBUG
            switch (rarch_midi_drv_output_event.data_size)
            {
               case 1:
                  RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                        rarch_midi_drv_output_event.data[0]);
                  break;
               case 2:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data[1]);
                  break;
               case 3:
                  RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data[1],
                        rarch_midi_drv_output_event.data[2]);
                  break;
               default:
                  RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                        rarch_midi_drv_output_event.data[0],
                        rarch_midi_drv_output_event.data_size);
                  break;
            }
#endif

            rarch_midi_drv_output_pending             = true;
            event_size                                = (int)midi_driver_get_event_size(byte);
            rarch_midi_drv_output_event.data_size     = 0;
            rarch_midi_drv_output_event.delta_time    = 0;
         }
      }
      else
      {
         event_size                                   = (int)midi_driver_get_event_size(byte);
         rarch_midi_drv_output_event.data_size        = 0;
         rarch_midi_drv_output_event.delta_time       = 0;
      }
   }

   if (rarch_midi_drv_output_event.data_size < MIDI_DRIVER_BUF_SIZE)
   {
      rarch_midi_drv_output_event.data[rarch_midi_drv_output_event.data_size] = byte;
      ++rarch_midi_drv_output_event.data_size;
      rarch_midi_drv_output_event.delta_time += delta_time;
   }
   else
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Output event dropped.\n");
#endif
      return false;
   }

   if (event_size == (int)rarch_midi_drv_output_event.data_size)
   {
      if (!midi_drv->write(rarch_midi_drv_data, &rarch_midi_drv_output_event))
         return false;

#ifdef DEBUG
      switch (rarch_midi_drv_output_event.data_size)
      {
         case 1:
            RARCH_LOG("[MIDI]: Out [0x%02X].\n",
                  rarch_midi_drv_output_event.data[0]);
            break;
         case 2:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X].\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data[1]);
            break;
         case 3:
            RARCH_LOG("[MIDI]: Out [0x%02X, 0x%02X, 0x%02X].\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data[1],
                  rarch_midi_drv_output_event.data[2]);
            break;
         default:
            RARCH_LOG("[MIDI]: Out [0x%02X, ...], size %u.\n",
                  rarch_midi_drv_output_event.data[0],
                  rarch_midi_drv_output_event.data_size);
            break;
      }
#endif

      rarch_midi_drv_output_pending             = true;
      rarch_midi_drv_output_event.data_size     = 0;
      rarch_midi_drv_output_event.delta_time    = 0;
   }

   return true;
}

bool midi_driver_flush(void)
{
   if (!rarch_midi_drv_data)
      return false;

   if (rarch_midi_drv_output_pending)
      rarch_midi_drv_output_pending = !midi_drv->flush(rarch_midi_drv_data);

   return !rarch_midi_drv_output_pending;
}

size_t midi_driver_get_event_size(uint8_t status)
{
   static const uint8_t midi_drv_ev_sizes[128]                     =
   {
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
      3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
      0, 2, 3, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
   };

   if (status < 0x80)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: midi_driver_get_event_size called with invalid status.\n");
#endif
      return 0;
   }

   return midi_drv_ev_sizes[status - 0x80];
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
static const void *find_driver_nonempty(
      const char *label, int i,
      char *s, size_t len)
{
   if (string_is_equal(label, "camera_driver"))
   {
      if (camera_drivers[i])
      {
         const char *ident = camera_drivers[i]->ident;

         strlcpy(s, ident, len);
         return camera_drivers[i];
      }
   }
   else if (string_is_equal(label, "location_driver"))
   {
      if (location_drivers[i])
      {
         const char *ident = location_drivers[i]->ident;

         strlcpy(s, ident, len);
         return location_drivers[i];
      }
   }
#ifdef HAVE_MENU
   else if (string_is_equal(label, "menu_driver"))
   {
      if (menu_ctx_drivers[i])
      {
         const char *ident = menu_ctx_drivers[i]->ident;

         strlcpy(s, ident, len);
         return menu_ctx_drivers[i];
      }
   }
#endif
   else if (string_is_equal(label, "input_driver"))
   {
      if (input_drivers[i])
      {
         const char *ident = input_drivers[i]->ident;

         strlcpy(s, ident, len);
         return input_drivers[i];
      }
   }
   else if (string_is_equal(label, "input_joypad_driver"))
   {
      if (joypad_drivers[i])
      {
         const char *ident = joypad_drivers[i]->ident;

         strlcpy(s, ident, len);
         return joypad_drivers[i];
      }
   }
   else if (string_is_equal(label, "video_driver"))
   {
      if (video_drivers[i])
      {
         const char *ident = video_drivers[i]->ident;

         strlcpy(s, ident, len);
         return video_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_driver"))
   {
      if (audio_drivers[i])
      {
         const char *ident = audio_drivers[i]->ident;

         strlcpy(s, ident, len);
         return audio_drivers[i];
      }
   }
#ifdef HAVE_MICROPHONE
   else if (string_is_equal(label, "microphone_driver"))
   {
      if (microphone_drivers[i])
      {
         const char *ident = microphone_drivers[i]->ident;

         strlcpy(s, ident, len);
         return microphone_drivers[i];
      }
   }
#endif
   else if (string_is_equal(label, "record_driver"))
   {
      if (record_drivers[i])
      {
         const char *ident = record_drivers[i]->ident;

         strlcpy(s, ident, len);
         return record_drivers[i];
      }
   }
   else if (string_is_equal(label, "midi_driver"))
   {
      if (midi_driver_find_handle(i))
      {
         const char *ident = midi_drivers[i]->ident;

         strlcpy(s, ident, len);
         return midi_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_resampler_driver"))
   {
      if (audio_resampler_driver_find_handle(i))
      {
         const char *ident = audio_resampler_driver_find_ident(i);

         strlcpy(s, ident, len);
         return audio_resampler_driver_find_handle(i);
      }
   }
#ifdef HAVE_BLUETOOTH
   else if (string_is_equal(label, "bluetooth_driver"))
   {
      if (bluetooth_drivers[i])
      {
         const char *ident = bluetooth_drivers[i]->ident;

         strlcpy(s, ident, len);
         return bluetooth_drivers[i];
      }
   }
#endif
#ifdef HAVE_WIFI
   else if (string_is_equal(label, "wifi_driver"))
   {
      if (wifi_drivers[i])
      {
         const char *ident = wifi_drivers[i]->ident;

         strlcpy(s, ident, len);
         return wifi_drivers[i];
      }
   }
#endif
#ifdef HAVE_CLOUDSYNC
   else if (string_is_equal(label, "cloud_sync_driver"))
   {
      if (cloud_sync_drivers[i])
      {
         const char *ident = cloud_sync_drivers[i]->ident;

         strlcpy(s, ident, len);
         return cloud_sync_drivers[i];
      }
   }
#endif
   return NULL;
}

int driver_find_index(const char *label, const char *drv)
{
   unsigned i;
   char str[NAME_MAX_LENGTH];

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

/**
 * driver_find_last:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find last driver in driver array.
 **/
static void driver_find_last(const char *label, char *s, size_t len)
{
   unsigned i;

   for (i = 0;
         find_driver_nonempty(label, i, s, len) != NULL; i++) { }

   if (i)
      i = i - 1;
   else
      i = 0;

   find_driver_nonempty(label, i, s, len);
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
static bool driver_find_next(const char *label, char *s, size_t len)
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

static float audio_driver_monitor_adjust_system_rates(
      double input_sample_rate,
      double input_fps,
      float video_refresh_rate,
      unsigned video_swap_interval,
      unsigned black_frame_insertion,
      unsigned shader_subframes,
      float audio_max_timing_skew)
{
   float inp_sample_rate        = input_sample_rate;

   /*  This much like the auto swap interval algorithm and will
    *  find the correct desired target rate the majority of sane
    *  cases. Any failures should be no worse than the previous
    *  very incomplete high hz skew adjustments. */
   float refresh_ratio                   = video_refresh_rate/input_fps;
   unsigned refresh_closest_multiple     = (unsigned)(refresh_ratio + 0.5f);
   float target_video_sync_rate          = video_refresh_rate;
   float timing_skew                     = 0.0f;

   if (refresh_closest_multiple > 1)
      target_video_sync_rate /= (((float)black_frame_insertion + 1.0f) * (float)video_swap_interval * (float)shader_subframes);

   timing_skew            =
      fabs(1.0f - input_fps / target_video_sync_rate);
   if (timing_skew <= audio_max_timing_skew)
      return (inp_sample_rate * target_video_sync_rate / input_fps);
   return inp_sample_rate;
}

static bool video_driver_monitor_adjust_system_rates(
      float timing_skew_hz,
      float video_refresh_rate,
      bool vrr_runloop_enable,
      float audio_max_timing_skew,
      unsigned video_swap_interval,
      unsigned black_frame_insertion,
      unsigned shader_subframes,
      double input_fps)
{
   float target_video_sync_rate = timing_skew_hz;

   /* Same concept as for audio driver adjust. */
   float refresh_ratio                   = target_video_sync_rate/input_fps;
   unsigned refresh_closest_multiple     = (unsigned)(refresh_ratio + 0.5f);
   float timing_skew                     = 0.0f;

   if (refresh_closest_multiple > 1)
      target_video_sync_rate /= (((float)black_frame_insertion + 1.0f) * (float)video_swap_interval * (float)shader_subframes);

   if (!vrr_runloop_enable)
   {
      timing_skew         =
         fabs(1.0f - input_fps / target_video_sync_rate);
      /* We don't want to adjust pitch too much. If we have extreme cases,
       * just don't readjust at all. */
      if (timing_skew <= audio_max_timing_skew)
         return true;
      RARCH_LOG("[Video]: Timings deviate too much. Will not adjust."
            " (Target = %.2f Hz, Game = %.2f Hz)\n",
            target_video_sync_rate,
            (float)input_fps);
   }
   return input_fps <= target_video_sync_rate;
}

static void driver_adjust_system_rates(
      runloop_state_t *runloop_st,
      video_driver_state_t *video_st,
      settings_t *settings)
{
   struct retro_system_av_info *av_info   = &video_st->av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   double input_sample_rate               = info->sample_rate;
   double input_fps                       = info->fps;
   float video_refresh_rate               = settings->floats.video_refresh_rate;
   float audio_max_timing_skew            = settings->floats.audio_max_timing_skew;
   unsigned video_swap_interval           = settings->uints.video_swap_interval;
   unsigned black_frame_insertion         = settings->uints.video_black_frame_insertion;
   unsigned shader_subframes              = settings->uints.video_shader_subframes;
   bool vrr_runloop_enable                = settings->bools.vrr_runloop_enable;
   bool video_adaptive_vsync              = settings->bools.video_adaptive_vsync;

   /* Update video swap interval if automatic
    * switching is enabled */
   runloop_set_video_swap_interval(
         vrr_runloop_enable,
         (video_st->flags & VIDEO_FLAG_CRT_SWITCHING_ACTIVE) ? true : false,
         video_swap_interval,
         black_frame_insertion,
         shader_subframes,
         audio_max_timing_skew,
         video_refresh_rate,
         input_fps);
   video_swap_interval = runloop_get_video_swap_interval(
         video_swap_interval);

   if (input_sample_rate > 0.0)
   {
      audio_driver_state_t *audio_st      = audio_state_get_ptr();
      if (vrr_runloop_enable)
         audio_st->input = input_sample_rate;
      else
         audio_st->input =
            audio_driver_monitor_adjust_system_rates(
                  input_sample_rate,
                  input_fps,
                  video_refresh_rate,
                  video_swap_interval,
                  black_frame_insertion,
                  shader_subframes,
                  audio_max_timing_skew);

      RARCH_LOG("[Audio]: Set audio input rate to: %.2f Hz.\n",
            audio_st->input);
   }

   runloop_st->flags &= ~RUNLOOP_FLAG_FORCE_NONBLOCK;

   if (input_fps > 0.0)
   {
      float timing_skew_hz          = video_refresh_rate;

      if (video_st->flags & VIDEO_FLAG_CRT_SWITCHING_ACTIVE)
         timing_skew_hz             = input_fps;
      video_st->core_hz             = input_fps;

      if (!video_driver_monitor_adjust_system_rates(
               timing_skew_hz,
               video_refresh_rate,
               vrr_runloop_enable,
               audio_max_timing_skew,
               video_swap_interval,
               black_frame_insertion,
               shader_subframes,
               input_fps))
      {
         /* We won't be able to do VSync reliably
            when game FPS > monitor FPS. */
         runloop_st->flags |= RUNLOOP_FLAG_FORCE_NONBLOCK;
         RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");

         if (VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
         {
            if (video_st->current_video->set_nonblock_state)
               video_st->current_video->set_nonblock_state(
                     video_st->data, true,
                     video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC)
                     && video_adaptive_vsync,
                     video_swap_interval);
         }
         return;
      }
   }

   if (VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
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
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   input_driver_state_t
      *input_st                = input_state_get_ptr();
   audio_driver_state_t
      *audio_st                = audio_state_get_ptr();
   video_driver_state_t
      *video_st                = video_state_get_ptr();
   bool                 enable = input_st ?
      (input_st->flags & INP_FLAG_NONBLOCKING) : false;
   settings_t       *settings  = config_get_ptr();
   bool audio_sync             = settings->bools.audio_sync;
   bool video_vsync            = settings->bools.video_vsync;
   bool adaptive_vsync         = settings->bools.video_adaptive_vsync;
   unsigned swap_interval      = runloop_get_video_swap_interval(
         settings->uints.video_swap_interval);
   bool video_driver_active    = (video_st->flags  & VIDEO_FLAG_ACTIVE) ? true : false;
   bool audio_driver_active    = (audio_st->flags  & AUDIO_FLAG_ACTIVE) ? true : false;
   bool runloop_force_nonblock = (runloop_st->flags & RUNLOOP_FLAG_FORCE_NONBLOCK) ? true : false;

   /* Only apply non-block-state for video if we're using vsync. */
   if (video_driver_active && VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
   {
      if (video_st->current_video->set_nonblock_state)
      {
         bool video_nonblock        = enable;
         if (!video_vsync || runloop_force_nonblock)
            video_nonblock = true;
         video_st->current_video->set_nonblock_state(video_st->data,
               video_nonblock,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC)
               && adaptive_vsync, swap_interval);
      }
   }

   if (audio_driver_active && audio_st->context_audio_data)
      audio_st->current_audio->set_nonblock_state(
            audio_st->context_audio_data,
            audio_sync ? enable : true);

   audio_st->chunk_size = enable
      ? audio_st->chunk_nonblock_size
      : audio_st->chunk_block_size;
}

void drivers_init(
      settings_t *settings,
      int flags,
      enum driver_lifetime_flags lifetime_flags,
      bool verbosity_enabled)
{
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   audio_driver_state_t *audio_st    = audio_state_get_ptr();
   input_driver_state_t *input_st    = input_state_get_ptr();
   video_driver_state_t *video_st    = video_state_get_ptr();
#ifdef HAVE_MICROPHONE
   microphone_driver_state_t *mic_st = microphone_state_get_ptr();
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st     = menu_state_get_ptr();
#endif
   camera_driver_state_t
      *camera_st                  = camera_state_get_ptr();
   location_driver_state_t
      *location_st                = location_state_get_ptr();
   bool video_is_threaded         = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   gfx_display_t *p_disp          = disp_get_ptr();
#if defined(HAVE_GFX_WIDGETS)
   bool video_font_enable         = settings->bools.video_font_enable;
   bool menu_enable_widgets       = settings->bools.menu_enable_widgets;
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   /* By default, we want display widgets to persist through driver reinits. */
   p_dispwidget->flags           |= DISPGFX_WIDGET_FLAG_PERSISTING;
#endif
#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   if (menu_st)
      menu_st->flags             |= MENU_ST_FLAG_DATA_OWN;
#endif

   /* Initialize video driver */
   if (flags & DRIVER_VIDEO_MASK)
   {
      struct retro_hw_render_callback *hwr   =
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);

      video_st->frame_time_count = 0;

      video_driver_lock_new();
#ifdef HAVE_VIDEO_FILTER
      video_driver_filter_free();
#endif
      video_st->frame_cache_data  = NULL;
      if (!video_driver_init_internal(&video_is_threaded,
               verbosity_enabled))
         retroarch_fail(1, "video_driver_init_internal()");

      if (   !(video_st->flags & VIDEO_FLAG_CACHE_CONTEXT_ACK)
            && hwr->context_reset)
         hwr->context_reset();
      video_st->flags            &= ~VIDEO_FLAG_CACHE_CONTEXT_ACK;
      runloop_st->frame_time_last = 0;
   }

   /* Regular display refresh rate startup autoswitch based on content av_info */
   if (     flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK)
         && !(runloop_st->flags & RUNLOOP_FLAG_IS_INITED))
   {
      struct retro_system_av_info *av_info = &video_st->av_info;
      float refresh_rate                   = av_info->timing.fps;
      unsigned autoswitch_refresh_rate     = settings->uints.video_autoswitch_refresh_rate;
      bool exclusive_fullscreen            = settings->bools.video_fullscreen && !settings->bools.video_windowed_fullscreen;
      bool windowed_fullscreen             = settings->bools.video_fullscreen &&  settings->bools.video_windowed_fullscreen;
      bool all_fullscreen                  = settings->bools.video_fullscreen ||  settings->bools.video_windowed_fullscreen;

      /* Making a switch from PC standard 60 Hz to NTSC 59.94 is excluded by the last condition. */
      if (     (refresh_rate > 0.0f)
            && !settings->uints.crt_switch_resolution
            && !settings->bools.vrr_runloop_enable
            && video_display_server_has_resolution_list()
            && (autoswitch_refresh_rate != AUTOSWITCH_REFRESH_RATE_OFF)
            && (fabs(settings->floats.video_refresh_rate - refresh_rate) > 1))
      {
         if (   ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN) && exclusive_fullscreen)
             || ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN)  && windowed_fullscreen)
             || ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN)       && all_fullscreen))
         {
            bool video_switch_refresh_rate = false;

            video_switch_refresh_rate_maybe(&refresh_rate, &video_switch_refresh_rate);
            if (video_switch_refresh_rate && video_display_server_set_refresh_rate(refresh_rate))
               video_monitor_set_refresh_rate(refresh_rate);
         }
      }
   }

   /* Adjust rates */
   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates(runloop_st, video_st, settings);

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init_internal(
            settings,
            audio_st->callback.callback != NULL);
      if (     audio_st->current_audio
            && audio_st->current_audio->device_list_new
            && audio_st->context_audio_data)
         audio_st->devices_list = (struct string_list*)
            audio_st->current_audio->device_list_new(
                  audio_st->context_audio_data);
   }

#ifdef HAVE_MICROPHONE
   if (flags & DRIVER_MICROPHONE_MASK)
   {
      microphone_driver_init_internal(settings);
      if (mic_st->driver && mic_st->driver->device_list_new && mic_st->driver_context)
         mic_st->devices_list = mic_st->driver->device_list_new(mic_st->driver_context);
   }
#endif

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (camera_st->active)
      {
         /* Resource leaks will follow if camera is initialized twice. */
         if (!camera_st->data)
         {
            if (!camera_driver_find_driver("camera driver",
                     verbosity_enabled))
               retroarch_fail(1, "find_camera_driver()");

            if (camera_st->driver)
            {
               camera_st->data = camera_st->driver->init(
                     *settings->arrays.camera_device ?
                     settings->arrays.camera_device : NULL,
                     camera_st->cb.caps,
                     settings->uints.camera_width ?
                     settings->uints.camera_width  : camera_st->cb.width,
                     settings->uints.camera_height ?
                     settings->uints.camera_height : camera_st->cb.height);

               if (!camera_st->data)
               {
                  RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
                  camera_st->active = false;
               }

               if (camera_st->cb.initialized)
                  camera_st->cb.initialized();
            }
         }
      }
   }

#ifdef HAVE_BLUETOOTH
   if (flags & DRIVER_BLUETOOTH_MASK)
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_INIT, NULL);
#endif
#ifdef HAVE_WIFI
   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_INIT, NULL);
#endif

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (location_st->active)
         if (!init_location(&runloop_state_get_ptr()->system,
                  &location_driver_st,
                  settings, verbosity_is_enabled()))
            location_st->active = false;
   }

   core_info_init_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* Note that we only enable widgets if 'video_font_enable'
    * is true. 'video_font_enable' corresponds to the generic
    * 'On-Screen Notifications' setting, which should serve as
    * a global notifications on/off toggle switch */
   if (   video_font_enable
       && menu_enable_widgets
       && video_st->current_video
       && video_st->current_video->gfx_widgets_enabled
       && video_st->current_video->gfx_widgets_enabled(video_st->data))
   {
      bool rarch_force_fullscreen = (video_st->flags &
         VIDEO_FLAG_FORCE_FULLSCREEN) ? true : false;
      bool video_is_fullscreen    = settings->bools.video_fullscreen
                                 || rarch_force_fullscreen;

      p_dispwidget->active= gfx_widgets_init(
            p_disp,
            anim_get_ptr(),
            settings,
            (uintptr_t)&p_dispwidget->active,
            video_is_threaded,
            video_st->width,
            video_st->height,
            video_is_fullscreen,
            settings->paths.directory_assets,
            settings->paths.path_font);
   }
   else
#endif
   {
      gfx_display_init_first_driver(p_disp, video_is_threaded);
   }

#ifdef HAVE_MENU
   if (flags & DRIVER_VIDEO_MASK)
   {
      /* Initialize menu driver */
      if (flags & DRIVER_MENU_MASK)
      {
         if (!menu_driver_init(video_is_threaded))
             RARCH_ERR("Unable to init menu driver.\n");

#ifdef HAVE_LIBRETRODB
         menu_explore_context_init();
#endif
         menu_contentless_cores_context_init();
      }
   }

   /* Initialising the menu driver will also initialise
    * core info - if we are not initialising the menu
    * driver, must initialise core info 'by hand' */
   if (   !(flags & DRIVER_VIDEO_MASK)
       || !(flags & DRIVER_MENU_MASK))
   {
      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
      command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
   }

#else
   /* Qt uses core info, even if the menu is disabled */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#endif

   /* Keep non-throttled state as good as possible. */
   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      if (input_st && (input_st->flags & INP_FLAG_NONBLOCKING))
         driver_set_nonblock_state();

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init(settings->arrays.led_driver);

   /* Initialize MIDI driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init(settings);

#ifdef HAVE_LAKKA
   cpu_scaling_driver_init();
#endif
}

void driver_uninit(int flags, enum driver_lifetime_flags lifetime_flags)
{
   runloop_state_t *runloop_st      = runloop_state_get_ptr();
   video_driver_state_t *video_st   = video_state_get_ptr();
   camera_driver_state_t *camera_st = camera_state_get_ptr();
#if defined(HAVE_GFX_WIDGETS)
   dispgfx_widget_t *p_dispwidget   = dispwidget_get_ptr();
#endif

   core_info_deinit_list();
   core_info_free_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* This absolutely has to be done before video_driver_free_internal()
    * is called/completes, otherwise certain menu drivers
    * (e.g. Vulkan) will segfault */
   if (p_dispwidget->flags & DISPGFX_WIDGET_FLAG_INITED)
   {
      gfx_widgets_deinit(p_dispwidget->flags & DISPGFX_WIDGET_FLAG_PERSISTING);
      p_dispwidget->active = false;
   }
#endif

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
#ifdef HAVE_LIBRETRODB
      menu_explore_context_deinit();
#endif
      menu_contentless_cores_context_deinit();

#ifdef HAVE_CHEEVOS
      rcheevos_menu_reset_badges();
#endif

      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      uninit_location(&runloop_st->system, &location_driver_st);

   if ((flags & DRIVER_CAMERA_MASK))
   {
      if (camera_st->data && camera_st->driver)
      {
         if (camera_st->cb.deinitialized)
            camera_st->cb.deinitialized();

         if (camera_st->driver->free)
            camera_st->driver->free(camera_st->data);
      }

      camera_st->data = NULL;
   }

#ifdef HAVE_BLUETOOTH
   if ((flags & DRIVER_BLUETOOTH_MASK))
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DEINIT, NULL);
#endif
#ifdef HAVE_WIFI
   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);
#endif

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
   {
      video_driver_free_internal();
      VIDEO_DRIVER_LOCK_FREE(video_st);
      video_st->data              = NULL;
      video_st->frame_cache_data  = NULL;
   }

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit();

   if ((flags & DRIVER_VIDEO_MASK))
      video_st->data = NULL;

   if ((flags & DRIVER_INPUT_MASK))
      input_state_get_ptr()->current_data = NULL;

   if ((flags & DRIVER_AUDIO_MASK))
      audio_state_get_ptr()->context_audio_data = NULL;

#ifdef HAVE_MICROPHONE
   if (flags & DRIVER_MICROPHONE_MASK)
      microphone_driver_deinit(lifetime_flags & DRIVER_LIFETIME_RESET);
#endif

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();

#ifdef HAVE_LAKKA
   cpu_scaling_driver_free();
#endif
}

static void retroarch_deinit_drivers(struct retro_callbacks *cbs)
{
   input_driver_state_t *input_st  = input_state_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   camera_driver_state_t *camera_st= camera_state_get_ptr();
   location_driver_state_t
      *location_st                 = location_state_get_ptr();
   runloop_state_t     *runloop_st = runloop_state_get_ptr();
#if defined(HAVE_GFX_WIDGETS)
   /* Tear down display widgets no matter what
    * in case the handle is lost in the threaded
    * video driver in the meantime
    * (breaking video_driver_has_widgets) */
   dispgfx_widget_t *p_dispwidget  = dispwidget_get_ptr();
   if (p_dispwidget->flags & DISPGFX_WIDGET_FLAG_INITED)
   {
      gfx_widgets_deinit(
            p_dispwidget->flags & DISPGFX_WIDGET_FLAG_PERSISTING);
      p_dispwidget->active         = false;
   }
#endif

#if defined(HAVE_CRTSWITCHRES)
   /* Switchres deinit */
   if (video_st->flags & VIDEO_FLAG_CRT_SWITCHING_ACTIVE)
      crt_destroy_modes(&video_st->crt_switch_st);
#endif

   /* Video */
   video_display_server_destroy();

   video_st->flags &= ~(VIDEO_FLAG_ACTIVE      | VIDEO_FLAG_USE_RGBA      |
                        VIDEO_FLAG_HDR_SUPPORT | VIDEO_FLAG_CACHE_CONTEXT |
                        VIDEO_FLAG_CACHE_CONTEXT_ACK
                       );
   video_st->record_gpu_buffer          = NULL;
   video_st->current_video              = NULL;
   video_st->frame_cache_data           = NULL;

   /* Audio */
   audio_state_get_ptr()->flags        &= ~AUDIO_FLAG_ACTIVE;
   audio_state_get_ptr()->current_audio = NULL;

   if (input_st)
   {
      /* Input */
      input_st->flags &= ~(INP_FLAG_KB_LINEFEED_ENABLE
                         | INP_FLAG_BLOCK_HOTKEY
                         | INP_FLAG_BLOCK_LIBRETRO_INPUT
                         | INP_FLAG_NONBLOCKING);

      memset(&input_st->turbo_btns, 0, sizeof(turbo_buttons_t));
      memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));
      input_st->current_driver           = NULL;
   }

#ifdef HAVE_MENU
   menu_driver_destroy(
         menu_state_get_ptr());
#endif
   location_st->active                              = false;
   location_st->driver                              = NULL;

   /* Camera */
   camera_st->active                                = false;
   camera_st->driver                                = NULL;
   camera_st->data                                  = NULL;

#ifdef HAVE_BLUETOOTH
   bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DESTROY, NULL);
#endif
#ifdef HAVE_WIFI
   wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);
#endif

   cbs->frame_cb                    = retro_frame_null;
   cbs->poll_cb                     = retro_input_poll_null;
   cbs->sample_cb                   = NULL;
   cbs->sample_batch_cb             = NULL;
   cbs->state_cb                    = NULL;

   runloop_st->current_core.flags  &= ~RETRO_CORE_FLAG_INITED;
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   driver_ctx_info_t      *drv = (driver_ctx_info_t*)data;

   switch (state)
   {
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz                     = (float*)data;
            audio_driver_state_t
               *audio_st                  = audio_state_get_ptr();
            settings_t *settings          = config_get_ptr();
            runloop_state_t *runloop_st   = runloop_state_get_ptr();
            video_driver_state_t*video_st = video_state_get_ptr();
            unsigned
               audio_output_sample_rate   = settings->uints.audio_output_sample_rate;

            video_monitor_set_refresh_rate(*hz);

            /* Sets audio monitor rate to new value. */
            audio_st->source_ratio_original   =
            audio_st->source_ratio_current    =
            (double)audio_output_sample_rate / audio_st->input;

            driver_adjust_system_rates(runloop_st, video_st, settings);
         }
         break;
      case RARCH_DRIVER_CTL_FIND_FIRST:
         if (!drv)
            return false;
         find_driver_nonempty(drv->label, 0, drv->s, drv->len);
         break;
      case RARCH_DRIVER_CTL_FIND_LAST:
         if (!drv)
            return false;
         driver_find_last(drv->label, drv->s, drv->len);
         break;
      case RARCH_DRIVER_CTL_FIND_PREV:
         if (!drv)
            return false;
         return driver_find_prev(drv->label, drv->s, drv->len);
      case RARCH_DRIVER_CTL_FIND_NEXT:
         if (!drv)
            return false;
         return driver_find_next(drv->label, drv->s, drv->len);
      case RARCH_DRIVER_CTL_NONE:
      default:
         break;
   }

   return true;
}

access_state_t *access_state_get_ptr(void)
{
   return &access_state_st;
}

/* GLOBAL POINTER GETTERS */
global_t *global_get_ptr(void)
{
   return &global_driver_st;
}

uint16_t retroarch_get_flags(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->flags;
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
   char ext_shaders[NAME_MAX_LENGTH];
#endif
   char ext_name[16];
   const char *exts    = NULL;
   bool recursive      = false;

   switch (type)
   {
      case DIR_LIST_AUTOCONFIG:
         exts = filter;
         break;
      case DIR_LIST_CORES:
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
            size_t _len         = 0;
            ext_shaders[0]      = '\0';

            if (video_shader_is_supported(RARCH_SHADER_CG))
            {
               _len    += strlcpy(ext_shaders + _len, "cgp", sizeof(ext_shaders) - _len);
               if (ext_shaders[_len-1] != '\0')
                  _len += strlcpy(ext_shaders + _len, "|",   sizeof(ext_shaders) - _len);
               _len    += strlcpy(ext_shaders + _len, "cg",  sizeof(ext_shaders) - _len);
            }

            if (video_shader_is_supported(RARCH_SHADER_GLSL))
            {
               if (ext_shaders[_len-1] != '\0')
                  _len += strlcpy(ext_shaders + _len, "|",     sizeof(ext_shaders) - _len);
               _len    += strlcpy(ext_shaders + _len, "glslp", sizeof(ext_shaders) - _len);
               if (ext_shaders[_len-1] != '\0')
                  _len += strlcpy(ext_shaders + _len, "|",     sizeof(ext_shaders) - _len);
               _len    += strlcpy(ext_shaders + _len, "glsl",  sizeof(ext_shaders) - _len);
            }

            if (video_shader_is_supported(RARCH_SHADER_SLANG))
            {
               if (ext_shaders[_len-1] != '\0')
                  _len += strlcpy(ext_shaders + _len, "|",      sizeof(ext_shaders) - _len);
               _len    += strlcpy(ext_shaders + _len, "slangp", sizeof(ext_shaders) - _len);
               if (ext_shaders[_len-1] != '\0')
                  _len += strlcpy(ext_shaders + _len, "|",      sizeof(ext_shaders) - _len);
               _len    += strlcpy(ext_shaders + _len, "slang",  sizeof(ext_shaders) - _len);
            }

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
#ifdef HAVE_MICROPHONE
      case STRING_LIST_MICROPHONE_DRIVERS:
         for (i = 0; microphone_drivers[i]; i++)
         {
            const char *opt  = microphone_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
         break;
#endif
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
      case STRING_LIST_CLOUD_SYNC_DRIVERS:
#ifdef HAVE_CLOUDSYNC
         for (i = 0; cloud_sync_drivers[i]; i++)
         {
            const char *opt  = cloud_sync_drivers[i]->ident;
            *len            += strlen(opt) + 1;

            string_list_append(s, opt, attr);
         }
#endif
         break;
#ifdef HAVE_LAKKA
      case STRING_LIST_TIMEZONES:
         {
            const char *opt  = DEFAULT_TIMEZONE;
            *len            += STRLEN_CONST(DEFAULT_TIMEZONE) + 1;
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
      case RARCH_PATH_CONFIG_OVERRIDE:
         if (!path_is_empty(RARCH_PATH_CONFIG_OVERRIDE))
            return p_rarch->path_config_override_file;
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
      case RARCH_PATH_CONFIG_OVERRIDE:
         if (!path_is_empty(RARCH_PATH_CONFIG_OVERRIDE))
            return p_rarch->path_config_override_file;
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

   switch (type)
   {
      case RARCH_PATH_CONTENT:
         return sizeof(p_rarch->path_content);
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         return sizeof(p_rarch->path_default_shader_preset);
      case RARCH_PATH_BASENAME:
         return sizeof(runloop_state_get_ptr()->runtime_content_path_basename);
      case RARCH_PATH_CORE_OPTIONS:
         return sizeof(p_rarch->path_core_options_file);
      case RARCH_PATH_SUBSYSTEM:
         return sizeof(runloop_state_get_ptr()->subsystem_path);
      case RARCH_PATH_CONFIG:
         return sizeof(p_rarch->path_config_file);
      case RARCH_PATH_CONFIG_APPEND:
         return sizeof(p_rarch->path_config_append_file);
      case RARCH_PATH_CONFIG_OVERRIDE:
         return sizeof(p_rarch->path_config_override_file);
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
   runloop_state_t *runloop_st = NULL;

   if (!path)
      return false;

   switch (type)
   {
      case RARCH_PATH_NAMES:
         runloop_path_set_basename(path);
         runloop_path_set_names();
         runloop_path_set_redirect(config_get_ptr(), p_rarch->dir_savefile,
               p_rarch->dir_savestate);
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
      case RARCH_PATH_CONFIG_OVERRIDE:
         strlcpy(p_rarch->path_config_override_file, path,
               sizeof(p_rarch->path_config_override_file));
         break;
      case RARCH_PATH_CORE_OPTIONS:
         strlcpy(p_rarch->path_core_options_file, path,
               sizeof(p_rarch->path_core_options_file));
         break;
      case RARCH_PATH_CONTENT:
         strlcpy(p_rarch->path_content, path,
               sizeof(p_rarch->path_content));
         break;
      case RARCH_PATH_BASENAME:
         runloop_st = runloop_state_get_ptr();
         strlcpy(runloop_st->runtime_content_path_basename, path,
               sizeof(runloop_st->runtime_content_path_basename));
         break;
      case RARCH_PATH_SUBSYSTEM:
         runloop_st = runloop_state_get_ptr();
         strlcpy(runloop_st->subsystem_path, path,
               sizeof(runloop_st->subsystem_path));
         break;
      case RARCH_PATH_NONE:
         break;
   }

   return true;
}

bool path_is_empty(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_PATH_DEFAULT_SHADER_PRESET:
         if (string_is_empty(p_rarch->path_default_shader_preset))
            return true;
         break;
      case RARCH_PATH_CONFIG:
         if (string_is_empty(p_rarch->path_config_file))
            return true;
         break;
      case RARCH_PATH_CONFIG_APPEND:
         if (string_is_empty(p_rarch->path_config_append_file))
            return true;
         break;
      case RARCH_PATH_CONFIG_OVERRIDE:
         if (string_is_empty(p_rarch->path_config_override_file))
            return true;
         break;
      case RARCH_PATH_CORE_OPTIONS:
         if (string_is_empty(p_rarch->path_core_options_file))
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
         if (string_is_empty(runloop_state_get_ptr()->runtime_content_path_basename))
            return true;
         break;
      case RARCH_PATH_SUBSYSTEM:
         if (string_is_empty(runloop_state_get_ptr()->subsystem_path))
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
   runloop_state_t *runloop_st = NULL;

   switch (type)
   {
      case RARCH_PATH_CORE:
         *p_rarch->path_libretro = '\0';
         break;
      case RARCH_PATH_CONFIG:
         *p_rarch->path_config_file = '\0';
         break;
      case RARCH_PATH_CONTENT:
         *p_rarch->path_content = '\0';
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
      case RARCH_PATH_CONFIG_OVERRIDE:
         *p_rarch->path_config_override_file = '\0';
         break;
      case RARCH_PATH_NONE:
      case RARCH_PATH_NAMES:
         break;
      case RARCH_PATH_BASENAME:
         runloop_st = runloop_state_get_ptr();
         *runloop_st->runtime_content_path_basename = '\0';
         break;
      case RARCH_PATH_SUBSYSTEM:
         runloop_st = runloop_state_get_ptr();
         *runloop_st->subsystem_path = '\0';
         break;
   }
}

static void path_clear_all(void)
{
   path_clear(RARCH_PATH_CONTENT);
   path_clear(RARCH_PATH_CONFIG);
   path_clear(RARCH_PATH_CONFIG_APPEND);
   path_clear(RARCH_PATH_CONFIG_OVERRIDE);
   path_clear(RARCH_PATH_CORE_OPTIONS);
   path_clear(RARCH_PATH_BASENAME);
}

static void ram_state_to_file(void)
{
   char state_path[PATH_MAX_LENGTH];

   if (!content_ram_state_pending())
      return;

   state_path[0] = '\0';

   if (runloop_get_current_savestate_path(state_path, sizeof(state_path)))
      command_event(CMD_EVENT_RAM_STATE_TO_FILE, state_path);
}

enum rarch_content_type path_is_media_type(const char *path)
{
   char ext_lower[16];
   strlcpy(ext_lower, path_get_extension(path), sizeof(ext_lower));

   string_to_lower(ext_lower);

   /* hack, to detect livestreams so the ffmpeg core can be started */
   if (   string_starts_with_size(path, "udp://",   STRLEN_CONST("udp://"))
       || string_starts_with_size(path, "http://",  STRLEN_CONST("http://"))
       || string_starts_with_size(path, "https://", STRLEN_CONST("https://"))
       || string_starts_with_size(path, "tcp://",   STRLEN_CONST("tcp://"))
       || string_starts_with_size(path, "rtmp://",  STRLEN_CONST("rtmp://"))
       || string_starts_with_size(path, "rtp://",   STRLEN_CONST("rtp://")))
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
      case FILE_TYPE_M4A:
#endif
#if defined(HAVE_FFMPEG) || defined(HAVE_MPV) || defined(HAVE_AUDIOMIXER)
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_STB_VORBIS)
      case FILE_TYPE_OGG:
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_MP3)
      case FILE_TYPE_MP3:
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_DR_FLAC)
      case FILE_TYPE_FLAC:
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_RWAV)
      case FILE_TYPE_WAV:
#endif
#if !defined(HAVE_AUDIOMIXER) || defined(HAVE_IBXM)
      case FILE_TYPE_MOD:
      case FILE_TYPE_S3M:
      case FILE_TYPE_XM:
#endif
         return RARCH_CONTENT_MUSIC;
#endif
#ifdef HAVE_IMAGEVIEWER
      case FILE_TYPE_JPEG:
      case FILE_TYPE_PNG:
      case FILE_TYPE_TGA:
      case FILE_TYPE_BMP:
         return RARCH_CONTENT_IMAGE;
#endif
      case FILE_TYPE_NONE:
      default:
         break;
   }

   return RARCH_CONTENT_NONE;
}

/* get size functions */

size_t dir_get_size(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;

   switch (type)
   {
      case RARCH_DIR_SYSTEM:
         return sizeof(p_rarch->dir_system);
      case RARCH_DIR_SAVESTATE:
         return sizeof(p_rarch->dir_savestate);
      case RARCH_DIR_CURRENT_SAVESTATE:
         return sizeof(runloop_state_get_ptr()->savestate_dir);
      case RARCH_DIR_SAVEFILE:
         return sizeof(p_rarch->dir_savefile);
      case RARCH_DIR_CURRENT_SAVEFILE:
         return sizeof(runloop_state_get_ptr()->savefile_dir);
      case RARCH_DIR_NONE:
         break;
   }

   return 0;
}

/* clear functions */

void dir_clear(enum rarch_dir_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = NULL;

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         *p_rarch->dir_savefile = '\0';
         break;
      case RARCH_DIR_SAVESTATE:
         *p_rarch->dir_savestate = '\0';
         break;
      case RARCH_DIR_SYSTEM:
         *p_rarch->dir_system = '\0';
         break;
      case RARCH_DIR_NONE:
         break;
      case RARCH_DIR_CURRENT_SAVEFILE:
         runloop_st = runloop_state_get_ptr();
         *runloop_st->savefile_dir = '\0';
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         runloop_st = runloop_state_get_ptr();
         *runloop_st->savestate_dir = '\0';
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

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         return p_rarch->dir_savefile;
      case RARCH_DIR_CURRENT_SAVEFILE:
         return runloop_state_get_ptr()->savefile_dir;
      case RARCH_DIR_SAVESTATE:
         return p_rarch->dir_savestate;
      case RARCH_DIR_CURRENT_SAVESTATE:
         return runloop_state_get_ptr()->savestate_dir;
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
   runloop_state_t *runloop_st = NULL;

   switch (type)
   {
      case RARCH_DIR_SAVEFILE:
         strlcpy(p_rarch->dir_savefile, path,
               sizeof(p_rarch->dir_savefile));
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
      case RARCH_DIR_CURRENT_SAVEFILE:
         runloop_st = runloop_state_get_ptr();
         strlcpy(runloop_st->savefile_dir, path,
               sizeof(runloop_st->savefile_dir));
         break;
      case RARCH_DIR_CURRENT_SAVESTATE:
         runloop_st = runloop_state_get_ptr();
         strlcpy(runloop_st->savestate_dir, path,
               sizeof(runloop_st->savestate_dir));
         break;
   }
}

void dir_check_defaults(const char *custom_ini_path)
{
   size_t i;

   /* Early return for people with a custom folder setup
    * so it doesn't create unnecessary directories */
   if (  !string_is_empty(custom_ini_path)
       && path_is_valid(custom_ini_path))
      return;

   for (i = 0; i < DEFAULT_DIR_LAST; i++)
   {
      const char *dir_path = g_defaults.dirs[i];
      char new_path[PATH_MAX_LENGTH];

      if (string_is_empty(dir_path))
         continue;

      fill_pathname_expand_special(new_path,
            dir_path, sizeof(new_path));

      if (!path_is_directory(new_path))
         path_mkdir(new_path);
   }
}

#ifdef __APPLE__
static void dir_check_config(void)
{
   settings_t *settings            = config_get_ptr();

#define ENSURE_DIRECTORY(DIRPATH)               \
   if (!string_is_empty(DIRPATH))               \
      if (!path_is_directory(DIRPATH))          \
         path_mkdir(DIRPATH)

   /* the order here mimics the order of enum default_dirs */
   /* TODO: not all of these are necessary/used and many are created on demand, make fewer */
   ENSURE_DIRECTORY(settings->paths.directory_menu_content);
   ENSURE_DIRECTORY(settings->paths.directory_core_assets);
   ENSURE_DIRECTORY(settings->paths.directory_menu_config);
   ENSURE_DIRECTORY(settings->paths.directory_autoconfig);
   ENSURE_DIRECTORY(settings->paths.directory_audio_filter);
   ENSURE_DIRECTORY(settings->paths.directory_video_filter);
   ENSURE_DIRECTORY(settings->paths.directory_assets);
#ifdef _3DS
   ENSURE_DIRECTORY(settings->paths.directory_bottom_assets);
#endif
   ENSURE_DIRECTORY(settings->paths.directory_libretro);
   ENSURE_DIRECTORY(settings->paths.path_libretro_info);
   ENSURE_DIRECTORY(settings->paths.directory_overlay);
   ENSURE_DIRECTORY(settings->paths.directory_osk_overlay);
   /* PORT */
   ENSURE_DIRECTORY(settings->paths.directory_video_shader);
   ENSURE_DIRECTORY(dir_get_ptr(RARCH_DIR_SAVESTATE));
   ENSURE_DIRECTORY(dir_get_ptr(RARCH_DIR_SAVEFILE));
   ENSURE_DIRECTORY(settings->paths.directory_screenshot);
   ENSURE_DIRECTORY(settings->paths.directory_system);
   ENSURE_DIRECTORY(settings->paths.directory_playlist);
   ENSURE_DIRECTORY(settings->paths.directory_content_favorites);
   ENSURE_DIRECTORY(settings->paths.directory_content_history);
   ENSURE_DIRECTORY(settings->paths.directory_content_image_history);
   ENSURE_DIRECTORY(settings->paths.directory_content_music_history);
   ENSURE_DIRECTORY(settings->paths.directory_content_video_history);
   ENSURE_DIRECTORY(settings->paths.directory_input_remapping);
   ENSURE_DIRECTORY(settings->paths.directory_cache);
   ENSURE_DIRECTORY(settings->paths.directory_dynamic_wallpapers);
   ENSURE_DIRECTORY(settings->paths.directory_thumbnails);
   ENSURE_DIRECTORY(settings->paths.path_content_database);
   ENSURE_DIRECTORY(settings->paths.path_cheat_database);
   /* RECORD_CONFIG */
   /* RECORD_OUTPUT */
   ENSURE_DIRECTORY(settings->paths.log_dir);

#undef ENSURE_DIRECTORY
}
#endif

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
   struct rarch_state *p_rarch     = &rarch_st;
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
         event_save_files(runloop_st->flags & RUNLOOP_FLAG_USE_SRAM);
         break;
      case CMD_EVENT_OVERLAY_UNLOAD:
#ifdef HAVE_OVERLAY
         input_overlay_unload();
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
         video_shader_dir_check_shader(menu_st->driver_data, settings,
               &video_st->dir_shader_list, true, false);
#else
         video_shader_dir_check_shader(NULL, settings,
               &video_st->dir_shader_list, true, false);
#endif
#endif
         break;
      case CMD_EVENT_SHADER_PREV:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#ifdef HAVE_MENU
         video_shader_dir_check_shader(menu_st->driver_data, settings,
               &video_st->dir_shader_list, false, true);
#else
         video_shader_dir_check_shader(NULL, settings,
               &video_st->dir_shader_list, false, true);
#endif
#endif
         break;
      case CMD_EVENT_SHADER_TOGGLE:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         video_shader_toggle(settings);
#endif
         break;
      case CMD_EVENT_AI_SERVICE_TOGGLE:
         {
#ifdef HAVE_TRANSLATE
            bool ai_service_pause     = settings->bools.ai_service_pause;

            if (!settings->bools.ai_service_enable)
               break;

            if (ai_service_pause)
            {
               /* Unpause on second press */
               if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
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
               else /* Pause on call */
               {
                  command_event(CMD_EVENT_PAUSE, NULL);
                  command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
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
               {
                  command_event(CMD_EVENT_AI_SERVICE_CALL, NULL);
               }
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
#if HAVE_RUNAHEAD
         {
            if (!core_info_current_supports_runahead())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD),
                     1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               break;
            }

            settings->bools.run_ahead_enabled =
               !(settings->bools.run_ahead_enabled);

            if (settings->bools.run_ahead_enabled)
            {
               char msg[128];
               if (settings->bools.run_ahead_secondary_instance)
                  snprintf(msg, sizeof(msg),
                        msg_hash_to_str(MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE),
                        settings->uints.run_ahead_frames);
               else
                  snprintf(msg, sizeof(msg),
                        msg_hash_to_str(MSG_RUNAHEAD_ENABLED),
                        settings->uints.run_ahead_frames);
               runloop_msg_queue_push(msg, 1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                     MESSAGE_QUEUE_CATEGORY_INFO);

               /* Disable preemptive frames */
               settings->bools.preemptive_frames_enable = false;
               preempt_deinit(runloop_st);
            }
            else
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_DISABLED),
                     1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

#ifdef HAVE_MENU
            /* Update menu */
            menu_update_runahead_mode();
#endif
         }
#endif
         break;
      case CMD_EVENT_PREEMPT_TOGGLE:
#if HAVE_RUNAHEAD
         {
            bool old_warn   = settings->bools.run_ahead_hide_warnings;
            bool old_inited = runloop_st->preempt_data != NULL;

            /* Toggle with warnings shown */
            settings->bools.run_ahead_hide_warnings  = false;
            settings->bools.preemptive_frames_enable =
                  !(settings->bools.preemptive_frames_enable);
            command_event(CMD_EVENT_PREEMPT_UPDATE, NULL);

            settings->bools.run_ahead_hide_warnings = old_warn;

            if (old_inited && !runloop_st->preempt_data)
               runloop_msg_queue_push(msg_hash_to_str(MSG_PREEMPT_DISABLED),
                     1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            else if (runloop_st->preempt_data)
            {
               char msg[128];
               snprintf(msg, sizeof(msg), msg_hash_to_str(MSG_PREEMPT_ENABLED),
                     settings->uints.run_ahead_frames);
               runloop_msg_queue_push(
                     msg, 1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

               /* Disable runahead */
               settings->bools.run_ahead_enabled        = false;
            }
            else /* Failed to init */
               settings->bools.preemptive_frames_enable = false;

#ifdef HAVE_MENU
            /* Update menu */
            menu_update_runahead_mode();
#endif
         }
#endif
         break;
      case CMD_EVENT_PREEMPT_UPDATE:
#if HAVE_RUNAHEAD
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            break;
#endif
         preempt_deinit(runloop_st);
         preempt_init(runloop_st);
#endif
         break;
      case CMD_EVENT_PREEMPT_RESET_BUFFER:
#if HAVE_RUNAHEAD
         if (runloop_st->preempt_data)
            runloop_st->preempt_data->frame_count = 0;
#endif
         break;
      case CMD_EVENT_RECORDING_TOGGLE:
         if (recording_st->enable)
            command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         else
            command_event(CMD_EVENT_RECORD_INIT, NULL);
         break;
      case CMD_EVENT_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;
            char desc[64] = {0};

            command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

            if (video_driver_get_video_output_size(&width, &height, desc, sizeof(desc)))
            {
               char msg[128];

               video_driver_set_video_mode(width, height, true);

               if (width == 0 || height == 0)
                  strlcpy(msg, msg_hash_to_str(MSG_SCREEN_RESOLUTION_DEFAULT), sizeof(msg));
               else
               {
                  msg[0] = '\0';
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
            rarch_system_info_t *sys_info     = &runloop_st->system;
            struct retro_system_info *sysinfo = &sys_info->info;
            const char *core_path             = path_get(RARCH_PATH_CORE);

#if defined(HAVE_DYNAMIC)
            if (string_is_empty(core_path))
               return false;
#endif

            if (!libretro_get_system_info(
                     core_path,
                     sysinfo,
                     &sys_info->load_no_content))
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
         if (   !(runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
             || !(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE))
            return false;

         if (!runloop_st->secondary_lib_handle)
         {
            if (!secondary_core_ensure_exists(runloop_st, settings))
            {
               runahead_secondary_core_destroy(runloop_st);
               runloop_st->flags &=
                  ~RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE;
               return false;
            }
         }
         break;
#endif
      case CMD_EVENT_LOAD_STATE:
         {
#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active())
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
               return false;
            }
#endif
#ifdef HAVE_NETWORKING
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_TIMESKIP, NULL))
               return false;
#endif
            if (!command_event_main_state(cmd))
               return false;
            /* Run next frame to see the core output while paused */
            else if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
            {
               runloop_st->flags               &= ~RUNLOOP_FLAG_PAUSED;
               runloop_st->run_frames_and_pause = 1;
            }

#if HAVE_RUNAHEAD
            command_event(CMD_EVENT_PREEMPT_RESET_BUFFER, NULL);
#endif
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
         if (menu_st->flags & MENU_ST_FLAG_ALIVE)
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
#ifdef HAVE_NETWORKING
         netplay_driver_ctl(RARCH_NETPLAY_CTL_RESET, NULL);
#endif
         /* Recalibrate frame delay target */
         if (settings->bools.video_frame_delay_auto)
            video_st->frame_delay_target = 0;

         /* Run a few frames to blank core output while paused */
         if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
         {
            runloop_st->flags               &= ~RUNLOOP_FLAG_PAUSED;
            runloop_st->run_frames_and_pause = 8;
         }

#if HAVE_RUNAHEAD
         command_event(CMD_EVENT_PREEMPT_RESET_BUFFER, NULL);
#endif
         return false;
      case CMD_EVENT_PLAY_REPLAY:
      {
#ifdef HAVE_BSV_MOVIE
         input_driver_state_t *input_st = input_state_get_ptr();
         char replay_path[PATH_MAX_LENGTH];
         bool res = true;
         /* TODO: Consider extending the current replay if we start recording during a playback */
         if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
            res = false;
         else if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
            res = movie_stop(input_st);
         if (!runloop_get_current_replay_path(replay_path, sizeof(replay_path)))
            res = false;
         if (res)
            res = movie_start_playback(input_st, replay_path);
         if (!res)
         {
            const char *movie_fail_str        =
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE);
            runloop_msg_queue_push(movie_fail_str,
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_ERR("%s.\n", movie_fail_str);
         }
         return res;
#else
         return false;
#endif
      }
      case CMD_EVENT_RECORD_REPLAY:
      {
#ifdef HAVE_BSV_MOVIE
         char replay_path[PATH_MAX_LENGTH];
         bool res                       = true;
         input_driver_state_t *input_st = input_state_get_ptr();
         int replay_slot                = settings->ints.replay_slot;
         if (settings->bools.replay_auto_index)
            replay_slot += 1;
         /* TODO: Consider cloning and extending the current replay if we start recording during a recording */
         if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
            res = false;
         else if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
            res = movie_stop(input_st);
         if (!runloop_get_replay_path(replay_path, sizeof(replay_path), replay_slot))
            res = false;
         if (res)
            res = movie_start_record(input_st, replay_path);
         if (res && settings->bools.replay_auto_index)
            configuration_set_int(settings, settings->ints.replay_slot, replay_slot);
         if (!res)
         {
             const char *movie_rec_fail_str        =
               msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD);
            runloop_msg_queue_push(movie_rec_fail_str,
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            RARCH_ERR("%s.\n", movie_rec_fail_str);
         }
         return res;
#else
         break;
#endif
      }
      case CMD_EVENT_HALT_REPLAY:
#ifdef HAVE_BSV_MOVIE
         movie_stop(input_state_get_ptr());
#endif
         break;
      case CMD_EVENT_SAVE_STATE:
      case CMD_EVENT_SAVE_STATE_TO_RAM:
         {
            if (settings->bools.savestate_auto_index)
            {
               configuration_set_int(settings, settings->ints.state_slot,
                  command_event_get_next_savestate_auto_index(settings));
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
      case CMD_EVENT_REPLAY_DECREMENT:
#ifdef HAVE_BSV_MOVIE
         {
            int slot            = settings->ints.replay_slot;

            /* Slot -1 is (auto) slot. */
            if (slot >= 0)
            {
               int new_slot = slot - 1;
               configuration_set_int(settings, settings->ints.replay_slot, new_slot);
            }
         }
#endif
         break;
      case CMD_EVENT_REPLAY_INCREMENT:
#ifdef HAVE_BSV_MOVIE
         {
            int new_slot        = settings->ints.replay_slot + 1;
            configuration_set_int(settings, settings->ints.replay_slot, new_slot);
         }
#endif
         break;
      case CMD_EVENT_TAKE_SCREENSHOT:
#ifdef HAVE_SCREENSHOTS
         {
            const char *dir_screenshot      = settings->paths.directory_screenshot;
            video_driver_state_t *video_st  = video_state_get_ptr();
            if (!take_screenshot(dir_screenshot,
                     path_get(RARCH_PATH_BASENAME),
                     false,
                     video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID),
                     false,
                     true))
               return false;
         }
#endif
         break;
      case CMD_EVENT_UNLOAD_CORE:
         {
            bool load_dummy_core            = data ? *(bool*)data : true;
            content_ctx_info_t content_info = {0};
            video_driver_state_t *video_st  = video_state_get_ptr();
            rarch_system_info_t *sys_info   = &runloop_st->system;
            uint8_t flags                   = content_get_flags();

            runloop_st->flags              &= ~RUNLOOP_FLAG_CORE_RUNNING;

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
            if (settings->bools.savestate_auto_save &&
                runloop_st->current_core_type != CORE_TYPE_DUMMY)
               command_event_save_auto_state();

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

#ifdef HAVE_CONFIGFILE
            if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
            {
               /* Reload the original config */
               config_unload_override();

               if (!settings->bools.video_fullscreen)
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  if (     video_st->poke
                        && video_st->poke->show_mouse)
                     video_st->poke->show_mouse(video_st->data, true);
                  if (input_driver_ungrab_mouse())
                     input_st->flags &= ~INP_FLAG_GRAB_MOUSE_STATE;
               }
            }
#endif
#ifdef HAVE_CLOUDSYNC
            task_push_cloud_sync();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
            runloop_st->runtime_shader_preset_path[0] = '\0';
#endif

            video_driver_restore_cached(settings);

            if (    (flags & CONTENT_ST_FLAG_IS_INITED)
                  && load_dummy_core)
            {
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
            if (flags & CONTENT_ST_FLAG_IS_INITED)
            {
               runloop_st->subsystem_current_count = 0;
               content_clear_subsystem();
            }
         }
         break;
      case CMD_EVENT_CLOSE_CONTENT:
#ifdef HAVE_MENU
         /* If we need to quit, skip unloading the core to avoid performing
          * cleanup actions (like writing autosave state) twice. */
         if (should_quit_on_close()) {
            command_event(CMD_EVENT_QUIT, NULL);
            break;
         }
         /* Closing content via hotkey requires toggling menu
          * and resetting the position later on to prevent
          * going to empty Quick Menu */
         if (!(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
         {
            menu_state_get_ptr()->flags |= MENU_ST_FLAG_PENDING_CLOSE_CONTENT;
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
         video_st->flags &= ~VIDEO_FLAG_FORCE_FULLSCREEN;
         /* this fallthrough is on purpose, it should do
            a CMD_EVENT_REINIT too */
      case CMD_EVENT_REINIT:
         command_event_reinit(
               data ? *(const int*)data : DRIVERS_CMD_ALL);

#if defined(HAVE_AUDIOMIXER) && defined(HAVE_MENU)
         /* Menu sounds require audio reinit. */
         if (settings->bools.audio_enable_menu)
            command_event(CMD_EVENT_AUDIO_REINIT, NULL);
#endif

         /* Recalibrate frame delay target if not pausing */
         if (settings->bools.video_frame_delay_auto && !video_st->frame_delay_pause)
            video_st->frame_delay_target = 0;

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
#ifdef HAVE_NETWORKING
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_TIMESKIP, NULL))
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
      case CMD_EVENT_REWIND_REINIT:
#ifdef HAVE_REWIND
         /* to reinitialize the the rewind state manager, we have to recreate it.
          * the easiest way to do that is a full deinit followed by an init. */
         if (runloop_st->rewind_st.state != NULL)
         {
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
            command_event(CMD_EVENT_REWIND_INIT, NULL);
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
         if (runloop_st->flags & RUNLOOP_FLAG_USE_SRAM)
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
            {
               if (autosave_init())
                  runloop_st->flags |=  RUNLOOP_FLAG_AUTOSAVE;
               else
                  runloop_st->flags &= ~RUNLOOP_FLAG_AUTOSAVE;
            }
         }
#endif
         break;
      case CMD_EVENT_AUDIO_STOP:
         {
            bool menu_pause_libretro = false;
            bool audio_enable_menu   = false;

#if defined(HAVE_AUDIOMIXER) && defined(HAVE_MENU)
            audio_enable_menu        = settings->bools.audio_enable_menu
                  && menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE;
#endif
#ifdef HAVE_NETWORKING
            menu_pause_libretro      = settings->bools.menu_pause_libretro
                  && netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            menu_pause_libretro      = settings->bools.menu_pause_libretro;
#endif

            if (audio_enable_menu || !menu_pause_libretro)
               return false;

            if (!audio_driver_stop())
               return false;
         }
         break;
      case CMD_EVENT_AUDIO_START:
         {
            bool menu_pause_libretro = false;
            bool audio_enable_menu   = false;

#if defined(HAVE_AUDIOMIXER) && defined(HAVE_MENU)
            audio_enable_menu        = settings->bools.audio_enable_menu
                  && menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE;
#endif
#ifdef HAVE_NETWORKING
            menu_pause_libretro      = settings->bools.menu_pause_libretro
                  && netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            menu_pause_libretro      = settings->bools.menu_pause_libretro;
#endif

            if (audio_enable_menu && !menu_pause_libretro)
               return false;

            if (!audio_driver_start(runloop_st->flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
               return false;
         }
         break;
#ifdef HAVE_MICROPHONE
      case CMD_EVENT_MICROPHONE_STOP:
         if (!microphone_driver_stop())
            return false;
         break;
      case CMD_EVENT_MICROPHONE_START:
         if (!microphone_driver_start())
            return false;
         break;
#endif
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
            input_overlay_t *ol            = input_st->overlay_ptr;
            float input_overlay_opacity;
            if (!ol)
               return false;

            ol->index                      = ol->next_index;
            ol->active                     = &ol->overlays[ol->index];

            input_overlay_opacity          = (ol->flags & INPUT_OVERLAY_IS_OSK)
                  ? settings->floats.input_osk_overlay_opacity
                  : settings->floats.input_overlay_opacity;

            input_overlay_load_active(input_st->overlay_visibility,
                  ol, input_overlay_opacity);

            ol->flags                     |= INPUT_OVERLAY_BLOCKED;
            ol->next_index                 =
                  (unsigned)((ol->index + 1) % ol->size);

            /* Check orientation, if required */
            if (inp_overlay_auto_rotate)
               if (check_rotation)
                  if (*check_rotation)
                     input_overlay_auto_rotate_(
                           video_st->width,
                           video_st->height,
                           settings->bools.input_overlay_enable,
                           ol);
         }
#endif
         break;
      case CMD_EVENT_OSK_TOGGLE:
#ifdef HAVE_OVERLAY
         {
            settings_t *settings           = config_get_ptr();
            input_driver_state_t *input_st = input_state_get_ptr();

            if (input_st->flags & INP_FLAG_KB_LINEFEED_ENABLE)
               input_st->flags &= ~INP_FLAG_KB_LINEFEED_ENABLE;
            else if (!string_is_empty(settings->paths.path_osk_overlay))
               input_st->flags |=  INP_FLAG_KB_LINEFEED_ENABLE;
            else
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_OSK_OVERLAY_NOT_SET), 1, 100, false,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

            command_event(CMD_EVENT_OVERLAY_INIT, NULL);

#ifdef HAVE_MENU
            /* Avoid stray menu input during transition */
            if (menu_st->flags & MENU_ST_FLAG_ALIVE)
            {
               menu_st->input_state.select_inhibit  = true;
               menu_st->input_state.cancel_inhibit  = true;
            }
#endif
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
            const char *_msg                       = NULL;
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

            if (!history_list_enable || !playlist_config.capacity)
               return false;

            _msg = msg_hash_to_str(MSG_LOADING_HISTORY_FILE);

            /* Note: Sorting is disabled by default for
             * all content history playlists */
            RARCH_LOG("[Playlist]: %s: \"%s\".\n", _msg,
                  path_content_history);
            playlist_config_set_path(&playlist_config, path_content_history);
            g_defaults.content_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.content_history, PLAYLIST_SORT_MODE_OFF);

            RARCH_LOG("[Playlist]: %s: \"%s\".\n", _msg,
                  path_content_music_history);
            playlist_config_set_path(&playlist_config, path_content_music_history);
            g_defaults.music_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.music_history, PLAYLIST_SORT_MODE_OFF);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            RARCH_LOG("[Playlist]: %s: \"%s\".\n", _msg,
                  path_content_video_history);
            playlist_config_set_path(&playlist_config, path_content_video_history);
            g_defaults.video_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.video_history, PLAYLIST_SORT_MODE_OFF);
#endif

#ifdef HAVE_IMAGEVIEWER
            RARCH_LOG("[Playlist]: %s: \"%s\".\n", _msg,
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
            char ext_name[16];
            const char *dir_libretro       = settings->paths.directory_libretro;
            const char *path_libretro_info = settings->paths.path_libretro_info;
            bool show_hidden_files         = settings->bools.show_hidden_files;
            bool core_info_cache_enable    = settings->bools.core_info_cache_enable;

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

            if (     runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING
                  && settings->bools.savestate_auto_save)
            {
               command_event_save_auto_state();
               content_wait_for_save_state_task();
            }

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
            if (!(runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_AVAILABLE))
               runahead_clear_variables(runloop_st);

            /* Deallocate preemptive frames */
            preempt_deinit(runloop_st);
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

            if (     !type
                  || !runloop_event_init_core(settings, input_st, *type,
                     p_rarch->dir_savefile, p_rarch->dir_savestate))
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
                        GFX_CTX_FLAGS_ADAPTIVE_VSYNC)
                     && adaptive_vsync, swap_interval);
         }
         break;
      case CMD_EVENT_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio();
         break;
      case CMD_EVENT_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         {
            overlay_layout_desc_t layout_desc;
            video_driver_state_t *video_st = video_state_get_ptr();
            input_driver_state_t *input_st = input_state_get_ptr();
            input_overlay_t *ol            = input_st->overlay_ptr;

            if (!ol)
               break;

            if (ol->flags & INPUT_OVERLAY_IS_OSK)
            {
               memset(&layout_desc, 0, sizeof(overlay_layout_desc_t));
               layout_desc.scale_landscape         = 1.0f;
               layout_desc.scale_portrait          = 1.0f;
               layout_desc.touch_scale             = 1.0f;
               layout_desc.auto_scale              = settings->bools.input_osk_overlay_auto_scale;
            }
            else
            {
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
            }

            input_overlay_set_scale_factor(ol,
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
            input_driver_state_t *input_st = input_state_get_ptr();
            input_overlay_t *ol            = input_st->overlay_ptr;

            if (ol)
            {
               float input_overlay_opacity = (ol->flags & INPUT_OVERLAY_IS_OSK)
                     ? settings->floats.input_osk_overlay_opacity
                     : settings->floats.input_overlay_opacity;

               input_overlay_set_alpha_mod(input_st->overlay_visibility,
                        ol, input_overlay_opacity);
            }
         }
#endif
         break;
      case CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY:
#ifdef HAVE_OVERLAY
         input_overlay_set_eightway_diagonal_sensitivity();
#endif
         break;
      case CMD_EVENT_AUDIO_REINIT:
         driver_uninit(DRIVER_AUDIO_MASK, DRIVER_LIFETIME_RESET);
         drivers_init(settings, DRIVER_AUDIO_MASK, DRIVER_LIFETIME_RESET, verbosity_is_enabled());
#if defined(HAVE_AUDIOMIXER)
         audio_driver_load_system_sounds();
#endif
         break;
#ifdef HAVE_MICROPHONE
      case CMD_EVENT_MICROPHONE_REINIT:
         driver_uninit(DRIVER_MICROPHONE_MASK, DRIVER_LIFETIME_RESET);
         drivers_init(settings, DRIVER_MICROPHONE_MASK, DRIVER_LIFETIME_RESET, verbosity_is_enabled());
         break;
#endif
      case CMD_EVENT_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         if (settings->bools.config_save_on_exit)
         {
            command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         }
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_SHUTTING_DOWN), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#ifdef HAVE_LAKKA
         system("nohup /usr/bin/lakka-shutdown.sh 2>&1 >/dev/null & exit");
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
            command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         }
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_REBOOTING), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#ifdef HAVE_LAKKA
         system("nohup /usr/bin/lakka-reboot.sh 2>&1 >/dev/null & exit");
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
         if (uico_st->flags & UICO_ST_FLAG_IS_ON_FOREGROUND)
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

            /* Check whether favourites playlist is at capacity */
            if (playlist_size(g_defaults.content_favorites) >=
                  playlist_capacity(g_defaults.content_favorites))
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MSG_ADD_TO_FAVORITES_FAILED), 1, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
               return true;
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
                     if (     (playlist_sort_alphabetical
                           && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT))
                           || (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
                        playlist_qsort(g_defaults.content_favorites);

                     playlist_write_file(g_defaults.content_favorites);
                     runloop_msg_queue_push(
                           msg_hash_to_str(MSG_ADDED_TO_FAVORITES), 1, 180, true, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
#if TARGET_OS_TV
                     update_topshelf();
#endif
                  }
               }
            }
            break;
         }
         case CMD_EVENT_ADD_TO_PLAYLIST:
         {
#ifdef HAVE_MENU
            struct string_list *str_list = (struct string_list*)data;
            struct menu_state *menu_st     = menu_state_get_ptr();
            settings_t *settings = config_get_ptr();

            if (str_list)
            {
               if (str_list->size >= 7)
               {
                  playlist_config_t playlist_config;
                  playlist_t * playlist;

                  struct playlist_entry entry     = {0};
                  bool playlist_sort_alphabetical = settings->bools.playlist_sort_alphabetical;

                  entry.path      = str_list->elems[0].data; /* content_path */
                  entry.label     = str_list->elems[1].data; /* content_label */
                  entry.core_path = str_list->elems[2].data; /* core_path */
                  entry.core_name = str_list->elems[3].data; /* core_name */
                  entry.crc32     = str_list->elems[4].data; /* crc32 */
                  entry.db_name   = str_list->elems[5].data; /* db_name */

                  /* load the playlist */
                  playlist_config.capacity            = COLLECTION_SIZE;
                  playlist_config.old_format          = settings->bools.playlist_use_old_format;
                  playlist_config.compress            = settings->bools.playlist_compression;
                  playlist_config.fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
                  playlist_config_set_base_content_directory(&playlist_config,
                        settings->bools.playlist_portable_paths
                        ? settings->paths.directory_menu_content
                        : NULL);
                  playlist_config_set_path(&playlist_config, str_list->elems[6].data);
                  playlist = playlist_init(&playlist_config);

                  /* Check whether favourites playlist is at capacity */
                  if (playlist_size(playlist) >=
                        playlist_capacity(playlist))
                  {
                     runloop_msg_queue_push(
                           msg_hash_to_str(MSG_ADD_TO_PLAYLIST_FAILED), 1, 180, true, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
                     return true;
                  }

                  /* Write playlist entry */
                  if (playlist_push(playlist, &entry))
                  {
                     enum playlist_sort_mode current_sort_mode =
                        playlist_get_sort_mode(playlist);

                     /* New addition - need to resort if option is enabled */
                     if (     (playlist_sort_alphabetical
                           && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT))
                           || (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
                        playlist_qsort(playlist);

                     playlist_write_file(playlist);
                     runloop_msg_queue_push(
                           msg_hash_to_str(MSG_ADDED_TO_PLAYLIST), 1, 180, true, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
                  menu_st->flags                  |= MENU_ST_FLAG_ENTRIES_NEED_REFRESH;
                  if (menu_st->driver_ctx->environ_cb)
                     menu_st->driver_ctx->environ_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST,
                           NULL, menu_st->userdata);
               }
            }
#endif
            break;
         }
      case CMD_EVENT_RESET_CORE_ASSOCIATION:
         {
            const char *core_name          = "DETECT";
            const char *core_path          = "DETECT";
            size_t *playlist_index         = (size_t*)data;
            struct playlist_entry entry    = {0};
            unsigned i                     = 0;
#ifdef HAVE_MENU
            struct menu_state *menu_st     = menu_state_get_ptr();
#endif

            /* the update function reads our entry as const,
             * so these casts are safe */
            entry.core_path                = (char*)core_path;
            entry.core_name                = (char*)core_name;

            command_playlist_update_write(
                  NULL, *playlist_index, &entry);

#ifdef HAVE_MENU
            /* Update playlist metadata */
            if (     menu_st->driver_ctx
                  && menu_st->driver_ctx->refresh_thumbnail_image)
               menu_st->driver_ctx->refresh_thumbnail_image(
                     menu_st->userdata, i);
#endif

            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_RESET_CORE_ASSOCIATION), 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
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
      case CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE:
#ifdef HAVE_CONFIGFILE
         command_event_remove_current_config(OVERRIDE_CORE);
#endif
         break;
      case CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR:
#ifdef HAVE_CONFIGFILE
         command_event_remove_current_config(OVERRIDE_CONTENT_DIR);
#endif
         break;
      case CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME:
#ifdef HAVE_CONFIGFILE
         command_event_remove_current_config(OVERRIDE_GAME);
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
            bool paused          = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
#ifdef HAVE_ACCESSIBILITY
            bool accessibility_enable
                                 = settings->bools.accessibility_enable;
            unsigned accessibility_narrator_speech_speed
                                 = settings->uints.accessibility_narrator_speech_speed;
#endif

#ifdef HAVE_NETWORKING
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
               break;
#endif

            paused               = !paused;

#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled(
                  accessibility_enable,
                  access_st->enabled))
            {
               if (paused)
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
            if (paused)
               runloop_st->flags |=  RUNLOOP_FLAG_PAUSED;
            else
               runloop_st->flags &= ~RUNLOOP_FLAG_PAUSED;
            runloop_pause_checks();
         }
         break;
      case CMD_EVENT_UNPAUSE:
#ifdef HAVE_NETWORKING
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
            break;
#endif
         runloop_st->flags      &= ~RUNLOOP_FLAG_PAUSED;
         runloop_pause_checks();
         break;
      case CMD_EVENT_PAUSE:
#ifdef HAVE_NETWORKING
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
            break;
#endif
         runloop_st->flags      |= RUNLOOP_FLAG_PAUSED;
         runloop_pause_checks();
         break;
      case CMD_EVENT_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         {
#ifdef HAVE_NETWORKING
            bool menu_pause_libretro = settings->bools.menu_pause_libretro
                  && netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL);
#else
            bool menu_pause_libretro = settings->bools.menu_pause_libretro;
#endif

            if (menu_pause_libretro)
            {
               command_event(CMD_EVENT_AUDIO_STOP, NULL);
#ifdef HAVE_MICROPHONE
               command_event(CMD_EVENT_MICROPHONE_STOP, NULL);
#endif
            }
            else
            {
               command_event(CMD_EVENT_AUDIO_START, NULL);
#ifdef HAVE_MICROPHONE
               command_event(CMD_EVENT_MICROPHONE_START, NULL);
#endif
            }
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
            char tmp_netplay_session[256];
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

            if (p_rarch->connect_mitm_id)
                netplay_session = strdup(p_rarch->connect_mitm_id);

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
               if (p_rarch->connect_mitm_id)
               {
                  free(p_rarch->connect_mitm_id);
                  free(netplay_session);
                  p_rarch->connect_mitm_id = NULL;
                  netplay_session          = NULL;
               }
               return false;
            }

            if (p_rarch->connect_mitm_id)
            {
               free(p_rarch->connect_mitm_id);
               free(netplay_session);
               p_rarch->connect_mitm_id = NULL;
               netplay_session          = NULL;
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
            char netplay_session[256];
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
            char netplay_session[256];
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
            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_USE_CORE_PACKET_INTERFACE, NULL))
            {
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);
               command_event(CMD_EVENT_NETPLAY_INIT, NULL);
            }
            else if (!task_push_netplay_content_reload(NULL))
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
#if HAVE_RUNAHEAD
            /* Deinit preemptive frames; not compatible with netplay */
            preempt_deinit(runloop_st);
#endif
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
            if (     netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL)
                  || netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
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
            bool ra_is_forced_fs      = (video_st->flags &
               VIDEO_FLAG_FORCE_FULLSCREEN) ? true : false;
            bool new_fullscreen_state = !video_fullscreen && !ra_is_forced_fs;

            if (!video_driver_has_windowed())
               return false;

            audio_st->flags |= AUDIO_FLAG_SUSPENDED;
            video_st->flags |= VIDEO_FLAG_IS_SWITCHING_DISPLAY_MODE;

            /* we toggled manually, write the new value to settings */
            configuration_set_bool(settings, settings->bools.video_fullscreen,
                  new_fullscreen_state);
            /* Need to grab this setting's value again */
            video_fullscreen = new_fullscreen_state;

            /* we toggled manually, the CLI arg is irrelevant now */
            if (ra_is_forced_fs)
               video_st->flags &= ~VIDEO_FLAG_FORCE_FULLSCREEN;

            /* If we go fullscreen we drop all drivers and
             * reinitialize to be safe. */
            command_event(CMD_EVENT_REINIT, NULL);
            if (video_fullscreen)
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, false);
               if (!settings->bools.video_windowed_fullscreen)
                  if (input_driver_grab_mouse())
                     input_st->flags |= INP_FLAG_GRAB_MOUSE_STATE;
            }
            else
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, true);
               if (!settings->bools.video_windowed_fullscreen)
                  if (input_driver_ungrab_mouse())
                     input_st->flags &= ~INP_FLAG_GRAB_MOUSE_STATE;
            }
#ifdef HAVE_OVERLAY
            input_overlay_check_mouse_cursor();
#endif

            video_st->flags &= ~VIDEO_FLAG_IS_SWITCHING_DISPLAY_MODE;
            audio_st->flags &= ~AUDIO_FLAG_SUSPENDED;

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
               struct menu_state *menu_st = menu_state_get_ptr();
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
                  menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                                  |  MENU_ST_FLAG_PREVENT_POPULATE;
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
               bool *show_msg                  = (bool*)data;
               bool eject                      = !disk_control_get_eject_state(
                                                  &sys_info->disk_control);
               bool verbose                    = true;
#if defined(HAVE_MENU)
               struct menu_state *menu_st      = menu_state_get_ptr();
#endif

               if (show_msg)
                  verbose                      = *show_msg;

               if (!settings->bools.notification_show_disk_control)
                  verbose                      = false;

               disk_control_set_eject_state(
                     &sys_info->disk_control, eject, verbose);

#if defined(HAVE_MENU)
               /* It is necessary to refresh the disk options
                * menu when toggling the tray state */
               menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                               |  MENU_ST_FLAG_PREVENT_POPULATE;
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

               if (!settings->bools.notification_show_disk_control)
                  verbose     = false;

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

               if (!settings->bools.notification_show_disk_control)
                  verbose     = false;

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
            bool grab_mouse_state = !(input_st->flags &
                  INP_FLAG_GRAB_MOUSE_STATE);

            if (grab_mouse_state)
            {
               if ((ret = input_driver_grab_mouse()))
                  input_st->flags |= INP_FLAG_GRAB_MOUSE_STATE;
            }
            else
            {
               if ((ret = input_driver_ungrab_mouse()))
                  input_st->flags &= ~INP_FLAG_GRAB_MOUSE_STATE;
            }

            if (!ret)
               return false;

            RARCH_LOG("[Input]: %s => %s\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "ON" : "OFF");

            if (grab_mouse_state)
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, false);
            }
            else
            {
               if (     video_st->poke
                     && video_st->poke->show_mouse)
                  video_st->poke->show_mouse(video_st->data, true);
            }
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
                  settings->bools.video_fullscreen
               || (video_st->flags & VIDEO_FLAG_FORCE_FULLSCREEN);
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
                  if (menu_st->flags & MENU_ST_FLAG_ALIVE)
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
                     input_st->flags |= INP_FLAG_GRAB_MOUSE_STATE;
                  if (     video_st->poke
                        && video_st->poke->show_mouse)
                     video_st->poke->show_mouse(video_st->data, false);
               }
               /* Ungrab only if windowed and auto mouse grab is disabled */
               else if (!video_fullscreen
                     && !settings->bools.input_auto_mouse_grab)
               {
                  if (input_driver_ungrab_mouse())
                     input_st->flags &= ~INP_FLAG_GRAB_MOUSE_STATE;
                  if (     video_st->poke
                        && video_st->poke->show_mouse)
                     video_st->poke->show_mouse(video_st->data, true);
               }

               if (input_st->game_focus_state.enabled)
                  input_st->flags |=  INP_FLAG_BLOCK_HOTKEY
                                   |  INP_FLAG_KB_MAPPING_BLOCKED;
               else
                  input_st->flags &= ~(INP_FLAG_BLOCK_HOTKEY
                                     | INP_FLAG_KB_MAPPING_BLOCKED);

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
            if (!discord_st->ready)
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
            if (    (ai_service_mode == 1)
                  && audio_driver_is_ai_service_speech_running())
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
                        (char*)msg_hash_to_str(MSG_AI_SERVICE_STOPPED),
                        10);
#endif
            }
            else
#endif
#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled(
                     accessibility_enable,
                     access_st->enabled)
                  && (ai_service_mode == 2)
                  && is_narrator_running(accessibility_enable))
               accessibility_speak_priority(
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     (char*)msg_hash_to_str(MSG_AI_SERVICE_STOPPED),
                     10);
            else
#endif
            {
               bool paused = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
               if (data)
                  paused = *((bool*)data);
               if (     (access_st->ai_service_auto == 0)
                     && !settings->bools.ai_service_pause)
                  access_st->ai_service_auto = 1;
               run_translation_service(settings, paused);
            }
#endif
            break;
         }
      case CMD_EVENT_CONTROLLER_INIT:
         {
            rarch_system_info_t *sys_info = &runloop_st->system;
            if (sys_info)
               command_event_init_controllers(sys_info, settings,
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

      /* Deprecated */
      case CMD_EVENT_SEND_DEBUG_INFO:
         break;
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
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_VERBOSITY;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_LIBRETRO;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_LIBRETRO_DIRECTORY;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_SAVE_PATH;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_STATE_PATH;
         break;
#ifdef HAVE_NETWORKING
     case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         net_st->flags |= NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_MODE;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         net_st->flags |= NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_ADDRESS;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         net_st->flags |= NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_PORT;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         net_st->flags |= NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_CHECK_FRAMES;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_UPS_PREF;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_BPS_PREF;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_IPS_PREF;
#endif
         break;
       case RARCH_OVERRIDE_SETTING_XDELTA_PREF:
#if defined(HAVE_PATCH) && defined(HAVE_XDELTA)
           p_rarch->flags |= RARCH_FLAGS_HAS_SET_XDELTA_PREF;
#endif
           break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->flags |= RARCH_FLAGS_HAS_SET_LOG_TO_FILE;
         break;
      case RARCH_OVERRIDE_SETTING_DATABASE_SCAN:
         p_rarch->flags |= RARCH_FLAGS_CLI_DATABASE_SCAN;
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
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_VERBOSITY;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_LIBRETRO;
         break;
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_LIBRETRO_DIRECTORY;
         break;
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_SAVE_PATH;
         break;
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_STATE_PATH;
         break;
#ifdef HAVE_NETWORKING
    case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         net_st->flags &= ~NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_MODE;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         net_st->flags &= ~NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_ADDRESS;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         net_st->flags &= ~NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_PORT;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         net_st->flags &= ~NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_CHECK_FRAMES;
         break;
#endif
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_UPS_PREF;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_BPS_PREF;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
#ifdef HAVE_PATCH
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_IPS_PREF;
#endif
         break;
       case RARCH_OVERRIDE_SETTING_XDELTA_PREF:
#if defined(HAVE_PATCH) && defined(HAVE_XDELTA)
           p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_XDELTA_PREF;
#endif
         break;
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         p_rarch->flags &= ~RARCH_FLAGS_HAS_SET_LOG_TO_FILE;
         break;
      case RARCH_OVERRIDE_SETTING_DATABASE_SCAN:
         p_rarch->flags &= ~RARCH_FLAGS_CLI_DATABASE_SCAN;
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

   runloop_path_deinit_subsystem();
   command_event(CMD_EVENT_RECORD_DEINIT, NULL);

   retro_main_log_file_deinit();

   runloop_st->flags &= ~(
                          RUNLOOP_FLAG_IS_SRAM_LOAD_DISABLED
                        | RUNLOOP_FLAG_IS_SRAM_SAVE_DISABLED
                        | RUNLOOP_FLAG_USE_SRAM);
#ifdef HAVE_PATCH
   p_rarch->flags    &= ~(
                         RARCH_FLAGS_BPS_PREF
                       | RARCH_FLAGS_IPS_PREF
                       | RARCH_FLAGS_UPS_PREF
                       | RARCH_FLAGS_XDELTA_PREF);
   runloop_st->flags &= ~RUNLOOP_FLAG_PATCH_BLOCKED;

#endif
#ifdef HAVE_CONFIGFILE
   p_rarch->flags    &= ~RARCH_FLAGS_BLOCK_CONFIG_READ;
   runloop_st->flags &= ~(RUNLOOP_FLAG_OVERRIDES_ACTIVE
                        | RUNLOOP_FLAG_REMAPS_CORE_ACTIVE
                        | RUNLOOP_FLAG_REMAPS_GAME_ACTIVE
                        | RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE);
#endif

   runloop_st->current_core.flags &= ~(RETRO_CORE_FLAG_HAS_SET_INPUT_DESCRIPTORS
                                     | RETRO_CORE_FLAG_HAS_SET_SUBSYSTEMS);

   global                                = global_get_ptr();
   path_clear_all();
   dir_clear_all();

   if (!string_is_empty(runloop_st->name.remapfile))
      free(runloop_st->name.remapfile);
   runloop_st->name.remapfile = NULL;
   *runloop_st->name.ups                 = '\0';
   *runloop_st->name.bps                 = '\0';
   *runloop_st->name.ips                 = '\0';
   *runloop_st->name.xdelta              = '\0';
   *runloop_st->name.savefile            = '\0';
   *runloop_st->name.savestate           = '\0';
   *runloop_st->name.replay              = '\0';
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
 **/
void main_exit(void *args)
{
   struct rarch_state *p_rarch  = &rarch_st;
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state  *menu_st  = menu_state_get_ptr();
#endif
   settings_t     *settings     = config_get_ptr();

   video_driver_restore_cached(settings);

#if defined(HAVE_GFX_WIDGETS)
   /* Do not want display widgets to live any more. */
   dispwidget_get_ptr()->flags &= ~DISPGFX_WIDGET_FLAG_PERSISTING;
#endif
#ifdef HAVE_MENU
   /* Do not want menu context to live any more. */
   if (menu_st)
      menu_st->flags &= ~MENU_ST_FLAG_DATA_OWN;
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
#ifdef PS2
   /* PS2 frontend driver deinit also detaches filesystem,
    * so make sure logs are written in advance. */
   retro_main_log_file_deinit();
#endif
   frontend_driver_deinit(args);
   frontend_driver_exitspawn(
         path_get_ptr(RARCH_PATH_CORE),
         path_get_realsize(RARCH_PATH_CORE),
         p_rarch->launch_arguments);

   p_rarch->flags                  &= ~RARCH_FLAGS_HAS_SET_USERNAME;
   runloop_st->flags               &= ~RUNLOOP_FLAG_IS_INITED;
   global_get_ptr()->flags         &= ~GLOB_FLG_ERR_ON_INIT;
#ifdef HAVE_CONFIGFILE
   p_rarch->flags                  &= ~RARCH_FLAGS_BLOCK_CONFIG_READ;
#endif

   runloop_msg_queue_deinit();
   driver_uninit(DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0);

   retro_main_log_file_deinit();

   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);
   task_queue_deinit();

   ui_companion_driver_deinit();
   retroarch_config_deinit();

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
   video_st->flags   |= VIDEO_FLAG_SHADER_PRESETS_NEED_RELOAD;
#endif
#ifdef HAVE_RUNAHEAD
   video_st->flags   |= VIDEO_FLAG_RUNAHEAD_IS_ACTIVE;
   runloop_st->flags |= (
                         RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE
                      |  RUNLOOP_FLAG_RUNAHEAD_AVAILABLE
                      |  RUNLOOP_FLAG_RUNAHEAD_FORCE_INPUT_DIRTY
                        );
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
   retroarch_favorites_deinit();

   retroarch_config_init();

   retroarch_deinit_drivers(&runloop_st->retro_ctx);
   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);

   frontend_driver_init_first(data);

   if (runloop_st->flags & RUNLOOP_FLAG_IS_INITED)
      driver_uninit(DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0);

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_create(&p_rarch->rarch_tls);
   sthread_tls_set(&p_rarch->rarch_tls, MAGIC_POINTER);
#endif
   video_st->flags              |= VIDEO_FLAG_ACTIVE;
   audio_state_get_ptr()->flags |= AUDIO_FLAG_ACTIVE;

   {
      int i;
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
#if HAVE_CLOUDSYNC
   task_push_cloud_sync();
#endif
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

#ifdef HAVE_RWEBAUDIO
void RWebAudioRecalibrateTime(void);
#endif

void emscripten_mainloop(void)
{
   int ret;
   static unsigned emscripten_frame_count = 0;
   video_driver_state_t *video_st         = video_state_get_ptr();
   settings_t        *settings            = config_get_ptr();
   input_driver_state_t *input_st         = input_state_get_ptr();
   bool black_frame_insertion             = settings->uints.video_black_frame_insertion;
   bool input_driver_nonblock_state       = input_st ?
      (input_st->flags & INP_FLAG_NONBLOCKING) : false;
   uint32_t runloop_flags                 = runloop_get_flags();
   bool runloop_is_slowmotion             = (runloop_flags & RUNLOOP_FLAG_SLOWMOTION) ? true : false;
   bool runloop_is_paused                 = (runloop_flags & RUNLOOP_FLAG_PAUSED)     ? true : false;

#ifdef HAVE_RWEBAUDIO
   RWebAudioRecalibrateTime();
#endif

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
 * Leaf function.
 *
 * @return controller description of found controller on success,
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
void libretro_free_system_info(struct retro_system_info *sysinfo)
{
   if (!sysinfo)
      return;

   free((void*)sysinfo->library_name);
   free((void*)sysinfo->library_version);
   free((void*)sysinfo->valid_extensions);
   memset(sysinfo, 0, sizeof(*sysinfo));
}

#define _PSUPP_BUF(buf, len, var, name, desc) snprintf(buf + len, sizeof(buf) - len, "  %-15s - %s: %s", name, desc, var ? "yes\n" : "no\n")

static void retroarch_print_features(void)
{
   size_t _len;
   char buf[4096];

   frontend_driver_attach_console();

   _len  = strlcpy(buf, "Features:\n", sizeof(buf));
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_LIBRETRODB,      "LibretroDB",      "LibretroDB support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_COMMAND,         "Command",         "Command interface support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_NETWORK_COMMAND, "Network Command", "Network Command interface support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_SDL,             "SDL",             "SDL input/audio/video drivers");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_SDL2,            "SDL2",            "SDL2 input/audio/video drivers");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_X11,             "X11",             "X11 input/video drivers");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_UDEV,            "UDEV",            "UDEV/EVDEV input driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_WAYLAND,         "Wayland",         "Wayland input/video drivers");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_THREAD,          "Threads",         "Threading support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_VULKAN,          "Vulkan",          "Video driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_METAL,           "Metal",           "Video driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_OPENGL,          "OpenGL",          "Video driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_OPENGLES,        "OpenGLES",        "Video driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_XVIDEO,          "XVideo",          "Video driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_EGL,             "EGL",             "Video context driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_KMS,             "KMS",             "Video context driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_VG,              "OpenVG",          "Video context driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_COREAUDIO,       "CoreAudio",       "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_COREAUDIO3,      "CoreAudioV3",     "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_ALSA,            "ALSA",            "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_OSS,             "OSS",             "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_JACK,            "Jack",            "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_RSOUND,          "RSound",          "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_ROAR,            "RoarAudio",       "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_PULSE,           "PulseAudio",      "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_DSOUND,          "DirectSound",     "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_WASAPI,          "WASAPI",          "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_XAUDIO,          "XAudio2",         "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_AL,              "OpenAL",          "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_SL,              "OpenSL",          "Audio driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_7ZIP,            "7zip",            "7zip extraction support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_ZLIB,            "zlib",            "zip extraction support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_DYLIB,           "External",        "External filter and plugin support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_CG,              "Cg",              "Fragment/vertex shader driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_GLSL,            "GLSL",            "Fragment/vertex shader driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_HLSL,            "HLSL",            "Fragment/vertex shader driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_SDL_IMAGE,       "SDL_image",       "SDL_image image loading");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_RPNG,            "rpng",            "PNG image loading/encoding");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_RJPEG,           "rjpeg",           "JPEG image loading");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_DYNAMIC,         "Dynamic",         "Dynamic run-time loading of libretro library");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_FFMPEG,          "FFmpeg",          "On-the-fly recording of gameplay with libavcodec");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_FREETYPE,        "FreeType",        "TTF font rendering driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_CORETEXT,        "CoreText",        "TTF font rendering driver");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_NETPLAY,         "Netplay",         "Peer-to-peer netplay");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_LIBUSB,          "Libusb",          "Libusb support");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_COCOA,           "Cocoa",           "Cocoa UI companion support (for OSX and/or iOS)");
   _len += _PSUPP_BUF(buf, _len, SUPPORTS_QT,              "Qt",              "Qt UI companion support");
   _PSUPP_BUF(buf, _len, SUPPORTS_V4L2,            "Video4Linux2",    "Camera driver");

   fputs(buf, stdout);
}

static void retroarch_print_version(void)
{
   char str[256];
   str[0] = '\0';

   frontend_driver_attach_console();

   fprintf(stdout, "%s - %s\n",
         msg_hash_to_str(MSG_PROGRAM),
         msg_hash_to_str(MSG_LIBRETRO_FRONTEND));

   fprintf(stdout, "Version: %s", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
   fprintf(stdout, " (Git %s)", retroarch_git_version);
#endif
   fprintf(stdout, " " __DATE__ "\n");

   retroarch_get_capabilities(RARCH_CAPABILITIES_COMPILER, str, sizeof(str));
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
   size_t _len = 0;
   buf[0]      = '\0';

   frontend_driver_attach_console();
   fputs("\n", stdout);
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");
   fputs("\n", stdout);

   fprintf(stdout, "Usage: %s [OPTIONS]... [FILE]\n\n", arg0);

   _len = strlcpy(buf + _len,
         "  -h, --help                     "
         "Show this help message.\n"
         "  -v, --verbose                  "
         "Verbose logging.\n"
         "      --log-file=FILE            "
         "Log messages to FILE.\n"
         "  -V, --version                  "
         "Show version.\n"
         "      --features                 "
         "Print available features compiled into program.\n"
         , sizeof(buf) - _len);
#ifdef HAVE_MENU
   _len += strlcpy(buf + _len,
         "      --menu                     "
         "Do not require content or libretro core to be loaded,\n"
         "                                 "
         "  starts directly in menu. If no arguments are passed to\n"
         "                                 "
         "  the program, it is equivalent to using --menu as only argument.\n"
         , sizeof(buf) - _len);
#endif

#ifdef HAVE_CONFIGFILE
   _len += strlcpy(buf + _len, "  -c, --config=FILE              "
         "Path for config file.\n", sizeof(buf) - _len);
#ifdef _WIN32
   _len += strlcpy(buf + _len, "                                 "
         "  Defaults to retroarch.cfg in same directory as retroarch.exe.\n"
         "                                 "
         "  If a default config is not found, the program will attempt to create one.\n"
         , sizeof(buf) - _len);
#else
   _len += strlcpy(buf + _len,
         "                                 "
         "  By default looks for config in\n"
         "                                 "
         "  $XDG_CONFIG_HOME/retroarch/retroarch.cfg,\n"
         "                                 "
         "  $HOME/.config/retroarch/retroarch.cfg, and\n"
         "                                 "
         "  $HOME/.retroarch.cfg.\n"
         "                                 "
         "  If a default config is not found, the program will attempt to create one\n"
         "                                 "
         "  based on the skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg).\n"
         , sizeof(buf) - _len);
#endif
   _len += strlcpy(buf + _len, "      --appendconfig=FILE        "
         "Extra config files are loaded in, and take priority over\n"
         "                                 "
         "  config selected in -c (or default). Multiple configs are\n"
         "                                 "
         "  delimited by '|'.\n"
         , sizeof(buf) - _len);
#endif

   fputs(buf, stdout);
   buf[0] = '\0';
   _len   = 0;

   _len += strlcpy(buf + _len,
         "      --subsystem=NAME           "
         "Use a subsystem of the libretro core. Multiple content\n"
         "                                 "
         "  files are loaded as multiple arguments. If a content\n"
         "                                 "
         "  file is skipped, use a blank (\"\") command line argument.\n"
         , sizeof(buf) - _len);

#ifdef HAVE_DYNAMIC
   _len += strlcpy(buf + _len,
         "  -L, --libretro=FILE            "
         "Path to libretro implementation. Overrides any config setting.\n"
         "                                 "
         "  FILE may be one of the following:\n"
         "                                 "
         "  1. The full path to a core shared object library: path/to/<core_name>_libretro.<lib_ext>\n"
         "                                 "
         "  2. A core shared object library 'file name' (*): <core_name>_libretro.<lib_ext>\n"
         , sizeof(buf) - _len);
   _len += strlcpy(buf + _len,
         "                                 "
         "  3. A core 'short name' (*): <core_name>_libretro OR <core_name>\n"
         "                                 "
         "  (*) If 'file name' or 'short name' do not correspond to an existing full file path,\n"
         "                                 "
         "  the configured frontend 'cores' directory will be searched for a match.\n"
         , sizeof(buf) - _len);
#endif

   _len += strlcpy(buf + _len,
         "                                 "
         "  Content must be loaded in an order which depends on the\n"
         "                                 "
         "  particular subsystem used. See verbose log output to learn\n"
         "                                 "
         "  how a particular subsystem wants content to be loaded.\n"
         , sizeof(buf) - _len);

#ifdef HAVE_LIBRETRODB
   _len += strlcpy(buf + _len,
         "      --scan=PATH|FILE           "
         "Import content from path.\n"
         , sizeof(buf) - _len);
#endif

   _len = strlcpy(buf + _len,
         "  -f, --fullscreen               "
         "Start the program in fullscreen regardless of config setting.\n"
         "      --set-shader=PATH          "
         "Path to a shader (preset) that will be loaded each time content is loaded.\n"
         "                                 "
         "  Effectively overrides automatic shader presets.\n"
         "                                 "
         "  An empty argument \"\" will disable automatic shader presets.\n"
         , sizeof(buf) - _len);

   fputs(buf, stdout);
   buf[0] = '\0';
   _len   = 0;

   _len += snprintf(buf + _len, sizeof(buf) - _len,"  -N, --nodevice=PORT            "
         "Disconnects controller device connected to PORT (1 to %d).\n", MAX_USERS);
   _len += snprintf(buf + _len, sizeof(buf) - _len,"  -A, --dualanalog=PORT          "
         "Connect a DualAnalog controller to PORT (1 to %d).\n", MAX_USERS);
   _len += snprintf(buf + _len, sizeof(buf) - _len,"  -d, --device=PORT:ID           "
         "Connect a generic device into PORT of the device (1 to %d).\n", MAX_USERS);

   _len += strlcpy(buf + _len,
         "                                 "
         "  Format is PORT:ID, where ID is a number corresponding to the particular device.\n"
         "  -M, --sram-mode=MODE           "
         "SRAM handling mode. MODE can be:\n"
         "                                 "
         "  'noload-nosave', 'noload-save', 'load-nosave' or 'load-save'.\n"
         "                                 "
         "  Note: 'noload-save' implies that save files *WILL BE OVERWRITTEN*.\n"
         , sizeof(buf) - _len);

#ifdef HAVE_NETWORKING
   _len += strlcpy(buf + _len,
         "  -H, --host                     "
         "Host netplay as user 1.\n"
         "  -C, --connect=HOST             "
         "Connect to netplay server as user 2.\n"
         "      --port=PORT                "
         "Port used to netplay. Default is 55435.\n"
         "      --mitm-session=ID           "
         "MITM (relay) session ID to join.\n"
         "      --nick=NICK                "
         "Picks a username (for use with netplay). Not mandatory.\n"
         "      --check-frames=NUMBER      "
         "Check frames when using netplay.\n"
         , sizeof(buf) - _len);
#ifdef HAVE_NETWORK_CMD
   _len += strlcpy(buf + _len,
         "      --command                  "
         "Sends a command over UDP to an already running program process.\n"
         "                                 "
         "  Available commands are listed if command is invalid.\n"
         , sizeof(buf) - _len);
#endif
#endif

#ifdef HAVE_BSV_MOVIE
   _len += strlcpy(buf + _len,
         "  -P, --play-replay=FILE         "
         "Playback a replay file.\n"
         "  -R, --record-replay=FILE       "
         "Start recording a replay file from the beginning.\n"
         "      --eof-exit                 "
         "Exit upon reaching the end of the replay file.\n"
         , sizeof(buf) - _len);
#endif

   _len += strlcpy(buf + _len,
         "  -r, --record=FILE              "
         "Path to record video file. Using mkv extension is recommended.\n"
         "      --recordconfig             "
         "Path to settings used during recording.\n"
         "      --size=WIDTHxHEIGHT        "
         "Overrides output video size when recording.\n"
         , sizeof(buf) - _len);

   fputs(buf, stdout);
   buf[0] = '\0';
   _len   = 0;

   _len   = strlcpy(buf + _len,
         "  -D, --detach                   "
         "Detach program from the running console. Not relevant for all platforms.\n"
         "      --max-frames=NUMBER        "
         "Runs for the specified number of frames, then exits.\n"
         , sizeof(buf) - _len);

#ifdef HAVE_PATCH
   _len += strlcpy(buf + _len,
         "  -U, --ups=FILE                 "
         "Specifies path for UPS patch that will be applied to content.\n"
         "      --bps=FILE                 "
         "Specifies path for BPS patch that will be applied to content.\n"
         "      --ips=FILE                 "
         "Specifies path for IPS patch that will be applied to content.\n"
         , sizeof(buf) - _len);
#ifdef HAVE_XDELTA
   _len += strlcpy(buf + _len,
         "      --xdelta=FILE              "
         "Specifies path for Xdelta patch that will be applied to content.\n"
         , sizeof(buf) - _len);
#endif /* HAVE_XDELTA */
   _len += strlcpy(buf + _len,
         "      --no-patch                 "
         "Disables all forms of content patching.\n"
         , sizeof(buf) - _len);
#endif /* HAVE_PATCH */

#ifdef HAVE_SCREENSHOTS
   _len += strlcpy(buf + _len,
         "      --max-frames-ss            "
         "Takes a screenshot at the end of max-frames.\n"
         "      --max-frames-ss-path=FILE  "
         "Path to save the screenshot to at the end of max-frames.\n"
         , sizeof(buf) - _len);
#endif

#ifdef HAVE_ACCESSIBILITY
   _len += strlcpy(buf + _len,
         "      --accessibility            "
         "Enables accessibility for blind users using text-to-speech.\n"
         , sizeof(buf) - _len);
#endif

   _len += strlcpy(buf + _len,
         "      --load-menu-on-error       "
         "Open menu instead of quitting if specified core or content fails to load.\n"
         "  -e, --entryslot=NUMBER         "
         "Slot from which to load an entry state.\n"
         "  -s, --save=PATH                "
         "Path for save files (*.srm). (DEPRECATED, use --appendconfig and savefile_directory)\n"
         "  -S, --savestate=PATH           "
         "Path for the save state files (*.state). (DEPRECATED, use --appendconfig and savestate_directory)\n"
         , sizeof(buf) - _len);

   fputs(buf, stdout);
}

#ifdef HAVE_DYNAMIC
static void retroarch_parse_input_libretro_path(const char *path, size_t path_len)
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
            path_len, STRLEN_CONST("builtin")))
   {
      RARCH_LOG("--libretro argument \"%s\" is a built-in core. Ignoring.\n",
            path);
      return;
   }

   path_stats = path_stat(path);

   /* Check if path is a directory */
   if (
       ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
#if defined(IOS) || defined(OSX)
       && !string_ends_with(path, ".framework")
#endif
       )
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
      char core_ext[16];
      if (    string_is_empty(settings->paths.directory_libretro)
          || !frontend_driver_get_core_extension(core_ext,
               sizeof(core_ext))
          || !string_is_equal(path_ext, core_ext))
         goto end;

      fill_pathname_join_special(tmp_path, settings->paths.directory_libretro,
            path, sizeof(tmp_path));

      if (string_is_empty(tmp_path))
         goto end;

      path_stats = path_stat(tmp_path);

      if (   (path_stats & RETRO_VFS_STAT_IS_VALID)     != 0
          && (path_stats & RETRO_VFS_STAT_IS_DIRECTORY) == 0)
      {
         core_path         = tmp_path;
         core_path_matched = true;
         goto end;
      }
   }
   else
   {
      size_t _len;
      const char *slash     = strrchr(path, '/');
      const char *backslash = strrchr(path, '\\');
      /* If path has no extension and contains no path
       * delimiters, check if it is a core 'name', matching
       * an existing file in the cores directory */
      if (((!slash || (backslash > slash)) ? (char*)backslash : (char*)slash))
         goto end;

      /* First check for built-in cores */
      if (string_is_equal(path, "ffmpeg"))
      {
         runloop_set_current_core_type(CORE_TYPE_FFMPEG, true);
         return;
      }
      else if (string_is_equal(path, "mpv"))
      {
         runloop_set_current_core_type(CORE_TYPE_MPV, true);
         return;
      }
      else if (string_is_equal(path, "imageviewer"))
      {
         runloop_set_current_core_type(CORE_TYPE_IMAGEVIEWER, true);
         return;
      }
      if (string_is_equal(path, "netretropad"))
      {
         runloop_set_current_core_type(CORE_TYPE_NETRETROPAD, true);
         return;
      }
      else if (string_is_equal(path, "videoprocessor"))
      {
         runloop_set_current_core_type(CORE_TYPE_VIDEO_PROCESSOR, true);
         return;
      }

      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);

      _len = strlcpy(tmp_path, path, sizeof(tmp_path));

      if (!string_ends_with_size(tmp_path, "_libretro",
            _len, STRLEN_CONST("_libretro")))
         strlcpy(tmp_path       + _len,
               "_libretro",
               sizeof(tmp_path) - _len);

      if (  !core_info_find(tmp_path, &core_info)
          || string_is_empty(core_info->path))
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

#if defined(HAVE_LIBRETRODB) && defined(HAVE_MENU)
void handle_dbscan_finished(retro_task_t *task,
      void *task_data, void *user_data, const char *err);
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
#ifdef HAVE_LIBRETRODB
   retro_task_callback_t cb_task_dbscan
                                   = NULL;
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
      { "play-replay",        1, NULL, 'P' },
      { "record-replay",      1, NULL, 'R' },
#endif
      { "sram-mode",          1, NULL, 'M' },
#ifdef HAVE_NETWORKING
      { "host",               0, NULL, 'H' },
      { "connect",            1, NULL, 'C' },
      { "mitm-session",       1, NULL, 'T' },
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
#ifdef HAVE_XDELTA
      { "xdelta",             1, NULL, RA_OPT_XDELTA },
#endif /* HAVE_XDELTA */
      { "no-patch",           0, NULL, RA_OPT_NO_PATCH },
#endif /* HAVE_PATCH */
      { "detach",             0, NULL, 'D' },
      { "features",           0, NULL, RA_OPT_FEATURES },
      { "subsystem",          1, NULL, RA_OPT_SUBSYSTEM },
      { "max-frames",         1, NULL, RA_OPT_MAX_FRAMES },
      { "max-frames-ss",      0, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT },
      { "max-frames-ss-path", 1, NULL, RA_OPT_MAX_FRAMES_SCREENSHOT_PATH },
      { "eof-exit",           0, NULL, RA_OPT_EOF_EXIT },
      { "version",            0, NULL, 'V' /* RA_OPT_VERSION */ },
      { "log-file",           1, NULL, RA_OPT_LOG_FILE },
      { "accessibility",      0, NULL, RA_OPT_ACCESSIBILITY},
      { "load-menu-on-error", 0, NULL, RA_OPT_LOAD_MENU_ON_ERROR },
      { "entryslot",          1, NULL, 'e' },
#ifdef HAVE_LIBRETRODB
      { "scan",               1, NULL, RA_OPT_DATABASE_SCAN },
#endif
      { NULL, 0, NULL, 0 }
   };

   if (first_run)
   {
      size_t _len = 0;
      /* Copy the args into a buffer so launch arguments can be reused */
      for (i = 0; i < (unsigned)argc; i++)
      {
         _len += strlcpy(p_rarch->launch_arguments        + _len,
               argv[i], sizeof(p_rarch->launch_arguments) - _len);
         _len += strlcpy(p_rarch->launch_arguments        + _len,
               " ",     sizeof(p_rarch->launch_arguments) - _len);
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

   if (!(runloop_st->flags & RUNLOOP_FLAG_HAS_SET_CORE))
      runloop_set_current_core_type(CORE_TYPE_DUMMY, false);

   path_clear(RARCH_PATH_SUBSYSTEM);

   retroarch_override_setting_free_state();

   p_rarch->flags                 &= ~RARCH_FLAGS_HAS_SET_USERNAME;
#ifdef HAVE_PATCH
   p_rarch->flags                 &= ~(  RARCH_FLAGS_UPS_PREF | RARCH_FLAGS_IPS_PREF
                                       | RARCH_FLAGS_BPS_PREF | RARCH_FLAGS_XDELTA_PREF);
   *runloop_st->name.ups           = '\0';
   *runloop_st->name.bps           = '\0';
   *runloop_st->name.ips           = '\0';
   *runloop_st->name.xdelta        = '\0';
#endif
#ifdef HAVE_CONFIGFILE
   runloop_st->flags              &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;
#endif
   global->flags                  &= ~GLOB_FLG_CLI_LOAD_MENU_ON_ERR;

   /* Make sure we can call retroarch_parse_input several times ... */
   optind                          = 0;
   optstring                       = "hs:fvVS:A:U:DN:d:e:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG CONFIG_FILE_ARG;

#if defined(WEBOS)
   argv                            = &(argv[1]);
   argc                            = argc - 1;
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

         /* Graceful failure with empty "-" parameter instead of allowing
          * to continue to segmentation fault by trying to load content */
         if (c == 0)
         {
            verbosity_enable();
            fprintf(stderr, "%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
            fprintf(stderr, "Try '%s --help' for more information\n", argv[0]);
            exit(EXIT_FAILURE);
         }

         switch (c)
         {
            case 'h':
               retroarch_print_help(argv[0]);
               exit(0);

            case 'V':
            case RA_OPT_VERSION:
               retroarch_print_version();
               exit(0);

            case RA_OPT_FEATURES:
               retroarch_print_features();
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
               strlcpy(runloop_st->name.replay, optarg,
                     sizeof(runloop_st->name.replay));
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

            case RA_OPT_MENU:
               explicit_menu = true;
               break;
            case RA_OPT_DATABASE_SCAN:
#ifdef HAVE_LIBRETRODB
               verbosity_enable();
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_DATABASE_SCAN, NULL);
#endif
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
   runloop_st->current_core.flags &= ~(RETRO_CORE_FLAG_HAS_SET_INPUT_DESCRIPTORS
                                     | RETRO_CORE_FLAG_HAS_SET_SUBSYSTEMS);

   /* Load the config file now that we know what it is */
#ifdef HAVE_CONFIGFILE
   if (!(p_rarch->flags & RARCH_FLAGS_BLOCK_CONFIG_READ))
#endif
   {
      /* Workaround for libdecor 0.2.0 setting unwanted locale */
#if defined(HAVE_WAYLAND) && defined(HAVE_DYNAMIC)
      setlocale(LC_NUMERIC,"C");
#endif
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
                  char *save               = NULL;
                  char *tok                = NULL;
                  int    port              = 0;
                  unsigned id              = 0;
                  char *optarg_cpy         = strdup(optarg);

                  if ((tok = strtok_r(optarg_cpy, ":", &save)))
                     port = (int)strtol(tok, NULL, 0);
                  if ((tok = strtok_r(NULL, ":", &save)))
                     id   = (unsigned)strtoul(tok, NULL, 0);
                  free(optarg_cpy);

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
               video_st->flags |= VIDEO_FLAG_FORCE_FULLSCREEN;
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
                  video_st->flags |= VIDEO_FLAG_CLI_SHADER_DISABLE;
                  break;
               }

               /* rebase on shader directory */
               if (path_is_absolute(optarg))
                  strlcpy(video_st->cli_shader_path, optarg,
                        sizeof(video_st->cli_shader_path));
               else
                  fill_pathname_join_special(video_st->cli_shader_path,
                        settings->paths.directory_video_shader,
                        optarg, sizeof(video_st->cli_shader_path));
#endif
               break;

#ifdef HAVE_DYNAMIC
            case 'L':
               retroarch_parse_input_libretro_path(optarg, strlen(optarg));
               break;
#endif
            case 'P':
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  strlcpy(input_st->bsv_movie_state.movie_start_path, optarg,
                        sizeof(input_st->bsv_movie_state.movie_start_path));
                  input_st->bsv_movie_state.flags |=
                      BSV_FLAG_MOVIE_START_PLAYBACK;
                  input_st->bsv_movie_state.flags &=
                     ~BSV_FLAG_MOVIE_START_RECORDING;
               }
#endif
               break;
            case 'R':
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st = input_state_get_ptr();
                  strlcpy(input_st->bsv_movie_state.movie_start_path, optarg,
                        sizeof(input_st->bsv_movie_state.movie_start_path));
                  input_st->bsv_movie_state.flags &=
                     ~BSV_FLAG_MOVIE_START_PLAYBACK;
                  input_st->bsv_movie_state.flags |=
                     BSV_FLAG_MOVIE_START_RECORDING;
               }
#endif
               break;

            case 'M':
               if (string_is_equal(optarg, "noload-nosave"))
                  runloop_st->flags |= RUNLOOP_FLAG_IS_SRAM_LOAD_DISABLED
                                     | RUNLOOP_FLAG_IS_SRAM_SAVE_DISABLED;
               else if (string_is_equal(optarg, "noload-save"))
                  runloop_st->flags |= RUNLOOP_FLAG_IS_SRAM_LOAD_DISABLED;
               else if (string_is_equal(optarg, "load-nosave"))
                  runloop_st->flags |= RUNLOOP_FLAG_IS_SRAM_SAVE_DISABLED;
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

            case 'T':
               p_rarch->connect_mitm_id = strdup(optarg);
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
               p_rarch->flags |= RARCH_FLAGS_BPS_PREF;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
#endif
               break;

            case 'U':
#ifdef HAVE_PATCH
               strlcpy(runloop_st->name.ups, optarg,
                     sizeof(runloop_st->name.ups));
               p_rarch->flags |= RARCH_FLAGS_UPS_PREF;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
#endif
               break;

            case RA_OPT_IPS:
#ifdef HAVE_PATCH
               strlcpy(runloop_st->name.ips, optarg,
                     sizeof(runloop_st->name.ips));
               p_rarch->flags |= RARCH_FLAGS_IPS_PREF;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_IPS_PREF, NULL);
#endif
               break;
             case RA_OPT_XDELTA:
#if defined(HAVE_PATCH) && defined(HAVE_XDELTA)
                 strlcpy(runloop_st->name.xdelta, optarg,
                     sizeof(runloop_st->name.xdelta));
               p_rarch->flags |= RARCH_FLAGS_XDELTA_PREF;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_XDELTA_PREF, NULL);
#endif
                 break;
            case RA_OPT_NO_PATCH:
#ifdef HAVE_PATCH
               runloop_st->flags |= RUNLOOP_FLAG_PATCH_BLOCKED;
#endif
               break;

            case 'D':
               frontend_driver_detach_console();
               break;

            case RA_OPT_MENU:
               explicit_menu = true;
               break;

            case RA_OPT_NICK:
               p_rarch->flags |= RARCH_FLAGS_HAS_SET_USERNAME;

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
               runloop_st->flags |= RUNLOOP_FLAG_MAX_FRAMES_SCREENSHOT;
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

            case RA_OPT_EOF_EXIT:
#ifdef HAVE_BSV_MOVIE
               {
                  input_driver_state_t *input_st   = input_state_get_ptr();
                  input_st->bsv_movie_state.flags |= BSV_FLAG_MOVIE_EOF_EXIT;
               }
#endif
               break;

            case 'h':
            case 'V':
            case RA_OPT_VERSION:
            case RA_OPT_FEATURES:
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
               global->flags |= GLOB_FLG_CLI_LOAD_MENU_ON_ERR;
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
            case RA_OPT_DATABASE_SCAN:
#ifdef HAVE_LIBRETRODB
               {
                  settings_t *settings           = config_get_ptr();
                  bool show_hidden_files         = settings->bools.show_hidden_files;
                  const char *directory_playlist = settings->paths.directory_playlist;
                  const char *path_content_db    = settings->paths.path_content_database;
                  int reinit_flags               = DRIVERS_CMD_ALL &
                        ~(DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK | DRIVER_MICROPHONE_MASK | DRIVER_INPUT_MASK | DRIVER_MIDI_MASK);

                  drivers_init(settings, reinit_flags, (enum driver_lifetime_flags)0, false);
                  retroarch_init_task_queue();

#ifdef HAVE_MENU
                  if (explicit_menu)
                     cb_task_dbscan = handle_dbscan_finished;
#endif

                  task_push_dbscan(
                        directory_playlist,
                        path_content_db,
                        optarg, path_is_directory(optarg),
                        show_hidden_files,
                        cb_task_dbscan);

                  if (!explicit_menu)
                  {
                     task_queue_wait(NULL, NULL);
                     driver_uninit(DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0);
                     exit(0);
                  }
               }
#endif
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
         runloop_path_set_special(argv + optind, argc - optind);

      /* Register that content has been set via the
       * command line interface */
      cli_content_set = true;
   }
   else if (runloop_st->entry_state_slot)
   {
      runloop_st->entry_state_slot = 0;
      RARCH_WARN("Trying to load entry state without content. Ignoring.\n");
   }
   #ifdef HAVE_BSV_MOVIE
   if (runloop_st->entry_state_slot)
   {
     input_driver_state_t *input_st = input_state_get_ptr();
     if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_PLAYBACK)
     {
        runloop_st->entry_state_slot = 0;
        RARCH_WARN("Trying to load entry state while replay playback is active. Ignoring entry state.\n");
     }
   }
   #endif


   /* Check whether a core has been set via the
    * command line interface */
   cli_core_set = (runloop_st->current_core_type != CORE_TYPE_DUMMY);

   /* Update global 'content launched from command
    * line' status flag */
   if (cli_active && (cli_core_set || cli_content_set))
      global->flags |=  (GLOB_FLG_LAUNCHED_FROM_CLI);
   else
      global->flags &= ~(GLOB_FLG_LAUNCHED_FROM_CLI);

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(runloop_st->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, runloop_st->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(runloop_st->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, runloop_st->name.savestate);

   return verbosity_enabled;
}

/**
 * retroarch_validate_cpu_features:
 *
 * Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now.
 **/
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
 * @return true on success, otherwise false if there was an error.
 **/
bool retroarch_main_init(int argc, char *argv[])
{
#if defined(DEBUG) && defined(HAVE_DRMINGW)
   char log_file_name[128];
#endif
   bool verbosity_enabled        = false;
   bool           init_failed    = false;
   struct rarch_state *p_rarch   = &rarch_st;
   runloop_state_t *runloop_st   = runloop_state_get_ptr();
   input_driver_state_t
      *input_st                  = input_state_get_ptr();
   video_driver_state_t*video_st = video_state_get_ptr();
   settings_t *settings          = config_get_ptr();
   recording_state_t
      *recording_st              = recording_state_get_ptr();
   global_t            *global   = global_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   access_state_t *access_st     = access_state_get_ptr();
   bool accessibility_enable     = false;
   unsigned accessibility_narrator_speech_speed = 0;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st    = menu_state_get_ptr();
#endif

   input_st->osk_idx             = OSK_LOWERCASE_LATIN;
   video_st->flags              |= VIDEO_FLAG_ACTIVE;
   audio_state_get_ptr()->flags |= AUDIO_FLAG_ACTIVE;

   if (setjmp(global->error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN),
            global_get_ptr()->error_string);
      goto error;
   }

   global->flags |= GLOB_FLG_ERR_ON_INIT;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   verbosity_enabled = retroarch_parse_input_and_config(p_rarch,
         global_get_ptr(), argc, argv);

#ifdef __APPLE__
   /* This doesn't have to be apple specific but it's currently the only
    * platform that doesn't call dir_check_defaults(). This does exactly the
    * same thing as dir_check_defaults() except that it makes the directories
    * pointed at by the config file rather than the defaults, which is why it
    * comes right after parsing the config. osx users seem to be more likely to
    * want to do change the default directories.
    */
   dir_check_config();
#endif

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
            (char*)msg_hash_to_str(MSG_ACCESSIBILITY_STARTUP),
            10);
#endif

   if (verbosity_enabled)
   {
      {
         char str_output[256];
         const char *cpu_model  = frontend_driver_get_cpu_model_name();
         size_t _len = strlcpy(str_output,
               "=== Build =======================================\n",
               sizeof(str_output));

         if (!string_is_empty(cpu_model))
         {
            /* TODO/FIXME - localize */
            _len += strlcpy(str_output + _len,
                  FILE_PATH_LOG_INFO " CPU Model Name: ",
                  sizeof(str_output)   - _len);
            _len              += strlcpy(str_output + _len, cpu_model,
                                 sizeof(str_output) - _len);
            str_output[  _len] = '\n';
            str_output[++_len] = '\0';
         }

         RARCH_LOG_OUTPUT("%s", str_output);
      }

      {
         char str_output[256];
         char str[128];
         retroarch_get_capabilities(RARCH_CAPABILITIES_CPU, str, sizeof(str));

#ifdef HAVE_GIT_VERSION
         snprintf(str_output, sizeof(str_output),
               "%s: %s" "\n"
               FILE_PATH_LOG_INFO " Version: " PACKAGE_VERSION "\n"
               FILE_PATH_LOG_INFO " Git: %s" "\n"
               FILE_PATH_LOG_INFO " Built: " __DATE__ "\n"
               FILE_PATH_LOG_INFO " =================================================\n",
               msg_hash_to_str(MSG_CAPABILITIES),
               str,
               retroarch_git_version
               );
#else
         snprintf(str_output, sizeof(str_output),
               "%s: %s" "\n"
               FILE_PATH_LOG_INFO " Version: " PACKAGE_VERSION "\n"
               FILE_PATH_LOG_INFO " Built: " __DATE__ "\n"
               FILE_PATH_LOG_INFO " =================================================\n",
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
   if (!input_driver_find_driver(settings,
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
#ifdef HAVE_CLOUDSYNC
   cloud_sync_find_driver(settings,
         "cloud sync driver", verbosity_enabled);
#endif
   location_driver_find_driver(settings,
         &location_driver_st,
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
   if (runloop_st->flags & RUNLOOP_FLAG_HAS_SET_CORE)
   {
      runloop_st->flags &= ~RUNLOOP_FLAG_HAS_SET_CORE;
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
      if (   (!(global->flags & GLOB_FLG_LAUNCHED_FROM_CLI))
          ||   (global->flags & GLOB_FLG_CLI_LOAD_MENU_ON_ERR)
#ifdef HAVE_MENU
          ||  (menu_st->flags & MENU_ST_FLAG_ALIVE)
#endif
         )
#endif
      {
         /* Before initialising the dummy core, ensure
          * that we:
          * - Unload any active input remaps
          * - Disable any active config overrides */
         if (     (runloop_st->flags & RUNLOOP_FLAG_REMAPS_CORE_ACTIVE)
               || (runloop_st->flags & RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE)
               || (runloop_st->flags & RUNLOOP_FLAG_REMAPS_GAME_ACTIVE)
               || !string_is_empty(runloop_st->name.remapfile)
            )
         {
            input_remapping_deinit(false);
            input_remapping_set_defaults(true);
         }
         else
            input_remapping_restore_global_config(true, false);

#ifdef HAVE_CONFIGFILE
         /* Reload the original config */
         if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
            config_unload_override();
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
   drivers_init(settings, DRIVERS_CMD_ALL, (enum driver_lifetime_flags)0, verbosity_enabled);
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

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   global->flags                   &= ~GLOB_FLG_ERR_ON_INIT;
   runloop_st->flags               |=  RUNLOOP_FLAG_IS_INITED;

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

#ifdef HAVE_RUNAHEAD
#ifdef HAVE_MENU
   menu_update_runahead_mode();
#endif
#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
#endif
      preempt_init(runloop_st);
#endif

   return true;

error:
   command_event(CMD_EVENT_CORE_DEINIT, NULL);
   runloop_state_get_ptr()->flags            &= ~RUNLOOP_FLAG_IS_INITED;

   return false;
}

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
#ifdef HAVE_BSV_MOVIE
      case RARCH_CTL_BSV_MOVIE_IS_INITED:
         return (input_state_get_ptr()->bsv_movie_state_handle != NULL);
#endif
#ifdef HAVE_PATCH
      case RARCH_CTL_UNSET_BPS_PREF:
         p_rarch->flags &= ~RARCH_FLAGS_BPS_PREF;
         break;
      case RARCH_CTL_UNSET_UPS_PREF:
         p_rarch->flags &= ~RARCH_FLAGS_UPS_PREF;
         break;
      case RARCH_CTL_UNSET_IPS_PREF:
         p_rarch->flags &= ~RARCH_FLAGS_IPS_PREF;
         break;
#ifdef HAVE_XDELTA
      case RARCH_CTL_UNSET_XDELTA_PREF:
         p_rarch->flags &= ~RARCH_FLAGS_XDELTA_PREF;
         break;
#endif /* HAVE_XDELTA */
#endif /* HAVE_PATCH */
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
                  (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
               && (runloop_st->flags & RUNLOOP_FLAG_RUNAHEAD_SECONDARY_CORE_AVAILABLE);
      case RARCH_CTL_IS_SECOND_CORE_LOADED:
         return
                   (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
               &&  (runloop_st->secondary_lib_handle != NULL);
#endif
      case RARCH_CTL_MAIN_DEINIT:
         {
            input_driver_state_t *input_st = input_state_get_ptr();
            if (!(runloop_st->flags & RUNLOOP_FLAG_IS_INITED))
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
            if (runloop_st->flags & RUNLOOP_FLAG_USE_SRAM)
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
            movie_stop(input_st);
#endif
            command_event(CMD_EVENT_CORE_DEINIT, NULL);

            content_deinit();

            runloop_path_deinit_subsystem();
            path_deinit_savefile();

            runloop_st->flags &= ~RUNLOOP_FLAG_IS_INITED;

#ifdef HAVE_THREAD_STORAGE
            sthread_tls_delete(&p_rarch->rarch_tls);
#endif
         }
         break;
#ifdef HAVE_CONFIGFILE
      case RARCH_CTL_SET_BLOCK_CONFIG_READ:
         p_rarch->flags |= RARCH_FLAGS_BLOCK_CONFIG_READ;
         break;
      case RARCH_CTL_UNSET_BLOCK_CONFIG_READ:
         p_rarch->flags &= ~RARCH_FLAGS_BLOCK_CONFIG_READ;
         break;
#endif
      case RARCH_CTL_CORE_OPTIONS_LIST_GET:
         {
            core_option_manager_t **coreopts = (core_option_manager_t**)data;
            if (!coreopts || !runloop_st->core_options)
               return false;
            *coreopts = runloop_st->core_options;
         }
         break;
      case RARCH_CTL_CORE_OPTION_UPDATE_DISPLAY:
         if (   runloop_st->core_options
             && runloop_st->core_options_callback.update_display)
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
      case RARCH_CTL_SET_REMAPS_CORE_ACTIVE:
         /* Only one type of remap can be active
          * at any one time */
         runloop_st->flags &= ~(RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE
                              | RUNLOOP_FLAG_REMAPS_GAME_ACTIVE);
         runloop_st->flags |=   RUNLOOP_FLAG_REMAPS_CORE_ACTIVE;
         break;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_st->flags &= ~(RUNLOOP_FLAG_REMAPS_CORE_ACTIVE
                              | RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE);
         runloop_st->flags |=   RUNLOOP_FLAG_REMAPS_GAME_ACTIVE;
         break;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_st->flags &= ~(RUNLOOP_FLAG_REMAPS_CORE_ACTIVE
                              | RUNLOOP_FLAG_REMAPS_GAME_ACTIVE);
         runloop_st->flags |=   RUNLOOP_FLAG_REMAPS_CONTENT_DIR_ACTIVE;
         break;
#endif
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
            runloop_st->perfcnt_enable     = false;
#ifdef HAVE_CONFIGFILE
            runloop_st->flags             &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;
#endif
            runloop_st->flags             &= ~(RUNLOOP_FLAG_AUTOSAVE
                                           |   RUNLOOP_FLAG_SLOWMOTION
                                           |   RUNLOOP_FLAG_IDLE
                                           |   RUNLOOP_FLAG_PAUSED
                                              );
            runloop_state_free(runloop_st);

            memset(&input_st->analog_requested, 0,
                  sizeof(input_st->analog_requested));
         }
         break;
      case RARCH_CTL_SET_SHUTDOWN:
         runloop_st->flags |= RUNLOOP_FLAG_SHUTDOWN_INITIATED;
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
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_VERBOSITY) > 0);
      case RARCH_OVERRIDE_SETTING_LIBRETRO:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_LIBRETRO) > 0);
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DIRECTORY:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_LIBRETRO_DIRECTORY) > 0);
      case RARCH_OVERRIDE_SETTING_SAVE_PATH:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_SAVE_PATH) > 0);
      case RARCH_OVERRIDE_SETTING_STATE_PATH:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_STATE_PATH) > 0);
#ifdef HAVE_NETWORKING
      case RARCH_OVERRIDE_SETTING_NETPLAY_MODE:
         return ((net_st->flags & NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_MODE) > 0);
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return ((net_st->flags & NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_ADDRESS) > 0);
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return ((net_st->flags & NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_IP_PORT) > 0);
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return ((net_st->flags & NET_DRIVER_ST_FLAG_HAS_SET_NETPLAY_CHECK_FRAMES) > 0);
#endif
#ifdef HAVE_PATCH
      case RARCH_OVERRIDE_SETTING_UPS_PREF:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_UPS_PREF) > 0);
      case RARCH_OVERRIDE_SETTING_BPS_PREF:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_BPS_PREF) > 0);
      case RARCH_OVERRIDE_SETTING_IPS_PREF:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_IPS_PREF) > 0);
#ifdef HAVE_XDELTA
      case RARCH_OVERRIDE_SETTING_XDELTA_PREF:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_XDELTA_PREF) > 0);
#endif /* HAVE_XDELTA */
#endif /* HAVE_PATCH */
      case RARCH_OVERRIDE_SETTING_LOG_TO_FILE:
         return ((p_rarch->flags & RARCH_FLAGS_HAS_SET_LOG_TO_FILE) > 0);
      case RARCH_OVERRIDE_SETTING_DATABASE_SCAN:
         return ((p_rarch->flags & RARCH_FLAGS_CLI_DATABASE_SCAN) > 0);
      case RARCH_OVERRIDE_SETTING_NONE:
      default:
         break;
   }

   return false;
}

int retroarch_get_capabilities(enum rarch_capabilities type,
      char *str_out, size_t str_len)
{
   size_t _len = 0;
   switch (type)
   {
      case RARCH_CAPABILITIES_CPU:
         {
            uint64_t cpu = cpu_features_get();
            if (cpu & RETRO_SIMD_MMX)
               _len += strlcpy(str_out + _len, "MMX ", str_len - _len);
            if (cpu & RETRO_SIMD_MMXEXT)
               _len += strlcpy(str_out + _len, "MMXEXT ", str_len - _len);
            if (cpu & RETRO_SIMD_SSE)
               _len += strlcpy(str_out + _len, "SSE ", str_len - _len);
            if (cpu & RETRO_SIMD_SSE2)
               _len += strlcpy(str_out + _len, "SSE2 ", str_len - _len);
            if (cpu & RETRO_SIMD_SSE3)
               _len += strlcpy(str_out + _len, "SSE3 ", str_len - _len);
            if (cpu & RETRO_SIMD_SSSE3)
               _len += strlcpy(str_out + _len, "SSSE3 ", str_len - _len);
            if (cpu & RETRO_SIMD_SSE4)
               _len += strlcpy(str_out + _len, "SSE4 ", str_len - _len);
            if (cpu & RETRO_SIMD_SSE42)
               _len += strlcpy(str_out + _len, "SSE42 ", str_len - _len);
            if (cpu & RETRO_SIMD_AES)
               _len += strlcpy(str_out + _len, "AES ", str_len - _len);
            if (cpu & RETRO_SIMD_AVX)
               _len += strlcpy(str_out + _len, "AVX ", str_len - _len);
            if (cpu & RETRO_SIMD_AVX2)
               _len += strlcpy(str_out + _len, "AVX2 ", str_len - _len);
            if (cpu & RETRO_SIMD_NEON)
               _len += strlcpy(str_out + _len, "NEON ", str_len - _len);
            if (cpu & RETRO_SIMD_VFPV3)
               _len += strlcpy(str_out + _len, "VFPV3 ", str_len - _len);
            if (cpu & RETRO_SIMD_VFPV4)
               _len += strlcpy(str_out + _len, "VFPV4 ", str_len - _len);
            if (cpu & RETRO_SIMD_VMX)
               _len += strlcpy(str_out + _len, "VMX ", str_len - _len);
            if (cpu & RETRO_SIMD_VMX128)
               _len += strlcpy(str_out + _len, "VMX128 ", str_len - _len);
            if (cpu & RETRO_SIMD_VFPU)
               _len += strlcpy(str_out + _len, "VFPU ", str_len - _len);
            if (cpu & RETRO_SIMD_PS)
               _len += strlcpy(str_out + _len, "PS ", str_len - _len);
            if (cpu & RETRO_SIMD_ASIMD)
               _len += strlcpy(str_out + _len, "ASIMD ", str_len - _len);
            break;
         }
         break;
      case RARCH_CAPABILITIES_COMPILER:
#if defined(_MSC_VER)
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_COMPILER), str_len);
         _len += snprintf(str_out + _len, str_len - _len, ": MSVC (%d)",
               _MSC_VER);
#elif defined(__SNC__)
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_COMPILER), str_len);
         _len += snprintf(str_out + _len, str_len - _len, ": SNC (%d)",
               __SN_VER__);
#elif defined(_WIN32) && defined(__GNUC__)
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_COMPILER), str_len);
         _len += snprintf(str_out + _len, str_len - _len, ": MinGW (%d.%d.%d)",
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(__clang__)
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_COMPILER), str_len);
         _len += strlcpy(str_out + _len, ": Clang/LLVM (", str_len - _len);
         _len += strlcpy(str_out + _len, __clang_version__, str_len - _len);
         _len += strlcpy(str_out + _len, ")", str_len - _len);
#elif defined(__GNUC__)
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_COMPILER), str_len);
         _len += snprintf(str_out + _len, str_len - _len, ": GCC (%d.%d.%d)",
               __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#else
         _len  = strlcpy(str_out, msg_hash_to_str(MSG_UNKNOWN_COMPILER), str_len);
#endif
         snprintf(str_out + _len, str_len - _len, " %u-bit",
               (unsigned)(CHAR_BIT * sizeof(size_t)));
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
   strlcpy(global->error_string,
         error, sizeof(global->error_string));
   longjmp(global->error_sjlj_context, error_code);
}

/* Called on close content, checks if we need to also exit retroarch */
bool should_quit_on_close(void)
{
#ifdef HAVE_MENU
   settings_t *settings   = config_get_ptr();
   global_t   *global     = global_get_ptr();
   if (       ((settings->uints.quit_on_close_content ==
               QUIT_ON_CLOSE_CONTENT_CLI)
            && (global->flags & GLOB_FLG_LAUNCHED_FROM_CLI))
            || (settings->uints.quit_on_close_content ==
               QUIT_ON_CLOSE_CONTENT_ENABLED)
      )
      return true;
#endif
   return false;
}

/*
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 */
bool retroarch_main_quit(void)
{
   runloop_state_t *runloop_st   = runloop_state_get_ptr();
   video_driver_state_t*video_st = video_state_get_ptr();
   settings_t *settings          = config_get_ptr();
   bool config_save_on_exit      = settings->bools.config_save_on_exit;

   /* Restore video driver before saving */
   video_driver_restore_cached(settings);

#if !defined(HAVE_DYNAMIC)
   {
      /* Salamander sets RUNLOOP_FLAG_SHUTDOWN_INITIATED prior, so we need to handle it separately */
      /* config_save_file_salamander() must be called independent of config_save_on_exit */
      config_save_file_salamander();
      if (config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
   }
#endif

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

   if (!(runloop_st->flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
   {
      if (settings->bools.savestate_auto_save &&
          runloop_st->current_core_type != CORE_TYPE_DUMMY)
         command_event_save_auto_state();

      /* If any save states are in progress, wait
       * until all tasks are complete (otherwise
       * save state file may be truncated) */
      content_wait_for_save_state_task();

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

#ifdef HAVE_CONFIGFILE
      if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
      {
         /* Reload the original config */
         config_unload_override();
      }
#endif

      /* Save configs before quitting
       * as for UWP depending on `OnSuspending` is not important as we can call it directly here
       * specifically we need to get width,height which requires UI thread and it will not be available on exit
       */
#if defined(HAVE_DYNAMIC)
      if (config_save_on_exit)
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
#endif

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
      runloop_st->runtime_shader_preset_path[0] = '\0';
#endif
   }

   runloop_st->flags |= RUNLOOP_FLAG_SHUTDOWN_INITIATED;
#ifdef HAVE_MENU
   retroarch_menu_running_finished(true);
#endif

   return true;
}

enum retro_language retroarch_get_language_from_iso(const char *iso639)
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
      {"ca_ES@valencia", RETRO_LANGUAGE_CATALAN_VALENCIA},
      {"en_CA", RETRO_LANGUAGE_BRITISH_ENGLISH}, /* Canada must be indexed before Catalan's "ca". */
      {"ca", RETRO_LANGUAGE_CATALAN},
      {"en_GB", RETRO_LANGUAGE_BRITISH_ENGLISH},
      {"en", RETRO_LANGUAGE_ENGLISH},
      {"hu", RETRO_LANGUAGE_HUNGARIAN},
      {"be", RETRO_LANGUAGE_BELARUSIAN},
      {"gl", RETRO_LANGUAGE_GALICIAN},
      {"no", RETRO_LANGUAGE_NORWEGIAN},
   };

   if (string_is_empty(iso639))
      return lang;

   for (i = 0; i < ARRAY_SIZE(pairs); i++)
   {
      if (string_starts_with_case_insensitive(iso639, pairs[i].iso639))
      {
         lang = pairs[i].lang;
         break;
      }
   }

   return lang;
}

void retroarch_favorites_init(void)
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

   retroarch_favorites_deinit();

   if (!playlist_config.capacity)
      return;

   RARCH_LOG("[Playlist]: %s: \"%s\".\n",
         msg_hash_to_str(MSG_LOADING_FAVORITES_FILE),
         path_content_favorites);
   playlist_config_set_path(&playlist_config, path_content_favorites);
   g_defaults.content_favorites = playlist_init(&playlist_config);

   /* Get current per-playlist sort mode */
   current_sort_mode = playlist_get_sort_mode(g_defaults.content_favorites);

   /* Ensure that playlist is sorted alphabetically,
    * if required */
   if (   (playlist_sort_alphabetical && (current_sort_mode == PLAYLIST_SORT_MODE_DEFAULT))
       || (current_sort_mode == PLAYLIST_SORT_MODE_ALPHABETICAL))
      playlist_qsort(g_defaults.content_favorites);
}

void retroarch_favorites_deinit(void)
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
