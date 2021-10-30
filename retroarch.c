/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2017 - Jean-Andr� Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2016-2019 - Andr�s Su�rez (input mapper/Discord code)
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
#include <errno.h>
#include <setjmp.h>
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

#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <libretro.h>
#define VFS_FRONTEND
#include <vfs/vfs_implementation.h>

#include <features/features_cpu.h>

#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <compat/getopt.h>
#include <compat/posix_string.h>
#include <streams/file_stream.h>
#include <streams/interface_stream.h>
#include <file/file_path.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <queues/task_queue.h>
#include <lists/dir_list.h>
#ifdef HAVE_NETWORKING
#include <net/net_http.h>
#endif

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

#ifdef HAVE_DISCORD
#include <discord_rpc.h>
#include "deps/discord-rpc/include/discord_rpc.h"
#include "network/discord.h"
#endif

#include "config.def.h"

#include "runtime_file.h"
#include "runloop.h"

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

#ifdef HAVE_TRANSLATE
#include <encodings/base64.h>
#include <formats/rbmp.h>
#include <formats/rpng.h>
#include <formats/rjson.h>
#include "translation_defines.h"
#endif

#ifdef HAVE_DISCORD
#include "network/discord.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#include "network/netplay/netplay_private.h"
#include "network/netplay/netplay_discovery.h"
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
#include "bluetooth/bluetooth_driver.h"
#include "wifi/wifi_driver.h"
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

#ifdef HAVE_ACCESSIBILITY
#include "accessibility.h"
#endif

#if defined(HAVE_SDL) || defined(HAVE_SDL2) || defined(HAVE_SDL_DINGUX)
#include "SDL.h"
#endif

/* RetroArch global state / macros */
#include "retroarch_data.h"
/* Forward declarations */
#include "retroarch_fwd_decls.h"

#ifdef HAVE_LAKKA
#include "lakka.h"
#endif

#ifdef HAVE_THREADS
#define RUNLOOP_MSG_QUEUE_LOCK(runloop_st) slock_lock((runloop_st)->msg_queue_lock)
#define RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st) slock_unlock((runloop_st)->msg_queue_lock)
#else
#define RUNLOOP_MSG_QUEUE_LOCK(runloop_st) (void)(runloop_st)
#define RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st) (void)(runloop_st)
#endif

/* Custom forward declarations */
static bool recording_init(settings_t *settings,
      struct rarch_state *p_rarch);
static bool recording_deinit(void);

static struct rarch_state         rarch_st;

#ifdef HAVE_THREAD_STORAGE
static const void *MAGIC_POINTER                                 = (void*)(uintptr_t)0x0DEFACED;
#endif

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { 1.3333f,         "4:3"           },
   { 1.7778f,         "16:9"          },
   { 1.6f,            "16:10"         },
   { 16.0f / 15.0f,   "16:15"         },
   { 21.0f / 9.0f,    "21:9"          },
   { 1.0f,            "1:1"           },
   { 2.0f,            "2:1"           },
   { 1.5f,            "3:2"           },
   { 0.75f,           "3:4"           },
   { 4.0f,            "4:1"           },
   { 0.5625f,         "9:16"          },
   { 1.25f,           "5:4"           },
   { 1.2f,            "6:5"           },
   { 0.7777f,         "7:9"           },
   { 2.6666f,         "8:3"           },
   { 1.1428f,         "8:7"           },
   { 1.5833f,         "19:12"         },
   { 1.3571f,         "19:14"         },
   { 1.7647f,         "30:17"         },
   { 3.5555f,         "32:9"          },
   { 0.0f,            "Config"        },
   { 1.0f,            "Square pixel"  },
   { 1.0f,            "Core provided" },
   { 0.0f,            "Custom"        },
   { 1.3333f,         "Full" }
};

/* TODO/FIXME - turn these into static global variable */
#ifdef HAVE_DISCORD
bool discord_is_inited                                          = false;
#endif
struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];

static runloop_core_status_msg_t runloop_core_status_msg         =
{
   0,
   0.0f,
   "",
   false
};

static runloop_state_t runloop_state;
static recording_state_t recording_state;

recording_state_t *recording_state_get_ptr(void)
{
   return &recording_state;
}

/* GLOBAL POINTER GETTERS */
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

settings_t *config_get_ptr(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->configuration_settings;
}

global_t *global_get_ptr(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return &p_rarch->g_extern;
}

/* DRIVERS */

int driver_find_index(const char *label, const char *drv)
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

#ifdef HAVE_MENU
/**
 * menu_iterate:
 * @input                    : input sample for this frame
 * @old_input                : input sample of the previous frame
 * @trigger_input            : difference' input sample - difference
 *                             between 'input' and 'old_input'
 *
 * Runs RetroArch menu for one frame.
 *
 * Returns: 0 on success, -1 if we need to quit out of the loop.
 **/
static int generic_menu_iterate(
      struct rarch_state *p_rarch,
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      menu_handle_t *menu,
      void *userdata, enum menu_action action,
      retro_time_t current_time)
{
#ifdef HAVE_ACCESSIBILITY
   static enum action_iterate_type
      last_iterate_type            = ITERATE_TYPE_DEFAULT;
   bool accessibility_enable       = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   enum action_iterate_type iterate_type;
   unsigned file_type              = 0;
   int ret                         = 0;
   const char *label               = NULL;
   file_list_t *list               = MENU_LIST_GET(menu_st->entries.list, 0);

   if (list && list->size)
      file_list_get_at_offset(list, list->size - 1, NULL, &label, &file_type, NULL);

   menu->menu_state_msg[0]         = '\0';

   iterate_type                    = action_iterate_type(label);
   menu_st->is_binding             = false;

   if (     action != MENU_ACTION_NOOP
         || MENU_ENTRIES_NEEDS_REFRESH(menu_st)
         || GFX_DISPLAY_GET_UPDATE_PENDING(p_anim, p_disp))
   {
      BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);
   }

   switch (iterate_type)
   {
      case ITERATE_TYPE_HELP:
         ret = menu_dialog_iterate(
               &menu_st->dialog_st,  settings,
               menu->menu_state_msg, sizeof(menu->menu_state_msg),
               current_time);

#ifdef HAVE_ACCESSIBILITY
         if (     (iterate_type != last_iterate_type)
               && is_accessibility_enabled(
                  accessibility_enable,
                  p_rarch->accessibility_enabled))
            accessibility_speak_priority(p_rarch,
                  accessibility_enable,
                  accessibility_narrator_speech_speed,
                  menu->menu_state_msg, 10);
#endif

         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

         {
            bool pop_stack = false;
            if (  ret == 1 ||
                  action == MENU_ACTION_OK ||
                  action == MENU_ACTION_CANCEL
               )
               pop_stack   = true;

            if (pop_stack)
               BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         }
         break;
      case ITERATE_TYPE_BIND:
         {
            menu_input_ctx_bind_t bind;

            menu_st->is_binding = true;

            bind.s              = menu->menu_state_msg;
            bind.len            = sizeof(menu->menu_state_msg);

            if (menu_input_key_bind_iterate(
                     settings,
                     &bind, current_time))
            {
               size_t selection = menu_st->selection_ptr;
               menu_entries_pop_stack(&selection, 0, 0);
               menu_st->selection_ptr      = selection;
            }
            else
               BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         }
         break;
      case ITERATE_TYPE_INFO:
         {
            menu_list_t *menu_list     = menu_st->entries.list;
            file_list_t *selection_buf = menu_list ? MENU_LIST_GET_SELECTION(menu_list, (unsigned)0) : NULL;
            size_t selection           = menu_st->selection_ptr;
            menu_file_list_cbs_t *cbs  = selection_buf ?
               (menu_file_list_cbs_t*)selection_buf->list[selection].actiondata
               : NULL;

            if (cbs && cbs->enum_idx != MSG_UNKNOWN)
            {
               /* Core updater/manager entries require special treatment */
               switch (cbs->enum_idx)
               {
#ifdef HAVE_NETWORKING
                  case MENU_ENUM_LABEL_CORE_UPDATER_ENTRY:
                     {
                        core_updater_list_t *core_list         =
                           core_updater_list_get_cached();
                        const core_updater_list_entry_t *entry = NULL;
                        const char *path                       =
                           selection_buf->list[selection].path;

                        /* Search for specified core */
                        if (
                                 core_list
                              && path
                              && core_updater_list_get_filename(core_list,
                                 path, &entry)
                              && !string_is_empty(entry->description)
                           )
                           strlcpy(menu->menu_state_msg, entry->description,
                                 sizeof(menu->menu_state_msg));
                        else
                           strlcpy(menu->menu_state_msg,
                                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                                 sizeof(menu->menu_state_msg));

                        ret = 0;
                     }
                     break;
#endif
                  case MENU_ENUM_LABEL_CORE_MANAGER_ENTRY:
                     {
                        core_info_t *core_info = NULL;
                        const char *path       = selection_buf->list[selection].path;

                        /* Search for specified core */
                        if (     path
                              && core_info_find(path, &core_info)
                              && !string_is_empty(core_info->description))
                           strlcpy(menu->menu_state_msg,
                                 core_info->description,
                                 sizeof(menu->menu_state_msg));
                        else
                           strlcpy(menu->menu_state_msg,
                                 msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                                 sizeof(menu->menu_state_msg));

                        ret = 0;
                     }
                     break;
                  default:
                     ret = msg_hash_get_help_enum(cbs->enum_idx,
                           menu->menu_state_msg, sizeof(menu->menu_state_msg));
                     break;
               }

#ifdef HAVE_ACCESSIBILITY
               if (  (iterate_type != last_iterate_type) &&
                     is_accessibility_enabled(
                        accessibility_enable,
                        p_rarch->accessibility_enabled))
               {
                  if (string_is_equal(menu->menu_state_msg,
                           msg_hash_to_str(
                              MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE)))
                  {
                     char current_sublabel[255];
                     get_current_menu_sublabel(
                           menu_st,
                           current_sublabel, sizeof(current_sublabel));
                     if (string_is_equal(current_sublabel, ""))
                        accessibility_speak_priority(p_rarch,
                              accessibility_enable,
                              accessibility_narrator_speech_speed,
                              menu->menu_state_msg, 10);
                     else
                        accessibility_speak_priority(p_rarch,
                              accessibility_enable,
                              accessibility_narrator_speech_speed,
                              current_sublabel, 10);
                  }
                  else
                     accessibility_speak_priority(p_rarch,
                           accessibility_enable,
                           accessibility_narrator_speech_speed,
                           menu->menu_state_msg, 10);
               }
#endif
            }
            else
            {
               enum msg_hash_enums enum_idx = MSG_UNKNOWN;
               size_t selection             = menu_st->selection_ptr;
               unsigned type                = selection_buf->list[selection].type;

               switch (type)
               {
                  case FILE_TYPE_FONT:
                  case FILE_TYPE_VIDEO_FONT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_FONT;
                     break;
                  case FILE_TYPE_RDB:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RDB;
                     break;
                  case FILE_TYPE_OVERLAY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY;
                     break;
#ifdef HAVE_VIDEO_LAYOUT
                  case FILE_TYPE_VIDEO_LAYOUT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_VIDEO_LAYOUT;
                     break;
#endif
                  case FILE_TYPE_CHEAT:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CHEAT;
                     break;
                  case FILE_TYPE_SHADER_PRESET:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET;
                     break;
                  case FILE_TYPE_SHADER:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_SHADER;
                     break;
                  case FILE_TYPE_REMAP:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_REMAP;
                     break;
                  case FILE_TYPE_RECORD_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG;
                     break;
                  case FILE_TYPE_CURSOR:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CURSOR;
                     break;
                  case FILE_TYPE_CONFIG:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_CONFIG;
                     break;
                  case FILE_TYPE_CARCHIVE:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE;
                     break;
                  case FILE_TYPE_DIRECTORY:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;
                     break;
                  case FILE_TYPE_VIDEOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_AUDIOFILTER:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_SLANG:           /* TODO/FIXME */
                  case FILE_TYPE_SHADER_GLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_HLSL:            /* TODO/FIXME */
                  case FILE_TYPE_SHADER_CG:              /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_GLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_HLSLP:    /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_CGP:      /* TODO/FIXME */
                  case FILE_TYPE_SHADER_PRESET_SLANGP:   /* TODO/FIXME */
                  case FILE_TYPE_PLAIN:
                     enum_idx = MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE;
                     break;
                  default:
                     break;
               }

               if (enum_idx != MSG_UNKNOWN)
                  ret = msg_hash_get_help_enum(enum_idx,
                        menu->menu_state_msg, sizeof(menu->menu_state_msg));
               else
               {
                  strlcpy(menu->menu_state_msg,
                        msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE),
                        sizeof(menu->menu_state_msg));

                  ret = 0;
               }
            }
         }
         BIT64_SET(menu->state, MENU_STATE_RENDER_MESSAGEBOX);
         BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);
         if (action == MENU_ACTION_OK || action == MENU_ACTION_CANCEL)
         {
            BIT64_SET(menu->state, MENU_STATE_POP_STACK);
         }
         break;
      case ITERATE_TYPE_DEFAULT:
         {
            menu_entry_t entry;
            menu_list_t *menu_list = menu_st->entries.list;
            size_t selection       = menu_st->selection_ptr;
            size_t menu_list_size  = menu_st->entries.list ? MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size : 0;
            /* FIXME: Crappy hack, needed for mouse controls
             * to not be completely broken in case we press back.
             *
             * We need to fix this entire mess, mouse controls
             * should not rely on a hack like this in order to work. */
            selection = MAX(MIN(selection, (menu_list_size - 1)), 0);

            MENU_ENTRY_INIT(entry);
            /* NOTE: If menu_entry_action() is modified,
             * will have to verify that these parameters
             * remain unused... */
            entry.rich_label_enabled = false;
            entry.value_enabled      = false;
            entry.sublabel_enabled   = false;
            menu_entry_get(&entry, 0, selection, NULL, false);
            ret                      = menu_entry_action(&entry,
                  selection, (enum menu_action)action);
            if (ret)
               return -1;

            BIT64_SET(menu->state, MENU_STATE_POST_ITERATE);

            /* Have to defer it so we let settings refresh. */
            if (menu_st->dialog_st.pending_push)
            {
               const char *label;
               menu_displaylist_info_t info;

               menu_displaylist_info_init(&info);

               info.list                 = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
               info.enum_idx             = MENU_ENUM_LABEL_HELP;

               /* Set the label string, if it exists. */
               label                     = msg_hash_to_str(MENU_ENUM_LABEL_HELP);
               if (label)
                  info.label             = strdup(label);

               menu_displaylist_ctl(DISPLAYLIST_HELP, &info, settings);
            }
         }
         break;
   }

#ifdef HAVE_ACCESSIBILITY
   if ((last_iterate_type == ITERATE_TYPE_HELP
            || last_iterate_type == ITERATE_TYPE_INFO)
         && last_iterate_type != iterate_type
         && is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
      accessibility_speak_priority(p_rarch,
            accessibility_enable,
            accessibility_narrator_speech_speed,
            "Closed dialog.", 10);

   last_iterate_type = iterate_type;
#endif

   BIT64_SET(menu->state, MENU_STATE_BLIT);

   if (BIT64_GET(menu->state, MENU_STATE_POP_STACK))
   {
      size_t selection         = menu_st->selection_ptr;
      size_t new_selection_ptr = selection;
      menu_entries_pop_stack(&new_selection_ptr, 0, 0);
      menu_st->selection_ptr   = selection;
   }

   if (BIT64_GET(menu->state, MENU_STATE_POST_ITERATE))
   {
      menu_input_t     *menu_input  = &menu_st->input_state;
      /* If pointer devices are disabled, just ensure mouse
       * cursor is hidden */
      if (menu_input->pointer.type == MENU_POINTER_DISABLED)
         ret = 0;
      else
         ret = menu_input_post_iterate(p_disp, menu_st, action,
               current_time);
      menu_input_set_pointer_visibility(
            &menu_st->input_pointer_hw_state,
             menu_input, current_time);
   }

   if (ret)
      return -1;
   return 0;
}

int generic_menu_entry_action(
      void *userdata, menu_entry_t *entry, size_t i, enum menu_action action)
{
   int ret                        = 0;
#ifdef HAVE_ACCESSIBILITY
   struct rarch_state *p_rarch    = &rarch_st;
#endif
   struct menu_state *menu_st     = menu_state_get_ptr();
   const menu_ctx_driver_t
      *menu_driver_ctx            = menu_st->driver_ctx;
   menu_handle_t  *menu           = menu_st->driver_data;
   settings_t   *settings         = config_get_ptr();
   void *menu_userdata            = menu_st->userdata;
   bool wraparound_enable         = settings->bools.menu_navigation_wraparound_enable;
   size_t scroll_accel            = menu_st->scroll.acceleration;
   menu_list_t *menu_list         = menu_st->entries.list;
   file_list_t *selection_buf     = menu_list ? MENU_LIST_GET_SELECTION(menu_list, (unsigned)0) : NULL;
   file_list_t *menu_stack        = menu_list ? MENU_LIST_GET(menu_list, (unsigned)0) : NULL;
   size_t selection_buf_size      = selection_buf ? selection_buf->size : 0;
   menu_file_list_cbs_t *cbs      = selection_buf ?
      (menu_file_list_cbs_t*)selection_buf->list[i].actiondata : NULL;
#ifdef HAVE_ACCESSIBILITY
   bool accessibility_enable      = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif

   switch (action)
   {
      case MENU_ACTION_UP:
         if (selection_buf_size > 0)
         {
            unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
            if (!(menu_st->selection_ptr == 0 && !wraparound_enable))
            {
               size_t idx             = 0;
               if (menu_st->selection_ptr >= scroll_speed)
                  idx = menu_st->selection_ptr - scroll_speed;
               else
               {
                  idx  = selection_buf_size - 1;
                  if (!wraparound_enable)
                     idx = 0;
               }

               menu_st->selection_ptr = idx;
               menu_driver_navigation_set(true);

               if (menu_driver_ctx->navigation_decrement)
                  menu_driver_ctx->navigation_decrement(menu_userdata);
            }
         }
         break;
      case MENU_ACTION_DOWN:
         if (selection_buf_size > 0)
         {
            unsigned scroll_speed  = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
            if (!(menu_st->selection_ptr >= selection_buf_size - 1
                  && !wraparound_enable))
            {
               if ((menu_st->selection_ptr + scroll_speed) < selection_buf_size)
               {
                  size_t idx  = menu_st->selection_ptr + scroll_speed;

                  menu_st->selection_ptr = idx;
                  menu_driver_navigation_set(true);
               }
               else
               {
                  if (wraparound_enable)
                  {
                     bool pending_push = false;
                     menu_driver_ctl(MENU_NAVIGATION_CTL_CLEAR, &pending_push);
                  }
                  else
                     menu_driver_ctl(MENU_NAVIGATION_CTL_SET_LAST,  NULL);
               }

               if (menu_driver_ctx->navigation_increment)
                  menu_driver_ctx->navigation_increment(menu_userdata);
            }
         }
         break;
      case MENU_ACTION_SCROLL_UP:
         if (
                  menu_st->scroll.index_size
               && menu_st->selection_ptr != 0
            )
         {
            size_t l   = menu_st->scroll.index_size - 1;

            while (l
                  && menu_st->scroll.index_list[l - 1]
                  >= menu_st->selection_ptr)
               l--;

            if (l > 0)
               menu_st->selection_ptr = menu_st->scroll.index_list[l - 1];

            if (menu_driver_ctx->navigation_descend_alphabet)
               menu_driver_ctx->navigation_descend_alphabet(
                     menu_userdata, &menu_st->selection_ptr);
         }
         break;
      case MENU_ACTION_SCROLL_DOWN:
         if (menu_st->scroll.index_size)
         {
            if (menu_st->selection_ptr == menu_st->scroll.index_list[menu_st->scroll.index_size - 1])
               menu_st->selection_ptr = selection_buf_size - 1;
            else
            {
               size_t l               = 0;
               while (l < menu_st->scroll.index_size - 1
                     && menu_st->scroll.index_list[l + 1] <= menu_st->selection_ptr)
                  l++;
               menu_st->selection_ptr = menu_st->scroll.index_list[l + 1];

               if (menu_st->selection_ptr >= selection_buf_size)
                  menu_st->selection_ptr = selection_buf_size - 1;
            }

            if (menu_driver_ctx->navigation_ascend_alphabet)
               menu_driver_ctx->navigation_ascend_alphabet(
                     menu_userdata, &menu_st->selection_ptr);
         }
         break;
      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path,
                  entry->label, entry->type, i);
         break;
      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path,
                  entry->label, entry->type, i, entry->entry_idx);
         break;
      case MENU_ACTION_SEARCH:
         menu_input_dialog_start_search();
         break;
      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path,
                  entry->label, entry->type, i);
         break;
      default:
         break;
   }

   if (MENU_ENTRIES_NEEDS_REFRESH(menu_st))
   {
      bool refresh            = false;
      menu_driver_displaylist_push(
            menu_st,
            settings,
            selection_buf,
            menu_stack);
      menu_entries_ctl(MENU_ENTRIES_CTL_UNSET_REFRESH, &refresh);
   }

#ifdef HAVE_ACCESSIBILITY
   if (     action != 0
         && is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled)
         && !menu_input_dialog_get_display_kb())
   {
      char current_label[128];
      char current_value[128];
      char title_name[255];
      char speak_string[512];

      speak_string[0]  = '\0';
      title_name  [0]  = '\0';
      current_label[0] = '\0';

      get_current_menu_value(menu_st,
            current_value, sizeof(current_value));

      switch (action)
      {
         case MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE:
            menu_entries_get_title(title_name, sizeof(title_name));
            break;
         case MENU_ACTION_START:
            /* if equal to '..' we break, else we fall-through */
            if (string_is_equal(current_value, "..."))
               break;
            /* fall-through */
         case MENU_ACTION_ACCESSIBILITY_SPEAK_TITLE_LABEL:
         case MENU_ACTION_OK:
         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
         case MENU_ACTION_CANCEL:
            menu_entries_get_title(title_name, sizeof(title_name));
            get_current_menu_label(menu_st, current_label, sizeof(current_label));
            break;
         case MENU_ACTION_UP:
         case MENU_ACTION_DOWN:
         case MENU_ACTION_SCROLL_UP:
         case MENU_ACTION_SCROLL_DOWN:
         case MENU_ACTION_SELECT:
         case MENU_ACTION_SEARCH:
         case MENU_ACTION_ACCESSIBILITY_SPEAK_LABEL:
            get_current_menu_label(menu_st, current_label, sizeof(current_label));
            break;
         case MENU_ACTION_SCAN:
         case MENU_ACTION_INFO:
         default:
            break;
      }

      if (!string_is_empty(title_name))
      {
         if (!string_is_equal(current_value, "..."))
            snprintf(speak_string, sizeof(speak_string),
                  "%s %s %s", title_name, current_label, current_value); 
         else
            snprintf(speak_string, sizeof(speak_string),
                  "%s %s", title_name, current_label); 
      }
      else
      {
         if (!string_is_equal(current_value, "..."))
            snprintf(speak_string, sizeof(speak_string),
                  "%s %s", current_label, current_value);
         else
            strlcpy(speak_string, current_label, sizeof(speak_string));
      }

      if (!string_is_empty(speak_string))
         accessibility_speak_priority(p_rarch,
               accessibility_enable,
               accessibility_narrator_speech_speed,
               speak_string, 10);
   }
#endif

   if (menu_st->pending_close_content)
   {
      const char *content_path  = path_get(RARCH_PATH_CONTENT);
      const char *menu_flush_to = msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);

      /* Flush to playlist entry menu if launched via playlist */
      if (menu &&
          !string_is_empty(menu->deferred_path) &&
          !string_is_empty(content_path) &&
          string_is_equal(menu->deferred_path, content_path))
         menu_flush_to = msg_hash_to_str(MENU_ENUM_LABEL_DEFERRED_RPL_ENTRY_ACTIONS);

      command_event(CMD_EVENT_UNLOAD_CORE, NULL);
      menu_entries_flush_stack(menu_flush_to, 0);
      menu_driver_ctl(RARCH_MENU_CTL_UNSET_PREVENT_POPULATE, NULL);
      menu_st->selection_ptr         = 0;
      menu_st->pending_close_content = false;
   }

   return ret;
}

/* Iterate the menu driver for one frame. */
static bool menu_driver_iterate(
      struct rarch_state *p_rarch,
      struct menu_state *menu_st,
      gfx_display_t *p_disp,
      gfx_animation_t *p_anim,
      settings_t *settings,
      enum menu_action action,
      retro_time_t current_time)
{
   return (menu_st->driver_data &&
         generic_menu_iterate(
            p_rarch,
            menu_st,
            p_disp,
            p_anim,
            settings,
            menu_st->driver_data,
            menu_st->userdata, action,
            current_time) != -1);
}
#endif

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
   else if (string_is_equal(label, "bluetooth_driver"))
   {
      if (bluetooth_drivers[i])
      {
         const char *ident = bluetooth_drivers[i]->ident;

         strlcpy(s, ident, len);
         return bluetooth_drivers[i];
      }
   }
   else if (string_is_equal(label, "wifi_driver"))
   {
      if (wifi_drivers[i])
      {
         const char *ident = wifi_drivers[i]->ident;

         strlcpy(s, ident, len);
         return wifi_drivers[i];
      }
   }

   return NULL;
}

#ifdef HAVE_DISCORD
bool discord_is_ready(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;
   return discord_st->ready;
}

static char *discord_get_own_username(struct rarch_state *p_rarch)
{
   discord_state_t *discord_st = &p_rarch->discord_st;

   if (discord_st->ready)
      return discord_st->user_name;
   return NULL;
}

char *discord_get_own_avatar(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;
   if (discord_st->ready)
      return discord_st->user_avatar;
   return NULL;
}

bool discord_avatar_is_ready(void)
{
   return false;
}

void discord_avatar_set_ready(bool ready)
{
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;
   discord_st->avatar_ready    = ready;
}

#ifdef HAVE_MENU
static bool discord_download_avatar(
      discord_state_t *discord_st,
      const char* user_id, const char* avatar_id)
{
   static char url[PATH_MAX_LENGTH];
   static char url_encoded[PATH_MAX_LENGTH];
   static char full_path[PATH_MAX_LENGTH];
   static char buf[PATH_MAX_LENGTH];
   file_transfer_t     *transf = NULL;

   RARCH_LOG("[DISCORD]: User avatar ID: %s\n", user_id);

   fill_pathname_application_special(buf,
            sizeof(buf),
            APPLICATION_SPECIAL_DIRECTORY_THUMBNAILS_DISCORD_AVATARS);
   fill_pathname_join(full_path, buf, avatar_id, sizeof(full_path));
   strlcpy(discord_st->user_avatar,
         avatar_id, sizeof(discord_st->user_avatar));

   if (path_is_valid(full_path))
      return true;

   if (string_is_empty(avatar_id))
      return false;

   snprintf(url, sizeof(url), "%s/%s/%s" FILE_PATH_PNG_EXTENSION, CDN_URL, user_id, avatar_id);
   net_http_urlencode_full(url_encoded, url, sizeof(url_encoded));
   snprintf(buf, sizeof(buf), "%s" FILE_PATH_PNG_EXTENSION, avatar_id);

   transf           = (file_transfer_t*)malloc(sizeof(*transf));

   transf->enum_idx  = MENU_ENUM_LABEL_CB_DISCORD_AVATAR;
   strlcpy(transf->path, buf, sizeof(transf->path));
   transf->user_data = NULL;

   RARCH_LOG("[DISCORD]: Downloading avatar from: %s\n", url_encoded);
   task_push_http_transfer_file(url_encoded, true, NULL, cb_generic_download, transf);

   return false;
}
#endif

static void handle_discord_ready(const DiscordUser* connectedUser)
{
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;

   strlcpy(discord_st->user_name,
         connectedUser->username, sizeof(discord_st->user_name));

   RARCH_LOG("[DISCORD]: Connected to user: %s#%s\n",
      connectedUser->username,
      connectedUser->discriminator);

#ifdef HAVE_MENU
   discord_download_avatar(discord_st,
         connectedUser->userId, connectedUser->avatar);
#endif
}

static void handle_discord_disconnected(int errcode, const char* message)
{
   RARCH_LOG("[DISCORD]: Disconnected (%d: %s)\n", errcode, message);
}

static void handle_discord_error(int errcode, const char* message)
{
   RARCH_LOG("[DISCORD]: Error (%d: %s)\n", errcode, message);
}

static void handle_discord_join_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   char join_hostname[PATH_MAX_LENGTH];
   struct netplay_room *room         = NULL;
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   struct rarch_state *p_rarch       = &rarch_st;
   discord_state_t *discord_st       = &p_rarch->discord_st;

   if (!data || err || !data->data)
      goto finish;

   data->data                        = (char*)realloc(data->data, data->len + 1);
   data->data[data->len]             = '\0';

   netplay_rooms_parse(data->data);
   room                              = netplay_room_get(0);

   if (room)
   {
      bool host_method_is_mitm = room->host_method == NETPLAY_HOST_METHOD_MITM;
      const char *srv_address  = host_method_is_mitm ? room->mitm_address : room->address;
      unsigned srv_port        = host_method_is_mitm ? room->mitm_port : room->port;

      if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
         deinit_netplay(p_rarch);
      netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

      snprintf(join_hostname, sizeof(join_hostname), "%s|%d",
            srv_address, srv_port);

      RARCH_LOG("[DISCORD]: Joining lobby at: %s\n", join_hostname);
      task_push_netplay_crc_scan(room->gamecrc,
         room->gamename, join_hostname, room->corename, room->subsystem_name);
      discord_st->connecting = true;
      if (discord_st->ready)
         discord_update(DISCORD_PRESENCE_NETPLAY_CLIENT);
   }

finish:

   if (err)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), err);

   if (user_data)
      free(user_data);
}

static void handle_discord_join(const char* secret)
{
   char url[2048];
   struct string_list    *list = string_split(secret, "|");
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;

   strlcpy(discord_st->peer_party_id,
         list->elems[0].data, sizeof(discord_st->peer_party_id));
   snprintf(url, sizeof(url), FILE_PATH_LOBBY_LIBRETRO_URL "%s/",
         discord_st->peer_party_id);

   RARCH_LOG("[DISCORD]: Querying lobby id: %s at %s\n",
         discord_st->peer_party_id, url);
   task_push_http_transfer(url, true, NULL, handle_discord_join_cb, NULL);
}

static void handle_discord_spectate(const char* secret)
{
   RARCH_LOG("[DISCORD]: Spectate (%s)\n", secret);
}

#ifdef HAVE_MENU
#if 0
static void handle_discord_join_response(void *ignore, const char *line)
{
   /* TODO/FIXME: needs in-game widgets */
   if (strstr(line, "yes"))
      Discord_Respond(user_id, DISCORD_REPLY_YES);

#ifdef HAVE_MENU
   menu_input_dialog_end();
   retroarch_menu_running_finished(false);
#endif
}
#endif
#endif

static void handle_discord_join_request(const DiscordUser* request)
{
#ifdef HAVE_MENU
#if 0
   char buf[PATH_MAX_LENGTH];
   menu_input_ctx_line_t line;
#endif
   struct rarch_state *p_rarch = &rarch_st;

   RARCH_LOG("[DISCORD]: Join request from %s#%s - %s %s\n",
      request->username,
      request->discriminator,
      request->userId,
      request->avatar);

   discord_download_avatar(&p_rarch->discord_st,
         request->userId, request->avatar);

#if 0
   /* TODO/FIXME: Needs in-game widgets */
   retroarch_menu_running();

   memset(&line, 0, sizeof(line));
   snprintf(buf, sizeof(buf), "%s %s?",
         msg_hash_to_str(MSG_DISCORD_CONNECTION_REQUEST), request->username);
   line.label         = buf;
   line.label_setting = "no_setting";
   line.cb            = handle_discord_join_response;

   /* TODO/FIXME: needs in-game widgets
    * TODO/FIXME: bespoke dialog, should show while in-game
    * and have a hotkey to accept
    * TODO/FIXME: show avatar of the user connecting
    */
   if (!menu_input_dialog_start(&line))
      return;
#endif
#endif
}

void discord_update(enum discord_presence presence)
{
   struct rarch_state *p_rarch = &rarch_st;
   discord_state_t *discord_st = &p_rarch->discord_st;
#ifdef HAVE_CHEEVOS
   char cheevos_richpresence[256];
#endif

   if (presence == discord_st->status)
      return;

   if (!discord_st->connecting
         &&
         (   presence == DISCORD_PRESENCE_NONE
          || presence == DISCORD_PRESENCE_MENU))
   {
      memset(&discord_st->presence,
            0, sizeof(discord_st->presence));
      discord_st->peer_party_id[0] = '\0';
   }

   switch (presence)
   {
      case DISCORD_PRESENCE_MENU:
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU);
         discord_st->presence.largeImageKey  = "base";
         discord_st->presence.largeImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_NO_CORE);
         discord_st->presence.instance       = 0;
         break;
      case DISCORD_PRESENCE_GAME_PAUSED:
         discord_st->presence.smallImageKey  = "paused";
         discord_st->presence.smallImageText = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED);
         discord_st->presence.details        = msg_hash_to_str(
               MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED);
         discord_st->pause_time              = time(0);
         discord_st->elapsed_time            = difftime(discord_st->pause_time,
               discord_st->start_time);
         discord_st->presence.startTimestamp = discord_st->pause_time;
         break;
      case DISCORD_PRESENCE_GAME:
         {
            core_info_t      *core_info     = NULL;
            core_info_get_current_core(&core_info);

            if (core_info)
            {
               const char *system_id               =
                    core_info->system_id
                  ? core_info->system_id
                  : "core";
               const char *label                   = NULL;
               const struct playlist_entry *entry  = NULL;
               playlist_t *current_playlist        = playlist_get_cached();

               if (current_playlist)
               {
                  playlist_get_index_by_path(
                        current_playlist,
                        path_get(RARCH_PATH_CONTENT),
                        &entry);

                  if (entry && !string_is_empty(entry->label))
                     label = entry->label;
               }

               if (!label)
                  label = path_basename(path_get(RARCH_PATH_BASENAME));
               discord_st->presence.largeImageKey = system_id;

               if (core_info->display_name)
                  discord_st->presence.largeImageText =
                     core_info->display_name;

               discord_st->start_time              = time(0);
               if (discord_st->pause_time != 0)
                  discord_st->start_time           = time(0) -
                     discord_st->elapsed_time;

               discord_st->pause_time              = 0;
               discord_st->elapsed_time            = 0;

               discord_st->presence.smallImageKey  = "playing";
               discord_st->presence.smallImageText = msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING);
               discord_st->presence.startTimestamp = discord_st->start_time;

#ifdef HAVE_CHEEVOS
               if (rcheevos_get_richpresence(cheevos_richpresence, sizeof(cheevos_richpresence)) > 0)
                  discord_st->presence.details     = cheevos_richpresence;
               else
#endif
                   discord_st->presence.details    = msg_hash_to_str(
                     MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME);

               discord_st->presence.state          = label;
               discord_st->presence.instance       = 0;

               if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
               {
                  discord_st->peer_party_id[0]     = '\0';
                  discord_st->connecting           = false;
                  discord_st->presence.partyId     = NULL;
                  discord_st->presence.partyMax    = 0;
                  discord_st->presence.partySize   = 0;
                  discord_st->presence.joinSecret  = (const char*)'\0';
               }
            }
         }
         break;
      case DISCORD_PRESENCE_NETPLAY_HOSTING:
         {
            char join_secret[128];
            struct netplay_room *room = &p_rarch->netplay_host_room;
            bool host_method_is_mitm  = room->host_method == NETPLAY_HOST_METHOD_MITM;
            const char *srv_address   = host_method_is_mitm ? room->mitm_address : room->address;
            unsigned srv_port         = host_method_is_mitm ? room->mitm_port : room->port;
            if (room->id == 0)
               return;

            RARCH_LOG("[DISCORD]: Netplay room details: ID=%d"
                  ", Nick=%s IP=%s Port=%d\n",
                  room->id, room->nickname,
                  srv_address, srv_port);

            snprintf(discord_st->self_party_id,
                  sizeof(discord_st->self_party_id), "%d", room->id);
            snprintf(join_secret,
                  sizeof(join_secret), "%d|%" PRId64,
                  room->id, cpu_features_get_time_usec());

            discord_st->presence.joinSecret     = strdup(join_secret);
#if 0
            discord_st->presence.spectateSecret = "SPECSPECSPEC";
#endif
            discord_st->presence.partyId        = strdup(discord_st->self_party_id);
            discord_st->presence.partyMax       = 2;
            discord_st->presence.partySize      = 1;

            RARCH_LOG("[DISCORD]: Join secret: %s\n", join_secret);
            RARCH_LOG("[DISCORD]: Party ID: %s\n", discord_st->self_party_id);
         }
         break;
      case DISCORD_PRESENCE_NETPLAY_CLIENT:
         RARCH_LOG("[DISCORD]: Party ID: %s\n", discord_st->peer_party_id);
         discord_st->presence.partyId    = strdup(discord_st->peer_party_id);
         break;
      case DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED:
         {
            if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
            !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            {
               discord_st->peer_party_id[0]     = '\0';
               discord_st->connecting           = false;
               discord_st->presence.partyId     = NULL;
               discord_st->presence.partyMax    = 0;
               discord_st->presence.partySize   = 0;
               discord_st->presence.joinSecret  = (const char*)'\0';
            }
         }
         break;
#ifdef HAVE_CHEEVOS
      case DISCORD_PRESENCE_RETROACHIEVEMENTS:
         if (discord_st->pause_time)
            return;

         if (rcheevos_get_richpresence(cheevos_richpresence, sizeof(cheevos_richpresence)) > 0)
            discord_st->presence.details = cheevos_richpresence;
         presence = DISCORD_PRESENCE_GAME;
         break;
#endif
      case DISCORD_PRESENCE_SHUTDOWN:
            discord_st->presence.partyId    = NULL;
            discord_st->presence.partyMax   = 0;
            discord_st->presence.partySize  = 0;
            discord_st->presence.joinSecret = (const char*)'\0';
            discord_st->connecting          = false;
      default:
         break;
   }

#ifdef DEBUG
   RARCH_LOG("[DISCORD]: Updating (%d)\n", presence);
#endif

   Discord_UpdatePresence(&discord_st->presence);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif
   discord_st->status = presence;
}

static void discord_init(
      discord_state_t *discord_st,
      const char *discord_app_id, char *args)
{
   DiscordEventHandlers handlers;
#ifdef _WIN32
   char full_path[PATH_MAX_LENGTH];
#endif
   char command[PATH_MAX_LENGTH];

   discord_st->start_time      = time(0);

   handlers.ready              = handle_discord_ready;
   handlers.disconnected       = handle_discord_disconnected;
   handlers.errored            = handle_discord_error;
   handlers.joinGame           = handle_discord_join;
   handlers.spectateGame       = handle_discord_spectate;
   handlers.joinRequest        = handle_discord_join_request;

   Discord_Initialize(discord_app_id, &handlers, 0, NULL);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif

#ifdef _WIN32
   fill_pathname_application_path(full_path, sizeof(full_path));
   if (strstr(args, full_path))
      strlcpy(command, args, sizeof(command));
   else
   {
      path_basedir(full_path);
      strlcpy(command, full_path, sizeof(command));
      strlcat(command, args,      sizeof(command));
   }
#else
   strcpy_literal(command, "sh -c ");
   strlcat(command, args,     sizeof(command));
#endif
   RARCH_LOG("[DISCORD]: Registering startup command: %s\n", command);
   Discord_Register(discord_app_id, command);
#ifdef DISCORD_DISABLE_IO_THREAD
   Discord_UpdateConnection();
#endif
   discord_st->ready = true;
}
#endif

#ifdef HAVE_NETWORKING
static bool init_netplay_deferred(
      struct rarch_state *p_rarch,
      const char* server, unsigned port)
{
   if (!string_is_empty(server) && port != 0)
   {
      strlcpy(p_rarch->server_address_deferred, server,
            sizeof(p_rarch->server_address_deferred));
      p_rarch->server_port_deferred    = port;
      p_rarch->netplay_client_deferred = true;
   }
   else
      p_rarch->netplay_client_deferred = false;

   return p_rarch->netplay_client_deferred;
}

/**
 * input_poll_net
 *
 * Poll the network if necessary.
 */
void input_poll_net(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   netplay_t          *netplay = p_rarch->netplay_data;
   if (!netplay_should_skip(netplay) && netplay && netplay->can_poll)
   {
      input_driver_state_t 
         *input_st      = input_state_get_ptr();
      netplay->can_poll = false;
      netplay_poll(
            input_st->block_libretro_input,
            config_get_ptr(),
            netplay);
   }
}

/* Netplay polling callbacks */
static void video_frame_net(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   struct rarch_state *p_rarch = &rarch_st;
   netplay_t          *netplay = p_rarch->netplay_data;
   if (!netplay_should_skip(netplay))
      netplay->cbs.frame_cb(data, width, height, pitch);
}

static void audio_sample_net(int16_t left, int16_t right)
{
   struct rarch_state *p_rarch = &rarch_st;
   netplay_t          *netplay = p_rarch->netplay_data;
   if (!netplay_should_skip(netplay) && !netplay->stall)
      netplay->cbs.sample_cb(left, right);
}

static size_t audio_sample_batch_net(const int16_t *data, size_t frames)
{
   struct rarch_state *p_rarch = &rarch_st;
   netplay_t          *netplay = p_rarch->netplay_data;
   if (!netplay_should_skip(netplay) && !netplay->stall)
      return netplay->cbs.sample_batch_cb(data, frames);
   return frames;
}

static void netplay_announce_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   if (task_data)
   {
      unsigned i, ip_len, port_len;
      struct rarch_state *p_rarch    = &rarch_st;
      http_transfer_data_t *data     = (http_transfer_data_t*)task_data;
      struct netplay_room *host_room = &p_rarch->netplay_host_room;
      struct string_list *lines      = NULL;
      char *mitm_ip                  = NULL;
      char *mitm_port                = NULL;
      char *buf                      = NULL;
      char *host_string              = NULL;

      if (data->len == 0)
         return;

      buf = (char*)calloc(1, data->len + 1);

      memcpy(buf, data->data, data->len);

      lines = string_split(buf, "\n");

      if (lines->size == 0)
      {
         string_list_free(lines);
         free(buf);
         return;
      }

      memset(host_room, 0, sizeof(*host_room));

      for (i = 0; i < lines->size; i++)
      {
         const char *line = lines->elems[i].data;

         if (!string_is_empty(line))
         {
            struct string_list *kv = string_split(line, "=");
            const char *key = NULL;
            const char *val = NULL;

            if (!kv)
               continue;

            if (kv->size != 2)
            {
               string_list_free(kv);
               continue;
            }

            key = kv->elems[0].data;
            val = kv->elems[1].data;

            if (string_is_equal(key, "id"))
               sscanf(val, "%i", &host_room->id);
            if (string_is_equal(key, "username"))
               strlcpy(host_room->nickname, val, sizeof(host_room->nickname));
            if (string_is_equal(key, "ip"))
               strlcpy(host_room->address, val, sizeof(host_room->address));
            if (string_is_equal(key, "mitm_ip"))
            {
               mitm_ip = strdup(val);
               strlcpy(host_room->mitm_address, val, sizeof(host_room->mitm_address));
            }
            if (string_is_equal(key, "port"))
               sscanf(val, "%i", &host_room->port);
            if (string_is_equal(key, "mitm_port"))
            {
               mitm_port = strdup(val);
               sscanf(mitm_port, "%i", &host_room->mitm_port);
            }
            if (string_is_equal(key, "core_name"))
               strlcpy(host_room->corename, val, sizeof(host_room->corename));
            if (string_is_equal(key, "frontend"))
               strlcpy(host_room->frontend, val, sizeof(host_room->frontend));
            if (string_is_equal(key, "core_version"))
               strlcpy(host_room->coreversion, val, sizeof(host_room->coreversion));
            if (string_is_equal(key, "game_name"))
               strlcpy(host_room->gamename, val, sizeof(host_room->gamename));
            if (string_is_equal(key, "game_crc"))
               sscanf(val, "%08d", &host_room->gamecrc);
            if (string_is_equal(key, "host_method"))
               sscanf(val, "%i", &host_room->host_method);
            if (string_is_equal(key, "has_password"))
            {
               if (string_is_equal_noncase(val, "true") || string_is_equal(val, "1"))
                  host_room->has_password = true;
               else
                  host_room->has_password = false;
            }
            if (string_is_equal(key, "has_spectate_password"))
            {
               if (string_is_equal_noncase(val, "true") || string_is_equal(val, "1"))
                  host_room->has_spectate_password = true;
               else
                  host_room->has_spectate_password = false;
            }
            if (string_is_equal(key, "fixed"))
            {
               if (string_is_equal_noncase(val, "true") || string_is_equal(val, "1"))
                  host_room->fixed = true;
               else
                  host_room->fixed = false;
            }
            if (string_is_equal(key, "retroarch_version"))
               strlcpy(host_room->retroarch_version, val, sizeof(host_room->retroarch_version));
            if (string_is_equal(key, "country"))
               strlcpy(host_room->country, val, sizeof(host_room->country));

            string_list_free(kv);
         }
      }

      if (mitm_ip && mitm_port)
      {
         ip_len   = (unsigned)strlen(mitm_ip);
         port_len = (unsigned)strlen(mitm_port);

         /* Enable Netplay client mode */
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
         {
            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
            p_rarch->is_mitm       = true;
            host_room->host_method = NETPLAY_HOST_METHOD_MITM;
         }

         netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);

         host_string = (char*)calloc(1, ip_len + port_len + 2);

         memcpy(host_string, mitm_ip, ip_len);
         memcpy(host_string + ip_len, "|", 1);
         memcpy(host_string + ip_len + 1, mitm_port, port_len);

         /* Enable Netplay */
         command_event(CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED, (void*)host_string);
         command_event(CMD_EVENT_NETPLAY_INIT, (void*)host_string);

         free(host_string);
      }

#ifdef HAVE_DISCORD
      if (discord_is_inited)
      {
         discord_userdata_t userdata;
         userdata.status = DISCORD_PRESENCE_NETPLAY_HOSTING;
         command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
      }
#endif

      string_list_free(lines);
      free(buf);

      if (mitm_ip)
         free(mitm_ip);
      if (mitm_port)
         free(mitm_port);
   }
}

static void netplay_announce(struct rarch_state *p_rarch)
{
   char buf[4600];
   char frontend_architecture[PATH_MAX_LENGTH];
   char frontend_architecture_tmp[32];
   const frontend_ctx_driver_t
      *frontend_drv                 =  NULL;
   char url[2048]                   = "http://lobby.libretro.com/add/";
   char *username                   = NULL;
   char *corename                   = NULL;
   char *gamename                   = NULL;
   char *subsystemname              = NULL;
   char *coreversion                = NULL;
   char *frontend_ident             = NULL;
   settings_t *settings             = config_get_ptr();
   runloop_state_t *runloop_st      = &runloop_state;
   struct retro_system_info *system = &runloop_st->system.info;
   uint32_t content_crc             = content_get_crc();
   struct string_list *subsystem    = path_get_subsystem_list();

   frontend_architecture[0]         = '\0';
   buf[0]                           = '\0';

   if (subsystem)
   {
      unsigned i;

      for (i = 0; i < subsystem->size; i++)
      {
         strlcat(buf, path_basename(subsystem->elems[i].data), sizeof(buf));
         if (i < subsystem->size - 1)
            strlcat(buf, "|", sizeof(buf));
      }
      net_http_urlencode(&gamename, buf);
      net_http_urlencode(&subsystemname, path_get(RARCH_PATH_SUBSYSTEM));
      content_crc = 0;
   }
   else
   {
      const char *base = path_basename(path_get(RARCH_PATH_BASENAME));

      net_http_urlencode(&gamename,
         !string_is_empty(base) ? base : "N/A");
      /* TODO/FIXME - subsystem should be implemented later? */
      net_http_urlencode(&subsystemname, "N/A");
   }

   frontend_drv =
      (const frontend_ctx_driver_t*)frontend_driver_get_cpu_architecture_str(
            frontend_architecture_tmp, sizeof(frontend_architecture_tmp));
   snprintf(frontend_architecture, 
         sizeof(frontend_architecture),
         "%s %s",
         frontend_drv->ident,
         frontend_architecture_tmp);

#ifdef HAVE_DISCORD
   if (discord_is_ready())
      net_http_urlencode(&username, discord_get_own_username(p_rarch));
   else
#endif
   net_http_urlencode(&username, settings->paths.username);
   net_http_urlencode(&corename, system->library_name);
   net_http_urlencode(&coreversion, system->library_version);
   net_http_urlencode(&frontend_ident, frontend_architecture);

   buf[0] = '\0';

   snprintf(buf, sizeof(buf), "username=%s&core_name=%s&core_version=%s&"
      "game_name=%s&game_crc=%08lX&port=%d&mitm_server=%s"
      "&has_password=%d&has_spectate_password=%d&force_mitm=%d"
      "&retroarch_version=%s&frontend=%s&subsystem_name=%s",
      username, corename, coreversion, gamename, (unsigned long)content_crc,
      settings->uints.netplay_port,
      settings->arrays.netplay_mitm_server,
      *settings->paths.netplay_password ? 1 : 0,
      *settings->paths.netplay_spectate_password ? 1 : 0,
      settings->bools.netplay_use_mitm_server,
      PACKAGE_VERSION, frontend_architecture, subsystemname);
   task_push_http_post_transfer(url, buf, true, NULL,
         netplay_announce_cb, NULL);

   if (username)
      free(username);
   if (corename)
      free(corename);
   if (gamename)
      free(gamename);
   if (coreversion)
      free(coreversion);
   if (frontend_ident)
      free(frontend_ident);
}

static int16_t input_state_net(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   struct rarch_state *p_rarch = &rarch_st;
   netplay_t          *netplay = p_rarch->netplay_data;
   if (netplay)
   {
      if (netplay_is_alive(netplay))
         return netplay_input_state(netplay, port, device, idx, id);
      return netplay->cbs.state_cb(port, device, idx, id);
   }
   return 0;
}

/* ^^^ Netplay polling callbacks */

/**
 * netplay_disconnect
 * @netplay              : pointer to netplay object
 *
 * Disconnect netplay.
 *
 * Returns: true (1) if successful. At present, cannot fail.
 **/
static void netplay_disconnect(
      struct rarch_state *p_rarch,
      netplay_t *netplay)
{
   size_t i;

   for (i = 0; i < netplay->connections_size; i++)
      netplay_hangup(netplay, &netplay->connections[i]);

   deinit_netplay(p_rarch);

#ifdef HAVE_DISCORD
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
#endif
}

/**
 * netplay_pre_frame:
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 *
 * Returns: true (1) if the frontend is cleared to emulate the frame, false (0)
 * if we're stalled or paused
 **/
static bool netplay_pre_frame(
      struct rarch_state *p_rarch,
      bool netplay_public_announce,
      bool netplay_use_mitm_server,
      netplay_t *netplay)
{
   bool sync_stalled     = false;

   retro_assert(netplay);

   if (netplay_public_announce)
   {
      p_rarch->reannounce++;
      if (
            (netplay->is_server || p_rarch->is_mitm) &&
            (p_rarch->reannounce % 300 == 0))
         netplay_announce(p_rarch);
   }
   /* Make sure that if announcement is turned on mid-game, it gets announced */
   else
      p_rarch->reannounce = -1;

   /* FIXME: This is an ugly way to learn we're not paused anymore */
   if (netplay->local_paused)
      if (netplay->local_paused != false)
         netplay_frontend_paused(netplay, false);

   /* Are we ready now? */
   if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
      netplay_try_init_serialization(netplay);

   if (netplay->is_server && !netplay_use_mitm_server)
   {
#ifdef HAVE_NETPLAYDISCOVERY
      /* Advertise our server */
      netplay_lan_ad_server(netplay);
#endif

      /* NAT traversal if applicable */
      if (netplay->nat_traversal &&
          !netplay->nat_traversal_task_oustanding &&
          netplay->nat_traversal_state.request_outstanding &&
          !netplay->nat_traversal_state.have_inet4)
      {
         struct timeval tmptv = {0};
         fd_set fds = netplay->nat_traversal_state.fds;
         if (socket_select(netplay->nat_traversal_state.nfds, &fds, NULL, NULL, &tmptv) > 0)
            natt_read(&netplay->nat_traversal_state);

#ifndef HAVE_SOCKET_LEGACY
         if (!netplay->nat_traversal_state.request_outstanding ||
             netplay->nat_traversal_state.have_inet4)
            netplay_announce_nat_traversal(netplay);
#endif
      }
   }

   sync_stalled = !netplay_sync_pre_frame(netplay);

   /* If we're disconnected, deinitialize */
   if (!netplay->is_server && !netplay->connections[0].active)
   {
      netplay_disconnect(p_rarch, netplay);
      return true;
   }

   if (sync_stalled ||
       ((!netplay->is_server || (netplay->connected_players>1)) &&
        (netplay->stall || netplay->remote_paused)))
   {
      /* We may have received data even if we're stalled, so run post-frame
       * sync */
      netplay_sync_post_frame(netplay, true);
      return false;
   }
   return true;
}

static void deinit_netplay(struct rarch_state *p_rarch)
{
   if (p_rarch->netplay_data)
   {
      netplay_free(p_rarch->netplay_data);
      p_rarch->netplay_enabled   = false;
      p_rarch->netplay_is_client = false;
      p_rarch->is_mitm           = false;
   }
   p_rarch->netplay_data         = NULL;
   core_unset_netplay_callbacks();
}

/**
 * init_netplay
 * @direct_host          : Host to connect to directly, if applicable (client only)
 * @server               : server address to connect to (client only)
 * @port                 : TCP port to host on/connect to
 *
 * Initializes netplay.
 *
 * If netplay is already initialized, will return false (0).
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool init_netplay(
      struct rarch_state *p_rarch,
      settings_t *settings,
      void *direct_host,
      const char *server, unsigned port)
{
   struct retro_callbacks cbs    = {0};
   uint64_t serialization_quirks = 0;
   uint64_t quirks               = 0;
   bool _netplay_is_client       = p_rarch->netplay_is_client;
   bool _netplay_enabled         = p_rarch->netplay_enabled;

   if (!_netplay_enabled)
      return false;

   core_set_default_callbacks(&cbs);
   if (!core_set_netplay_callbacks())
      return false;

   /* Map the core's quirks to our quirks */
   serialization_quirks = core_serialization_quirks();

   /* Quirks we don't support! Just disable everything. */
   if (serialization_quirks & ~((uint64_t) NETPLAY_QUIRK_MAP_UNDERSTOOD))
      quirks |= NETPLAY_QUIRK_NO_SAVESTATES;

   if (serialization_quirks & NETPLAY_QUIRK_MAP_NO_SAVESTATES)
      quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_NO_TRANSMISSION)
      quirks |= NETPLAY_QUIRK_NO_TRANSMISSION;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_INITIALIZATION)
      quirks |= NETPLAY_QUIRK_INITIALIZATION;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_ENDIAN_DEPENDENT)
      quirks |= NETPLAY_QUIRK_ENDIAN_DEPENDENT;
   if (serialization_quirks & NETPLAY_QUIRK_MAP_PLATFORM_DEPENDENT)
      quirks |= NETPLAY_QUIRK_PLATFORM_DEPENDENT;

   if (_netplay_is_client)
   {
      RARCH_LOG("[Netplay]: %s\n", msg_hash_to_str(MSG_CONNECTING_TO_NETPLAY_HOST));
   }
   else
   {
      RARCH_LOG("[Netplay]: %s\n", msg_hash_to_str(MSG_WAITING_FOR_CLIENT));
      runloop_msg_queue_push(
         msg_hash_to_str(MSG_WAITING_FOR_CLIENT),
         0, 180, false,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

      if (settings->bools.netplay_public_announce)
         netplay_announce(p_rarch);
   }

   p_rarch->netplay_data = (netplay_t*)netplay_new(
         _netplay_is_client
         ? direct_host
         : NULL,
         _netplay_is_client
         ? (!p_rarch->netplay_client_deferred
            ? server
            : p_rarch->server_address_deferred)
            : NULL,
         _netplay_is_client ? (!p_rarch->netplay_client_deferred
            ? port
            : p_rarch->server_port_deferred)
            : (port != 0 ? port : RARCH_DEFAULT_PORT),
         settings->bools.netplay_stateless_mode,
         settings->ints.netplay_check_frames,
         &cbs,
         settings->bools.netplay_nat_traversal && !settings->bools.netplay_use_mitm_server,
#ifdef HAVE_DISCORD
         discord_get_own_username(p_rarch)
         ? discord_get_own_username(p_rarch)
         :
#endif
         settings->paths.username,
         quirks);

   if (p_rarch->netplay_data)
   {
      if (      p_rarch->netplay_data->is_server
            && !settings->bools.netplay_start_as_spectator)
         netplay_toggle_play_spectate(p_rarch->netplay_data);
      return true;
   }

   RARCH_WARN("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));

   runloop_msg_queue_push(
         msg_hash_to_str(MSG_NETPLAY_FAILED),
         0, 180, false,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   return false;
}

/**
 * netplay_driver_ctl
 *
 * Frontend access to Netplay functionality
 */
bool netplay_driver_ctl(enum rarch_netplay_ctl_state state, void *data)
{
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = config_get_ptr();
   netplay_t *netplay          = p_rarch->netplay_data;
   bool ret                    = true;

   if (p_rarch->in_netplay)
      return true;
   p_rarch->in_netplay         = true;

   if (!netplay)
   {
      switch (state)
      {
         case RARCH_NETPLAY_CTL_ENABLE_SERVER:
            p_rarch->netplay_enabled    = true;
            p_rarch->netplay_is_client  = false;
            goto done;

         case RARCH_NETPLAY_CTL_ENABLE_CLIENT:
            p_rarch->netplay_enabled    = true;
            p_rarch->netplay_is_client  = true;
            break;

         case RARCH_NETPLAY_CTL_DISABLE:
            p_rarch->netplay_enabled    = false;
#ifdef HAVE_DISCORD
            if (discord_is_inited)
            {
               discord_userdata_t userdata;
               userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
               command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
            }
#endif
            goto done;

         case RARCH_NETPLAY_CTL_IS_ENABLED:
            ret = p_rarch->netplay_enabled;
            goto done;

         case RARCH_NETPLAY_CTL_IS_REPLAYING:
         case RARCH_NETPLAY_CTL_IS_DATA_INITED:
            ret = false;
            goto done;

         case RARCH_NETPLAY_CTL_IS_SERVER:
            ret =  p_rarch->netplay_enabled
               && !p_rarch->netplay_is_client;
            goto done;

         case RARCH_NETPLAY_CTL_IS_CONNECTED:
            ret = false;
            goto done;

         default:
            goto done;
      }
   }

   switch (state)
   {
      case RARCH_NETPLAY_CTL_ENABLE_SERVER:
      case RARCH_NETPLAY_CTL_ENABLE_CLIENT:
      case RARCH_NETPLAY_CTL_IS_DATA_INITED:
         goto done;
      case RARCH_NETPLAY_CTL_DISABLE:
         ret = false;
         goto done;
      case RARCH_NETPLAY_CTL_IS_ENABLED:
         goto done;
      case RARCH_NETPLAY_CTL_IS_REPLAYING:
         ret = netplay->is_replay;
         goto done;
      case RARCH_NETPLAY_CTL_IS_SERVER:
         ret =  p_rarch->netplay_enabled
            && !p_rarch->netplay_is_client;
         goto done;
      case RARCH_NETPLAY_CTL_IS_CONNECTED:
         ret = netplay->is_connected;
         goto done;
      case RARCH_NETPLAY_CTL_POST_FRAME:
         netplay_post_frame(netplay);
	 /* If we're disconnected, deinitialize */
	 if (!netplay->is_server && !netplay->connections[0].active)
		 netplay_disconnect(p_rarch, netplay);
         break;
      case RARCH_NETPLAY_CTL_PRE_FRAME:
         ret = netplay_pre_frame(p_rarch,
               settings->bools.netplay_public_announce,
               settings->bools.netplay_use_mitm_server,
               netplay);
         goto done;
      case RARCH_NETPLAY_CTL_GAME_WATCH:
         netplay_toggle_play_spectate(netplay);
         break;
      case RARCH_NETPLAY_CTL_PAUSE:
         if (netplay->local_paused != true)
            netplay_frontend_paused(netplay, true);
         break;
      case RARCH_NETPLAY_CTL_UNPAUSE:
         if (netplay->local_paused != false)
            netplay_frontend_paused(netplay, false);
         break;
      case RARCH_NETPLAY_CTL_LOAD_SAVESTATE:
         netplay_load_savestate(netplay, (retro_ctx_serialize_info_t*)data, true);
         break;
      case RARCH_NETPLAY_CTL_RESET:
         netplay_core_reset(netplay);
         break;
      case RARCH_NETPLAY_CTL_DISCONNECT:
         ret    = true;
         if (netplay)
            netplay_disconnect(p_rarch, netplay);
         goto done;
      case RARCH_NETPLAY_CTL_FINISHED_NAT_TRAVERSAL:
         netplay->nat_traversal_task_oustanding = false;
#ifndef HAVE_SOCKET_LEGACY
         netplay_announce_nat_traversal(netplay);
#endif
         goto done;
      case RARCH_NETPLAY_CTL_DESYNC_PUSH:
         netplay->desync++;
         break;
      case RARCH_NETPLAY_CTL_DESYNC_POP:
         if (netplay->desync)
         {
            netplay->desync--;
            if (!netplay->desync)
               netplay_load_savestate(netplay, NULL, true);
         }
         break;
      default:
      case RARCH_NETPLAY_CTL_NONE:
         ret = false;
   }

done:
   p_rarch->in_netplay = false;
   return ret;
}
#endif

static void log_counters(
      struct retro_perf_counter **counters, unsigned num)
{
   unsigned i;
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

static void retro_perf_log(void)
{
   RARCH_LOG("[PERF]: Performance counters (libretro):\n");
   log_counters(runloop_state.perf_counters_libretro,
         runloop_state.perf_ptr_libretro);
}

struct retro_perf_counter **retro_get_perf_counter_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_counters_rarch;
}

struct retro_perf_counter **retro_get_perf_counter_libretro(void)
{
   return runloop_state.perf_counters_libretro;
}

unsigned retro_get_perf_count_rarch(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->perf_ptr_rarch;
}

unsigned retro_get_perf_count_libretro(void)
{
   return runloop_state.perf_ptr_libretro;
}

void rarch_perf_register(struct retro_perf_counter *perf)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = &runloop_state;
   if (
            !runloop_st->perfcnt_enable
         || perf->registered
         || p_rarch->perf_ptr_rarch >= MAX_COUNTERS
      )
      return;

   p_rarch->perf_counters_rarch[p_rarch->perf_ptr_rarch++] = perf;
   perf->registered = true;
}

static void performance_counter_register(struct retro_perf_counter *perf)
{
   if (     perf->registered 
         || runloop_state.perf_ptr_libretro >= MAX_COUNTERS)
      return;

   runloop_state.perf_counters_libretro[runloop_state.perf_ptr_libretro++] = perf;
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

            if (zones_file != NULL)
            {
               char zone_desc[TIMEZONE_LENGTH];
               while (fgets(zone_desc, TIMEZONE_LENGTH, zones_file))
               {
                  size_t zone_desc_len = strlen(zone_desc);

                  if (zone_desc_len > 0)
                     if (zone_desc[--zone_desc_len] == '\n')
                        zone_desc[zone_desc_len] = '\0';

                  if (strlen(zone_desc) > 0)
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

static void path_set_redirect(struct rarch_state *p_rarch,
      settings_t *settings)
{
   char content_dir_name[PATH_MAX_LENGTH];
   char new_savefile_dir[PATH_MAX_LENGTH];
   char new_savestate_dir[PATH_MAX_LENGTH];
   global_t   *global                          = global_get_ptr();
   const char *old_savefile_dir                = p_rarch->dir_savefile;
   const char *old_savestate_dir               = p_rarch->dir_savestate;
   runloop_state_t *runloop_st                 = &runloop_state;
   struct retro_system_info *system            = &runloop_st->system.info;
   bool sort_savefiles_enable                  = settings->bools.sort_savefiles_enable;
   bool sort_savefiles_by_content_enable       = settings->bools.sort_savefiles_by_content_enable;
   bool sort_savestates_enable                 = settings->bools.sort_savestates_enable;
   bool sort_savestates_by_content_enable      = settings->bools.sort_savestates_by_content_enable;
   bool savefiles_in_content_dir               = settings->bools.savefiles_in_content_dir;
   bool savestates_in_content_dir              = settings->bools.savestates_in_content_dir;

   content_dir_name[0]  = '\0';
   new_savefile_dir[0]  = '\0';
   new_savestate_dir[0] = '\0';

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
         RARCH_LOG("Cannot resolve save file path.\n",
            msg_hash_to_str(MSG_REVERTING_SAVEFILE_DIRECTORY_TO),
            new_savefile_dir);
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
         RARCH_LOG("Cannot resolve save state file path.\n",
            msg_hash_to_str(MSG_REVERTING_SAVESTATE_DIRECTORY_TO),
            new_savestate_dir);
      else if (sort_savestates_enable || sort_savestates_by_content_enable)
         RARCH_LOG("Saving save states in content directory is set. This overrides other save state file directory settings.\n");
   }

   if (global && system && !string_is_empty(system->library_name))
   {
      bool savefile_is_dir  = path_is_directory(new_savefile_dir);
      bool savestate_is_dir = path_is_directory(new_savestate_dir);
      if (savefile_is_dir)
         strlcpy(global->name.savefile, new_savefile_dir,
               sizeof(global->name.savefile));
      else
         savefile_is_dir    = path_is_directory(global->name.savefile);

      if (savestate_is_dir)
         strlcpy(global->name.savestate, new_savestate_dir,
               sizeof(global->name.savestate));
      else
         savestate_is_dir   = path_is_directory(global->name.savestate);

      if (savefile_is_dir)
      {
         fill_pathname_dir(global->name.savefile,
               !string_is_empty(runloop_st->runtime_content_path_basename)
               ? runloop_st->runtime_content_path_basename
               : system->library_name,
               FILE_PATH_SRM_EXTENSION,
               sizeof(global->name.savefile));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }

      if (savestate_is_dir)
      {
         fill_pathname_dir(global->name.savestate,
               !string_is_empty(runloop_st->runtime_content_path_basename)
               ? runloop_st->runtime_content_path_basename
               : system->library_name,
               FILE_PATH_STATE_EXTENSION,
               sizeof(global->name.savestate));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }

#ifdef HAVE_CHEATS
      if (path_is_directory(global->name.cheatfile))
      {
         fill_pathname_dir(global->name.cheatfile,
               !string_is_empty(runloop_st->runtime_content_path_basename)
               ? runloop_st->runtime_content_path_basename
               : system->library_name,
               FILE_PATH_CHT_EXTENSION,
               sizeof(global->name.cheatfile));
         RARCH_LOG("[Overrides]: %s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_CHEATFILE_TO),
               global->name.cheatfile);
      }
#endif
   }

   dir_set(RARCH_DIR_CURRENT_SAVEFILE,  new_savefile_dir);
   dir_set(RARCH_DIR_CURRENT_SAVESTATE, new_savestate_dir);
}

static void runloop_path_set_basename(runloop_state_t *runloop_st,
      const char *path)
{
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

struct string_list *path_get_subsystem_list(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->subsystem_fullpaths;
}

void path_set_special(char **argv, unsigned num_content)
{
   unsigned i;
   char str[PATH_MAX_LENGTH];
   union string_list_elem_attr attr;
   struct string_list subsystem_paths  = {0};
   runloop_state_t         *runloop_st = runloop_state_get_ptr();
   global_t   *global                  = global_get_ptr();
   const char *savestate_dir           = runloop_st->savestate_dir;


   /* First content file is the significant one. */
   runloop_path_set_basename(runloop_st, argv[0]);

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
   if (global)
   {
      bool is_dir = path_is_directory(savestate_dir);

      if (is_dir)
         strlcpy(global->name.savestate, savestate_dir,
               sizeof(global->name.savestate));
      else
         is_dir   = path_is_directory(global->name.savestate);

      if (is_dir)
      {
         fill_pathname_dir(global->name.savestate,
               str,
               ".state",
               sizeof(global->name.savestate));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVESTATE_TO),
               global->name.savestate);
      }
   }
}

static bool path_init_subsystem(void)
{
   unsigned i, j;
   const struct retro_subsystem_info *info = NULL;
   global_t   *global                      = global_get_ptr();
   runloop_state_t             *runloop_st = &runloop_state;
   rarch_system_info_t             *system = &runloop_st->system;
   bool subsystem_path_empty               = path_is_empty(RARCH_PATH_SUBSYSTEM);
   const char                *savefile_dir = runloop_st->savefile_dir;


   if (!system || subsystem_path_empty)
      return false;
   /* For subsystems, we know exactly which RAM types are supported. */

   info = libretro_find_subsystem_info(
         system->subsystem.data,
         system->subsystem.size,
         path_get(RARCH_PATH_SUBSYSTEM));

   /* We'll handle this error gracefully later. */
   if (info)
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

            path[0] = ext[0] = '\0';
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

   if (global)
   {
      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!retroarch_override_setting_is_set(
               RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               runloop_st->runtime_content_path_basename,
               ".srm",
               sizeof(global->name.savefile));

      if (path_is_directory(global->name.savefile))
      {
         fill_pathname_dir(global->name.savefile,
               runloop_st->runtime_content_path_basename,
               ".srm",
               sizeof(global->name.savefile));
         RARCH_LOG("%s \"%s\".\n",
               msg_hash_to_str(MSG_REDIRECTING_SAVEFILE_TO),
               global->name.savefile);
      }
   }

   return true;
}

static void path_init_savefile(runloop_state_t *runloop_st)
{
   bool    should_sram_be_used = runloop_st->use_sram
      && !runloop_st->is_sram_save_disabled;

   runloop_st->use_sram     = should_sram_be_used;

   if (!runloop_st->use_sram)
   {
      RARCH_LOG("[SRAM]: %s\n",
            msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));
      return;
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
}

static void path_init_savefile_internal(global_t *global)
{
   path_deinit_savefile();
   path_init_savefile_new();

   if (!path_init_subsystem())
      path_init_savefile_rtc(global->name.savefile);
}

static void runloop_path_fill_names(runloop_state_t *runloop_st,
      input_driver_state_t *input_st)
{
   global_t            *global = global_get_ptr();

   path_init_savefile_internal(global);

#ifdef HAVE_BSV_MOVIE
   if (global)
      strlcpy(input_st->bsv_movie_state.movie_path,
            global->name.savefile,
            sizeof(input_st->bsv_movie_state.movie_path));
#endif

   if (string_is_empty(runloop_st->runtime_content_path_basename))
      return;

   if (global)
   {
      if (string_is_empty(global->name.ups))
         fill_pathname_noext(global->name.ups,
               runloop_st->runtime_content_path_basename,
               ".ups",
               sizeof(global->name.ups));

      if (string_is_empty(global->name.bps))
         fill_pathname_noext(global->name.bps,
               runloop_st->runtime_content_path_basename,
               ".bps",
               sizeof(global->name.bps));

      if (string_is_empty(global->name.ips))
         fill_pathname_noext(global->name.ips,
               runloop_st->runtime_content_path_basename,
               ".ips",
               sizeof(global->name.ips));
   }
}

char *path_get_ptr(enum rarch_path_type type)
{
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = &runloop_state;

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
   runloop_state_t *runloop_st = &runloop_state;

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
   runloop_state_t *runloop_st = &runloop_state;

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

static void runloop_path_set_names(runloop_state_t *runloop_st,
      global_t *global)
{
   if (global)
   {
      if (!retroarch_override_setting_is_set(
               RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL))
         fill_pathname_noext(global->name.savefile,
               runloop_st->runtime_content_path_basename,
               ".srm", sizeof(global->name.savefile));

      if (!retroarch_override_setting_is_set(
               RARCH_OVERRIDE_SETTING_STATE_PATH, NULL))
         fill_pathname_noext(global->name.savestate,
               runloop_st->runtime_content_path_basename,
               ".state", sizeof(global->name.savestate));

#ifdef HAVE_CHEATS
      if (!string_is_empty(runloop_st->runtime_content_path_basename))
         fill_pathname_noext(global->name.cheatfile,
               runloop_st->runtime_content_path_basename,
               ".cht", sizeof(global->name.cheatfile));
#endif
   }
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
         runloop_path_set_basename(runloop_st, path);
         runloop_path_set_names(runloop_st, global_get_ptr());
         path_set_redirect(p_rarch, config_get_ptr());
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

void ram_state_to_file(void)
{
   char state_path[PATH_MAX_LENGTH];

   if (!content_ram_state_pending())
      return;

   state_path[0] = '\0';

   if (retroarch_get_current_savestate_path(state_path, sizeof(state_path)))
      command_event(CMD_EVENT_RAM_STATE_TO_FILE, state_path);
}

bool retroarch_get_current_savestate_path(char *path, size_t len)
{
   const global_t *global      = global_get_ptr();
   settings_t *settings        = config_get_ptr();
   int state_slot              = settings ? settings->ints.state_slot : 0;
   const char *name_savestate  = NULL;

   if (!path || !global)
      return false;

   name_savestate = global->name.savestate;
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

enum rarch_content_type path_is_media_type(const char *path)
{
   char ext_lower[128];

   ext_lower[0] = '\0';

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
#ifdef HAVE_GONG
      case FILE_TYPE_GONG:
         return RARCH_CONTENT_GONG;
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
   runloop_state_t *runloop_st = &runloop_state;

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
static bool is_accessibility_enabled(bool accessibility_enable,
      bool accessibility_enabled)
{
   return accessibility_enabled || accessibility_enable;
}
#endif

#ifdef HAVE_MENU
bool menu_input_dialog_start_search(void)
{
   input_driver_state_t 
      *input_st                = input_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = config_get_ptr();
   bool accessibility_enable   = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   struct menu_state *menu_st  = menu_state_get_ptr();
   menu_handle_t         *menu = menu_st->driver_data;

   if (!menu)
      return false;

   menu_st->input_dialog_kb_display = true;
   strlcpy(menu_st->input_dialog_kb_label,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH),
         sizeof(menu_st->input_dialog_kb_label));

   if (input_st->keyboard_line.buffer)
      free(input_st->keyboard_line.buffer);
   input_st->keyboard_line.buffer                    = NULL;
   input_st->keyboard_line.ptr                       = 0;
   input_st->keyboard_line.size                      = 0;
   input_st->keyboard_line.cb                        = NULL;
   input_st->keyboard_line.userdata                  = NULL;
   input_st->keyboard_line.enabled                   = false;

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
         accessibility_speak_priority(p_rarch,
            accessibility_enable,
            accessibility_narrator_speech_speed,
            (char*)msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH), 10);
#endif

   menu_st->input_dialog_keyboard_buffer   =
      input_keyboard_start_line(menu,
            &input_st->keyboard_line,
            menu_input_search_cb);
   /* While reading keyboard line input, we have to block all hotkeys. */
   input_st->keyboard_mapping_blocked = true;

   return true;
}

bool menu_input_dialog_start(menu_input_ctx_line_t *line)
{
   input_driver_state_t *input_st   = input_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   struct rarch_state *p_rarch      = &rarch_st;
   settings_t *settings             = config_get_ptr();
   bool accessibility_enable        = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   struct menu_state *menu_st       = menu_state_get_ptr();
   menu_handle_t         *menu      = menu_st->driver_data;
   if (!line || !menu)
      return false;

   menu_st->input_dialog_kb_display = true;

   /* Only copy over the menu label and setting if they exist. */
   if (line->label)
      strlcpy(menu_st->input_dialog_kb_label,
            line->label,
            sizeof(menu_st->input_dialog_kb_label));
   if (line->label_setting)
      strlcpy(menu_st->input_dialog_kb_label_setting,
            line->label_setting,
            sizeof(menu_st->input_dialog_kb_label_setting));

   menu_st->input_dialog_kb_type   = line->type;
   menu_st->input_dialog_kb_idx    = line->idx;

   if (input_st->keyboard_line.buffer)
      free(input_st->keyboard_line.buffer);
   input_st->keyboard_line.buffer                    = NULL;
   input_st->keyboard_line.ptr                       = 0;
   input_st->keyboard_line.size                      = 0;
   input_st->keyboard_line.cb                        = NULL;
   input_st->keyboard_line.userdata                  = NULL;
   input_st->keyboard_line.enabled                   = false;

#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
      accessibility_speak_priority(p_rarch,
            accessibility_enable,
            accessibility_narrator_speech_speed,
            "Keyboard input:", 10);
#endif

   menu_st->input_dialog_keyboard_buffer =
      input_keyboard_start_line(menu,
            &input_st->keyboard_line,
            line->cb);
   /* While reading keyboard line input, we have to block all hotkeys. */
   input_st->keyboard_mapping_blocked = true;

   return true;
}
#endif

/* MESSAGE QUEUE */

static void runloop_msg_queue_deinit(void)
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

static void runloop_msg_queue_init(void)
{
   runloop_state_t *runloop_st = &runloop_state;

   runloop_msg_queue_deinit();
   msg_queue_initialize(&runloop_st->msg_queue, 8);

#ifdef HAVE_THREADS
   runloop_st->msg_queue_lock   = slock_new();
#endif
}

/* COMMAND */

#ifdef HAVE_COMMAND
bool command_get_config_param(command_t *cmd, const char* arg)
{
   char reply[8192]             = {0};
   struct rarch_state  *p_rarch = &rarch_st;
   const char      *value       = "unsupported";
   settings_t       *settings   = config_get_ptr();
   bool       video_fullscreen  = settings->bools.video_fullscreen;
   const char *dir_runtime_log  = settings->paths.directory_runtime_log;
   const char *log_dir          = settings->paths.log_dir;
   const char *directory_cache  = settings->paths.directory_cache;
   const char *directory_system = settings->paths.directory_system;
   const char *path_username    = settings->paths.username;

   if (string_is_equal(arg, "video_fullscreen"))
   {
      if (video_fullscreen)
         value = "true";
      else
         value = "false";
   }
   else if (string_is_equal(arg, "savefile_directory"))
      value = p_rarch->dir_savefile;
   else if (string_is_equal(arg, "savestate_directory"))
      value = p_rarch->dir_savestate;
   else if (string_is_equal(arg, "runtime_log_directory"))
      value = dir_runtime_log;
   else if (string_is_equal(arg, "log_dir"))
      value = log_dir;
   else if (string_is_equal(arg, "cache_directory"))
      value = directory_cache;
   else if (string_is_equal(arg, "system_directory"))
      value = directory_system;
   else if (string_is_equal(arg, "netplay_nickname"))
      value = path_username;
   /* TODO: query any string */

   snprintf(reply, sizeof(reply), "GET_CONFIG_PARAM %s %s\n", arg, value);
   cmd->replier(cmd, reply, strlen(reply));
   return true;
}
#endif

/* TRANSLATION */
#ifdef HAVE_TRANSLATE
static void task_auto_translate_handler(retro_task_t *task)
{
   int               *mode_ptr = (int*)task->user_data;
   runloop_state_t *runloop_st = &runloop_state;
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings        = config_get_ptr();
#endif

   if (task_get_cancelled(task))
      goto task_finished;

   switch (*mode_ptr)
   {
      case 1: /* Speech   Mode */
#ifdef HAVE_AUDIOMIXER
         if (!audio_driver_is_ai_service_speech_running())
            goto task_finished;
#endif
         break;
      case 2: /* Narrator Mode */
#ifdef HAVE_ACCESSIBILITY
         if (!is_narrator_running(p_rarch,
                  settings->bools.accessibility_enable))
            goto task_finished;
#endif
         break;
      default:
         break;
   }

   return;

task_finished:
   if (p_rarch->ai_service_auto == 1)
      p_rarch->ai_service_auto = 2;

   task_set_finished(task, true);

   if (*mode_ptr == 1 || *mode_ptr == 2)
   {
      bool was_paused = runloop_st->paused;
      command_event(CMD_EVENT_AI_SERVICE_CALL, &was_paused);
   }
   if (task->user_data)
       free(task->user_data);
}

static void call_auto_translate_task(
      struct rarch_state *p_rarch,
      settings_t *settings,
      bool *was_paused)
{
   int ai_service_mode  = settings->uints.ai_service_mode;

   /*Image Mode*/
   if (ai_service_mode == 0)
   {
      if (p_rarch->ai_service_auto == 1)
         p_rarch->ai_service_auto = 2;

      command_event(CMD_EVENT_AI_SERVICE_CALL, was_paused);
   }
   else /* Speech or Narrator Mode */
   {
      int* mode                          = NULL;
      retro_task_t  *t                   = task_init();
      if (!t)
         return;

      mode                               = (int*)malloc(sizeof(int));
      *mode                              = ai_service_mode;

      t->handler                         = task_auto_translate_handler;
      t->user_data                       = mode;
      t->mute                            = true;
      task_queue_push(t);
   }
}

static void handle_translation_cb(
      retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   size_t pitch;
   unsigned width, height;
   unsigned image_width, image_height;
   uint8_t* raw_output_data          = NULL;
   char* raw_image_file_data         = NULL;
   struct scaler_ctx* scaler         = NULL;
   http_transfer_data_t *data        = (http_transfer_data_t*)task_data;
   int new_image_size                = 0;
#ifdef HAVE_AUDIOMIXER
   int new_sound_size                = 0;
#endif
   const void* dummy_data            = NULL;
   void* raw_image_data              = NULL;
   void* raw_image_data_alpha        = NULL;
   void* raw_sound_data              = NULL;
   int retval                        = 0;
   rjson_t* json                     = NULL;
   int json_current_key              = 0;
   char* err_string                  = NULL;
   char* text_string                 = NULL;
   char* auto_string                 = NULL;
   char* key_string                  = NULL;
   struct rarch_state *p_rarch       = &rarch_st;
   settings_t* settings              = config_get_ptr();
   runloop_state_t *runloop_st       = &runloop_state;
   bool was_paused                   = runloop_st->paused;
   video_driver_state_t 
      *video_st                      = video_state_get_ptr();
   const enum retro_pixel_format
      video_driver_pix_fmt           = video_st->pix_fmt;
#ifdef HAVE_ACCESSIBILITY
   bool accessibility_enable         = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool gfx_widgets_paused           = video_st->widgets_paused;

   /* When auto mode is on, we turn off the overlay
    * once we have the result for the next call.*/
   if (dispwidget_get_ptr()->ai_service_overlay_state != 0
       && p_rarch->ai_service_auto == 2)
      gfx_widgets_ai_service_overlay_unload();
#endif

#ifdef DEBUG
   if (p_rarch->ai_service_auto != 2)
      RARCH_LOG("RESULT FROM AI SERVICE...\n");
#endif

   if (!data || error || !data->data)
      goto finish;

   json = rjson_open_buffer(data->data, data->len);
   if (!json)
      goto finish;

   /* Parse JSON body for the image and sound data */
   for (;;)
   {
      static const char* keys[] = { "image", "sound", "text", "error", "auto", "press" };

      const char *str           = NULL;
      size_t str_len            = 0;
      enum rjson_type json_type = rjson_next(json);

      if (json_type == RJSON_DONE || json_type == RJSON_ERROR)
         break;
      if (json_type != RJSON_STRING)
         continue;
      if (rjson_get_context_type(json) != RJSON_OBJECT)
         continue;
      str                       = rjson_get_string(json, &str_len);

      if ((rjson_get_context_count(json) & 1) == 1)
      {
         int i;
         json_current_key = -1;

         for (i = 0; i < ARRAY_SIZE(keys); i++)
         {
            if (string_is_equal(str, keys[i]))
            {
               json_current_key = i;
               break;
            }
         }
      }
      else
      {
         switch (json_current_key)
         {
            case 0: /* image */
               raw_image_file_data = (char*)unbase64(str,
                    (int)str_len, &new_image_size);
               break;
#ifdef HAVE_AUDIOMIXER
            case 1: /* sound */
               raw_sound_data = (void*)unbase64(str,
                    (int)str_len, &new_sound_size);
               break;
#endif
            case 2: /* text */
               text_string = strdup(str);
               break;
            case 3: /* error */
               err_string  = strdup(str);
               break;
            case 4: /* auto */
               auto_string = strdup(str);
               break;
            case 5: /* press */
               key_string  = strdup(str);
               break;
         }
         json_current_key = -1;
      }
   }

   if (string_is_equal(err_string, "No text found."))
   {
#ifdef DEBUG
      RARCH_LOG("No text found...\n");
#endif
      if (text_string)
      {
         free(text_string);
         text_string = NULL;
      }

      text_string = (char*)malloc(15);

      strlcpy(text_string, err_string, 15);
#ifdef HAVE_GFX_WIDGETS
      if (gfx_widgets_paused)
      {
         /* In this case we have to unpause and then repause for a frame */
         dispwidget_get_ptr()->ai_service_overlay_state = 2;
         command_event(CMD_EVENT_UNPAUSE, NULL);
      }
#endif
   }

   if (     !raw_image_file_data
         && !raw_sound_data
         && !text_string
         && (p_rarch->ai_service_auto != 2)
         && !key_string)
   {
      error = "Invalid JSON body.";
      goto finish;
   }

   if (raw_image_file_data)
   {
      /* Get the video frame dimensions reference */
      video_driver_cached_frame_get(&dummy_data, &width, &height, &pitch);

      /* try two different modes for text display *
       * In the first mode, we use display widget overlays, but they require
       * the video poke interface to be able to load image buffers.
       *
       * The other method is to draw to the video buffer directly, which needs
       * a software core to be running. */
#ifdef HAVE_GFX_WIDGETS
      if (   video_st->poke
          && video_st->poke->load_texture
          && video_st->poke->unload_texture)
      {
         bool ai_res;
         enum image_type_enum image_type;
         /* Write to overlay */
         if (  raw_image_file_data[0] == 'B' &&
               raw_image_file_data[1] == 'M')
             image_type = IMAGE_TYPE_BMP;
         else if (raw_image_file_data[1] == 'P' &&
                  raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
            image_type = IMAGE_TYPE_PNG;
         else
         {
            RARCH_LOG("Invalid image type returned from server.\n");
            goto finish;
         }

         ai_res = gfx_widgets_ai_service_overlay_load(
               raw_image_file_data, (unsigned)new_image_size,
               image_type);

         if (!ai_res)
         {
            RARCH_LOG("Video driver not supported for AI Service.");
            runloop_msg_queue_push(
               /* msg_hash_to_str(MSG_VIDEO_DRIVER_NOT_SUPPORTED), */
               "Video driver not supported.",
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         else if (gfx_widgets_paused)
         {
            /* In this case we have to unpause and then repause for a frame */
#ifdef HAVE_TRANSLATE
            /* Unpausing state */
            dispwidget_get_ptr()->ai_service_overlay_state = 2;
#endif
            command_event(CMD_EVENT_UNPAUSE, NULL);
         }
      }
      else
#endif
      /* Can't use display widget overlays, so try writing to video buffer */
      {
         /* Write to video buffer directly (software cores only) */
         if (raw_image_file_data[0] == 'B' && raw_image_file_data[1] == 'M')
         {
            /* This is a BMP file coming back. */
            /* Get image data (24 bit), and convert to the emulated pixel format */
            image_width    =
               ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 0);

            image_height   =
               ((uint32_t) ((uint8_t)raw_image_file_data[25]) << 24) +
               ((uint32_t) ((uint8_t)raw_image_file_data[24]) << 16) +
               ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 8) +
               ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 0);
            raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
            memcpy(raw_image_data,
                   raw_image_file_data+54*sizeof(uint8_t),
                   image_width*image_height*3*sizeof(uint8_t));
         }
         else if (raw_image_file_data[1] == 'P' && raw_image_file_data[2] == 'N' &&
                  raw_image_file_data[3] == 'G')
         {
            rpng_t *rpng = NULL;
            /* PNG coming back from the url */
            image_width  =
                ((uint32_t) ((uint8_t)raw_image_file_data[16]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[17]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[18]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[19]) << 0);
            image_height =
                ((uint32_t) ((uint8_t)raw_image_file_data[20]) << 24)+
                ((uint32_t) ((uint8_t)raw_image_file_data[21]) << 16)+
                ((uint32_t) ((uint8_t)raw_image_file_data[22]) << 8)+
                ((uint32_t) ((uint8_t)raw_image_file_data[23]) << 0);
            rpng = rpng_alloc();

            if (!rpng)
            {
               error = "Can't allocate memory.";
               goto finish;
            }

            rpng_set_buf_ptr(rpng, raw_image_file_data, (size_t)new_image_size);
            rpng_start(rpng);
            while (rpng_iterate_image(rpng));

            do
            {
               retval = rpng_process_image(rpng, &raw_image_data_alpha,
                     (size_t)new_image_size, &image_width, &image_height);
            } while (retval == IMAGE_PROCESS_NEXT);

            /* Returned output from the png processor is an upside down RGBA
             * image, so we have to change that to RGB first.  This should
             * probably be replaced with a scaler call.*/
            {
               unsigned ui;
               int d,tw,th,tc;
               d=0;
               raw_image_data = (void*)malloc(image_width*image_height*3*sizeof(uint8_t));
               for (ui = 0; ui < image_width * image_height * 4; ui++)
               {
                  if (ui % 4 != 3)
                  {
                     tc = d%3;
                     th = image_height-d / (3*image_width)-1;
                     tw = (d%(image_width*3)) / 3;
                     ((uint8_t*) raw_image_data)[tw*3+th*3*image_width+tc] = ((uint8_t *)raw_image_data_alpha)[ui];
                     d+=1;
                  }
               }
            }
            rpng_free(rpng);
         }
         else
         {
            RARCH_LOG("Output from URL not a valid file type, or is not supported.\n");
            goto finish;
         }

         scaler = (struct scaler_ctx*)calloc(1, sizeof(struct scaler_ctx));
         if (!scaler)
            goto finish;

         if (dummy_data == RETRO_HW_FRAME_BUFFER_VALID)
         {
            /*
               In this case, we used the viewport to grab the image
               and translate it, and we have the translated image in
               the raw_image_data buffer.
            */
            RARCH_LOG("Hardware frame buffer core, but selected video driver isn't supported.\n");
            goto finish;
         }

         /* The assigned pitch may not be reliable.  The width of
            the video frame can change during run-time, but the
            pitch may not, so we just assign it as the width
            times the byte depth.
         */

         if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 4 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_ARGB8888;
            pitch              = width * 4;
            scaler->out_stride = width * 4;
         }
         else
         {
            raw_output_data    = (uint8_t*)malloc(width * height * 2 * sizeof(uint8_t));
            scaler->out_fmt    = SCALER_FMT_RGB565;
            pitch              = width * 2;
            scaler->out_stride = width * 1;
         }

         if (!raw_output_data)
            goto finish;

         scaler->in_fmt        = SCALER_FMT_BGR24;
         scaler->in_width      = image_width;
         scaler->in_height     = image_height;
         scaler->out_width     = width;
         scaler->out_height    = height;
         scaler->scaler_type   = SCALER_TYPE_POINT;
         scaler_ctx_gen_filter(scaler);
         scaler->in_stride     = -1 * width * 3;

         scaler_ctx_scale_direct(scaler, raw_output_data,
               (uint8_t*)raw_image_data + (image_height - 1) * width * 3);
         video_driver_frame(raw_output_data, image_width, image_height, pitch);
      }
   }

#ifdef HAVE_AUDIOMIXER
   if (raw_sound_data)
   {
      audio_mixer_stream_params_t params;

      params.volume               = 1.0f;
      params.slot_selection_type  = AUDIO_MIXER_SLOT_SELECTION_MANUAL; /* user->slot_selection_type; */
      params.slot_selection_idx   = 10;
      params.stream_type          = AUDIO_STREAM_TYPE_SYSTEM; /* user->stream_type; */
      params.type                 = AUDIO_MIXER_TYPE_WAV;
      params.state                = AUDIO_STREAM_STATE_PLAYING;
      params.buf                  = raw_sound_data;
      params.bufsize              = new_sound_size;
      params.cb                   = NULL;
      params.basename             = NULL;

      audio_driver_mixer_add_stream(&params);

      if (raw_sound_data)
      {
         free(raw_sound_data);
         raw_sound_data = NULL;
      }
   }
#endif

   if (key_string)
   {
      char key[8];
      size_t length = strlen(key_string);
      int i         = 0;
      int start     = 0;
      char t        = ' ';

      for (i = 1; i < (int)length; i++)
      {
         t = key_string[i];
         if (i == length-1 || t == ' ' || t == ',')
         {
            if (i == length-1 && t != ' ' && t!= ',')
               i++;

            if (i-start > 7)
            {
               start = i;
               continue;
            }

            strncpy(key, key_string+start, i-start);
            key[i-start] = '\0';

#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
            if (string_is_equal(key, "b"))
               p_rarch->ai_gamepad_state[0] = 2;
            if (string_is_equal(key, "y"))
               p_rarch->ai_gamepad_state[1] = 2;
            if (string_is_equal(key, "select"))
               p_rarch->ai_gamepad_state[2] = 2;
            if (string_is_equal(key, "start"))
               p_rarch->ai_gamepad_state[3] = 2;

            if (string_is_equal(key, "up"))
               p_rarch->ai_gamepad_state[4] = 2;
            if (string_is_equal(key, "down"))
               p_rarch->ai_gamepad_state[5] = 2;
            if (string_is_equal(key, "left"))
               p_rarch->ai_gamepad_state[6] = 2;
            if (string_is_equal(key, "right"))
               p_rarch->ai_gamepad_state[7] = 2;

            if (string_is_equal(key, "a"))
               p_rarch->ai_gamepad_state[8] = 2;
            if (string_is_equal(key, "x"))
               p_rarch->ai_gamepad_state[9] = 2;
            if (string_is_equal(key, "l"))
               p_rarch->ai_gamepad_state[10] = 2;
            if (string_is_equal(key, "r"))
               p_rarch->ai_gamepad_state[11] = 2;

            if (string_is_equal(key, "l2"))
               p_rarch->ai_gamepad_state[12] = 2;
            if (string_is_equal(key, "r2"))
               p_rarch->ai_gamepad_state[13] = 2;
            if (string_is_equal(key, "l3"))
               p_rarch->ai_gamepad_state[14] = 2;
            if (string_is_equal(key, "r3"))
               p_rarch->ai_gamepad_state[15] = 2;
#endif
#endif

            if (string_is_equal(key, "pause"))
               command_event(CMD_EVENT_PAUSE, NULL);
            if (string_is_equal(key, "unpause"))
               command_event(CMD_EVENT_UNPAUSE, NULL);

            start = i+1;
         }
      }
   }

#ifdef HAVE_ACCESSIBILITY
   if (text_string && is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
      accessibility_speak_priority(p_rarch,
            accessibility_enable,
            accessibility_narrator_speech_speed,
            text_string, 10);
#endif

finish:
   if (error)
      RARCH_ERR("%s: %s\n", msg_hash_to_str(MSG_DOWNLOAD_FAILED), error);

   if (user_data)
      free(user_data);

   if (json)
      rjson_free(json);
   if (raw_image_file_data)
      free(raw_image_file_data);
   if (raw_image_data_alpha)
       free(raw_image_data_alpha);
   if (raw_image_data)
      free(raw_image_data);
   if (scaler)
      free(scaler);
   if (err_string)
      free(err_string);
   if (text_string)
      free(text_string);
   if (raw_output_data)
      free(raw_output_data);

   if (string_is_equal(auto_string, "auto"))
   {
      if (     (p_rarch->ai_service_auto != 0)
            && !settings->bools.ai_service_pause)
         call_auto_translate_task(p_rarch, settings, &was_paused);
   }
   if (auto_string)
      free(auto_string);
   if (key_string)
      free(key_string);
}

static const char *ai_service_get_str(enum translation_lang id)
{
   switch (id)
   {
      case TRANSLATION_LANG_EN:
         return "en";
      case TRANSLATION_LANG_ES:
         return "es";
      case TRANSLATION_LANG_FR:
         return "fr";
      case TRANSLATION_LANG_IT:
         return "it";
      case TRANSLATION_LANG_DE:
         return "de";
      case TRANSLATION_LANG_JP:
         return "ja";
      case TRANSLATION_LANG_NL:
         return "nl";
      case TRANSLATION_LANG_CS:
         return "cs";
      case TRANSLATION_LANG_DA:
         return "da";
      case TRANSLATION_LANG_SV:
         return "sv";
      case TRANSLATION_LANG_HR:
         return "hr";
      case TRANSLATION_LANG_KO:
         return "ko";
      case TRANSLATION_LANG_ZH_CN:
         return "zh-CN";
      case TRANSLATION_LANG_ZH_TW:
         return "zh-TW";
      case TRANSLATION_LANG_CA:
         return "ca";
      case TRANSLATION_LANG_BG:
         return "bg";
      case TRANSLATION_LANG_BN:
         return "bn";
      case TRANSLATION_LANG_EU:
         return "eu";
      case TRANSLATION_LANG_AZ:
         return "az";
      case TRANSLATION_LANG_AR:
         return "ar";
      case TRANSLATION_LANG_AST:
         return "ast";
      case TRANSLATION_LANG_SQ:
         return "sq";
      case TRANSLATION_LANG_AF:
         return "af";
      case TRANSLATION_LANG_EO:
         return "eo";
      case TRANSLATION_LANG_ET:
         return "et";
      case TRANSLATION_LANG_TL:
         return "tl";
      case TRANSLATION_LANG_FI:
         return "fi";
      case TRANSLATION_LANG_GL:
         return "gl";
      case TRANSLATION_LANG_KA:
         return "ka";
      case TRANSLATION_LANG_EL:
         return "el";
      case TRANSLATION_LANG_GU:
         return "gu";
      case TRANSLATION_LANG_HT:
         return "ht";
      case TRANSLATION_LANG_HE:
         return "he";
      case TRANSLATION_LANG_HI:
         return "hi";
      case TRANSLATION_LANG_HU:
         return "hu";
      case TRANSLATION_LANG_IS:
         return "is";
      case TRANSLATION_LANG_ID:
         return "id";
      case TRANSLATION_LANG_GA:
         return "ga";
      case TRANSLATION_LANG_KN:
         return "kn";
      case TRANSLATION_LANG_LA:
         return "la";
      case TRANSLATION_LANG_LV:
         return "lv";
      case TRANSLATION_LANG_LT:
         return "lt";
      case TRANSLATION_LANG_MK:
         return "mk";
      case TRANSLATION_LANG_MS:
         return "ms";
      case TRANSLATION_LANG_MT:
         return "mt";
      case TRANSLATION_LANG_NO:
         return "no";
      case TRANSLATION_LANG_FA:
         return "fa";
      case TRANSLATION_LANG_PL:
         return "pl";
      case TRANSLATION_LANG_PT:
         return "pt";
      case TRANSLATION_LANG_RO:
         return "ro";
      case TRANSLATION_LANG_RU:
         return "ru";
      case TRANSLATION_LANG_SR:
         return "sr";
      case TRANSLATION_LANG_SK:
         return "sk";
      case TRANSLATION_LANG_SL:
         return "sl";
      case TRANSLATION_LANG_SW:
         return "sw";
      case TRANSLATION_LANG_TA:
         return "ta";
      case TRANSLATION_LANG_TE:
         return "te";
      case TRANSLATION_LANG_TH:
         return "th";
      case TRANSLATION_LANG_TR:
         return "tr";
      case TRANSLATION_LANG_UK:
         return "uk";
      case TRANSLATION_LANG_UR:
         return "ur";
      case TRANSLATION_LANG_VI:
         return "vi";
      case TRANSLATION_LANG_CY:
         return "cy";
      case TRANSLATION_LANG_YI:
         return "yi";
      case TRANSLATION_LANG_DONT_CARE:
      case TRANSLATION_LANG_LAST:
         break;
   }

   return "";
}


/*
   This function does all the stuff needed to translate the game screen,
   using the URL given in the settings.  Once the image from the frame
   buffer is sent to the server, the callback will write the translated
   image to the screen.

   Supported client/services (thus far)
   -VGTranslate client ( www.gitlab.com/spherebeaker/vg_translate )
   -Ztranslate client/service ( www.ztranslate.net/docs/service )

   To use a client, download the relevant code/release, configure
   them, and run them on your local machine, or network.  Set the
   retroarch configuration to point to your local client (usually
   listening on localhost:4404 ) and enable translation service.

   If you don't want to run a client, you can also use a service,
   which is basically like someone running a client for you.  The
   downside here is that your retroarch device will have to have
   an internet connection, and you may have to sign up for it.

   To make your own server, it must listen for a POST request, which
   will consist of a JSON body, with the "image" field as a base64
   encoded string of a 24bit-BMP/PNG that the will be translated.
   The server must output the translated image in the form of a
   JSON body, with the "image" field also as a base64 encoded
   24bit-BMP, or as an alpha channel png.

  "paused" boolean is passed in to indicate if the current call
   was made during a paused frame.  Due to how the menu widgets work,
   if the ai service is called in "auto" mode, then this call will
   be made while the menu widgets unpause the core for a frame to update
   the on-screen widgets.  To tell the ai service what the pause
   mode is honestly, we store the runloop_paused variable from before
   the handle_translation_cb wipes the widgets, and pass that in here.
*/

static bool run_translation_service(
      settings_t *settings,
      struct rarch_state *p_rarch,
      bool paused)
{
   struct video_viewport vp;
   uint8_t header[54];
   size_t pitch;
   unsigned width, height;
   const void *data                      = NULL;
   uint8_t *bit24_image                  = NULL;
   uint8_t *bit24_image_prev             = NULL;
   struct scaler_ctx *scaler             = (struct scaler_ctx*)
      calloc(1, sizeof(struct scaler_ctx));
   bool error                            = false;

   uint8_t *bmp_buffer                   = NULL;
   uint64_t buffer_bytes                 = 0;
   char *bmp64_buffer                    = NULL;
   rjsonwriter_t* jsonwriter             = NULL;
   const char *json_buffer               = NULL;

   int bmp64_length                      = 0;
   bool TRANSLATE_USE_BMP                = false;
   bool use_overlay                      = false;

   const char *label                     = NULL;
   char* system_label                    = NULL;
   core_info_t *core_info                = NULL;
   video_driver_state_t 
      *video_st                          = video_state_get_ptr();
   const enum retro_pixel_format
      video_driver_pix_fmt               = video_st->pix_fmt;

#ifdef HAVE_GFX_WIDGETS
   /* For the case when ai service pause is disabled. */
   if (  (dispwidget_get_ptr()->ai_service_overlay_state != 0)
         && (p_rarch->ai_service_auto == 1))
   {
      gfx_widgets_ai_service_overlay_unload();
      goto finish;
   }
#endif

#ifdef HAVE_GFX_WIDGETS
   if (     video_st->poke
         && video_st->poke->load_texture
         && video_st->poke->unload_texture)
      use_overlay = true;
#endif

   /* get the core info here so we can pass long the game name */
   core_info_get_current_core(&core_info);

   if (core_info)
   {
      size_t label_len;
      const char *system_id               = core_info->system_id
         ? core_info->system_id : "core";
      size_t system_id_len                = strlen(system_id);
      const struct playlist_entry *entry  = NULL;
      playlist_t *current_playlist        = playlist_get_cached();

      if (current_playlist)
      {
         playlist_get_index_by_path(
            current_playlist, path_get(RARCH_PATH_CONTENT), &entry);

         if (entry && !string_is_empty(entry->label))
            label = entry->label;
      }

      if (!label)
         label     = path_basename(path_get(RARCH_PATH_BASENAME));
      label_len    = strlen(label);
      system_label = (char*)malloc(label_len + system_id_len + 3);
      memcpy(system_label, system_id, system_id_len);
      memcpy(system_label + system_id_len, "__", 2);
      memcpy(system_label + 2 + system_id_len, label, label_len);
      system_label[system_id_len + 2 + label_len] = '\0';
   }

   if (!scaler)
      goto finish;

   video_driver_cached_frame_get(&data, &width, &height, &pitch);

   if (!data)
      goto finish;

   if (data == RETRO_HW_FRAME_BUFFER_VALID)
   {
      /*
        The direct frame capture didn't work, so try getting it
        from the viewport instead.  This isn't as good as the
        raw frame buffer, since the viewport may us bilinear
        filtering, or other shaders that will completely trash
        the OCR, but it's better than nothing.
      */
      vp.x                           = 0;
      vp.y                           = 0;
      vp.width                       = 0;
      vp.height                      = 0;
      vp.full_width                  = 0;
      vp.full_height                 = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
         goto finish;

      bit24_image_prev = (uint8_t*)malloc(vp.width * vp.height * 3);
      bit24_image      = (uint8_t*)malloc(width * height * 3);

      if (!bit24_image_prev || !bit24_image)
         goto finish;

      if (!video_driver_read_viewport(bit24_image_prev, false))
      {
         RARCH_LOG("Could not read viewport for translation service...\n");
         goto finish;
      }

      /* TODO: Rescale down to regular resolution */
      scaler->in_fmt      = SCALER_FMT_BGR24;
      scaler->out_fmt     = SCALER_FMT_BGR24;
      scaler->scaler_type = SCALER_TYPE_POINT;
      scaler->in_width    = vp.width;
      scaler->in_height   = vp.height;
      scaler->out_width   = width;
      scaler->out_height  = height;
      scaler_ctx_gen_filter(scaler);

      scaler->in_stride   = vp.width*3;
      scaler->out_stride  = width*3;
      scaler_ctx_scale_direct(scaler, bit24_image, bit24_image_prev);
   }
   else
   {
      /* This is a software core, so just change the pixel format to 24-bit. */
      bit24_image = (uint8_t*)malloc(width * height * 3);
      if (!bit24_image)
          goto finish;

      if (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
         scaler->in_fmt = SCALER_FMT_ARGB8888;
      else
         scaler->in_fmt = SCALER_FMT_RGB565;
      video_frame_convert_to_bgr24(
         scaler,
         (uint8_t *)bit24_image,
         (const uint8_t*)data + ((int)height - 1)*pitch,
         width, height,
         (int)-pitch);
   }
   scaler_ctx_gen_reset(scaler);

   if (!bit24_image)
   {
      error = true;
      goto finish;
   }

   if (TRANSLATE_USE_BMP)
   {
      /*
        At this point, we should have a screenshot in the buffer,
        so allocate an array to contain the BMP image along with
        the BMP header as bytes, and then covert that to a
        b64 encoded array for transport in JSON.
      */

      form_bmp_header(header, width, height, false);
      bmp_buffer  = (uint8_t*)malloc(width * height * 3 + 54);
      if (!bmp_buffer)
         goto finish;

      memcpy(bmp_buffer, header, 54 * sizeof(uint8_t));
      memcpy(bmp_buffer + 54,
            bit24_image,
            width * height * 3 * sizeof(uint8_t));
      buffer_bytes = sizeof(uint8_t) * (width * height * 3 + 54);
   }
   else
   {
      pitch        = width * 3;
      bmp_buffer   = rpng_save_image_bgr24_string(
            bit24_image + width * (height-1) * 3,
            width, height, (signed)-pitch, &buffer_bytes);
   }

   bmp64_buffer    = base64((void *)bmp_buffer,
         sizeof(uint8_t) * buffer_bytes,
         &bmp64_length);

   if (!bmp64_buffer)
      goto finish;

   jsonwriter = rjsonwriter_open_memory();
   if (!jsonwriter)
      goto finish;

   rjsonwriter_add_start_object(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_string(jsonwriter, "image");
   rjsonwriter_add_colon(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_string_len(jsonwriter, bmp64_buffer, bmp64_length);

   /* Form request... */
   if (system_label)
   {
      rjsonwriter_add_comma(jsonwriter);
      rjsonwriter_add_space(jsonwriter);
      rjsonwriter_add_string(jsonwriter, "label");
      rjsonwriter_add_colon(jsonwriter);
      rjsonwriter_add_space(jsonwriter);
      rjsonwriter_add_string(jsonwriter, system_label);
   }

   rjsonwriter_add_comma(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_string(jsonwriter, "state");
   rjsonwriter_add_colon(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_start_object(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_string(jsonwriter, "paused");
   rjsonwriter_add_colon(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_unsigned(jsonwriter, (paused ? 1 : 0));
   {
      static const char* state_labels[] = { "b", "y", "select", "start", "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3" };
      int i;
      for (i = 0; i < ARRAY_SIZE(state_labels); i++)
      {
         rjsonwriter_add_comma(jsonwriter);
         rjsonwriter_add_space(jsonwriter);
         rjsonwriter_add_string(jsonwriter, state_labels[i]);
         rjsonwriter_add_colon(jsonwriter);
         rjsonwriter_add_space(jsonwriter);
#ifdef HAVE_ACCESSIBILITY
         rjsonwriter_add_unsigned(jsonwriter,
               (p_rarch->ai_gamepad_state[i] ? 1 : 0)
               );
#else
         rjsonwriter_add_unsigned(jsonwriter, 0);
#endif
      }
   }
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_end_object(jsonwriter);
   rjsonwriter_add_space(jsonwriter);
   rjsonwriter_add_end_object(jsonwriter);

   json_buffer = rjsonwriter_get_memory_buffer(jsonwriter, NULL);
   if (!json_buffer)
      goto finish; /* ran out of memory */

#ifdef DEBUG
   if (p_rarch->ai_service_auto != 2)
      RARCH_LOG("Request size: %d\n", bmp64_length);
#endif
   {
      char new_ai_service_url[PATH_MAX_LENGTH];
      char separator                  = '?';
      unsigned ai_service_source_lang = settings->uints.ai_service_source_lang;
      unsigned ai_service_target_lang = settings->uints.ai_service_target_lang;
      const char *ai_service_url      = settings->arrays.ai_service_url;

      strlcpy(new_ai_service_url, ai_service_url, sizeof(new_ai_service_url));

      /* if query already exists in url, then use &'s instead */
      if (strrchr(new_ai_service_url, '?'))
          separator = '&';

      /* source lang */
      if (ai_service_source_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_source = ai_service_get_str(
               (enum translation_lang)ai_service_source_lang);

         if (!string_is_empty(lang_source))
         {
            char temp_string[PATH_MAX_LENGTH];
            snprintf(temp_string,
                  sizeof(temp_string),
                  "%csource_lang=%s", separator, lang_source);
            separator = '&';
            strlcat(new_ai_service_url,
                  temp_string, sizeof(new_ai_service_url));
         }
      }

      /* target lang */
      if (ai_service_target_lang != TRANSLATION_LANG_DONT_CARE)
      {
         const char *lang_target = ai_service_get_str(
               (enum translation_lang)ai_service_target_lang);

         if (!string_is_empty(lang_target))
         {
            char temp_string[PATH_MAX_LENGTH];
            snprintf(temp_string,
                  sizeof(temp_string),
                  "%ctarget_lang=%s", separator, lang_target);
            separator = '&';

            strlcat(new_ai_service_url, temp_string,
                  sizeof(new_ai_service_url));
         }
      }

      /* mode */
      {
         char temp_string[PATH_MAX_LENGTH];
         const char *mode_chr                    = NULL;
         unsigned ai_service_mode                = settings->uints.ai_service_mode;
         /*"image" is included for backwards compatability with
          * vgtranslate < 1.04 */

         temp_string[0] = '\0';

         switch (ai_service_mode)
         {
            case 0:
               if (use_overlay)
                  mode_chr = "image,png,png-a";
               else
                  mode_chr = "image,png";
               break;
            case 1:
               mode_chr    = "sound,wav";
               break;
            case 2:
               mode_chr    = "text";
               break;
            case 3:
               if (use_overlay)
                  mode_chr = "image,png,png-a,sound,wav";
               else
                  mode_chr = "image,png,sound,wav";
               break;
            default:
               break;
         }

         snprintf(temp_string,
               sizeof(temp_string),
               "%coutput=%s", separator, mode_chr);
         separator = '&';

         strlcat(new_ai_service_url, temp_string,
                 sizeof(new_ai_service_url));
      }
#ifdef DEBUG
      if (p_rarch->ai_service_auto != 2)
         RARCH_LOG("SENDING... %s\n", new_ai_service_url);
#endif
      task_push_http_post_transfer(new_ai_service_url,
            json_buffer, true, NULL, handle_translation_cb, NULL);
   }

   error = false;
finish:
   if (bit24_image_prev)
      free(bit24_image_prev);
   if (bit24_image)
      free(bit24_image);

   if (scaler)
      free(scaler);

   if (bmp_buffer)
      free(bmp_buffer);

   if (bmp64_buffer)
      free(bmp64_buffer);
   if (system_label)
      free(system_label);
   if (jsonwriter)
      rjsonwriter_free(jsonwriter);
   return !error;
}

#ifdef HAVE_ACCESSIBILITY
static bool is_narrator_running(struct rarch_state *p_rarch,
      bool accessibility_enable)
{
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
   {
      frontend_ctx_driver_t *frontend = 
         frontend_state_get_ptr()->current_frontend_ctx;
      if (frontend && frontend->is_narrator_running)
         return frontend->is_narrator_running();
   }
   return true;
}
#endif

#endif

/**
 * command_event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
static bool command_event_disk_control_append_image(
      runloop_state_t *runloop_st,
      rarch_system_info_t *sys_info,
      const char *path)
{
   input_driver_state_t *input_st = input_state_get_ptr();
   if (  !sys_info ||
         !disk_control_append_image(&sys_info->disk_control, path))
      return false;

#ifdef HAVE_THREADS
   if (runloop_st->use_sram)
      autosave_deinit();
#endif

   /* TODO/FIXME: Need to figure out what to do with subsystems case. */
   if (path_is_empty(RARCH_PATH_SUBSYSTEM))
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      path_set(RARCH_PATH_NAMES, path);
      runloop_path_fill_names(runloop_st, input_st);
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);

   return true;
}

static void command_event_deinit_core(
      struct rarch_state *p_rarch,
      bool reinit)
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

   RARCH_LOG("[Core]: Unloading core symbols..\n");
   uninit_libretro_symbols(p_rarch, &runloop_st->current_core);
   runloop_st->current_core.symbols_inited = false;

   /* Restore original refresh rate, if it has been changed
    * automatically in SET_SYSTEM_AV_INFO */
   if (video_st->video_refresh_rate_original)
      video_display_server_restore_refresh_rate();

   if (reinit)
      driver_uninit(p_rarch, DRIVERS_CMD_ALL);

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

   if (     runloop_st->remaps_core_active
         || runloop_st->remaps_content_dir_active
         || runloop_st->remaps_game_active
      )
   {
      input_remapping_deinit();
      input_remapping_set_defaults(true);
   }
   else
      input_remapping_restore_global_config(true);
}

static bool event_init_content(
      settings_t *settings,
      input_driver_state_t *input_st)
{
   runloop_state_t *runloop_st                  = &runloop_state;
   bool contentless                             = false;
   bool is_inited                               = false;
#ifdef HAVE_CHEEVOS
   bool cheevos_enable                          =
      settings->bools.cheevos_enable;
   bool cheevos_hardcore_mode_enable            =
      settings->bools.cheevos_hardcore_mode_enable;
#endif
   global_t   *global                           = global_get_ptr();
   const enum rarch_core_type current_core_type = runloop_st->current_core_type;

   content_get_status(&contentless, &is_inited);

   runloop_st->use_sram   = (current_core_type == CORE_TYPE_PLAIN);

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (current_core_type == CORE_TYPE_DUMMY)
      return true;

   content_set_subsystem_info();

   content_get_status(&contentless, &is_inited);

   /* If core is contentless, just initialise SRAM
    * interface, otherwise fill all content-related
    * paths */
   if (contentless)
      path_init_savefile_internal(global);
   else
      runloop_path_fill_names(runloop_st, input_st);

   if (!content_init())
      return false;

   command_event_set_savestate_auto_index(settings, global);

   if (!event_load_save_files(runloop_st->is_sram_load_disabled))
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
      if (global && settings->bools.savestate_auto_load)
         command_event_load_auto_state(global);

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


static void command_event_runtime_log_deinit(
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

static void command_event_runtime_log_init(runloop_state_t *runloop_st)
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

static INLINE float runloop_set_frame_limit(
      const struct retro_system_av_info *av_info,
      float fastforward_ratio)
{
   if (fastforward_ratio < 1.0f)
      return 0.0f;
   return (retro_time_t)roundf(1000000.0f / 
         (av_info->timing.fps * fastforward_ratio));
}

static INLINE float runloop_get_fastforward_ratio(
      settings_t *settings,
      struct retro_fastforwarding_override *fastmotion_override)
{
   if (      fastmotion_override->fastforward 
         && (fastmotion_override->ratio >= 0.0f))
      return fastmotion_override->ratio;
   return settings->floats.fastforward_ratio;
}

static bool command_event_init_core(
      settings_t *settings,
      struct rarch_state *p_rarch,
      input_driver_state_t *input_st,
      enum rarch_core_type type)
{
   runloop_state_t *runloop_st     = &runloop_state;
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

   if (!init_libretro_symbols(p_rarch, runloop_st,
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

   fill_pathname_join_concat_noext(
         video_st->title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         " ",
         sys_info->info.library_name,
         sizeof(video_st->title_buf));
   strlcat(video_st->title_buf, " ",
         sizeof(video_st->title_buf));
   strlcat(video_st->title_buf,
         sys_info->info.library_version,
         sizeof(video_st->title_buf));

   strlcpy(sys_info->valid_extensions,
         sys_info->info.valid_extensions ?
         sys_info->info.valid_extensions : DEFAULT_EXT,
         sizeof(sys_info->valid_extensions));

#ifdef HAVE_CONFIGFILE
   if (auto_overrides_enable)
      runloop_st->overrides_active =
         config_load_override(&runloop_st->system);
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
   video_st->shader_presets_need_reload       = true;
   runloop_st->shader_delay_timer.timer_begin = false; /* not initialized */
   runloop_st->shader_delay_timer.timer_end   = false; /* not expired */
#endif

   /* reset video format to libretro's default */
   video_st->pix_fmt = RETRO_PIXEL_FORMAT_0RGB1555;

   runloop_st->current_core.retro_set_environment(retroarch_environment_cb);

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
   path_set_redirect(p_rarch, settings);

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
      runloop_st->core_running = false;
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

   command_event_runtime_log_init(runloop_st);
   return true;
}

#ifdef HAVE_CONFIGFILE
/**
 * command_event_save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool command_event_save_core_config(
      const char *dir_menu_config,
      const char *rarch_path_config)
{
   char msg[128];
   char config_name[PATH_MAX_LENGTH];
   char config_path[PATH_MAX_LENGTH];
   char config_dir[PATH_MAX_LENGTH];
   bool found_path                 = false;
   bool overrides_active           = false;
   const char *core_path           = NULL;
   runloop_state_t *runloop_st     = &runloop_state;

   msg[0]                          = '\0';
   config_dir[0]                   = '\0';

   if (!string_is_empty(dir_menu_config))
      strlcpy(config_dir, dir_menu_config, sizeof(config_dir));
   else if (!string_is_empty(rarch_path_config)) /* Fallback */
      fill_pathname_basedir(config_dir, rarch_path_config,
            sizeof(config_dir));

   if (string_is_empty(config_dir))
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("[Config]: %s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      return false;
   }

   core_path                       = path_get(RARCH_PATH_CORE);
   config_name[0]                  = '\0';
   config_path[0]                  = '\0';

   /* Infer file name based on libretro core. */
   if (path_is_valid(core_path))
   {
      unsigned i;
      RARCH_LOG("%s\n", msg_hash_to_str(MSG_USING_CORE_NAME_FOR_NEW_CONFIG));

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];

         fill_pathname_base_noext(
               config_name,
               core_path,
               sizeof(config_name));

         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));

         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
         {
            tmp[0] = '\0';
            strlcpy(tmp, ".cfg", sizeof(tmp));
         }

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_is_valid(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   if (!found_path)
   {
      /* Fallback to system time... */
      RARCH_WARN("[Config]: %s\n",
            msg_hash_to_str(MSG_CANNOT_INFER_NEW_CONFIG_PATH));
      fill_dated_filename(config_name, ".cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if (runloop_st->overrides_active)
   {
      /* Overrides block config file saving,
       * make it appear as overrides weren't enabled
       * for a manual save. */
      runloop_st->overrides_active      = false;
      overrides_active                  = true;
   }

#ifdef HAVE_CONFIGFILE
   command_event_save_config(config_path, msg, sizeof(msg));
#endif

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   runloop_st->overrides_active = overrides_active;

   return true;
}

/**
 * event_save_current_config:
 *
 * Saves current configuration file to disk, and (optionally)
 * autosave state.
 **/
static void command_event_save_current_config(
      enum override_type type)
{
   runloop_state_t *runloop_st     = &runloop_state;

   switch (type)
   {
      case OVERRIDE_NONE:
         {
            if (path_is_empty(RARCH_PATH_CONFIG))
            {
               char msg[128];
               msg[0] = '\0';
               strcpy_literal(msg, "[Config]: Config directory not set, cannot save configuration.");
               runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else
            {
               char msg[256];
               msg[0] = '\0';
               command_event_save_config(path_get(RARCH_PATH_CONFIG), msg, sizeof(msg));
               runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }
         break;
      case OVERRIDE_GAME:
      case OVERRIDE_CORE:
      case OVERRIDE_CONTENT_DIR:
         {
            char msg[128];
            msg[0] = '\0';
            if (config_save_overrides(type, &runloop_st->system))
            {
               strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_SAVED_SUCCESSFULLY), sizeof(msg));
               /* set overrides to active so the original config can be
                  restored after closing content */
               runloop_st->overrides_active = true;
            }
            else
               strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ERROR_SAVING), sizeof(msg));
            RARCH_LOG("[Config - Overrides]: %s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
   }
}
#endif

static bool command_event_main_state(unsigned cmd)
{
   retro_ctx_size_info_t info;
   char msg[128];
   char state_path[16384];
   const global_t *global      = global_get_ptr();
   settings_t *settings        = config_get_ptr();
   bool ret                    = false;
   bool push_msg               = true;

   state_path[0] = msg[0]      = '\0';

   retroarch_get_current_savestate_path(state_path, sizeof(state_path));

   core_serialize_size(&info);

   if (info.size)
   {
      switch (cmd)
      {
         case CMD_EVENT_SAVE_STATE:
         case CMD_EVENT_SAVE_STATE_TO_RAM:
            {
               video_driver_state_t *video_st                 = 
                  video_state_get_ptr();
               bool savestate_auto_index                      =
                     settings->bools.savestate_auto_index;
               unsigned savestate_max_keep                    =
                     settings->uints.savestate_max_keep;
               bool frame_time_counter_reset_after_save_state =
                     settings->bools.frame_time_counter_reset_after_save_state;

               if (cmd == CMD_EVENT_SAVE_STATE)
                  content_save_state(state_path, true, false);
               else
                  content_save_state_to_ram();

               /* Clean up excess savestates if necessary */
               if (savestate_auto_index && (savestate_max_keep > 0))
                  command_event_set_savestate_garbage_collect(global,
                        settings->uints.savestate_max_keep,
                        settings->bools.show_hidden_files
                        );

               if (frame_time_counter_reset_after_save_state)
                  video_st->frame_time_count = 0;

               ret      = true;
               push_msg = false;
            }
            break;
         case CMD_EVENT_LOAD_STATE:
         case CMD_EVENT_LOAD_STATE_FROM_RAM:
            {
               bool res = false;
               if (cmd == CMD_EVENT_LOAD_STATE)
                  res = content_load_state(state_path, false, false);
               else
                  res = content_load_state_from_ram();

               if (res)
               {
#ifdef HAVE_CHEEVOS
                  if (rcheevos_hardcore_active())
                  {
                     rcheevos_pause_hardcore();
                     runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
#endif
                  ret = true;
#ifdef HAVE_NETWORKING
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif
                  {
                     video_driver_state_t *video_st                 = 
                        video_state_get_ptr();
                     bool frame_time_counter_reset_after_load_state =
                        settings->bools.frame_time_counter_reset_after_load_state;
                     if (frame_time_counter_reset_after_load_state)
                        video_st->frame_time_count = 0;
                  }
               }
            }
            push_msg = false;
            break;
         case CMD_EVENT_UNDO_LOAD_STATE:
            command_event_undo_load_state(msg, sizeof(msg));
            ret = true;
            break;
         case CMD_EVENT_UNDO_SAVE_STATE:
            command_event_undo_save_state(msg, sizeof(msg));
            ret = true;
            break;
      }
   }
   else
      strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   if (push_msg)
      runloop_msg_queue_push(msg, 2, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (!string_is_empty(msg))
      RARCH_LOG("%s\n", msg);

   return ret;
}


static void command_event_reinit(const int flags)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
   video_driver_state_t *video_st = 
      video_state_get_ptr();
#ifdef HAVE_MENU
   bool video_fullscreen          = settings->bools.video_fullscreen;
   bool adaptive_vsync            = settings->bools.video_adaptive_vsync;
   unsigned swap_interval         = settings->uints.video_swap_interval;
   gfx_display_t *p_disp          = disp_get_ptr();
#endif
   enum input_game_focus_cmd_type 
      game_focus_cmd              = GAME_FOCUS_CMD_REAPPLY;
   const input_device_driver_t 
      *joypad                     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t 
      *sec_joypad                 = input_st->secondary_joypad;
#else
   const input_device_driver_t 
      *sec_joypad                 = NULL;
#endif

   video_driver_reinit(flags);
   /* Poll input to avoid possibly stale data to corrupt things. */
   if (  joypad && joypad->poll)
      joypad->poll();
   if (  sec_joypad && sec_joypad->poll)
      sec_joypad->poll();
   if (  input_st->current_driver &&
         input_st->current_driver->poll)
      input_st->current_driver->poll(input_st->current_data);
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);

#ifdef HAVE_MENU
   p_disp->framebuf_dirty = true;
   if (video_fullscreen)
      video_driver_hide_mouse();
   if (     menu_state_get_ptr()->alive 
         && video_st->current_video->set_nonblock_state)
      video_st->current_video->set_nonblock_state(
            video_st->data, false,
            video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
            adaptive_vsync,
            swap_interval);
#endif
}

static void runloop_pause_checks(void)
{
#ifdef HAVE_DISCORD
   discord_userdata_t userdata;
#endif
   runloop_state_t *runloop_st    = &runloop_state;
   bool is_paused                 = runloop_st->paused;
   bool is_idle                   = runloop_st->idle;
#if defined(HAVE_GFX_WIDGETS)
   video_driver_state_t *video_st = video_state_get_ptr();
   bool widgets_active            = dispwidget_get_ptr()->active;
   if (widgets_active)
      video_st->widgets_paused    = is_paused;
#endif

   if (is_paused)
   {
      RARCH_LOG("[Core]: %s\n", msg_hash_to_str(MSG_PAUSED));

#if defined(HAVE_GFX_WIDGETS)
      if (!widgets_active)
#endif
         runloop_msg_queue_push(msg_hash_to_str(MSG_PAUSED), 1,
               1, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);


      if (!is_idle)
         video_driver_cached_frame();

#ifdef HAVE_DISCORD
      userdata.status = DISCORD_PRESENCE_GAME_PAUSED;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
#endif

#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_MENU);
#endif
   }
   else
   {
      RARCH_LOG("[Core]: %s\n", msg_hash_to_str(MSG_UNPAUSED));

#ifdef HAVE_LAKKA
      set_cpu_scaling_signal(CPUSCALING_EVENT_FOCUS_CORE);
#endif
   }

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (dispwidget_get_ptr()->ai_service_overlay_state == 1)
      gfx_widgets_ai_service_overlay_unload();
#endif
}

static void runloop_frame_time_free(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
   memset(&runloop_st->frame_time, 0,
         sizeof(struct retro_frame_time_callback));
   runloop_st->frame_time_last    = 0;
   runloop_st->max_frames         = 0;
}

static void runloop_audio_buffer_status_free(void)
{
   runloop_state_t *runloop_st    = &runloop_state;
   memset(&runloop_st->audio_buffer_status, 0,
         sizeof(struct retro_audio_buffer_status_callback));
   runloop_st->audio_latency = 0;
}

static void runloop_fastmotion_override_free(runloop_state_t *runloop_st)
{
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

static void runloop_core_options_cb_free(runloop_state_t *runloop_st)
{
   /* Only a single core options callback is used at present */
   runloop_st->core_options_callback.update_display = NULL;
}

static void runloop_system_info_free(runloop_state_t *runloop_st)
{
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

static bool libretro_get_system_info(
      const char *path,
      struct retro_system_info *info,
      bool *load_no_content);

#ifdef HAVE_RUNAHEAD
static void runloop_runahead_clear_variables(runloop_state_t *runloop_st)
{
   video_driver_state_t 
      *video_st                                  = video_state_get_ptr();
   runloop_st->runahead_save_state_size          = 0;
   runloop_st->runahead_save_state_size_known    = false;
   video_st->runahead_is_active                  = true;
   runloop_st->runahead_available                = true;
   runloop_st->runahead_secondary_core_available = true;
   runloop_st->runahead_force_input_dirty        = true;
   runloop_st->runahead_last_frame_count         = 0;
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
   bool boolean                = false;
   struct rarch_state *p_rarch = &rarch_st;
   runloop_state_t *runloop_st = &runloop_state;
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();
#endif
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
   settings_t *settings        = config_get_ptr();

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
            if (!recording_is_enabled())
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
                           p_rarch->accessibility_enabled))
                     accessibility_speak_priority(p_rarch,
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
               if (p_rarch->ai_service_auto == 2)
               {
                  /* Auto mode was turned on, but we pressed the
                   * toggle button, so turn it off now. */
                  p_rarch->ai_service_auto = 0;
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
         if (streaming_is_enabled())
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
         if (recording_is_enabled())
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
         {
            bool success                        = false;
            runloop_st->subsystem_current_count = 0;
            content_clear_subsystem();
            success = command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
            (void)success;

#ifndef HAVE_DYNAMIC
            command_event(CMD_EVENT_QUIT, NULL);
#else
            if (!success)
               return false;
#endif
            break;
         }
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
      case CMD_EVENT_LOAD_SECOND_CORE:
         if (!runloop_st->core_running ||
             !runloop_st->runahead_secondary_core_available)
            return false;
         if (runloop_st->secondary_lib_handle)
            return true;
         if (!secondary_core_ensure_exists(p_rarch, runloop_st, settings))
         {
            secondary_core_destroy(runloop_st);
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
               return false;
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
         return false;
      case CMD_EVENT_SAVE_STATE:
      case CMD_EVENT_SAVE_STATE_TO_RAM:
         {
            bool savestate_auto_index = settings->bools.savestate_auto_index;
            int state_slot            = settings->ints.state_slot;

            if (savestate_auto_index)
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

            command_event_runtime_log_deinit(runloop_st,
                  settings->bools.content_runtime_log,
                  settings->bools.content_runtime_log_aggregate,
                  settings->paths.directory_runtime_log,
                  settings->paths.directory_playlist);
            command_event_save_auto_state(settings->bools.savestate_auto_save,
                  global, runloop_st->current_core_type);

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

            if (     runloop_st->remaps_core_active
                  || runloop_st->remaps_content_dir_active
                  || runloop_st->remaps_game_active
               )
            {
               input_remapping_deinit();
               input_remapping_set_defaults(true);
            }
            else
               input_remapping_restore_global_config(true);

            if (is_inited)
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
#ifdef HAVE_DISCORD
            if (discord_is_inited)
            {
               discord_userdata_t userdata;
               userdata.status = DISCORD_PRESENCE_NETPLAY_NETPLAY_STOPPED;
               command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
               userdata.status = DISCORD_PRESENCE_MENU;
               command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
            }
#endif
#ifdef HAVE_DYNAMIC
            path_clear(RARCH_PATH_CORE);
            runloop_system_info_free(&runloop_state);
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

         if (rcheevos_hardcore_active())
            runloop_st->slowmotion = false;
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
            state_manager_event_deinit(&runloop_st->rewind_st);
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
         recording_state.enable = false;
         streaming_set_state(false);
         if (!recording_deinit())
            return false;
         break;
      case CMD_EVENT_RECORD_INIT:
         recording_state.enable = true;
         if (!recording_init(settings, p_rarch))
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
            RARCH_LOG("[Playlist]: %s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_history);
            playlist_config_set_path(&playlist_config, path_content_history);
            g_defaults.content_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.content_history, PLAYLIST_SORT_MODE_OFF);

            RARCH_LOG("[Playlist]: %s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_music_history);
            playlist_config_set_path(&playlist_config, path_content_music_history);
            g_defaults.music_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.music_history, PLAYLIST_SORT_MODE_OFF);

#if defined(HAVE_FFMPEG) || defined(HAVE_MPV)
            RARCH_LOG("[Playlist]: %s: [%s].\n",
                  msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
                  path_content_video_history);
            playlist_config_set_path(&playlist_config, path_content_video_history);
            g_defaults.video_history = playlist_init(&playlist_config);
            playlist_set_sort_mode(
                  g_defaults.video_history, PLAYLIST_SORT_MODE_OFF);
#endif

#ifdef HAVE_IMAGEVIEWER
            RARCH_LOG("[Playlist]: %s: [%s].\n",
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

            command_event_runtime_log_deinit(runloop_st,
                  settings->bools.content_runtime_log,
                  settings->bools.content_runtime_log_aggregate,
                  settings->paths.directory_runtime_log,
                  settings->paths.directory_playlist);
            content_reset_savestate_backups();
            hwr = VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
#ifdef HAVE_CHEEVOS
            rcheevos_unload();
#endif
            command_event_deinit_core(p_rarch, true);

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
            enum rarch_core_type *type    = (enum rarch_core_type*)data;
            rarch_system_info_t *sys_info = &runloop_st->system;
            input_driver_state_t *input_st= input_state_get_ptr();

            content_reset_savestate_backups();

            /* Ensure that disk control interface is reset */
            if (sys_info)
               disk_control_set_ext_callback(&sys_info->disk_control, NULL);

            if (!type || !command_event_init_core(settings, p_rarch, input_st, *type))
               return false;
         }
         break;
      case CMD_EVENT_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case CMD_EVENT_VIDEO_SET_BLOCKING_STATE:
         {
            bool adaptive_vsync       = settings->bools.video_adaptive_vsync;
            unsigned swap_interval    = settings->uints.video_swap_interval;
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
         driver_uninit(p_rarch, DRIVER_AUDIO_MASK);
         drivers_init(p_rarch, settings, DRIVER_AUDIO_MASK, verbosity_is_enabled());
#if defined(HAVE_AUDIOMIXER)
         audio_driver_load_system_sounds();
#endif
         break;
      case CMD_EVENT_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_SHUTTING_DOWN), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -P now");
#endif
         break;
      case CMD_EVENT_REBOOT:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push(msg_hash_to_str(MSG_VALUE_REBOOTING), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);
         command_event(CMD_EVENT_QUIT, NULL);
         system("shutdown -r now");
#endif
         break;
      case CMD_EVENT_RESUME:
#ifdef HAVE_MENU
         retroarch_menu_running_finished(false);
#endif
         if (p_rarch->main_ui_companion_is_on_foreground)
         {
#ifdef HAVE_QT
            bool desktop_menu_enable = settings->bools.desktop_menu_enable;
            bool ui_companion_toggle = settings->bools.ui_companion_toggle;
#else
            bool desktop_menu_enable = false;
            bool ui_companion_toggle = false;
#endif
            ui_companion_driver_toggle(p_rarch, desktop_menu_enable, ui_companion_toggle, false);
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
         config_set_defaults(&p_rarch->g_extern);
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
            boolean                                      = runloop_st->paused;
            boolean                                      = !boolean;

#ifdef HAVE_ACCESSIBILITY
            if (is_accessibility_enabled(
                  accessibility_enable,
                  p_rarch->accessibility_enabled))
            {
               if (boolean)
                  accessibility_speak_priority(p_rarch,
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     (char*)msg_hash_to_str(MSG_PAUSED), 10);
               else
                  accessibility_speak_priority(p_rarch,
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
         boolean                 = false;
         runloop_st->paused      = boolean;
         runloop_pause_checks();
         break;
      case CMD_EVENT_PAUSE:
         boolean                 = true;
         runloop_st->paused      = boolean;
         runloop_pause_checks();
         break;
      case CMD_EVENT_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (menu_st->alive)
         {
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_STOP, NULL);
            else
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
         else
         {
            bool menu_pause_libretro  = settings->bools.menu_pause_libretro;
            if (menu_pause_libretro)
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
#endif
         break;
#ifdef HAVE_NETWORKING
      case CMD_EVENT_NETPLAY_GAME_WATCH:
         netplay_driver_ctl(RARCH_NETPLAY_CTL_GAME_WATCH, NULL);
         break;
      case CMD_EVENT_NETPLAY_DEINIT:
         deinit_netplay(p_rarch);
         break;
      case CMD_EVENT_NETWORK_INIT:
         network_init();
         break;
         /* init netplay manually */
      case CMD_EVENT_NETPLAY_INIT:
         {
            char       *hostname       = (char*)data;
            const char *netplay_server = settings->paths.netplay_server;
            unsigned netplay_port      = settings->uints.netplay_port;

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            if (!init_netplay(p_rarch,
                     settings,
                     NULL,
                     hostname
                     ? hostname
                     : netplay_server, netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               return false;
            }

            /* Disable rewind & SRAM autosave if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* Initialize netplay via lobby when content is loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT:
         {
            /* buf is expected to be address|port */
            static struct string_list *hostname = NULL;
            char *buf                           = (char *)data;
            unsigned netplay_port               = settings->uints.netplay_port;

            hostname                            = string_split(buf, "|");

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            RARCH_LOG("[Netplay]: Connecting to %s:%d (direct)\n",
                  hostname->elems[0].data, !string_is_empty(hostname->elems[1].data)
                  ? atoi(hostname->elems[1].data)
                  : netplay_port);

            if (!init_netplay(
                     p_rarch,
                     settings,
                     NULL,
                     hostname->elems[0].data,
                     !string_is_empty(hostname->elems[1].data)
                     ? atoi(hostname->elems[1].data)
                     : netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               string_list_free(hostname);
               return false;
            }

            string_list_free(hostname);

            /* Disable rewind if it was enabled
               TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
         /* init netplay via lobby when content is not loaded */
      case CMD_EVENT_NETPLAY_INIT_DIRECT_DEFERRED:
         {
            static struct string_list *hostname = NULL;
            /* buf is expected to be address|port */
            char *buf                           = (char *)data;
            unsigned netplay_port               = settings->uints.netplay_port;

            hostname = string_split(buf, "|");

            command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);

            RARCH_LOG("[Netplay]: Connecting to %s:%d (deferred)\n",
                  hostname->elems[0].data, !string_is_empty(hostname->elems[1].data)
                  ? atoi(hostname->elems[1].data)
                  : netplay_port);

            if (!init_netplay_deferred(p_rarch,
                     hostname->elems[0].data,
                     !string_is_empty(hostname->elems[1].data)
                     ? atoi(hostname->elems[1].data)
                     : netplay_port))
            {
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
               string_list_free(hostname);
               return false;
            }

            string_list_free(hostname);

            /* Disable rewind if it was enabled
             * TODO/FIXME: Add a setting for these tweaks */
#ifdef HAVE_REWIND
            state_manager_event_deinit(&runloop_st->rewind_st);
#endif
#ifdef HAVE_THREADS
            autosave_deinit();
#endif
         }
         break;
      case CMD_EVENT_NETPLAY_ENABLE_HOST:
         {
#ifdef HAVE_MENU
            bool contentless  = false;
            bool is_inited    = false;

            content_get_status(&contentless, &is_inited);

            if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
               command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_SERVER, NULL);

            /* If we haven't yet started, this will load on its own */
            if (!is_inited)
            {
               runloop_msg_queue_push(
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED),
                     1, 480, true,
                     NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return false;
            }

            /* Enable Netplay itself */
            if (!command_event(CMD_EVENT_NETPLAY_INIT, NULL))
               return false;
#endif
            break;
         }
      case CMD_EVENT_NETPLAY_DISCONNECT:
         {
            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISCONNECT, NULL);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_DISABLE, NULL);

            {
               bool rewind_enable                  = settings->bools.rewind_enable;
               unsigned autosave_interval          = settings->uints.autosave_interval;

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
         }
      case CMD_EVENT_NETPLAY_HOST_TOGGLE:
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL))
            command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
         else if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL) &&
               !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_SERVER, NULL) &&
               netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_CONNECTED, NULL))
            command_event(CMD_EVENT_NETPLAY_DISCONNECT, NULL);
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
      case CMD_EVENT_NETPLAY_GAME_WATCH:
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
               rarch_system_info_t *
                  sys_info                = &runloop_st->system;
               /* Append disk image */
               bool success               =
                  command_event_disk_control_append_image(&runloop_state,
                        sys_info, path);

#if defined(HAVE_MENU)
               /* Appending a disk image may or may not affect
                * the disk tray eject status. If status has changed,
                * must refresh the disk options menu */
               if (initial_disk_ejected != disk_control_get_eject_state(&sys_info->disk_control))
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
            ui_companion_driver_toggle(p_rarch, desktop_menu_enable, ui_companion_toggle, true);
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
	    runloop_state_t *runloop_st         = &runloop_state;
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
            bool discord_enable        = settings ? settings->bools.discord_enable : false;
            const char *discord_app_id = settings ? settings->arrays.discord_app_id : NULL;
            discord_state_t *discord_st = &p_rarch->discord_st;

            if (!settings)
               return false;
            if (!discord_enable)
               return false;
            if (discord_st->ready)
               return true;
            discord_init(discord_st,
                  discord_app_id,
                  p_rarch->launch_arguments);
         }
#endif
         break;
      case CMD_EVENT_DISCORD_UPDATE:
         {
#ifdef HAVE_DISCORD
            discord_state_t *discord_st  = &p_rarch->discord_st;
            if (!data || !discord_st->ready)
               return false;

            discord_userdata_t *userdata = (discord_userdata_t*)data;

            if (discord_st->ready)
               discord_update(userdata->status);
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
                        p_rarch->accessibility_enabled))
                  accessibility_speak_priority(p_rarch,
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
                     p_rarch->accessibility_enabled) &&
                  ai_service_mode == 2 &&
                  is_narrator_running(p_rarch, accessibility_enable))
               accessibility_speak_priority(p_rarch,
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     "stopped.", 10);
            else
#endif
            {
               bool paused = runloop_st->paused;
               if (data)
                  paused = *((bool*)data);

               if (      p_rarch->ai_service_auto == 0
                     && !settings->bools.ai_service_pause)
                  p_rarch->ai_service_auto = 1;

               run_translation_service(settings,
                     p_rarch, paused);
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

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
	       runloop_state_t *runloop_st = &runloop_state;
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
         p_rarch->has_set_netplay_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         p_rarch->has_set_netplay_ip_address = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         p_rarch->has_set_netplay_ip_port = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         p_rarch->has_set_netplay_stateless_mode = true;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         p_rarch->has_set_netplay_check_frames = true;
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
   struct rarch_state            *p_rarch = &rarch_st;

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
	       runloop_state_t *runloop_st = &runloop_state;
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
         p_rarch->has_set_netplay_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         p_rarch->has_set_netplay_ip_address = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         p_rarch->has_set_netplay_ip_port = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         p_rarch->has_set_netplay_stateless_mode = false;
         break;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         p_rarch->has_set_netplay_check_frames = false;
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
                  (enum rarch_override_setting)(i), &j);
      }
      else
         retroarch_override_setting_unset(
               (enum rarch_override_setting)(i), NULL);
   }
}

static void global_free(struct rarch_state *p_rarch)
{
   global_t            *global                        = NULL;
   runloop_state_t *runloop_st                        = &runloop_state;

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

   global                                             = &p_rarch->g_extern;
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
   runloop_state_t *runloop_st  = &runloop_state;
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
      log_counters(p_rarch->perf_counters_rarch, p_rarch->perf_ptr_rarch);
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
   p_rarch->rarch_error_on_init     = false;
#ifdef HAVE_CONFIGFILE
   p_rarch->rarch_block_config_read = false;
#endif

   runloop_msg_queue_deinit();
   driver_uninit(p_rarch, DRIVERS_CMD_ALL);

   retro_main_log_file_deinit();

   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);
   task_queue_deinit();

   if (p_rarch->configuration_settings)
      free(p_rarch->configuration_settings);
   p_rarch->configuration_settings = NULL;

   ui_companion_driver_deinit(p_rarch);

   frontend_driver_shutdown(false);

   retroarch_deinit_drivers(p_rarch, &runloop_st->retro_ctx);
   p_rarch->ui_companion = NULL;
   frontend_driver_free();

   rtime_deinit();

#if defined(ANDROID)
   play_feature_delivery_deinit();
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
   runloop_state_t *runloop_st         = &runloop_state;
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

   libretro_free_system_info(&runloop_st->system.info);
   command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
   rarch_favorites_deinit();

   p_rarch->configuration_settings = (settings_t*)calloc(1, sizeof(settings_t));

   retroarch_deinit_drivers(p_rarch, &runloop_st->retro_ctx);
   retroarch_ctl(RARCH_CTL_STATE_FREE,  NULL);
   global_free(p_rarch);

   frontend_driver_init_first(data);

   if (runloop_st->is_inited)
      driver_uninit(p_rarch, DRIVERS_CMD_ALL);

#ifdef HAVE_THREAD_STORAGE
   sthread_tls_create(&p_rarch->rarch_tls);
   sthread_tls_set(&p_rarch->rarch_tls, MAGIC_POINTER);
#endif
   video_st->active              = true;
   audio_state_get_ptr()->active = true;

   {
      uint8_t i;
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

   ui_companion_driver_init_first(p_rarch->configuration_settings,
         p_rarch);

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
   runloop_state_t *runloop_st            = &runloop_state;
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
      if (string_is_equal(info[i].ident, ident))
         return &info[i];
      else if (string_is_equal(info[i].desc, ident))
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
         unsigned i, j, size;
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
               ? size
               : SUBSYSTEM_MAX_SUBSYSTEMS;
         }
         break;
      }
      default:
         return false;
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

      while (mmap_reduce(top_addr & ~desc->core.select, desc->core.disconnect) >> 1 > desc->core.len - 1)
         desc->core.disconnect |= mmap_highest_bit(top_addr & ~desc->core.select & ~desc->core.disconnect);

      desc->disconnect_mask = mmap_add_bits_down(desc->core.len - 1);
      desc->core.disconnect &= desc->disconnect_mask;

      while ((~desc->disconnect_mask) >> 1 & desc->core.disconnect)
      {
         desc->disconnect_mask >>= 1;
         desc->core.disconnect &= desc->disconnect_mask;
      }
   }

   return true;
}

static bool rarch_clear_all_thread_waits(
      unsigned clear_threads, void *data)
{
   if (clear_threads > 0)
      audio_driver_start(false);
   else
      audio_driver_stop();

   return true;
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
         RARCH_LOG("[Core Options]: Saved %s-specific core options to \"%s\"\n",
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
      RARCH_LOG("[Core Options]: Saved core options file to \"%s\"\n", path);
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

   if (!s ||
       (len < 1) ||
       string_is_empty(core_name) ||
       string_is_empty(game_name))
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
      new_path[0]             = '\0';

      fill_pathname_join(new_path,
            config_directory, core_name, sizeof(new_path));

      if (!path_is_directory(new_path))
         path_mkdir(new_path);
   }

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

static bool validate_game_options(
      const char *core_name,
      char *s, size_t len, bool mkdir)
{
   const char *game_name       = path_basename(path_get(RARCH_PATH_BASENAME));
   return validate_per_core_options(s, len, mkdir,
         core_name, game_name);
}

/**
 * game_specific_options:
 *
 * Returns: true (1) if a game specific core
 * options path has been found,
 * otherwise false (0).
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

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         game_options_path);
   *output = strdup(game_options_path);
   return true;
}

/**
 * folder_specific_options:
 *
 * Returns: true (1) if a folder specific core
 * options path has been found,
 * otherwise false (0).
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

   RARCH_LOG("%s %s\n",
         msg_hash_to_str(MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT),
         folder_options_path);
   *output = strdup(folder_options_path);
   return true;
}


/* Fetches core options path for current core/content
 * - path: path from which options should be read
 *   from/saved to
 * - src_path: in the event that 'path' file does not
 *   yet exist, provides source path from which initial
 *   options should be extracted
 *
 *   NOTE: caller must ensure 
 *   path and src_path are NULL-terminated
 *  */
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
      runloop_st->game_options_active   = true;
      runloop_st->folder_options_active = false;

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
      runloop_st->game_options_active   = false;
      runloop_st->folder_options_active = true;

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

      global_options_path[0]        = '\0';
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
      runloop_st->game_options_active   = false;
      runloop_st->folder_options_active = false;
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


/**
 * retroarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
static bool retroarch_environment_cb(unsigned cmd, void *data)
{
   unsigned p;
   struct rarch_state *p_rarch            = &rarch_st;
   runloop_state_t *runloop_st            = &runloop_state;

   settings_t         *settings           = config_get_ptr();
   rarch_system_info_t *system            = &runloop_st->system;
   bool ignore_environment_cb             = runloop_st->ignore_environment_cb;

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
               runloop_st->has_variable_update = true;
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

               snprintf(s, sizeof(s), "[Environ]: GET_VARIABLE %s:\n\t%s\n",
                     var->key, var->value ? var->value :
                           msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
               RARCH_LOG(s);
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
                     runloop_st->game_options_active,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->game_options_active   = false;
               runloop_st->folder_options_active = false;
               runloop_st->core_options          = NULL;
            }
            if ((new_vars = runloop_init_core_variables(
                  settings,
                  (const struct retro_variable *)data)))
               runloop_st->core_options = new_vars;
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
                     runloop_st->game_options_active,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->game_options_active   = false;
               runloop_st->folder_options_active = false;
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
                     runloop_st->game_options_active,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->game_options_active   = false;
               runloop_st->folder_options_active = false;
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
                     runloop_st->game_options_active,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->game_options_active   = false;
               runloop_st->folder_options_active = false;
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
                     runloop_st->game_options_active,
                     path_get(RARCH_PATH_CORE_OPTIONS),
                     runloop_st->core_options);
               runloop_st->game_options_active   = false;
               runloop_st->folder_options_active = false;
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
         RARCH_LOG("[Environ]: SET_MESSAGE: %s\n", msg->msg);
#if defined(HAVE_GFX_WIDGETS)
         if (dispwidget_get_ptr()->active)
            gfx_widget_set_libretro_message(
                  msg->msg,
                  roundf((float)msg->frames / 60.0f * 1000.0f));
         else
#endif
            runloop_msg_queue_push(msg->msg, 3, msg->frames,
                  true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
                  MESSAGE_QUEUE_CATEGORY_INFO);
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
                   * speaking, runloop_core_status_msg is not part
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
                  if (!runloop_core_status_msg.set ||
                      (runloop_core_status_msg.priority <= msg->priority))
                  {
                     if (!string_is_empty(msg->msg))
                     {
                        strlcpy(runloop_core_status_msg.str, msg->msg,
                              sizeof(runloop_core_status_msg.str));

                        runloop_core_status_msg.duration = (float)msg->duration;
                        runloop_core_status_msg.set      = true;
                     }
                     else
                     {
                        /* Ensure sane behaviour if core sends an
                         * empty message */
                        runloop_core_status_msg.str[0] = '\0';
                        runloop_core_status_msg.priority = 0;
                        runloop_core_status_msg.duration = 0.0f;
                        runloop_core_status_msg.set      = false;
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
                     if (dispwidget_get_ptr()->active)
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
                     if (dispwidget_get_ptr()->active)
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
            break;

         if (system)
            system->rotation = rotation;

         if (!video_driver_set_rotation(rotation))
            return false;
         break;
      }

      case RETRO_ENVIRONMENT_SHUTDOWN:
         RARCH_LOG("[Environ]: SHUTDOWN.\n");

         /* This case occurs when a core (internally) requests
          * a shutdown event. Must save runtime log file here,
          * since normal command.c CMD_EVENT_CORE_DEINIT event
          * will not occur until after the current content has
          * been cleared (causing log to be skipped) */
         command_event_runtime_log_deinit(runloop_st,
               settings->bools.content_runtime_log,
               settings->bools.content_runtime_log_aggregate,
               settings->paths.directory_runtime_log,
               settings->paths.directory_playlist);

         /* Similarly, since the CMD_EVENT_CORE_DEINIT will
          * be called *after* the runloop state has been
          * cleared, must also perform the following actions
          * here:
          * - Disable any active config overrides
          * - Unload any active input remaps */
#ifdef HAVE_CONFIGFILE
         if (runloop_st->overrides_active)
         {
            /* Reload the original config */
            config_unload_override();
            runloop_st->overrides_active = false;
         }
#endif
         if (     runloop_st->remaps_core_active
               || runloop_st->remaps_content_dir_active
               || runloop_st->remaps_game_active
            )
         {
            input_remapping_deinit();
            input_remapping_set_defaults(true);
         }
         else
            input_remapping_restore_global_config(true);

         runloop_st->shutdown_initiated      = true;
         runloop_st->core_shutdown_initiated = true;
         break;

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
                  char temp_path[PATH_MAX_LENGTH];
                  temp_path[0]     = '\0';

                  if (string_is_empty(dir_system))
                     RARCH_WARN("[Environ]: SYSTEM DIR is empty, assume CONTENT DIR %s\n",
                           fullpath);
                  fill_pathname_basedir(temp_path, fullpath, sizeof(temp_path));
                  dir_set(RARCH_DIR_SYSTEM, temp_path);
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

                        RARCH_LOG("\tRetroPad, Port %u, Button \"%s\" => \"%s\"\n",
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

         if (!dynamic_verify_hw_context(settings->arrays.video_driver,
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
         audio_driver_state_t 
            *audio_st                = audio_state_get_ptr();
         const struct 
            retro_audio_callback *cb = (const struct retro_audio_callback*)data;
         RARCH_LOG("[Environ]: SET_AUDIO_CALLBACK.\n");
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            return false;
#endif
         if (recording_state.data) /* A/V sync is a must. */
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
            bool video_fullscreen = settings->bools.video_fullscreen;
            int reinit_flags      = DRIVERS_CMD_ALL &
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK | DRIVER_MENU_MASK);

            RARCH_LOG("[Environ]: Setting audio latency to %u ms.\n", audio_latency_new);

            command_event(CMD_EVENT_REINIT, &reinit_flags);
            video_driver_set_aspect_ratio();

            /* Cannot continue recording with different parameters.
             * Take the easiest route out and just restart the recording. */
            if (recording_state.data)
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
         if (!input_st->current_driver->get_capabilities ||
!input_st->current_data)
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

         RARCH_LOG("[Environ]: GET_CAMERA_INTERFACE.\n");
         cb->start                        = driver_camera_start;
         cb->stop                         = driver_camera_stop;

         p_rarch->camera_cb               = *cb;

         if (cb->caps != 0)
            p_rarch->camera_driver_active = true;
         else
            p_rarch->camera_driver_active = false;
         break;
      }

      case RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE:
      {
         struct retro_location_callback *cb =
            (struct retro_location_callback*)data;

         RARCH_LOG("[Environ]: GET_LOCATION_INTERFACE.\n");
         cb->start                       = driver_location_start;
         cb->stop                        = driver_location_stop;
         cb->get_position                = driver_location_get_position;
         cb->set_interval                = driver_location_set_interval;

         if (system)
            system->location_cb          = *cb;

         p_rarch->location_driver_active = false;
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

         cb->perf_register    = performance_counter_register;
         cb->perf_start       = core_performance_counter_start;
         cb->perf_stop        = core_performance_counter_stop;
         cb->perf_log         = retro_perf_log;
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
            bool video_has_resolution_list        = video_display_server_has_resolution_list();
            bool video_switch_refresh_rate        = false;
            bool no_video_reinit                  = true;

            /* Refresh rate switch for regular displays */
            if (video_has_resolution_list)
            {
               float refresh_mod                  = 0.0f;
               float video_refresh_rate           = settings->floats.video_refresh_rate;
               unsigned video_swap_interval       = settings->uints.video_swap_interval;
               unsigned video_bfi                 = settings->uints.video_black_frame_insertion;
               bool video_windowed_full           = settings->bools.video_windowed_fullscreen;
               bool vrr_runloop_enable            = settings->bools.vrr_runloop_enable;

               /* Roundings to PAL & NTSC standards */
               refresh_rate = (refresh_rate > 54 && refresh_rate < 60) ? 59.94f : refresh_rate;
               refresh_rate = (refresh_rate > 49 && refresh_rate < 55) ? 50.00f : refresh_rate;

               /* Black frame insertion + swap interval multiplier */
               refresh_mod  = video_bfi + 1.0f;
               refresh_rate = (refresh_rate * refresh_mod * video_swap_interval);

               /* Fallback when target refresh rate is not exposed */
               if (!video_display_server_has_refresh_rate(refresh_rate))
                  refresh_rate = (60.0f * refresh_mod * video_swap_interval);

               /* Store original refresh rate on automatic change, and
                * restore it in deinit_core and main_quit, because not all
                * cores announce refresh rate via SET_SYSTEM_AV_INFO */
               if (!video_st->video_refresh_rate_original)
                  video_st->video_refresh_rate_original = video_refresh_rate;

               /* Try to switch display rate when:
                * - Not already at correct rate
                * - In exclusive fullscreen
                * - 'CRT SwitchRes' OFF & 'Sync to Exact Content Framerate' OFF
                */
               video_switch_refresh_rate = (
                     refresh_rate != video_refresh_rate &&
                     !crt_switch_resolution && !vrr_runloop_enable &&
                     video_fullscreen && !video_windowed_full);
            }

            no_video_reinit                       = (
                     crt_switch_resolution == 0
                  && video_switch_refresh_rate == false
                  && data
                  && ((*info)->geometry.max_width  == av_info->geometry.max_width)
                  && ((*info)->geometry.max_height == av_info->geometry.max_height));

            /* First set new refresh rate and display rate, then after REINIT do
             * another display rate change to make sure the change stays */
            if (video_switch_refresh_rate)
            {
               video_monitor_set_refresh_rate(refresh_rate);
               video_display_server_set_refresh_rate(refresh_rate);
            }

            /* When not doing video reinit, we also must not do input and menu
             * reinit, otherwise the input driver crashes and the menu gets
             * corrupted. */
            if (no_video_reinit)
               reinit_flags = 
                  DRIVERS_CMD_ALL & 
                  ~(DRIVER_VIDEO_MASK | DRIVER_INPUT_MASK |
                                        DRIVER_MENU_MASK);

            RARCH_LOG("[Environ]: SET_SYSTEM_AV_INFO: %ux%u, aspect: %.3f, fps: %.3f, sample rate: %.2f Hz.\n",
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
             * Take the easiest route out and just restart the recording. */
            if (recording_state.data)
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

            RARCH_LOG("Controller port: %u\n", i + 1);
            for (j = 0; j < info[i].num_types; j++)
               RARCH_LOG("   %s (ID: %u)\n", info[i].types[j].desc,
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
            const struct retro_memory_map *mmaps        =
               (const struct retro_memory_map*)data;
            rarch_memory_descriptor_t *descriptors = NULL;

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

            if (sizeof(void *) == 8)
               RARCH_LOG("   ndx flags  ptr              offset   start    select   disconn  len      addrspace\n");
            else
               RARCH_LOG("   ndx flags  ptr          offset   start    select   disconn  len      addrspace\n");

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

               RARCH_LOG("   %03u %s %p %08X %08X %08X %08X %08X %s\n",
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

            RARCH_LOG("[Environ]: SET_GEOMETRY: %ux%u, aspect: %.3f.\n",
                  geom->base_width, geom->base_height, geom->aspect_ratio);

            /* Forces recomputation of aspect ratios if
             * using core-dependent aspect ratios. */
            video_driver_set_aspect_ratio();

            /* TODO: Figure out what to do, if anything, with recording. */
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
         runloop_st->core_set_shared_context = true;
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
            RARCH_LOG("Core requested VFS version >= v%d, providing v%d\n", vfs_iface_info->required_interface_version, supported_vfs_version);
            vfs_iface_info->required_interface_version = supported_vfs_version;
            vfs_iface_info->iface                      = &vfs_iface;
            system->supports_vfs = true;
         }
         else
         {
            RARCH_WARN("Core requested VFS version v%d which is higher than what we support (v%d)\n", vfs_iface_info->required_interface_version, supported_vfs_version);
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
      }
      break;

      case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
      {
         int result           = 0;
         video_driver_state_t 
            *video_st         = video_state_get_ptr();
         audio_driver_state_t 
            *audio_st         = audio_state_get_ptr();
         if ( !audio_st->suspended &&
               audio_st->active)
            result |= 2;
         if (       video_st->active
               && !(video_st->current_video->frame == video_null.frame))
            result |= 1;
#ifdef HAVE_RUNAHEAD
         if (runloop_st->request_fast_savestate)
            result |= 4;
         if (audio_st->hard_disable)
            result |= 8;
#endif
#ifdef HAVE_NETWORKING
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_REPLAYING, NULL))
            result &= ~(1|2);
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
            result |= 4;
#endif
         if (data)
         {
            int* result_p = (int*)data;
            *result_p = result;
         }
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
         *(bool *)data = runloop_st->fastmotion;
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
         bool core_paused = runloop_st->paused;
         bool no_audio    = (audio_st->suspended || !audio_st->active);
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
         menu_opened = menu_state_get_ptr()->alive;
         if (menu_opened)
            core_paused = settings->bools.menu_pause_libretro;
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

         if (runloop_st->fastmotion)
         {
            throttle_state->mode  = RETRO_THROTTLE_FAST_FORWARD;
            throttle_state->rate *= runloop_get_fastforward_ratio(
                  settings, &runloop_st->fastmotion_override.current);
         }
         else if (runloop_st->slowmotion && !no_audio)
         {
            throttle_state->mode = RETRO_THROTTLE_SLOW_MOTION;
            throttle_state->rate /= (settings->floats.slowmotion_ratio > 0.0f ?
                  settings->floats.slowmotion_ratio : 1.0f);
         }

         /* VSync overrides the mode if the rate is limited by the display. */
         if (menu_opened || /* Menu currently always runs with vsync on. */
               (settings->bools.video_vsync && !runloop_st->force_nonblock
                     && !input_state_get_ptr()->nonblocking_flag))
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
            runloop_state_t *runloop_st = &runloop_state;
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
         float video_refresh_rate  = settings ? settings->floats.video_refresh_rate : 0.0;

         /* If the above function failed [possibly because it is not
          * implemented], use the refresh rate set in the config instead. */
         if (target_refresh_rate == 0.0f && video_refresh_rate != 0.0f)
            target_refresh_rate = video_refresh_rate;

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
         *(retro_environment_t *)data = rarch_clear_all_thread_waits;
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
               RARCH_ERR("[Environ]: Failed to retrieve extended game info\n");
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
static void libretro_get_environment_info(
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
   runloop_st->ignore_environment_cb = true;
   func(retroarch_environment_cb);
   runloop_st->ignore_environment_cb = false;
}

static dylib_t load_dynamic_core(
      struct rarch_state *p_rarch,
      const char *path, char *buf, size_t size)
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
      retroarch_fail(p_rarch, 1, "load_dynamic_core()");
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
static bool libretro_get_system_info(
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
   lib                         = libretro_get_system_info_lib(
         path, &dummy_info, load_no_content);

   if (!lib)
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
      runloop_st->ignore_environment_cb = true;
      retro_set_environment(retroarch_environment_cb);
      runloop_st->ignore_environment_cb = false;
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
 * load_symbols:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will
 *                                load dummy symbols.
 *
 * Setup libretro callback symbols. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols_custom(
      struct rarch_state *p_rarch,
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
                  retroarch_fail(p_rarch, 1, "init_libretro_symbols()");
               }

               RARCH_LOG("[Core]: Loading dynamic libretro core from: \"%s\"\n",
                     path);

               if (!(runloop_st->lib_handle = load_dynamic_core(
                           p_rarch,
                           path,
                           path_get_ptr(RARCH_PATH_CORE),
                           path_get_realsize(RARCH_PATH_CORE)
                           )))
               {
                  RARCH_ERR("%s: \"%s\"\nError(s): %s\n",
                        msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
                        path, dylib_error());
                  runloop_msg_queue_push(msg_hash_to_str(MSG_FAILED_TO_OPEN_LIBRETRO_CORE),
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
      case CORE_TYPE_GONG:
#ifdef HAVE_GONG
         CORE_SYMBOLS(SYMBOL_GONG);
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
 * setups environment callback functions. Returns true on success,
 * or false if symbols could not be loaded.
 **/
static bool init_libretro_symbols(
      struct rarch_state *p_rarch,
      runloop_state_t *runloop_st,
      enum rarch_core_type type,
      struct retro_core_t *current_core)
{
   /* Load symbols */
   if (!init_libretro_symbols_custom(p_rarch, runloop_st,
            type, current_core, NULL, NULL))
      return false;

#ifdef HAVE_RUNAHEAD
   /* remember last core type created, so creating a
    * secondary core will know what core type to use. */
   runloop_st->last_core_type = type;
#endif
   return true;
}

bool libretro_get_shared_context(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->core_set_shared_context;
}

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
static void uninit_libretro_symbols(
      struct rarch_state *p_rarch,
      struct retro_core_t *current_core)
{
   runloop_state_t 
	   *runloop_st = &runloop_state;
   input_driver_state_t 
      *input_st        = input_state_get_ptr();
   audio_driver_state_t 
      *audio_st        = audio_state_get_ptr();
#ifdef HAVE_DYNAMIC
   if (runloop_st->lib_handle)
      dylib_close(runloop_st->lib_handle);
   runloop_st->lib_handle = NULL;
#endif

   memset(current_core, 0, sizeof(struct retro_core_t));

   runloop_st->core_set_shared_context   = false;

   if (runloop_st->core_options)
   {
      runloop_deinit_core_options(
            runloop_st->game_options_active,
            path_get(RARCH_PATH_CORE_OPTIONS),
            runloop_st->core_options);
      runloop_st->game_options_active          = false;
      runloop_st->folder_options_active        = false;
      runloop_st->core_options                 = NULL;
   }
   runloop_system_info_free(&runloop_state);
   audio_st->callback.callback                   = NULL;
   audio_st->callback.set_state                  = NULL;
   runloop_frame_time_free();
   runloop_audio_buffer_status_free();
   input_game_focus_free();
   runloop_fastmotion_override_free(&runloop_state);
   runloop_core_options_cb_free(&runloop_state);
   p_rarch->camera_driver_active      = false;
   p_rarch->location_driver_active    = false;

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

static void secondary_core_destroy(runloop_state_t *runloop_st)
{
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

static bool secondary_core_ensure_exists(
      struct rarch_state *p_rarch,
      runloop_state_t *runloop_st,
      settings_t *settings)
{
   if (!runloop_st->secondary_lib_handle)
      if (!secondary_core_create(p_rarch, runloop_st, settings))
         return false;
   return true;
}

#if defined(HAVE_RUNAHEAD) && defined(HAVE_DYNAMIC)
static bool secondary_core_deserialize(
      struct rarch_state *p_rarch,
      settings_t *settings,
      const void *buffer, int size)
{
   runloop_state_t *runloop_st   = &runloop_state;
   if (secondary_core_ensure_exists(p_rarch, runloop_st, settings))
      return runloop_st->secondary_core.retro_unserialize(buffer, size);
   secondary_core_destroy(runloop_st);
   return false;
}
#endif

static void remember_controller_port_device(
      struct rarch_state *p_rarch,
      long port, long device)
{
   runloop_state_t *runloop_st   = &runloop_state;
   if (port >= 0 && port < MAX_USERS)
      p_rarch->port_map[port] = (int)device;
   if (     runloop_st->secondary_lib_handle
         && runloop_st->secondary_core.retro_set_controller_port_device)
      runloop_st->secondary_core.retro_set_controller_port_device((unsigned)port, (unsigned)device);
}

static void clear_controller_port_map(struct rarch_state *p_rarch)
{
   unsigned port;

   for (port = 0; port < MAX_USERS; port++)
      p_rarch->port_map[port] = -1;
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
   int ext_len;
   unsigned i;
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

   ext_len                  = (int)strlen(ext);

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

   tmpdir                      = get_tmpdir_alloc(dir_libretro);
   if (!tmpdir)
      return NULL;

   tmp_path[0]                 = '\0';
   fill_pathname_join(tmp_path,
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

static bool retroarch_environment_secondary_core_hook(
      unsigned cmd, void *data)
{
   runloop_state_t *runloop_st    = &runloop_state;
   bool result                    = retroarch_environment_cb(cmd, data);

   if (runloop_st->has_variable_update)
   {
      if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE)
      {
         bool *bool_p                      = (bool*)data;
         *bool_p                           = true;
         runloop_st->has_variable_update   = false;
         return true;
      }
      else if (cmd == RETRO_ENVIRONMENT_GET_VARIABLE)
         runloop_st->has_variable_update   = false;
   }
   return result;
}

static bool secondary_core_create(struct rarch_state *p_rarch,
      runloop_state_t *runloop_st, settings_t *settings)
{
   unsigned port;
   bool contentless            = false;
   bool is_inited              = false;
   const enum rarch_core_type
      last_core_type           = runloop_st->last_core_type;
   rarch_system_info_t *info   = &runloop_st->system;
   unsigned num_active_users   = settings->uints.input_max_users;

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
   if (!init_libretro_symbols_custom(p_rarch, runloop_st,
            CORE_TYPE_PLAIN, &runloop_st->secondary_core,
            runloop_st->secondary_library_path,
            &runloop_st->secondary_lib_handle))
      return false;

   runloop_st->secondary_core.symbols_inited = true;
   runloop_st->secondary_core.retro_set_environment(
         retroarch_environment_secondary_core_hook);
#ifdef HAVE_RUNAHEAD
   runloop_st->has_variable_update  = true;
#endif

   runloop_st->secondary_core.retro_init();

   content_get_status(&contentless, &is_inited);
   runloop_st->secondary_core.inited = is_inited;

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
   else if (contentless)
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
      for (port = 0; port < MAX_USERS; port++)
      {
         if (port < info->ports.size)
         {
            unsigned device = (port < num_active_users) ?
                  p_rarch->port_map[port] : RETRO_DEVICE_NONE;

            runloop_st->secondary_core.retro_set_controller_port_device(
                  port, device);
         }
      }

   clear_controller_port_map(p_rarch);

   return true;

error:
   secondary_core_destroy(runloop_st);
   return false;
}

static void secondary_core_input_poll_null(void) { }

static bool secondary_core_run_use_last_input(struct rarch_state *p_rarch)
{
   retro_input_poll_t old_poll_function;
   retro_input_state_t old_input_function;
   runloop_state_t *runloop_st = &runloop_state;

   if (!secondary_core_ensure_exists(p_rarch, runloop_st,
            config_get_ptr()))
   {
      secondary_core_destroy(runloop_st);
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
static void secondary_core_destroy(runloop_state_t *runloop_st) { }
static void remember_controller_port_device(
      struct rarch_state *p_rarch,
      long port, long device) { }
static void clear_controller_port_map(struct rarch_state *p_rarch) { }
#endif

#endif

/* BLUETOOTH DRIVER  */

/**
 * config_get_bluetooth_driver_options:
 *
 * Get an enumerated list of all bluetooth driver names,
 * separated by '|'.
 *
 * Returns: string listing of all bluetooth driver names,
 * separated by '|'.
 **/
const char* config_get_bluetooth_driver_options(void)
{
   return char_list_new_special(STRING_LIST_BLUETOOTH_DRIVERS, NULL);
}

void driver_bluetooth_scan(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   if ( (p_rarch->bluetooth_driver_active) &&
        (p_rarch->bluetooth_driver->scan) )
      p_rarch->bluetooth_driver->scan(p_rarch->bluetooth_data);
}

void driver_bluetooth_get_devices(struct string_list* devices)
{
   struct rarch_state       *p_rarch = &rarch_st;
   if ( (p_rarch->bluetooth_driver_active) &&
        (p_rarch->bluetooth_driver->get_devices) )
      p_rarch->bluetooth_driver->get_devices(p_rarch->bluetooth_data, devices);
}

bool driver_bluetooth_device_is_connected(unsigned i)
{
   struct rarch_state       *p_rarch = &rarch_st;
   if ( (p_rarch->bluetooth_driver_active) &&
        (p_rarch->bluetooth_driver->device_is_connected) )
      return p_rarch->bluetooth_driver->device_is_connected(p_rarch->bluetooth_data, i);
   return false;
}

void driver_bluetooth_device_get_sublabel(char *s, unsigned i, size_t len)
{
   struct rarch_state       *p_rarch = &rarch_st;
   if ( (p_rarch->bluetooth_driver_active) &&
        (p_rarch->bluetooth_driver->device_get_sublabel) )
      p_rarch->bluetooth_driver->device_get_sublabel(p_rarch->bluetooth_data, s, i, len);
}

bool driver_bluetooth_connect_device(unsigned i)
{
   struct rarch_state       *p_rarch = &rarch_st;
   if (p_rarch->bluetooth_driver_active)
      return p_rarch->bluetooth_driver->connect_device(p_rarch->bluetooth_data, i);
   return false;
}

bool bluetooth_driver_ctl(enum rarch_bluetooth_ctl_state state, void *data)
{
   struct rarch_state     *p_rarch  = &rarch_st;
   settings_t             *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_BLUETOOTH_CTL_DESTROY:
         p_rarch->bluetooth_driver          = NULL;
         p_rarch->bluetooth_data            = NULL;
         p_rarch->bluetooth_driver_active   = false;
         break;
      case RARCH_BLUETOOTH_CTL_FIND_DRIVER:
         {
            const char *prefix   = "bluetooth driver";
            int i                = (int)driver_find_index(
                  "bluetooth_driver",
                  settings->arrays.bluetooth_driver);

            if (i >= 0)
               p_rarch->bluetooth_driver = (const bluetooth_driver_t*)bluetooth_drivers[i];
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
                        settings->arrays.bluetooth_driver);
                  RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
                  for (d = 0; bluetooth_drivers[d]; d++)
                     RARCH_LOG_OUTPUT("\t%s\n", bluetooth_drivers[d]->ident);

                  RARCH_WARN("Going to default to first %s...\n", prefix);
               }

               p_rarch->bluetooth_driver = (const bluetooth_driver_t*)bluetooth_drivers[0];

               if (!p_rarch->bluetooth_driver)
                  retroarch_fail(p_rarch, 1, "find_bluetooth_driver()");
            }
         }
         break;
      case RARCH_BLUETOOTH_CTL_DEINIT:
        if (p_rarch->bluetooth_data && p_rarch->bluetooth_driver)
        {
           if (p_rarch->bluetooth_driver->free)
              p_rarch->bluetooth_driver->free(p_rarch->bluetooth_data);
        }

        p_rarch->bluetooth_data = NULL;
        p_rarch->bluetooth_driver_active = false;
        break;
      case RARCH_BLUETOOTH_CTL_INIT:
        /* Resource leaks will follow if bluetooth is initialized twice. */
        if (p_rarch->bluetooth_data)
           return false;

        bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_FIND_DRIVER, NULL);

        if (p_rarch->bluetooth_driver && p_rarch->bluetooth_driver->init)
        {
           p_rarch->bluetooth_driver_active = true;
           p_rarch->bluetooth_data = p_rarch->bluetooth_driver->init();

           if (!p_rarch->bluetooth_data)
           {
              RARCH_ERR("Failed to initialize bluetooth driver. Will continue without bluetooth.\n");
              p_rarch->bluetooth_driver_active = false;
           }
        } else {
           p_rarch->bluetooth_driver_active = false;
        }

        break;
      default:
         break;
   }

   return false;
}

/* WIFI DRIVER  */

/**
 * config_get_wifi_driver_options:
 *
 * Get an enumerated list of all wifi driver names,
 * separated by '|'.
 *
 * Returns: string listing of all wifi driver names,
 * separated by '|'.
 **/
const char* config_get_wifi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_WIFI_DRIVERS, NULL);
}

void driver_wifi_scan(void)
{
   struct rarch_state       *p_rarch = &rarch_st;
   p_rarch->wifi_driver->scan(p_rarch->wifi_data);
}

bool driver_wifi_enable(bool enabled)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->enable(p_rarch->wifi_data, enabled);
}

bool driver_wifi_connection_info(wifi_network_info_t *netinfo)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->connection_info(p_rarch->wifi_data, netinfo);
}

wifi_network_scan_t* driver_wifi_get_ssids()
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->get_ssids(p_rarch->wifi_data);
}

bool driver_wifi_ssid_is_online(unsigned i)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->ssid_is_online(p_rarch->wifi_data, i);
}

bool driver_wifi_connect_ssid(const wifi_network_info_t* net)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->connect_ssid(p_rarch->wifi_data, net);
}

bool driver_wifi_disconnect_ssid(const wifi_network_info_t* net)
{
   struct rarch_state       *p_rarch = &rarch_st;
   return p_rarch->wifi_driver->disconnect_ssid(p_rarch->wifi_data, net);
}

void driver_wifi_tether_start_stop(bool start, char* configfile)
{
   struct rarch_state       *p_rarch = &rarch_st;
   p_rarch->wifi_driver->tether_start_stop(p_rarch->wifi_data, start, configfile);
}

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data)
{
   struct rarch_state     *p_rarch  = &rarch_st;
   settings_t             *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_WIFI_CTL_DESTROY:
         p_rarch->wifi_driver_active   = false;
         p_rarch->wifi_driver          = NULL;
         p_rarch->wifi_data            = NULL;
         break;
      case RARCH_WIFI_CTL_SET_ACTIVE:
         p_rarch->wifi_driver_active   = true;
         break;
      case RARCH_WIFI_CTL_FIND_DRIVER:
         {
            const char *prefix   = "wifi driver";
            int i                = (int)driver_find_index(
                  "wifi_driver",
                  settings->arrays.wifi_driver);

            if (i >= 0)
               p_rarch->wifi_driver = (const wifi_driver_t*)wifi_drivers[i];
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
                        settings->arrays.wifi_driver);
                  RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
                  for (d = 0; wifi_drivers[d]; d++)
                     RARCH_LOG_OUTPUT("\t%s\n", wifi_drivers[d]->ident);

                  RARCH_WARN("Going to default to first %s...\n", prefix);
               }

               p_rarch->wifi_driver = (const wifi_driver_t*)wifi_drivers[0];

               if (!p_rarch->wifi_driver)
                  retroarch_fail(p_rarch, 1, "find_wifi_driver()");
            }
         }
         break;
      case RARCH_WIFI_CTL_UNSET_ACTIVE:
         p_rarch->wifi_driver_active = false;
         break;
      case RARCH_WIFI_CTL_IS_ACTIVE:
        return p_rarch->wifi_driver_active;
      case RARCH_WIFI_CTL_DEINIT:
        if (p_rarch->wifi_data && p_rarch->wifi_driver)
        {
           if (p_rarch->wifi_driver->free)
              p_rarch->wifi_driver->free(p_rarch->wifi_data);
        }

        p_rarch->wifi_data = NULL;
        break;
      case RARCH_WIFI_CTL_STOP:
        if (     p_rarch->wifi_driver
              && p_rarch->wifi_driver->stop
              && p_rarch->wifi_data)
           p_rarch->wifi_driver->stop(p_rarch->wifi_data);
        break;
      case RARCH_WIFI_CTL_START:
        if (     p_rarch->wifi_driver
              && p_rarch->wifi_data
              && p_rarch->wifi_driver->start)
        {
           bool wifi_allow      = settings->bools.wifi_allow;
           if (wifi_allow)
              return p_rarch->wifi_driver->start(p_rarch->wifi_data);
        }
        return false;
      case RARCH_WIFI_CTL_INIT:
        /* Resource leaks will follow if wifi is initialized twice. */
        if (p_rarch->wifi_data)
           return false;

        wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);

        if (p_rarch->wifi_driver && p_rarch->wifi_driver->init)
        {
           p_rarch->wifi_data = p_rarch->wifi_driver->init();

           if (p_rarch->wifi_data)
           {
              p_rarch->wifi_driver->enable(p_rarch->wifi_data,
                 settings->bools.wifi_enabled);
           }
           else
           {
              RARCH_ERR("Failed to initialize wifi driver. Will continue without wifi.\n");
              wifi_driver_ctl(RARCH_WIFI_CTL_UNSET_ACTIVE, NULL);
           }
        }

        break;
      default:
         break;
   }

   return false;
}

/* UI COMPANION */

void ui_companion_set_foreground(unsigned enable)
{
   struct rarch_state     *p_rarch = &rarch_st;
   p_rarch->main_ui_companion_is_on_foreground = enable;
}

bool ui_companion_is_on_foreground(void)
{
   struct rarch_state     *p_rarch = &rarch_st;
   return p_rarch->main_ui_companion_is_on_foreground;
}

void ui_companion_event_command(enum event_command action)
{
   struct rarch_state     *p_rarch = &rarch_st;
#ifdef HAVE_QT
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (ui && ui->event_command)
      ui->event_command(p_rarch->ui_companion_data, action);
#ifdef HAVE_QT
   if (ui_companion_qt.toggle && qt_is_inited)
      ui_companion_qt.event_command(
            p_rarch->ui_companion_qt_data, action);
#endif
}

static void ui_companion_driver_deinit(struct rarch_state *p_rarch)
{
#ifdef HAVE_QT
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (!ui)
      return;
   if (ui->deinit)
      ui->deinit(p_rarch->ui_companion_data);

#ifdef HAVE_QT
   if (qt_is_inited)
   {
      ui_companion_qt.deinit(p_rarch->ui_companion_qt_data);
      p_rarch->ui_companion_qt_data = NULL;
   }
#endif
   p_rarch->ui_companion_data = NULL;
}

static void ui_companion_driver_toggle(
      struct rarch_state *p_rarch,
      bool desktop_menu_enable,
      bool ui_companion_toggle,
      bool force)
{
   if (p_rarch->ui_companion && p_rarch->ui_companion->toggle)
      p_rarch->ui_companion->toggle(p_rarch->ui_companion_data, false);

#ifdef HAVE_QT
   if (desktop_menu_enable)
   {
      if ((ui_companion_toggle || force) && !p_rarch->qt_is_inited)
      {
         p_rarch->ui_companion_qt_data   = ui_companion_qt.init();
         p_rarch->qt_is_inited           = true;
      }

      if (ui_companion_qt.toggle && p_rarch->qt_is_inited)
         ui_companion_qt.toggle(p_rarch->ui_companion_qt_data, force);
   }
#endif
}

static void ui_companion_driver_init_first(
      settings_t *settings,
      struct rarch_state *p_rarch)
{
#ifdef HAVE_QT
   bool desktop_menu_enable            = settings->bools.desktop_menu_enable;
   bool ui_companion_toggle            = settings->bools.ui_companion_toggle;

   if (desktop_menu_enable && ui_companion_toggle)
   {
      p_rarch->ui_companion_qt_data    = ui_companion_qt.init();
      p_rarch->qt_is_inited            = true;
   }
#else
   bool desktop_menu_enable            = false;
   bool ui_companion_toggle            = false;
#endif
   unsigned ui_companion_start_on_boot =
      settings->bools.ui_companion_start_on_boot;
   p_rarch->ui_companion               = (ui_companion_driver_t*)ui_companion_drivers[0];

   if (p_rarch->ui_companion)
      if (ui_companion_start_on_boot)
      {
         if (p_rarch->ui_companion->init)
            p_rarch->ui_companion_data = p_rarch->ui_companion->init();

         ui_companion_driver_toggle(p_rarch,
                                    desktop_menu_enable,
                                    ui_companion_toggle,
                                    false);
      }
}

void ui_companion_driver_notify_refresh(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
#ifdef HAVE_QT
   settings_t      *settings       = config_get_ptr();
   bool desktop_menu_enable        = settings->bools.desktop_menu_enable;
   bool qt_is_inited               = p_rarch->qt_is_inited;
#endif

   if (!ui)
      return;
   if (ui->notify_refresh)
      ui->notify_refresh(p_rarch->ui_companion_data);

#ifdef HAVE_QT
   if (desktop_menu_enable)
      if (ui_companion_qt.notify_refresh && qt_is_inited)
         ui_companion_qt.notify_refresh(p_rarch->ui_companion_qt_data);
#endif
}

void ui_companion_driver_notify_list_loaded(
      file_list_t *list, file_list_t *menu_list)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (ui && ui->notify_list_loaded)
      ui->notify_list_loaded(p_rarch->ui_companion_data, list, menu_list);
}

void ui_companion_driver_notify_content_loaded(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (ui && ui->notify_content_loaded)
      ui->notify_content_loaded(p_rarch->ui_companion_data);
}

const ui_msg_window_t *ui_companion_driver_get_msg_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->msg_window;
}

const ui_window_t *ui_companion_driver_get_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->window;
}

const ui_browser_window_t *ui_companion_driver_get_browser_window_ptr(void)
{
   struct rarch_state *p_rarch     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return NULL;
   return ui->browser_window;
}

static void ui_companion_driver_msg_queue_push(
      struct rarch_state *p_rarch,
      const char *msg, unsigned priority, unsigned duration, bool flush)
{
   const ui_companion_driver_t *ui = p_rarch->ui_companion;

   if (ui && ui->msg_queue_push)
      ui->msg_queue_push(p_rarch->ui_companion_data, msg, priority, duration, flush);

#ifdef HAVE_QT
   {
      settings_t *settings     = config_get_ptr();
      bool qt_is_inited        = p_rarch->qt_is_inited;
      bool desktop_menu_enable = settings->bools.desktop_menu_enable;

      if (desktop_menu_enable)
         if (ui_companion_qt.msg_queue_push && qt_is_inited)
            ui_companion_qt.msg_queue_push(
                  p_rarch->ui_companion_qt_data,
                  msg, priority, duration, flush);
   }
#endif
}

void *ui_companion_driver_get_main_window(void)
{
   struct rarch_state
      *p_rarch                     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui || !ui->get_main_window)
      return NULL;
   return ui->get_main_window(p_rarch->ui_companion_data);
}

const char *ui_companion_driver_get_ident(void)
{
   struct rarch_state
      *p_rarch                     = &rarch_st;
   const ui_companion_driver_t *ui = p_rarch->ui_companion;
   if (!ui)
      return "null";
   return ui->ident;
}

void ui_companion_driver_log_msg(const char *msg)
{
#ifdef HAVE_QT
   struct rarch_state *p_rarch = &rarch_st;
   settings_t *settings        = config_get_ptr();
   bool qt_is_inited           = p_rarch->qt_is_inited;
   bool desktop_menu_enable    = settings->bools.desktop_menu_enable;
   bool window_is_active       = p_rarch->ui_companion_qt_data && qt_is_inited
      && ui_companion_qt.is_active(p_rarch->ui_companion_qt_data);

   if (desktop_menu_enable)
      if (window_is_active)
         ui_companion_qt.log_msg(p_rarch->ui_companion_qt_data, msg);
#endif
}

/* RECORDING */

/**
 * config_get_record_driver_options:
 *
 * Get an enumerated list of all record driver names, separated by '|'.
 *
 * Returns: string listing of all record driver names, separated by '|'.
 **/
const char* config_get_record_driver_options(void)
{
   return char_list_new_special(STRING_LIST_RECORD_DRIVERS, NULL);
}

#if 0
/* TODO/FIXME - not used apparently */
static void find_record_driver(const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings = config_get_ptr();
   int i                = (int)driver_find_index(
         "record_driver",
         settings->arrays.record_driver);

   if (i >= 0)
      recording_state.driver = (const record_driver_t*)record_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;

         RARCH_ERR("[recording] Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.record_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; record_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", record_drivers[d].ident);
         RARCH_WARN("[recording] Going to default to first %s...\n", prefix);
      }

      recording_state.driver = (const record_driver_t*)record_drivers[0];

      if (!recording_state.driver)
         retroarch_fail(p_rarch, 1, "find_record_driver()");
   }
}

/**
 * ffemu_find_backend:
 * @ident                   : Identifier of driver to find.
 *
 * Finds a recording driver with the name @ident.
 *
 * Returns: recording driver handle if successful, otherwise
 * NULL.
 **/
static const record_driver_t *ffemu_find_backend(const char *ident)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      if (string_is_equal(record_drivers[i]->ident, ident))
         return record_drivers[i];
   }

   return NULL;
}

static void recording_driver_free_state(void)
{
   /* TODO/FIXME - this is not being called anywhere */
   recording_state.gpu_width     = 0;
   recording_state.gpu_height    = 0;
   recording_state.width         = 0;
   recording_stte.height         = 0;
}
#endif

/**
 * gfx_ctx_init_first:
 * @backend                 : Recording backend handle.
 * @data                    : Recording data handle.
 * @params                  : Recording info parameters.
 *
 * Finds first suitable recording context driver and initializes.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool record_driver_init_first(
      const record_driver_t **backend, void **data,
      const struct record_params *params)
{
   unsigned i;

   for (i = 0; record_drivers[i]; i++)
   {
      void *handle = record_drivers[i]->init(params);

      if (!handle)
         continue;

      *backend = record_drivers[i];
      *data    = handle;
      return true;
   }

   return false;
}

static bool recording_deinit(void)
{
   if (!recording_state.data || !recording_state.driver)
      return false;

   if (recording_state.driver->finalize)
      recording_state.driver->finalize(recording_state.data);

   if (recording_state.driver->free)
      recording_state.driver->free(recording_state.data);

   recording_state.data            = NULL;
   recording_state.driver          = NULL;

   video_driver_gpu_record_deinit();

   return true;
}

bool recording_is_enabled(void)
{
   return recording_state.enable;
}

bool streaming_is_enabled(void)
{
   struct rarch_state *p_rarch = &rarch_st;
   return p_rarch->streaming_enable;
}

void streaming_set_state(bool state)
{
   struct rarch_state *p_rarch = &rarch_st;
   p_rarch->streaming_enable = state;
}

/**
 * recording_init:
 *
 * Initializes recording.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
static bool recording_init(
      settings_t *settings,
      struct rarch_state *p_rarch)
{
   char output[PATH_MAX_LENGTH];
   char buf[PATH_MAX_LENGTH];
   struct record_params params          = {0};
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   runloop_state_t *runloop_st          = &runloop_state;
   global_t *global                     = global_get_ptr();
   bool video_gpu_record                = settings->bools.video_gpu_record;
   bool video_force_aspect              = settings->bools.video_force_aspect;
   const enum rarch_core_type
      current_core_type                 = runloop_st->current_core_type;
   const enum retro_pixel_format
      video_driver_pix_fmt              = video_st->pix_fmt;
   bool recording_enable                = recording_state.enable;

   if (!recording_enable)
      return false;

   output[0] = '\0';

   if (current_core_type == CORE_TYPE_DUMMY)
   {
      RARCH_WARN("[recording] %s\n",
            msg_hash_to_str(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED));
      return false;
   }

   if (!video_gpu_record && video_driver_is_hw_context())
   {
      RARCH_WARN("[recording] %s.\n",
            msg_hash_to_str(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING));
      return false;
   }

   RARCH_LOG("[recording] %s: FPS: %.4f, Sample rate: %.4f\n",
         msg_hash_to_str(MSG_CUSTOM_TIMING_GIVEN),
         (float)av_info->timing.fps,
         (float)av_info->timing.sample_rate);

   if (!string_is_empty(global->record.path))
      strlcpy(output, global->record.path, sizeof(output));
   else
   {
      const char *stream_url        = settings->paths.path_stream_url;
      unsigned video_record_quality = settings->uints.video_record_quality;
      unsigned video_stream_port    = settings->uints.video_stream_port;
      if (p_rarch->streaming_enable)
         if (!string_is_empty(stream_url))
            strlcpy(output, stream_url, sizeof(output));
         else
            /* Fallback, stream locally to 127.0.0.1 */
            snprintf(output, sizeof(output), "udp://127.0.0.1:%u",
                  video_stream_port);
      else
      {
         const char *game_name = path_basename(path_get(RARCH_PATH_BASENAME));
         /* Fallback to core name if started without content */
         if (string_is_empty(game_name))
            game_name          = runloop_st->system.info.library_name;

         if (video_record_quality < RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST)
         {
            fill_str_dated_filename(buf, game_name,
                     "mkv", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_GIF)
         {
            fill_str_dated_filename(buf, game_name,
                     "webm", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else if (video_record_quality >= RECORD_CONFIG_TYPE_RECORDING_GIF
               && video_record_quality < RECORD_CONFIG_TYPE_RECORDING_APNG)
         {
            fill_str_dated_filename(buf, game_name,
                     "gif", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
         else
         {
            fill_str_dated_filename(buf, game_name,
                     "png", sizeof(buf));
            fill_pathname_join(output, global->record.output_dir, buf, sizeof(output));
         }
      }
   }

   params.audio_resampler           = settings->arrays.audio_resampler;
   params.video_gpu_record          = settings->bools.video_gpu_record;
   params.video_record_scale_factor = settings->uints.video_record_scale_factor;
   params.video_stream_scale_factor = settings->uints.video_stream_scale_factor;
   params.video_record_threads      = settings->uints.video_record_threads;
   params.streaming_mode            = settings->uints.streaming_mode;

   params.out_width                 = av_info->geometry.base_width;
   params.out_height                = av_info->geometry.base_height;
   params.fb_width                  = av_info->geometry.max_width;
   params.fb_height                 = av_info->geometry.max_height;
   params.channels                  = 2;
   params.filename                  = output;
   params.fps                       = av_info->timing.fps;
   params.samplerate                = av_info->timing.sample_rate;
   params.pix_fmt                   =
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
      ? FFEMU_PIX_ARGB8888
      : FFEMU_PIX_RGB565;
   params.config                    = NULL;

   if (!string_is_empty(global->record.config))
      params.config                 = global->record.config;
   else
   {
      if (p_rarch->streaming_enable)
      {
         params.config = settings->paths.path_stream_config;
         params.preset = (enum record_config_type)
            settings->uints.video_stream_quality;
      }
      else
      {
         params.config = settings->paths.path_record_config;
         params.preset = (enum record_config_type)
            settings->uints.video_record_quality;
      }
   }

   if (settings->bools.video_gpu_record
      && video_st->current_video->read_viewport)
   {
      unsigned gpu_size;
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      video_driver_get_viewport_info(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("[recording] Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return false;
      }

      params.out_width                    = vp.width;
      params.out_height                   = vp.height;
      params.fb_width                     = next_pow2(vp.width);
      params.fb_height                    = next_pow2(vp.height);

      if (video_force_aspect &&
            (video_st->aspect_ratio > 0.0f))
         params.aspect_ratio              = video_st->aspect_ratio;
      else
         params.aspect_ratio              = (float)vp.width / vp.height;

      params.pix_fmt                      = FFEMU_PIX_BGR24;
      recording_state.gpu_width           = vp.width;
      recording_state.gpu_height          = vp.height;

      RARCH_LOG("[recording] %s %u x %u\n", msg_hash_to_str(MSG_DETECTED_VIEWPORT_OF),
            vp.width, vp.height);

      gpu_size = vp.width * vp.height * 3;
      if (!(video_st->record_gpu_buffer = (uint8_t*)malloc(gpu_size)))
         return false;
   }
   else
   {
      if (recording_state.width || recording_state.height)
      {
         params.out_width  = recording_state.width;
         params.out_height = recording_state.height;
      }

      if (video_force_aspect &&
            (video_st->aspect_ratio > 0.0f))
         params.aspect_ratio = video_st->aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

#ifdef HAVE_VIDEO_FILTER
      if (settings->bools.video_post_filter_record
            && !!video_st->state_filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         params.pix_fmt      = FFEMU_PIX_RGB565;

         if (video_st->state_out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;

         rarch_softfilter_get_max_output_size(
               video_st->state_filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
#endif
   }

   RARCH_LOG("[recording] %s %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         msg_hash_to_str(MSG_RECORDING_TO),
         output,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!record_driver_init_first(
            &recording_state.driver,
            &recording_state.data, &params))
   {
      RARCH_ERR("[recording] %s\n",
            msg_hash_to_str(MSG_FAILED_TO_START_RECORDING));
      video_driver_gpu_record_deinit();

      return false;
   }

   return true;
}

void recording_driver_update_streaming_url(void)
{
   settings_t     *settings      = config_get_ptr();
   const char     *youtube_url   = "rtmp://a.rtmp.youtube.com/live2/";
   const char     *twitch_url    = "rtmp://live.twitch.tv/app/";
   const char     *facebook_url  = "rtmps://live-api-s.facebook.com:443/rtmp/";

   if (!settings)
      return;

   switch (settings->uints.streaming_mode)
   {
      case STREAMING_MODE_TWITCH:
         if (!string_is_empty(settings->arrays.twitch_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  twitch_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.twitch_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
      case STREAMING_MODE_YOUTUBE:
         if (!string_is_empty(settings->arrays.youtube_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  youtube_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.youtube_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
      case STREAMING_MODE_LOCAL:
         /* TODO: figure out default interface and bind to that instead */
         snprintf(settings->paths.path_stream_url, sizeof(settings->paths.path_stream_url),
            "udp://%s:%u", "127.0.0.1", settings->uints.video_stream_port);
         break;
      case STREAMING_MODE_CUSTOM:
      default:
         /* Do nothing, let the user input the URL */
         break;
      case STREAMING_MODE_FACEBOOK:
         if (!string_is_empty(settings->arrays.facebook_stream_key))
         {
            strlcpy(settings->paths.path_stream_url,
                  facebook_url,
                  sizeof(settings->paths.path_stream_url));
            strlcat(settings->paths.path_stream_url,
                  settings->arrays.facebook_stream_key,
                  sizeof(settings->paths.path_stream_url));
         }
         break;
   }
}

/* INPUT */

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_DRIVERS, NULL);
}

/* MENU INPUT */

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void)
{
   return char_list_new_special(STRING_LIST_INPUT_JOYPAD_DRIVERS, NULL);
}

#if defined(HAVE_MENU) && defined(HAVE_ACCESSIBILITY)
static const char *accessibility_lut_name(char key)
{
   switch (key)
   {
#if 0
      /* TODO/FIXME - overlaps with tilde */
      case '`':
         return "left quote";
#endif
      case '`':
         return "tilde";
      case '!':
         return "exclamation point";
      case '@':
         return "at sign";
      case '#':
         return "hash sign";
      case '$':
         return "dollar sign";
      case '%':
         return "percent sign";
      case '^':
         return "carrot";
      case '&':
         return "ampersand";
      case '*':
         return "asterisk";
      case '(':
         return "left bracket";
      case ')':
         return "right bracket";
      case '-':
         return "minus";
      case '_':
         return "underscore";
      case '=':
         return "equals";
      case '+':
         return "plus";
      case '[':
         return "left square bracket";
      case '{':
         return "left curl bracket";
      case ']':
         return "right square bracket";
      case '}':
         return "right curl bracket";
      case '\\':
         return "back slash";
      case '|':
         return "pipe";
      case ';':
         return "semicolon";
      case ':':
         return "colon";
      case '\'':
         return "single quote";
      case '\"':
         return "double quote";
      case ',':
         return "comma";
      case '<':
         return "left angle bracket";
      case '.':
         return "period";
      case '>':
         return "right angle bracket";
      case '/':
         return "front slash";
      case '?':
         return "question mark";
      case ' ':
         return "space";
      default:
         break;
   }
   return NULL;
}
#endif

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events 
 * are fired.
 * This interfaces with the global system driver struct 
 * and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device)
{
   static bool deferred_wait_keys;
   runloop_state_t *runloop_st   = &runloop_state;
   retro_keyboard_event_t 
	   *key_event            = &runloop_st->key_event;
   input_driver_state_t 
      *input_st                  = input_state_get_ptr();
#ifdef HAVE_ACCESSIBILITY
   struct rarch_state *p_rarch   = &rarch_st;
   settings_t *settings          = config_get_ptr();
   bool accessibility_enable     = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st    = menu_state_get_ptr();

   /* If screensaver is active, then it should be
    * disabled if:
    * - Key is down AND
    * - OSK is active, OR:
    * - Key is *not* mapped to RetroPad input (these
    *   inputs are handled in menu_event() - if we
    *   allow mapped RetroPad keys to toggle off
    *   the screensaver, then we end up with a 'duplicate'
    *   input that will trigger unwanted menu action)
    * - For extra amusement, a number of keyboard keys
    *   are hard-coded to RetroPad inputs (while the menu
    *   is running) in such a way that they cannot be
    *   detected via the regular 'keyboard_mapping_bits'
    *   record. We therefore have to check each of these
    *   explicitly...
    * Otherwise, input is ignored whenever screensaver
    * is active */
   if (menu_st->screensaver_active)
   {
      if (down &&
          (code != RETROK_UNKNOWN) &&
          (menu_input_dialog_get_display_kb() ||
               !((code == RETROK_SPACE)     || /* RETRO_DEVICE_ID_JOYPAD_START */
                 (code == RETROK_SLASH)     || /* RETRO_DEVICE_ID_JOYPAD_X */
                 (code == RETROK_RSHIFT)    || /* RETRO_DEVICE_ID_JOYPAD_SELECT */
                 (code == RETROK_RIGHT)     || /* RETRO_DEVICE_ID_JOYPAD_RIGHT */
                 (code == RETROK_LEFT)      || /* RETRO_DEVICE_ID_JOYPAD_LEFT */
                 (code == RETROK_DOWN)      || /* RETRO_DEVICE_ID_JOYPAD_DOWN */
                 (code == RETROK_UP)        || /* RETRO_DEVICE_ID_JOYPAD_UP */
                 (code == RETROK_PAGEUP)    || /* RETRO_DEVICE_ID_JOYPAD_L */
                 (code == RETROK_PAGEDOWN)  || /* RETRO_DEVICE_ID_JOYPAD_R */
                 (code == RETROK_BACKSPACE) || /* RETRO_DEVICE_ID_JOYPAD_B */
                 (code == RETROK_RETURN)    || /* RETRO_DEVICE_ID_JOYPAD_A */
                 (code == RETROK_DELETE)    || /* RETRO_DEVICE_ID_JOYPAD_Y */
                 BIT512_GET(input_st->keyboard_mapping_bits, code))))
      {
         menu_ctx_environment_t menu_environ;
         menu_environ.type           = MENU_ENVIRON_DISABLE_SCREENSAVER;
         menu_environ.data           = NULL;
         menu_st->screensaver_active = false;
         menu_st->input_last_time_us = menu_st->current_time_us;
         menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
      }
      return;
   }

   if (down)
      menu_st->input_last_time_us = menu_st->current_time_us;

#ifdef HAVE_ACCESSIBILITY
   if (menu_input_dialog_get_display_kb()
         && down && is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
   {
      if (code != 303 && code != 0)
      {
         char* say_char = (char*)malloc(sizeof(char)+1);

         if (say_char)
         {
            char c    = (char) character;
            *say_char = c;
            say_char[1] = '\0';

            if (character == 127 || character == 8)
               accessibility_speak_priority(p_rarch,
                     accessibility_enable,
                     accessibility_narrator_speech_speed,
                     "backspace", 10);
            else
            {
               const char *lut_name = accessibility_lut_name(c);

               if (lut_name)
                  accessibility_speak_priority(p_rarch,
                        accessibility_enable,
                        accessibility_narrator_speech_speed,
                        lut_name, 10);
               else if (character != 0)
                  accessibility_speak_priority(p_rarch,
                        accessibility_enable,
                        accessibility_narrator_speech_speed,
                        say_char, 10);
            }
            free(say_char);
         }
      }
   }
#endif
#endif

   if (deferred_wait_keys)
   {
      if (down)
         return;

      input_st->keyboard_press_cb                      = NULL;
      input_st->keyboard_press_data                    = NULL;
      input_st->keyboard_mapping_blocked               = false;
      deferred_wait_keys                               = false;
   }
   else if (input_st->keyboard_press_cb)
   {
      if (!down || code == RETROK_UNKNOWN)
         return;
      if (input_st->keyboard_press_cb(input_st->keyboard_press_data, code))
         return;
      deferred_wait_keys = true;
   }
   else if (input_st->keyboard_line.enabled)
   {
      if (!down)
         return;

      switch (device)
      {
         case RETRO_DEVICE_POINTER:
            if (code != 0x12d)
               character = (char)code;
            /* fall-through */
         default:
            if (!input_keyboard_line_event(input_st,
                     &input_st->keyboard_line, character))
               return;
            break;
      }

      /* Line is complete, can free it now. */
      if (input_st->keyboard_line.buffer)
         free(input_st->keyboard_line.buffer);
      input_st->keyboard_line.buffer                    = NULL;
      input_st->keyboard_line.ptr                       = 0;
      input_st->keyboard_line.size                      = 0;
      input_st->keyboard_line.cb                        = NULL;
      input_st->keyboard_line.userdata                  = NULL;
      input_st->keyboard_line.enabled                   = false;

      /* Unblock all hotkeys. */
      input_st->keyboard_mapping_blocked               = false;
   }
   else
   {
      if (code == RETROK_UNKNOWN)
         return;

      /* Block hotkey + RetroPad mapped keyboard key events,
       * but not with game focus, and from keyboard device type,
       * and with 'enable_hotkey' modifier set and unpressed */
      if (!input_st->game_focus_state.enabled &&
            BIT512_GET(input_st->keyboard_mapping_bits, code))
      {
         input_mapper_t *handle      = &input_st->mapper;
         struct retro_keybind hotkey = input_config_binds[0][RARCH_ENABLE_HOTKEY];
         bool hotkey_pressed         =
                  (input_st->input_hotkey_block_counter > 0) 
               || (hotkey.key == code);

         if (!(MAPPER_GET_KEY(handle, code)) &&
               !(!hotkey_pressed && (
                  hotkey.key     != RETROK_UNKNOWN ||
                  hotkey.joykey  != NO_BTN ||
                  hotkey.joyaxis != AXIS_NONE
               )))
            return;
      }

      if (*key_event)
         (*key_event)(down, code, character, mod);
   }
}

/* AUDIO */

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char *config_get_audio_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_DRIVERS, NULL);
}

/* VIDEO */
/* Stub functions */

static bool video_driver_init_internal(
      settings_t *settings,
      bool *video_is_threaded,
      bool verbosity_enabled
     )
{
   video_info_t video;
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   input_driver_t *tmp                    = NULL;
   static uint16_t dummy_pixels[32]       = {0};
   runloop_state_t *runloop_st            = &runloop_state;
   input_driver_state_t *input_st         = input_state_get_ptr();
   video_driver_state_t *video_st         = video_state_get_ptr();
   struct retro_game_geometry *geom       = &video_st->av_info.geometry;
   const enum retro_pixel_format
      video_driver_pix_fmt                = video_st->pix_fmt;
#ifdef HAVE_VIDEO_FILTER
   const char *path_softfilter_plugin     = settings->paths.path_softfilter_plugin;

   if (!string_is_empty(path_softfilter_plugin))
      video_driver_init_filter(video_driver_pix_fmt, settings);
#endif

   max_dim   = MAX(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = MAX(scale, 1);

#ifdef HAVE_VIDEO_FILTER
   if (video_st->state_filter)
      scale  = video_st->state_scale;
#endif

   /* Update core-dependent aspect ratio values. */
   video_driver_set_viewport_square_pixel(geom);
   video_driver_set_viewport_core();
   video_driver_set_viewport_config(geom,
         settings->floats.video_aspect_ratio,
         settings->bools.video_aspect_ratio_auto);

   /* Update CUSTOM viewport. */
   custom_vp = &settings->video_viewport_custom;

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   {
      /* Guard against aspect ratio index possibly being out of bounds */
      unsigned new_aspect_idx = settings->uints.video_aspect_ratio_idx;
      if (new_aspect_idx > ASPECT_RATIO_END)
         new_aspect_idx = settings->uints.video_aspect_ratio_idx = 0;

      video_driver_set_aspect_ratio_value(
            aspectratio_lut[new_aspect_idx].value);
   }

   if (settings->bools.video_fullscreen || video_st->force_fullscreen)
   {
      width  = settings->uints.video_fullscreen_x;
      height = settings->uints.video_fullscreen_y;
   }
   else
   {
#ifdef __WINRT__
      if (is_running_on_xbox())
      {
         width  = settings->uints.video_fullscreen_x != 0 ? settings->uints.video_fullscreen_x : 3840;
         height = settings->uints.video_fullscreen_y != 0 ? settings->uints.video_fullscreen_y : 2160;
      }
      else
#endif
      {
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
         bool window_custom_size_enable = settings->bools.video_window_save_positions;
#else
         bool window_custom_size_enable = settings->bools.video_window_custom_size_enable;
#endif
         /* TODO: remove when the new window resizing core is hooked */
         if (window_custom_size_enable &&
            settings->uints.window_position_width &&
            settings->uints.window_position_height)
         {
            width = settings->uints.window_position_width;
            height = settings->uints.window_position_height;
         }
         else
         {
            float video_scale = settings->floats.video_scale;
            /* Determine maximum allowed window dimensions
             * NOTE: We cannot read the actual display
             * metrics here, because the context driver
             * has not yet been initialised... */

             /* > Try explicitly configured values */
            unsigned max_win_width = settings->uints.window_auto_width_max;
            unsigned max_win_height = settings->uints.window_auto_height_max;

            /* > Handle invalid settings */
            if ((max_win_width == 0) || (max_win_height == 0))
            {
               /* > Try configured fullscreen width/height */
               max_win_width = settings->uints.video_fullscreen_x;
               max_win_height = settings->uints.video_fullscreen_y;

               if ((max_win_width == 0) || (max_win_height == 0))
               {
                  /* Maximum window width/size *must* be non-zero;
                   * if all else fails, used defined default
                   * maximum window size */
                  max_win_width = DEFAULT_WINDOW_AUTO_WIDTH_MAX;
                  max_win_height = DEFAULT_WINDOW_AUTO_HEIGHT_MAX;
               }
            }

            /* Determine nominal window size based on
             * core geometry */
            if (settings->bools.video_force_aspect)
            {
               /* Do rounding here to simplify integer
                * scale correctness. */
               unsigned base_width = roundf(geom->base_height *
                  video_st->aspect_ratio);
               width = roundf(base_width * video_scale);
            }
            else
               width = roundf(geom->base_width * video_scale);

            height = roundf(geom->base_height * video_scale);

            /* Cap window size to maximum allowed values */
            if ((width > max_win_width) || (height > max_win_height))
            {
               unsigned geom_width = (width > 0) ? width : 1;
               unsigned geom_height = (height > 0) ? height : 1;
               float geom_aspect = (float)geom_width / (float)geom_height;
               float max_win_aspect = (float)max_win_width / (float)max_win_height;

               if (geom_aspect > max_win_aspect)
               {
                  width = max_win_width;
                  height = geom_height * max_win_width / geom_width;
                  /* Account for any possible rounding errors... */
                  height = (height < 1) ? 1 : height;
                  height = (height > max_win_height) ? max_win_height : height;
               }
               else
               {
                  height = max_win_height;
                  width = geom_width * max_win_height / geom_height;
                  /* Account for any possible rounding errors... */
                  width = (width < 1) ? 1 : width;
                  width = (width > max_win_width) ? max_win_width : width;
               }
            }
         }
      }
   }

   if (width && height)
      RARCH_LOG("[Video]: Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("[Video]: Video @ fullscreen\n");

   video_st->display_type     = RARCH_DISPLAY_NONE;
   video_st->display          = 0;
   video_st->display_userdata = 0;
   video_st->window           = 0;

   video_st->scaler_ptr       = video_driver_pixel_converter_init(
         video_st->pix_fmt,
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st),
         RARCH_SCALE_BASE * scale);

   video.width                       = width;
   video.height                      = height;
   video.fullscreen                  = settings->bools.video_fullscreen ||
                                       video_st->force_fullscreen;
   video.vsync                       = settings->bools.video_vsync &&
      !runloop_st->force_nonblock;
   video.force_aspect                = settings->bools.video_force_aspect;
   video.font_enable                 = settings->bools.video_font_enable;
   video.swap_interval               = settings->uints.video_swap_interval;
   video.adaptive_vsync              = settings->bools.video_adaptive_vsync;
#ifdef GEKKO
   video.viwidth                     = settings->uints.video_viwidth;
   video.vfilter                     = settings->bools.video_vfilter;
#endif
   video.smooth                      = settings->bools.video_smooth;
   video.ctx_scaling                 = settings->bools.video_ctx_scaling;
   video.input_scale                 = scale;
   video.font_size                   = settings->floats.video_font_size;
   video.path_font                   = settings->paths.path_font;
#ifdef HAVE_VIDEO_FILTER
   video.rgb32                       =
        video_st->state_filter
      ? video_st->state_out_rgb32
      : (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#else
   video.rgb32                       =
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#endif
   video.parent                      = 0;

   video_st->started_fullscreen      = video.fullscreen;
   /* Reset video frame count */
   video_st->frame_count             = 0;

   tmp                               = input_state_get_ptr()->current_driver;
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver(settings,
         "video driver", verbosity_enabled);

#ifdef HAVE_THREADS
   video.is_threaded                 =
VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   *video_is_threaded                = video.is_threaded;

   if (video.is_threaded)
   {
      bool ret;
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("[Video]: Starting threaded video driver ...\n");

      ret = video_init_thread(
            (const video_driver_t**)&video_st->current_video,
            &video_st->data,
            &input_state_get_ptr()->current_driver,
            (void**)&input_state_get_ptr()->current_data,
            video_st->current_video,
            video);
      if (!ret)
      {
         RARCH_ERR("[Video]: Cannot open threaded video driver ... Exiting ...\n");
         return false;
      }
   }
   else
#endif
      video_st->data = video_st->current_video->init(
            &video,
            &input_state_get_ptr()->current_driver,
            (void**)&input_state_get_ptr()->current_data);

   if (!video_st->data)
   {
      RARCH_ERR("[Video]: Cannot open video driver ... Exiting ...\n");
      return false;
   }

   video_st->poke = NULL;
   if (video_st->current_video->poke_interface)
      video_st->current_video->poke_interface(
            video_st->data, &video_st->poke);

   if (video_st->current_video->viewport_info &&
         (!custom_vp->width  ||
          !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;

      video_driver_get_viewport_info(custom_vp);
   }

   video_driver_set_rotation(retroarch_get_rotation() % 4);

   video_st->current_video->suppress_screensaver(video_st->data,
         settings->bools.ui_suspend_screensaver_enable);

   if (!video_driver_init_input(tmp, settings, verbosity_enabled))
         return false;

#ifdef HAVE_OVERLAY
   input_overlay_deinit();
   input_overlay_init();
#endif

#ifdef HAVE_VIDEO_LAYOUT
   if (settings->bools.video_layout_enable)
   {
      video_layout_init(video_st->data,
            video_driver_layout_render_interface());
      video_layout_load(settings->paths.path_video_layout);
      video_layout_view_select(settings->uints.video_layout_selected_view);
   }
#endif

   if (!runloop_st->current_core.game_loaded)
      video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   video_context_driver_reset();

   video_display_server_init(video_st->display_type);

   if ((enum rotation)settings->uints.screen_orientation != ORIENTATION_NORMAL)
      video_display_server_set_screen_orientation((enum rotation)settings->uints.screen_orientation);

   /* Ensure that we preserve the 'grab mouse'
    * state if it was enabled prior to driver
    * (re-)initialisation */
   if (input_st->grab_mouse_state)
   {
      video_driver_hide_mouse();
      if (input_driver_grab_mouse())
         input_st->grab_mouse_state = true;
   }
   else if (video.fullscreen)
   {
      video_driver_hide_mouse();
      if (!settings->bools.video_windowed_fullscreen)
         if (input_driver_grab_mouse())
            input_st->grab_mouse_state = true;
   }

   return true;
}

static void video_driver_reinit_context(struct rarch_state *p_rarch,
      settings_t *settings, int flags)
{
   /* RARCH_DRIVER_CTL_UNINIT clears the callback struct so we
    * need to make sure to keep a copy */
   struct retro_hw_render_callback hwr_copy;
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_hw_render_callback *hwr =
      VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
   const struct retro_hw_render_context_negotiation_interface *iface =
      video_st->hw_render_context_negotiation;
   memcpy(&hwr_copy, hwr, sizeof(hwr_copy));

   driver_uninit(p_rarch, flags);

   memcpy(hwr, &hwr_copy, sizeof(*hwr));
   video_st->hw_render_context_negotiation = iface;

   drivers_init(p_rarch, settings, flags, verbosity_is_enabled());
}

void video_driver_reinit(int flags)
{
   settings_t *settings                    = config_get_ptr();
   struct rarch_state   *p_rarch           = &rarch_st;
   video_driver_state_t *video_st          = video_state_get_ptr();
   struct retro_hw_render_callback *hwr    =
      VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);

   video_st->cache_context     = (hwr->cache_context != false);
   video_st->cache_context_ack = false;
   video_driver_reinit_context(p_rarch, settings, flags);
   video_st->cache_context     = false;
}

/**
 * video_driver_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
static void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   char status_text[128];
   static char video_driver_msg[256];
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static float last_fps, frame_time;
   static uint64_t last_used_memory, last_total_memory;
   retro_time_t new_time;
   video_frame_info_t video_info;
   video_driver_state_t *video_st= video_state_get_ptr();
   runloop_state_t *runloop_st   = &runloop_state;
   const enum retro_pixel_format
      video_driver_pix_fmt       = video_st->pix_fmt;
   bool runloop_idle             = runloop_st->idle;
   bool video_driver_active      = video_st->active;
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active           = dispwidget_get_ptr()->active;
#endif

   status_text[0]                = '\0';
   video_driver_msg[0]           = '\0';

   if (!video_driver_active)
      return;

   new_time                      = cpu_features_get_time_usec();

   if (data)
      video_st->frame_cache_data = data;
   video_st->frame_cache_width   = width;
   video_st->frame_cache_height  = height;
   video_st->frame_cache_pitch   = pitch;

   if (
            video_st->scaler_ptr
         && data
         && (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
         && (data != RETRO_HW_FRAME_BUFFER_VALID)
         && video_pixel_frame_scale(
            video_st->scaler_ptr->scaler,
            video_st->scaler_ptr->scaler_out,
            data, width, height, pitch)
      )
   {
      data                = video_st->scaler_ptr->scaler_out;
      pitch               = video_st->scaler_ptr->scaler->out_stride;
   }

   video_driver_build_info(&video_info);

   /* Get the amount of frames per seconds. */
   if (video_st->frame_count)
   {
      unsigned fps_update_interval                 =
         video_info.fps_update_interval;
      unsigned memory_update_interval              =
         video_info.memory_update_interval;
      size_t buf_pos                               = 1;
      /* set this to 1 to avoid an offset issue */
      unsigned write_index                         =
         video_st->frame_time_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      frame_time                                   = new_time - fps_time;
      video_st->frame_time_samples
         [write_index]                             = frame_time;
      fps_time                                     = new_time;

      if (video_info.fps_show)
         buf_pos = snprintf(
               status_text, sizeof(status_text),
               "FPS: %6.2f", last_fps);

      if (video_info.framecount_show)
      {
         char frames_text[64];
         if (status_text[buf_pos-1] != '\0')
            strlcat(status_text, " || ", sizeof(status_text));
         snprintf(frames_text,
               sizeof(frames_text),
               "%s: %" PRIu64, msg_hash_to_str(MSG_FRAMES),
               (uint64_t)video_st->frame_count);
         buf_pos = strlcat(status_text, frames_text, sizeof(status_text));
      }

      if (video_info.memory_show)
      {
         char mem[128];

         if ((video_st->frame_count % memory_update_interval) == 0)
         {
            last_total_memory = frontend_driver_get_total_memory();
            last_used_memory  = last_total_memory - frontend_driver_get_free_memory();
         }

         mem[0] = '\0';
         snprintf(
               mem, sizeof(mem), "MEM: %.2f/%.2fMB", last_used_memory / (1024.0f * 1024.0f),
               last_total_memory / (1024.0f * 1024.0f));
         if (status_text[buf_pos-1] != '\0')
            strlcat(status_text, " || ", sizeof(status_text));
         strlcat(status_text, mem, sizeof(status_text));
      }

      if ((video_st->frame_count % fps_update_interval) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time,
               fps_update_interval);

         strlcpy(video_st->window_title,
               video_st->title_buf,
               sizeof(video_st->window_title));

         if (!string_is_empty(status_text))
         {
            strlcat(video_st->window_title,
                  " || ", sizeof(video_st->window_title));
            strlcat(video_st->window_title,
                  status_text, sizeof(video_st->window_title));
         }

         curr_time                     = new_time;
         video_st->window_title_update = true;
      }
   }
   else
   {
      curr_time = fps_time = new_time;

      strlcpy(video_st->window_title,
            video_st->title_buf,
            sizeof(video_st->window_title));

      if (video_info.fps_show)
         strlcpy(status_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(status_text));

      video_st->window_title_update = true;
   }

   /* Add core status message to status text */
   if (video_info.core_status_msg_show)
   {
      /* Note: We need to lock a mutex here. Strictly
       * speaking, runloop_core_status_msg is not part
       * of the message queue, but:
       * - It may be implemented as a queue in the future
       * - It seems unnecessary to create a new slock_t
       *   object for this type of message when
       *   _runloop_msg_queue_lock is already available
       * We therefore just call runloop_msg_queue_lock()/
       * runloop_msg_queue_unlock() in this case */
      RUNLOOP_MSG_QUEUE_LOCK(runloop_st);

      /* Check whether duration timer has elapsed */
      runloop_core_status_msg.duration -= anim_get_ptr()->delta_time;

      if (runloop_core_status_msg.duration < 0.0f)
      {
         runloop_core_status_msg.str[0]   = '\0';
         runloop_core_status_msg.priority = 0;
         runloop_core_status_msg.duration = 0.0f;
         runloop_core_status_msg.set      = false;
      }
      else
      {
         /* If status text is already set, add status
          * message at the end */
         if (!string_is_empty(status_text))
         {
            strlcat(status_text,
                  " || ", sizeof(status_text));
            strlcat(status_text,
                  runloop_core_status_msg.str, sizeof(status_text));
         }
         else
            strlcpy(status_text, runloop_core_status_msg.str,
                  sizeof(status_text));
      }

      RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
   }

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
#ifdef HAVE_VIDEO_FILTER
             !video_st->state_filter ||
#endif
             !video_info.post_filter_record
          || !data
          || video_st->record_gpu_buffer
         ) && recording_state.data
           && recording_state.driver
           && recording_state.driver->push_video)
      recording_dump_frame(
            data, width, height,
            pitch, runloop_idle);

#ifdef HAVE_VIDEO_FILTER
   if (data && video_st->state_filter)
   {
      unsigned output_width                             = 0;
      unsigned output_height                            = 0;
      unsigned output_pitch                             = 0;

      rarch_softfilter_get_output_size(video_st->state_filter,
            &output_width, &output_height, width, height);

      output_pitch = (output_width) * video_st->state_out_bpp;

      rarch_softfilter_process(video_st->state_filter,
            video_st->state_buffer, output_pitch,
            data, width, height, pitch);

      if (video_info.post_filter_record
            && recording_state.data
            && recording_state.driver
            && recording_state.driver->push_video)
         recording_dump_frame(
               video_st->state_buffer,
               output_width, output_height, output_pitch,
               runloop_idle);

      data   = video_st->state_buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }
#endif

   if (runloop_st->msg_queue_size > 0)
   {
      /* If widgets are currently enabled, then
       * messages were pushed to the queue before
       * widgets were initialised - in this case, the
       * first item in the message queue should be
       * extracted and pushed to the widget message
       * queue instead */
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
      {
         msg_queue_entry_t msg_entry;
         bool msg_found = false;

         RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
         msg_found                       = msg_queue_extract(
               &runloop_st->msg_queue, &msg_entry);
         runloop_st->msg_queue_size = msg_queue_size(
               &runloop_st->msg_queue);
         RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);

         if (msg_found)
            gfx_widgets_msg_queue_push(
                  NULL,
                  msg_entry.msg,
                  roundf((float)msg_entry.duration / 60.0f * 1000.0f),
                  msg_entry.title,
                  msg_entry.icon,
                  msg_entry.category,
                  msg_entry.prio,
                  false,
#ifdef HAVE_MENU
                  menu_state_get_ptr()->alive
#else
                  false
#endif
            );
      }
      /* ...otherwise, just output message via
       * regular OSD notification text (if enabled) */
      else if (video_info.font_enable)
#else
      if (video_info.font_enable)
#endif
      {
         const char *msg                 = NULL;
         RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
         msg                             = msg_queue_pull(&runloop_st->msg_queue);
         runloop_st->msg_queue_size = msg_queue_size(&runloop_st->msg_queue);
         if (msg)
            strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));
         RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
      }
   }

   if (video_info.statistics_show)
   {
      audio_statistics_t audio_stats;
      double stddev                          = 0.0;
      struct retro_system_av_info *av_info   = &video_st->av_info;
      unsigned red                           = 255;
      unsigned green                         = 255;
      unsigned blue                          = 255;
      unsigned alpha                         = 255;

      audio_stats.samples                    = 0;
      audio_stats.average_buffer_saturation  = 0.0f;
      audio_stats.std_deviation_percentage   = 0.0f;
      audio_stats.close_to_underrun          = 0.0f;
      audio_stats.close_to_blocking          = 0.0f;

      video_monitor_fps_statistics(NULL, &stddev, NULL);

      video_info.osd_stat_params.x           = 0.010f;
      video_info.osd_stat_params.y           = 0.950f;
      video_info.osd_stat_params.scale       = 1.0f;
      video_info.osd_stat_params.full_screen = true;
      video_info.osd_stat_params.drop_x      = -2;
      video_info.osd_stat_params.drop_y      = -2;
      video_info.osd_stat_params.drop_mod    = 0.3f;
      video_info.osd_stat_params.drop_alpha  = 1.0f;
      video_info.osd_stat_params.color       = COLOR_ABGR(
            red, green, blue, alpha);

      audio_compute_buffer_statistics(&audio_stats);

      snprintf(video_info.stat_text,
            sizeof(video_info.stat_text),
            "Video Statistics:\n -Frame rate: %6.2f fps\n -Frame time: %6.2f ms\n -Frame time deviation: %.3f %%\n"
            " -Frame count: %" PRIu64"\n -Viewport: %d x %d x %3.2f\n"
            "Audio Statistics:\n -Average buffer saturation: %.2f %%\n -Standard deviation: %.2f %%\n -Time spent close to underrun: %.2f %%\n -Time spent close to blocking: %.2f %%\n -Sample count: %d\n"
            "Core Geometry:\n -Size: %u x %u\n -Max Size: %u x %u\n -Aspect: %3.2f\nCore Timing:\n -FPS: %3.2f\n -Sample Rate: %6.2f\n",
            last_fps,
            frame_time / 1000.0f,
            100.0f * stddev,
            video_st->frame_count,
            video_info.width,
            video_info.height,
            video_info.refresh_rate,
            audio_stats.average_buffer_saturation,
            audio_stats.std_deviation_percentage,
            audio_stats.close_to_underrun,
            audio_stats.close_to_blocking,
            audio_stats.samples,
            av_info->geometry.base_width,
            av_info->geometry.base_height,
            av_info->geometry.max_width,
            av_info->geometry.max_height,
            av_info->geometry.aspect_ratio,
            av_info->timing.fps,
            av_info->timing.sample_rate);

      /* TODO/FIXME - add OSD chat text here */
   }

   if (video_st->current_video && video_st->current_video->frame)
      video_st->active = video_st->current_video->frame(
            video_st->data, data, width, height,
            video_st->frame_count, (unsigned)pitch,
            video_info.menu_screensaver_active ? "" : video_driver_msg,
            &video_info);

   video_st->frame_count++;

   /* Display the status text, with a higher priority. */
   if (  (   video_info.fps_show
          || video_info.framecount_show
          || video_info.memory_show
          || video_info.core_status_msg_show
         )
       && !video_info.menu_screensaver_active
      )
   {
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
         strlcpy(
               dispwidget_get_ptr()->gfx_widgets_status_text,
               status_text,
               sizeof(dispwidget_get_ptr()->gfx_widgets_status_text)
               );
      else
#endif
      {
         runloop_msg_queue_push(status_text, 2, 1, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

#if defined(HAVE_CRTSWITCHRES)
   /* trigger set resolution*/
   if (video_info.crt_switch_resolution)
   {
      unsigned native_width                      = width;
      bool dynamic_super_width                   = false;

      video_st->crt_switching_active             = true;

      switch (video_info.crt_switch_resolution_super)
      {
         case 2560:
         case 3840:
         case 1920:
            width               = video_info.crt_switch_resolution_super;
            dynamic_super_width = false;
            break;
         case 1:
            dynamic_super_width = true;
            break;
         default:
            break;
      }

      crt_switch_res_core(
            &video_st->crt_switch_st,
            native_width, width,
            height,
            video_st->core_hz,
            video_info.crt_switch_resolution,
            video_info.crt_switch_center_adjust,
            video_info.crt_switch_porch_adjust,
            video_info.monitor_index,
            dynamic_super_width,
            video_info.crt_switch_resolution_super,
            video_info.crt_switch_hires_menu);
   }
   else if (!video_info.crt_switch_resolution)
#endif
      video_st->crt_switching_active = false;
}

void crt_switch_driver_refresh(void)
{
   /*video_context_driver_reset();*/
   video_driver_reinit(DRIVERS_CMD_ALL);
}

char* crt_switch_core_name(void)
{
   return (char*)runloop_state.system.info.library_name;
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   video_viewport_t *custom_vp             = NULL;
   runloop_state_t             *runloop_st = &runloop_state;
   settings_t *settings                    = config_get_ptr();
   video_driver_state_t *video_st          = video_state_get_ptr();
   input_driver_state_t *input_st          = input_state_get_ptr();
#ifdef HAVE_THREADS
   bool is_threaded                        =
      VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);

   VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded);
#endif
   custom_vp                               = &settings->video_viewport_custom;
#ifdef HAVE_GFX_WIDGETS
   video_info->widgets_active              = dispwidget_get_ptr()->active;
#else
   video_info->widgets_active              = false;
#endif
   video_info->refresh_rate                = settings->floats.video_refresh_rate;
   video_info->crt_switch_resolution       = settings->uints.crt_switch_resolution;
   video_info->crt_switch_resolution_super = settings->uints.crt_switch_resolution_super;
   video_info->crt_switch_center_adjust    = settings->ints.crt_switch_center_adjust;
   video_info->crt_switch_porch_adjust     = settings->ints.crt_switch_porch_adjust;
   video_info->crt_switch_hires_menu       = settings->bools.crt_switch_hires_menu;
   video_info->black_frame_insertion       = settings->uints.video_black_frame_insertion;
   video_info->hard_sync                   = settings->bools.video_hard_sync;
   video_info->hard_sync_frames            = settings->uints.video_hard_sync_frames;
   video_info->fps_show                    = settings->bools.video_fps_show;
   video_info->memory_show                 = settings->bools.video_memory_show;
   video_info->statistics_show             = settings->bools.video_statistics_show;
   video_info->framecount_show             = settings->bools.video_framecount_show;
   video_info->core_status_msg_show        = runloop_core_status_msg.set;
   video_info->aspect_ratio_idx            = settings->uints.video_aspect_ratio_idx;
   video_info->post_filter_record          = settings->bools.video_post_filter_record;
   video_info->input_menu_swap_ok_cancel_buttons    = settings->bools.input_menu_swap_ok_cancel_buttons;
   video_info->max_swapchain_images        = settings->uints.video_max_swapchain_images;
   video_info->windowed_fullscreen         = settings->bools.video_windowed_fullscreen;
   video_info->fullscreen                  = settings->bools.video_fullscreen
      || video_st->force_fullscreen;
   video_info->menu_mouse_enable           = settings->bools.menu_mouse_enable;
   video_info->monitor_index               = settings->uints.video_monitor_index;

   video_info->font_enable                 = settings->bools.video_font_enable;
   video_info->font_msg_pos_x              = settings->floats.video_msg_pos_x;
   video_info->font_msg_pos_y              = settings->floats.video_msg_pos_y;
   video_info->font_msg_color_r            = settings->floats.video_msg_color_r;
   video_info->font_msg_color_g            = settings->floats.video_msg_color_g;
   video_info->font_msg_color_b            = settings->floats.video_msg_color_b;
   video_info->custom_vp_x                 = custom_vp->x;
   video_info->custom_vp_y                 = custom_vp->y;
   video_info->custom_vp_width             = custom_vp->width;
   video_info->custom_vp_height            = custom_vp->height;
   video_info->custom_vp_full_width        = custom_vp->full_width;
   video_info->custom_vp_full_height       = custom_vp->full_height;

#if defined(HAVE_GFX_WIDGETS)
   video_info->widgets_userdata            = dispwidget_get_ptr();
   video_info->widgets_is_paused           = video_st->widgets_paused;
   video_info->widgets_is_fast_forwarding  = video_st->widgets_fast_forward;
   video_info->widgets_is_rewinding        = video_st->widgets_rewinding;
#else
   video_info->widgets_userdata            = NULL;
   video_info->widgets_is_paused           = false;
   video_info->widgets_is_fast_forwarding  = false;
   video_info->widgets_is_rewinding        = false;
#endif

   video_info->width                       = video_st->width;
   video_info->height                      = video_st->height;

   video_info->use_rgba                    = video_st->use_rgba;
   video_info->hdr_enable                  = settings->bools.video_hdr_enable;

   video_info->libretro_running            = false;
   video_info->msg_bgcolor_enable          =
      settings->bools.video_msg_bgcolor_enable;

   video_info->fps_update_interval         = settings->uints.fps_update_interval;
   video_info->memory_update_interval      = settings->uints.memory_update_interval;

#ifdef HAVE_MENU
   video_info->menu_is_alive               = menu_state_get_ptr()->alive;
   video_info->menu_screensaver_active     = menu_state_get_ptr()->screensaver_active;
   video_info->menu_footer_opacity         = settings->floats.menu_footer_opacity;
   video_info->menu_header_opacity         = settings->floats.menu_header_opacity;
   video_info->materialui_color_theme      = settings->uints.menu_materialui_color_theme;
   video_info->ozone_color_theme           = settings->uints.menu_ozone_color_theme;
   video_info->menu_shader_pipeline        = settings->uints.menu_xmb_shader_pipeline;
   video_info->xmb_theme                   = settings->uints.menu_xmb_theme;
   video_info->xmb_color_theme             = settings->uints.menu_xmb_color_theme;
   video_info->timedate_enable             = settings->bools.menu_timedate_enable;
   video_info->battery_level_enable        = settings->bools.menu_battery_level_enable;
   video_info->xmb_shadows_enable          =
      settings->bools.menu_xmb_shadows_enable;
   video_info->xmb_alpha_factor            =
      settings->uints.menu_xmb_alpha_factor;
   video_info->menu_wallpaper_opacity      =
      settings->floats.menu_wallpaper_opacity;
   video_info->menu_framebuffer_opacity    =
      settings->floats.menu_framebuffer_opacity;

   video_info->libretro_running            = runloop_st->current_core.game_loaded;
#else
   video_info->menu_is_alive               = false;
   video_info->menu_screensaver_active     = false;
   video_info->menu_footer_opacity         = 0.0f;
   video_info->menu_header_opacity         = 0.0f;
   video_info->materialui_color_theme      = 0;
   video_info->menu_shader_pipeline        = 0;
   video_info->xmb_color_theme             = 0;
   video_info->xmb_theme                   = 0;
   video_info->timedate_enable             = false;
   video_info->battery_level_enable        = false;
   video_info->xmb_shadows_enable          = false;
   video_info->xmb_alpha_factor            = 0.0f;
   video_info->menu_framebuffer_opacity    = 0.0f;
   video_info->menu_wallpaper_opacity      = 0.0f;
#endif

   video_info->runloop_is_paused             = runloop_st->paused;
   video_info->runloop_is_slowmotion         = runloop_st->slowmotion;

   video_info->input_driver_nonblock_state   = input_st ?
      input_st->nonblocking_flag : false;
   video_info->input_driver_grab_mouse_state = input_st->grab_mouse_state;
   video_info->disp_userdata                 = disp_get_ptr();

   video_info->userdata                      =
VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);

#ifdef HAVE_THREADS
   VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded);
#endif
}

bool video_driver_has_focus(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   return VIDEO_HAS_FOCUS(video_st);
}

void video_driver_get_window_title(char *buf, unsigned len)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   if (buf && video_st->window_title_update)
   {
      strlcpy(buf, video_st->window_title, len);
      video_st->window_title_update = false;
   }
}

#ifdef HAVE_VULKAN
static const gfx_ctx_driver_t *vk_context_driver_init_first(
      runloop_state_t *runloop_st,
      settings_t *settings,
      void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   unsigned j;
   int i = -1;
   video_driver_state_t *video_st = video_state_get_ptr();

   for (j = 0; gfx_ctx_vk_drivers[j]; j++)
   {
      if (string_is_equal_noncase(ident, gfx_ctx_vk_drivers[j]->ident))
      {
         i = j;
         break;
      }
   }

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(
            runloop_st->core_set_shared_context,
            settings,
            data,
            gfx_ctx_vk_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_vk_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(
               runloop_st->core_set_shared_context,
               settings,
               data,
               gfx_ctx_vk_drivers[i], ident,
               api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}
#endif

static const gfx_ctx_driver_t *gl_context_driver_init_first(
      runloop_state_t *runloop_st,
      settings_t *settings,
      void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   unsigned j;
   int i = -1;
   video_driver_state_t *video_st = video_state_get_ptr();

   for (j = 0; gfx_ctx_gl_drivers[j]; j++)
   {
      if (string_is_equal_noncase(ident, gfx_ctx_gl_drivers[j]->ident))
      {
         i = j;
         break;
      }
   }

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(
            runloop_st->core_set_shared_context,
            settings,
            data,
            gfx_ctx_gl_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_gl_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(
               runloop_st->core_set_shared_context,
               settings,
               data,
               gfx_ctx_gl_drivers[i], ident,
               api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   runloop_state_t *runloop_st = &runloop_state;
   settings_t *settings        = config_get_ptr();

   switch (api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         {
            const gfx_ctx_driver_t *ptr = vk_context_driver_init_first(
                  runloop_st, settings,
                  data, ident, api, major, minor, hw_render_ctx, ctx_data);
            if (ptr && !string_is_equal(ptr->ident, "null"))
               return ptr;
            /* fall-through if no valid driver was found */
         }
#endif
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
      case GFX_CTX_METAL_API:
      case GFX_CTX_RSX_API:
         return gl_context_driver_init_first(
               runloop_st, settings,
               data, ident, api, major, minor,
               hw_render_ctx, ctx_data);
      case GFX_CTX_NONE:
      default:
         break;
   }


   return NULL;
}

void video_context_driver_free(void)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   video_context_driver_destroy(&video_st->current_video_context);
   video_st->context_data    = NULL;
}

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (video_st->current_video_context.get_metrics)
      return video_st->current_video_context.get_metrics(
            video_st->context_data,
            metrics->type,
            metrics->value);
   return false;
}

bool video_context_driver_get_refresh_rate(float *refresh_rate)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (!video_st->current_video_context.get_refresh_rate || !refresh_rate)
      return false;
   if (!video_st->context_data)
      return false;

   if (!video_st->crt_switching_active)
   {
      if (refresh_rate)
         *refresh_rate =
             video_st->current_video_context.get_refresh_rate(
                   video_st->context_data);
   }
   else
   {
      float refresh_holder      = 0;
      if (refresh_rate)
         refresh_holder         =
             video_st->current_video_context.get_refresh_rate(
                   video_st->context_data);

      /* Fix for incorrect interlacing detection --
       * HARD SET VSYNC TO REQUIRED REFRESH FOR CRT*/
      if (refresh_holder != video_st->core_hz)
         *refresh_rate          = video_st->core_hz;
   }

   return true;
}

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (!ident)
      return false;
   ident->ident = video_st->current_video_context.ident;
   return true;
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (!video_st->current_video_context.get_flags)
      return false;

   if (video_st->deferred_video_context_driver_set_flags)
   {
      flags->flags                                     =
         video_st->deferred_flag_data.flags;
      video_st->deferred_video_context_driver_set_flags = false;
      return true;
   }

   flags->flags = video_st->current_video_context.get_flags(
         video_st->context_data);
   return true;
}

static bool video_driver_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (!video_st->poke || !video_st->poke->get_flags)
      return false;
   flags->flags = video_st->poke->get_flags(video_st->data);
   return true;
}

gfx_ctx_flags_t video_driver_get_flags_wrapper(void)
{
   gfx_ctx_flags_t flags;
   flags.flags                 = 0;

   if (!video_driver_get_flags(&flags))
      video_context_driver_get_flags(&flags);

   return flags;
}

/**
 * video_driver_test_all_flags:
 * @testflag          : flag to test
 *
 * Poll both the video and context driver's flags and test
 * whether @testflag is set or not.
 **/
bool video_driver_test_all_flags(enum display_flags testflag)
{
   gfx_ctx_flags_t flags;

   if (video_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   if (video_context_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   return false;
}

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (!flags)
      return false;

   if (video_st->current_video_context.set_flags)
   {
      video_st->current_video_context.set_flags(
            video_st->context_data, flags->flags);
      return true;
   }

   video_st->deferred_flag_data.flags                = flags->flags;
   video_st->deferred_video_context_driver_set_flags = true;
   return false;
}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   video_driver_state_t *video_st   = video_state_get_ptr();
   enum gfx_ctx_api         ctx_api = video_st->context_data ?
      video_st->current_video_context.get_api(
            video_st->context_data) : GFX_CTX_NONE;

   if (ctx_api == GFX_CTX_NONE)
   {
      const char *video_ident  = (video_st->current_video)
         ? video_st->current_video->ident
         : NULL;
      if (string_starts_with_size(video_ident, "d3d", STRLEN_CONST("d3d")))
      {
         if (string_is_equal(video_ident, "d3d9"))
            return GFX_CTX_DIRECT3D9_API;
         else if (string_is_equal(video_ident, "d3d10"))
            return GFX_CTX_DIRECT3D10_API;
         else if (string_is_equal(video_ident, "d3d11"))
            return GFX_CTX_DIRECT3D11_API;
         else if (string_is_equal(video_ident, "d3d12"))
            return GFX_CTX_DIRECT3D12_API;
      }
      if (string_starts_with_size(video_ident, "gl", STRLEN_CONST("gl")))
      {
         if (string_is_equal(video_ident, "gl"))
            return GFX_CTX_OPENGL_API;
         else if (string_is_equal(video_ident, "gl1"))
            return GFX_CTX_OPENGL_API;
         else if (string_is_equal(video_ident, "glcore"))
            return GFX_CTX_OPENGL_API;
      }
      else if (string_is_equal(video_ident, "vulkan"))
         return GFX_CTX_VULKAN_API;
      else if (string_is_equal(video_ident, "metal"))
         return GFX_CTX_METAL_API;

      return GFX_CTX_NONE;
   }

   return ctx_api;
}

bool video_driver_has_windowed(void)
{
#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
   video_driver_state_t *video_st   = video_state_get_ptr();
   if (video_st->data && video_st->current_video->has_windowed)
      return video_st->current_video->has_windowed(video_st->data);
#endif
   return false;
}

bool video_driver_cached_frame_has_valid_framebuffer(void)
{
   video_driver_state_t *video_st  = video_state_get_ptr();
   if (video_st->frame_cache_data)
      return (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID);
   return false;
}


bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   void *video_driver                       = video_st->data;
   const video_poke_interface_t *video_poke = video_st->poke;

   shader->data = NULL;
   if (!video_poke || !video_driver || !video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

float video_driver_get_refresh_rate(void)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   if (video_st->poke && video_st->poke->get_refresh_rate)
      return video_st->poke->get_refresh_rate(video_st->data);

   return 0.0f;
}

#if defined(HAVE_GFX_WIDGETS)
bool video_driver_has_widgets(void)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   return   video_st->current_video
         && video_st->current_video->gfx_widgets_enabled
         && video_st->current_video->gfx_widgets_enabled(video_st->data);
}
#endif

void video_driver_set_gpu_device_string(const char *str)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   strlcpy(video_st->gpu_device_string, str,
         sizeof(video_st->gpu_device_string));
}

const char* video_driver_get_gpu_device_string(void)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   return video_st->gpu_device_string;
}

void video_driver_set_gpu_api_version_string(const char *str)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   strlcpy(video_st->gpu_api_version_string, str,
         sizeof(video_st->gpu_api_version_string));
}

const char* video_driver_get_gpu_api_version_string(void)
{
   video_driver_state_t *video_st           = video_state_get_ptr();
   return video_st->gpu_api_version_string;
}

/* CAMERA */

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char *config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

static bool driver_camera_start(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (  p_rarch->camera_driver &&
         p_rarch->camera_data   &&
         p_rarch->camera_driver->start)
   {
      settings_t *settings = config_get_ptr();
      bool camera_allow    = settings->bools.camera_allow;
      if (camera_allow)
         return p_rarch->camera_driver->start(p_rarch->camera_data);

      runloop_msg_queue_push(
            "Camera is explicitly disabled.\n", 1, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

static void driver_camera_stop(void)
{
   struct rarch_state  *p_rarch = &rarch_st;
   if (     p_rarch->camera_driver
         && p_rarch->camera_driver->stop
         && p_rarch->camera_data)
      p_rarch->camera_driver->stop(p_rarch->camera_data);
}

static void camera_driver_find_driver(
      struct rarch_state *p_rarch,
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled)
{
   int i                        = (int)driver_find_index(
         "camera_driver",
         settings->arrays.camera_driver);

   if (i >= 0)
      p_rarch->camera_driver = (const camera_driver_t*)camera_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.camera_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; camera_drivers[d]; d++)
         {
            if (camera_drivers[d])
            {
               RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);
            }
         }

         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      p_rarch->camera_driver = (const camera_driver_t*)camera_drivers[0];

      if (!p_rarch->camera_driver)
         retroarch_fail(p_rarch, 1, "find_camera_driver()");
   }
}

static void driver_adjust_system_rates(
      bool vrr_runloop_enable,
      float video_refresh_rate,
      float audio_max_timing_skew,
      bool video_adaptive_vsync,
      unsigned video_swap_interval)
{
   runloop_state_t     *runloop_st        = &runloop_state;
   video_driver_state_t *video_st         = video_state_get_ptr();
   struct retro_system_av_info *av_info   = &video_st->av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   double input_sample_rate               = info->sample_rate;
   double input_fps                       = info->fps;

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
                  audio_max_timing_skew);

      RARCH_LOG("[Audio]: Set audio input rate to: %.2f Hz.\n",
            audio_st->input);
   }

   runloop_st->force_nonblock       = false;

   if (input_fps > 0.0)
   {
      float timing_skew_hz          = video_refresh_rate;

      if (video_st->crt_switching_active)
         timing_skew_hz             = input_fps;
      video_st->core_hz             = input_fps;

      if (!video_driver_monitor_adjust_system_rates(
         timing_skew_hz,
         video_refresh_rate,
         vrr_runloop_enable,
         audio_max_timing_skew,
         input_fps))
      {
         /* We won't be able to do VSync reliably 
            when game FPS > monitor FPS. */
         runloop_st->force_nonblock = true;
         RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");

         if (VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
         {
            if (video_st->current_video->set_nonblock_state)
               video_st->current_video->set_nonblock_state(
                     video_st->data, true,
                     video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
                     video_adaptive_vsync,
                     video_swap_interval
                     );
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
   runloop_state_t *runloop_st = &runloop_state;
   input_driver_state_t 
      *input_st                = input_state_get_ptr();
   audio_driver_state_t 
      *audio_st                = audio_state_get_ptr();
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
   bool                 enable = input_st ?
      input_st->nonblocking_flag : false;
   settings_t       *settings  = config_get_ptr();
   bool audio_sync             = settings->bools.audio_sync;
   bool video_vsync            = settings->bools.video_vsync;
   bool adaptive_vsync         = settings->bools.video_adaptive_vsync;
   unsigned swap_interval      = settings->uints.video_swap_interval;
   bool video_driver_active    = video_st->active;
   bool audio_driver_active    = audio_st->active;
   bool runloop_force_nonblock = runloop_st->force_nonblock;

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
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
               adaptive_vsync, swap_interval);
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

/**
 * drivers_init:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
static void drivers_init(
      struct rarch_state *p_rarch,
      settings_t *settings,
      int flags,
      bool verbosity_enabled)
{
   runloop_state_t *runloop_st = &runloop_state;
   audio_driver_state_t 
      *audio_st                = audio_state_get_ptr();
   input_driver_state_t 
      *input_st                = input_state_get_ptr();
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();
#endif
   bool video_is_threaded      = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   gfx_display_t *p_disp       = disp_get_ptr();
#if defined(HAVE_GFX_WIDGETS)
   bool video_font_enable      = settings->bools.video_font_enable;
   bool menu_enable_widgets    = settings->bools.menu_enable_widgets;

   /* By default, we want display widgets to persist through driver reinits. */
   dispwidget_get_ptr()->persisting = true;
#endif

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   if (menu_st)
      menu_st->data_own = true;
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates(
                                 settings->bools.vrr_runloop_enable,
                                 settings->floats.video_refresh_rate,
                                 settings->floats.audio_max_timing_skew,
                                 settings->bools.video_adaptive_vsync,
                                 settings->uints.video_swap_interval
                                 );

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
      video_driver_set_cached_frame_ptr(NULL);
      if (!video_driver_init_internal(settings, &video_is_threaded,
               verbosity_enabled))
         retroarch_fail(p_rarch, 1, "video_driver_init_internal()");

      if (!video_st->cache_context_ack
            && hwr->context_reset)
         hwr->context_reset();
      video_st->cache_context_ack = false;
      runloop_st->frame_time_last = 0;
   }

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init_internal(
            settings,
            audio_st->callback.callback != NULL);
      if (  audio_st->current_audio &&
            audio_st->current_audio->device_list_new &&
            audio_st->context_audio_data)
         audio_st->devices_list = (struct string_list*)
            audio_st->current_audio->device_list_new(
                  audio_st->context_audio_data);
   }

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (p_rarch->camera_driver_active)
      {
         /* Resource leaks will follow if camera is initialized twice. */
         if (!p_rarch->camera_data)
         {
            camera_driver_find_driver(p_rarch, settings, "camera driver",
                  verbosity_enabled);

            if (p_rarch->camera_driver)
            {
               p_rarch->camera_data = p_rarch->camera_driver->init(
                     *settings->arrays.camera_device ?
                     settings->arrays.camera_device : NULL,
                     p_rarch->camera_cb.caps,
                     settings->uints.camera_width ?
                     settings->uints.camera_width : p_rarch->camera_cb.width,
                     settings->uints.camera_height ?
                     settings->uints.camera_height : p_rarch->camera_cb.height);

               if (!p_rarch->camera_data)
               {
                  RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
                  p_rarch->camera_driver_active = false;
               }

               if (p_rarch->camera_cb.initialized)
                  p_rarch->camera_cb.initialized();
            }
         }
      }
   }

   if (flags & DRIVER_BLUETOOTH_MASK)
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_INIT, NULL);

   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_INIT, NULL);

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (p_rarch->location_driver_active)
         if (!init_location(&runloop_state.system,
                  settings, verbosity_is_enabled()))
            p_rarch->location_driver_active = false;
   }

   core_info_init_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* Note that we only enable widgets if 'video_font_enable'
    * is true. 'video_font_enable' corresponds to the generic
    * 'On-Screen Notifications' setting, which should serve as
    * a global notifications on/off toggle switch */
   if (video_font_enable &&
       menu_enable_widgets &&
       video_driver_has_widgets())
   {
      bool rarch_force_fullscreen = video_st->force_fullscreen;
      bool video_is_fullscreen    = settings->bools.video_fullscreen ||
            rarch_force_fullscreen;

      dispwidget_get_ptr()->active= gfx_widgets_init(
            p_disp,
            anim_get_ptr(),
            settings,
            (uintptr_t)&dispwidget_get_ptr()->active,
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
      }
   }

   /* Initialising the menu driver will also initialise
    * core info - if we are not initialising the menu
    * driver, must initialise core info 'by hand' */
   if (!(flags & DRIVER_VIDEO_MASK) ||
       !(flags & DRIVER_MENU_MASK))
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
      if (input_st && input_st->nonblocking_flag)
         driver_set_nonblock_state();

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init(settings->arrays.led_driver);

   /* Initialize MIDI  driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init(settings);

#ifdef HAVE_LAKKA
   cpu_scaling_driver_init();
#endif
}

/**
 * Driver ownership - set this to true if the platform in
 * question needs to 'own'
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
static void driver_uninit(struct rarch_state *p_rarch, int flags)
{
   runloop_state_t *runloop_st = &runloop_state;
   video_driver_state_t 
      *video_st                = video_state_get_ptr();

   core_info_deinit_list();
   core_info_free_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* This absolutely has to be done before video_driver_free_internal()
    * is called/completes, otherwise certain menu drivers
    * (e.g. Vulkan) will segfault */
   if (dispwidget_get_ptr()->inited)
   {
      gfx_widgets_deinit(dispwidget_get_ptr()->persisting);
      dispwidget_get_ptr()->active = false;
   }
#endif

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
#ifdef HAVE_LIBRETRODB
      menu_explore_context_deinit();
#endif

      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      uninit_location(&runloop_st->system);

   if ((flags & DRIVER_CAMERA_MASK))
   {
      if (p_rarch->camera_data && p_rarch->camera_driver)
      {
         if (p_rarch->camera_cb.deinitialized)
            p_rarch->camera_cb.deinitialized();

         if (p_rarch->camera_driver->free)
            p_rarch->camera_driver->free(p_rarch->camera_data);
      }

      p_rarch->camera_data = NULL;
   }

   if ((flags & DRIVER_BLUETOOTH_MASK))
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DEINIT, NULL);

   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
   {
      video_driver_free_internal();
      VIDEO_DRIVER_LOCK_FREE(video_st);
      video_st->data = NULL;
      video_driver_set_cached_frame_ptr(NULL);
   }

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit(p_rarch->configuration_settings);

   if ((flags & DRIVER_VIDEO_MASK))
      video_st->data = NULL;

   if ((flags & DRIVER_INPUT_MASK))
      input_state_get_ptr()->current_data = NULL;

   if ((flags & DRIVER_AUDIO_MASK))
      audio_state_get_ptr()->context_audio_data = NULL;

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();

#ifdef HAVE_LAKKA
   cpu_scaling_driver_free();
#endif
}

static void retroarch_deinit_drivers(
      struct rarch_state *p_rarch, struct retro_callbacks *cbs)
{
   input_driver_state_t *input_st  = input_state_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   runloop_state_t     *runloop_st = &runloop_state;

#if defined(HAVE_GFX_WIDGETS)
   /* Tear down display widgets no matter what
    * in case the handle is lost in the threaded
    * video driver in the meantime
    * (breaking video_driver_has_widgets) */
   if (dispwidget_get_ptr()->inited)
   {
      gfx_widgets_deinit(
            dispwidget_get_ptr()->persisting);
      dispwidget_get_ptr()->active = false;
   }
#endif

#if defined(HAVE_CRTSWITCHRES)
   /* Switchres deinit */
   if (video_st->crt_switching_active)
   {
#if defined(DEBUG)
      RARCH_LOG("[CRT]: Getting video info\n");
      RARCH_LOG("[CRT]: About to destroy SR\n");
#endif
      crt_destroy_modes(&video_st->crt_switch_st);
   }
#endif

   /* Video */
   video_display_server_destroy();

   video_st->use_rgba                   = false;
   video_st->hdr_support                = false;
   video_st->active                     = false;
   video_st->cache_context              = false;
   video_st->cache_context_ack          = false;
   video_st->record_gpu_buffer          = NULL;
   video_st->current_video              = NULL;
   video_driver_set_cached_frame_ptr(NULL);

   /* Audio */
   audio_state_get_ptr()->active                    = false;
   audio_state_get_ptr()->current_audio             = NULL;

   /* Input */
   input_st->keyboard_linefeed_enable               = false;
   input_st->block_hotkey                           = false;
   input_st->block_libretro_input                   = false;

   if (input_st)
      input_st->nonblocking_flag                    = false;

   memset(&input_st->turbo_btns, 0, sizeof(turbo_buttons_t));
   memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));
   input_st->current_driver                         = NULL;

#ifdef HAVE_MENU
   menu_driver_destroy(
         menu_state_get_ptr());
#endif
   p_rarch->location_driver_active                  = false;
   destroy_location();

   /* Camera */
   p_rarch->camera_driver_active                    = false;
   p_rarch->camera_driver                           = NULL;
   p_rarch->camera_data                             = NULL;

   bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DESTROY, NULL);
   wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);

   cbs->frame_cb                                    = retro_frame_null;
   cbs->poll_cb                                     = retro_input_poll_null;
   cbs->sample_cb                                   = NULL;
   cbs->sample_batch_cb                             = NULL;
   cbs->state_cb                                    = NULL;

   runloop_st->current_core.inited                  = false;
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   driver_ctx_info_t      *drv = (driver_ctx_info_t*)data;

   switch (state)
   {
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz                    = (float*)data;
            audio_driver_state_t 
               *audio_st                 = audio_state_get_ptr();
            settings_t *settings         = config_get_ptr();
            unsigned 
               audio_output_sample_rate  = settings->uints.audio_output_sample_rate;
            bool vrr_runloop_enable      = settings->bools.vrr_runloop_enable;
            float video_refresh_rate     = settings->floats.video_refresh_rate;
            float audio_max_timing_skew  = settings->floats.audio_max_timing_skew;
            bool video_adaptive_vsync    = settings->bools.video_adaptive_vsync;
            unsigned video_swap_interval = settings->uints.video_swap_interval;

            video_monitor_set_refresh_rate(*hz);

            /* Sets audio monitor rate to new value. */
            audio_st->source_ratio_original   =
            audio_st->source_ratio_current    = 
            (double)audio_output_sample_rate / audio_st->input;

            driver_adjust_system_rates(
                                       vrr_runloop_enable,
                                       video_refresh_rate,
                                       audio_max_timing_skew,
                                       video_adaptive_vsync,
                                       video_swap_interval
                                       );
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

/* RUNAHEAD */

#ifdef HAVE_RUNAHEAD
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
   element->state              = (int16_t*)calloc(256, sizeof(int16_t));
   element->state_size         = 256;

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

static int16_t input_state_get_last(unsigned port,
      unsigned device, unsigned index, unsigned id)
{
   unsigned i;
   runloop_state_t      *runloop_st = &runloop_state;

   if (!runloop_st->input_state_list)
      return 0;

   /* find list item */
   for (i = 0; i < (unsigned)runloop_st->input_state_list->size; i++)
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
         runloop_st->input_is_dirty = true;
      /*arbitrary limit of up to 65536 elements in state array*/
      if (id < 65536)
         input_state_set_last(runloop_st, port, device, index, id, result);

      return result;
   }
   return 0;
}

static void reset_hook(void)
{
   runloop_state_t     *runloop_st = &runloop_state;

   runloop_st->input_is_dirty      = true;

   if (runloop_st->retro_reset_callback_original)
      runloop_st->retro_reset_callback_original();
}

static bool unserialize_hook(const void *buf, size_t size)
{
   runloop_state_t     *runloop_st = &runloop_state;

   runloop_st->input_is_dirty      = true;

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

   if (    (runloop_st->runahead_save_state_size > 0)
         && runloop_st->runahead_save_state_size_known)
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
   runloop_st->runahead_save_state_size_known = true;

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
   secondary_core_destroy(runloop_st);
   if (runloop_st->current_core.retro_unload_game)
      runloop_st->current_core.retro_unload_game();
   runloop_st->core_poll_type_override = POLL_TYPE_OVERRIDE_DONTCARE;
}

static void runahead_deinit_hook(void)
{
   runloop_state_t     *runloop_st = &runloop_state;

   runahead_remove_hooks(runloop_st);
   runahead_destroy(runloop_st);
   secondary_core_destroy(runloop_st);
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
   runloop_st->runahead_available             = false;
   mylist_destroy(&runloop_st->runahead_save_state_list);
   runahead_remove_hooks(runloop_st);
   runloop_st->runahead_save_state_size       = 0;
   runloop_st->runahead_save_state_size_known = true;
}

static bool runahead_create(runloop_state_t *runloop_st)
{
   /* get savestate size and allocate buffer */
   retro_ctx_size_info_t info;
   video_driver_state_t *video_st           = video_state_get_ptr();

   runloop_st->request_fast_savestate       = true;
   core_serialize_size(&info);
   runloop_st->request_fast_savestate       = false;

   runahead_save_state_list_init(runloop_st, info.size);
   video_st->runahead_is_active             = video_st->active;

   if (  (runloop_st->runahead_save_state_size == 0) ||
         !runloop_st->runahead_save_state_size_known)
   {
      runahead_error(runloop_st);
      return false;
   }

   runahead_add_hooks(runloop_st);
   runloop_st->runahead_force_input_dirty = true;
   if (runloop_st->runahead_save_state_list)
      mylist_resize(runloop_st->runahead_save_state_list, 1, true);
   return true;
}

static bool runahead_save_state(runloop_state_t *runloop_st)
{
   retro_ctx_serialize_info_t *serialize_info;
   bool okay                       = false;

   if (!runloop_st->runahead_save_state_list)
      return false;

   serialize_info                  =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   runloop_st->request_fast_savestate = true;
   okay                               = core_serialize(serialize_info);
   runloop_st->request_fast_savestate = false;

   if (okay)
      return true;

   runahead_error(runloop_st);
   return false;
}

static bool runahead_load_state(runloop_state_t *runloop_st)
{
   bool okay                                  = false;
   retro_ctx_serialize_info_t *serialize_info = 
      (retro_ctx_serialize_info_t*)
      runloop_st->runahead_save_state_list->data[0];
   bool last_dirty                            = runloop_st->input_is_dirty;

   runloop_st->request_fast_savestate         = true;
   /* calling core_unserialize has side effects with
    * netplay (it triggers transmitting your save state)
      call retro_unserialize directly from the core instead */
   okay = runloop_st->current_core.retro_unserialize(
         serialize_info->data_const, serialize_info->size);

   runloop_st->request_fast_savestate         = false;
   runloop_st->input_is_dirty                 = last_dirty;

   if (!okay)
      runahead_error(runloop_st);

   return okay;
}

#if HAVE_DYNAMIC
static bool runahead_load_state_secondary(struct rarch_state *p_rarch)
{
   bool okay                                  = false;
   runloop_state_t                *runloop_st = &runloop_state;
   settings_t                       *settings = config_get_ptr();
   retro_ctx_serialize_info_t *serialize_info =
      (retro_ctx_serialize_info_t*)runloop_st->runahead_save_state_list->data[0];

   runloop_st->request_fast_savestate         = true;
   okay                                       = 
      secondary_core_deserialize(p_rarch, settings,
         serialize_info->data_const, (int)serialize_info->size);
   runloop_st->request_fast_savestate         = false;

   if (!okay)
   {
      runloop_st->runahead_secondary_core_available = false;
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
      struct rarch_state *p_rarch,
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

   if (runahead_count <= 0 || !runloop_st->runahead_available)
      goto force_input_dirty;

   if (!runloop_st->runahead_save_state_size_known)
   {
      if (!runahead_create(runloop_st))
      {
         if (!runahead_hide_warnings)
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES), 0, 2 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }
   }

   /* Check for GUI */
   /* Hack: If we were in the GUI, force a resync. */
   if (frame_count != runloop_st->runahead_last_frame_count + 1)
      runloop_st->runahead_force_input_dirty = true;

   runloop_st->runahead_last_frame_count        = frame_count;

   if (     !use_secondary
         || !have_dynamic
         || !runloop_st->runahead_secondary_core_available)
   {
      /* TODO: multiple savestates for higher performance
       * when not using secondary core */
      for (frame_number = 0; frame_number <= runahead_count; frame_number++)
      {
         last_frame      = frame_number == runahead_count;
         suspended_frame = !last_frame;

         if (suspended_frame)
         {
            audio_st->suspended          = true;
            video_st->active             = false;
         }

         if (frame_number == 0)
            core_run();
         else
            runahead_core_run_use_last_input(runloop_st);

         if (suspended_frame)
         {
            RUNAHEAD_RESUME_VIDEO(video_st);
            audio_st->suspended     = false;
         }

         if (frame_number == 0)
         {
            if (!runahead_save_state(runloop_st))
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }

         if (last_frame)
         {
            if (!runahead_load_state(runloop_st))
            {
               runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
               return;
            }
         }
      }
   }
   else
   {
#if HAVE_DYNAMIC
      if (!secondary_core_ensure_exists(p_rarch,
               runloop_st, config_get_ptr()))
      {
         secondary_core_destroy(runloop_st);
         runloop_st->runahead_secondary_core_available = false;
         runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         goto force_input_dirty;
      }

      /* run main core with video suspended */
      video_st->active     = false;
      core_run();
      RUNAHEAD_RESUME_VIDEO(video_st);

      if (     runloop_st->input_is_dirty
            || runloop_st->runahead_force_input_dirty)
      {
         runloop_st->input_is_dirty       = false;

         if (!runahead_save_state(runloop_st))
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_SAVE_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         if (!runahead_load_state_secondary(p_rarch))
         {
            runloop_msg_queue_push(msg_hash_to_str(MSG_RUNAHEAD_FAILED_TO_LOAD_STATE), 0, 3 * 60, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            return;
         }

         for (frame_number = 0; frame_number < runahead_count - 1; frame_number++)
         {
            video_st->active             = false;
            audio_st->suspended          = true;
            audio_st->hard_disable       = true;
            RUNAHEAD_RUN_SECONDARY(p_rarch);
            audio_st->hard_disable       = false;
            audio_st->suspended          = false;
            RUNAHEAD_RESUME_VIDEO(video_st);
         }
      }
      audio_st->suspended                = true;
      audio_st->hard_disable             = true;
      RUNAHEAD_RUN_SECONDARY(p_rarch);
      audio_st->hard_disable             = false;
      audio_st->suspended                = false;
#endif
   }
   runloop_st->runahead_force_input_dirty= false;
   return;

force_input_dirty:
   core_run();
   runloop_st->runahead_force_input_dirty= true;
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
   bool runloop_slowmotion              = runloop_st->slowmotion;
   bool runloop_fastmotion              = runloop_st->fastmotion;

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
   _PSUPP_BUF(buf, SUPPORTS_PYTHON,          "Python",          "Script support in shaders");
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

   fprintf(stderr, "%s: %s -- v%s",
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
   frontend_driver_attach_console();
   puts("===================================================================");
   retroarch_print_version();
   puts("===================================================================");

   printf("Usage: %s [OPTIONS]... [FILE]\n", arg0);

   {
      char buf[2148];
      buf[0] = '\0';

      strlcpy(buf, "  -h, --help            Show this help message.\n", sizeof(buf));
      strlcat(buf, "  -v, --verbose         Verbose logging.\n",        sizeof(buf));
      strlcat(buf, "      --log-file FILE   Log messages to FILE.\n",   sizeof(buf));
      strlcat(buf, "      --version         Show version.\n",           sizeof(buf));
      strlcat(buf, "      --features        Prints available features compiled into "
            "program.\n", sizeof(buf));

#ifdef HAVE_MENU
      strlcat(buf, "      --menu            Do not require content or libretro core to "
            "be loaded,\n"
            "                        starts directly in menu. If no arguments "
            "are passed to\n"
            "                        the program, it is equivalent to using "
            "--menu as only argument.\n", sizeof(buf));
#endif

      strlcat(buf, "  -s, --save=PATH       Path for save files (*.srm). (DEPRECATED, use --appendconfig and savefile_directory)\n", sizeof(buf));
      strlcat(buf, "  -S, --savestate=PATH  Path for the save state files (*.state). (DEPRECATED, use --appendconfig and savestate_directory)\n", sizeof(buf));
      strlcat(buf, "      --set-shader PATH Path to a shader (preset) that will be loaded each time content is loaded.\n"
            "                        Effectively overrides automatic shader presets.\n"
            "                        An empty argument \"\" will disable automatic shader presets.\n", sizeof(buf));
      strlcat(buf, "  -f, --fullscreen      Start the program in fullscreen regardless "
            "of config settings.\n", sizeof(buf));
#ifdef HAVE_CONFIGFILE
#ifdef _WIN32
      strlcat(buf, "  -c, --config=FILE     Path for config file."
            "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe."
            "\n\t\tIf a default config is not found, the program will attempt to "
            "create one.\n"
            , sizeof(buf));
#else
      strlcat(buf, "  -c, --config=FILE     Path for config file."
            "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/"
            "retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\t"
            "and $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, "
            "the program will attempt to create one based on the \n\t\t"
            "skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg). \n"
            , sizeof(buf));
#endif
#endif
      strlcat(buf, "      --appendconfig=FILE\n"
            "                        Extra config files are loaded in, "
            "and take priority over\n"
            "                        config selected in -c (or default). "
            "Multiple configs are\n"
            "                        delimited by '|'.\n", sizeof(buf));
#ifdef HAVE_DYNAMIC
      strlcat(buf, "  -L, --libretro=FILE   Path to libretro implementation. "
            "Overrides any config setting.\n", sizeof(buf));
#endif
      strlcat(buf, "      --subsystem=NAME  Use a subsystem of the libretro core. "
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
            "to be loaded.\n", sizeof(buf));
      puts(buf);
   }

   printf("  -N, --nodevice=PORT\n"
          "                        Disconnects controller device connected "
          "to PORT (1 to %d).\n", MAX_USERS);
   printf("  -A, --dualanalog=PORT\n"
          "                        Connect a DualAnalog controller to PORT "
          "(1 to %d).\n", MAX_USERS);
   printf("  -d, --device=PORT:ID\n"
          "                        Connect a generic device into PORT of "
          "the device (1 to %d).\n", MAX_USERS);

   {
      char buf[2560];
      buf[0] = '\0';
      strlcpy(buf, "                        Format is PORT:ID, where ID is a number "
            "corresponding to the particular device.\n", sizeof(buf));
#ifdef HAVE_BSV_MOVIE
      strlcat(buf, "  -P, --bsvplay=FILE    Playback a BSV movie file.\n", sizeof(buf));
      strlcat(buf, "  -R, --bsvrecord=FILE  Start recording a BSV movie file from "
            "the beginning.\n", sizeof(buf));
      strlcat(buf, "      --eof-exit        Exit upon reaching the end of the "
            "BSV movie file.\n", sizeof(buf));
#endif
      strlcat(buf, "  -M, --sram-mode=MODE  SRAM handling mode. MODE can be "
            "'noload-nosave',\n"
            "                        'noload-save', 'load-nosave' or "
            "'load-save'.\n"
            "                        Note: 'noload-save' implies that "
            "save files *WILL BE OVERWRITTEN*.\n", sizeof(buf));
#ifdef HAVE_NETWORKING
      strlcat(buf, "  -H, --host            Host netplay as user 1.\n", sizeof(buf));
      strlcat(buf, "  -C, --connect=HOST    Connect to netplay server as user 2.\n", sizeof(buf));
      strlcat(buf, "      --port=PORT       Port used to netplay. Default is 55435.\n", sizeof(buf));
      strlcat(buf, "      --stateless       Use \"stateless\" mode for netplay\n", sizeof(buf));
      strlcat(buf, "                        (requires a very fast network).\n", sizeof(buf));
      strlcat(buf, "      --check-frames=NUMBER\n"
            "                        Check frames when using netplay.\n", sizeof(buf));
#ifdef HAVE_NETWORK_CMD
      strlcat(buf, "      --command         Sends a command over UDP to an already "
            "running program process.\n", sizeof(buf));
      strlcat(buf, "      Available commands are listed if command is invalid.\n", sizeof(buf));
#endif

#endif

      strlcat(buf, "      --nick=NICK       Picks a username (for use with netplay). "
            "Not mandatory.\n", sizeof(buf));
      strlcat(buf, "  -r, --record=FILE     Path to record video file.\n        "
            "Using .mkv extension is recommended.\n", sizeof(buf));
      strlcat(buf, "      --recordconfig    Path to settings used during recording.\n", sizeof(buf));
      strlcat(buf, "      --size=WIDTHxHEIGHT\n"
            "                        Overrides output video size when recording.\n", sizeof(buf));
#ifdef HAVE_PATCH
      strlcat(buf, "  -U, --ups=FILE        Specifies path for UPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --bps=FILE        Specifies path for BPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --ips=FILE        Specifies path for IPS patch that will be "
            "applied to content.\n", sizeof(buf));
      strlcat(buf, "      --no-patch        Disables all forms of content patching.\n", sizeof(buf));
#endif
      strlcat(buf, "  -D, --detach          Detach program from the running console. "
            "Not relevant for all platforms.\n", sizeof(buf));
      strlcat(buf, "      --max-frames=NUMBER\n"
            "                        Runs for the specified number of frames, "
            "then exits.\n", sizeof(buf));
#ifdef HAVE_SCREENSHOTS
      strlcat(buf, "      --max-frames-ss\n"
            "                        Takes a screenshot at the end of max-frames.\n", sizeof(buf));
      strlcat(buf, "      --max-frames-ss-path=FILE\n"
            "                        Path to save the screenshot to at the end of max-frames.\n", sizeof(buf));
#endif
#ifdef HAVE_ACCESSIBILITY
      strlcat(buf, "      --accessibility\n"
            "                        Enables accessibilty for blind users using text-to-speech.\n", sizeof(buf));
#endif
      strlcat(buf, "      --load-menu-on-error\n"
            "                        Open menu instead of quitting if specified core or content fails to load.\n", sizeof(buf));
      puts(buf);
   }
}

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
   video_driver_state_t *video_st  = video_state_get_ptr();
   runloop_state_t     *runloop_st = &runloop_state;
   settings_t          *settings   = config_get_ptr();

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

      first_run = false;

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
   *global->name.ups                     = '\0';
   *global->name.bps                     = '\0';
   *global->name.ips                     = '\0';
#endif
#ifdef HAVE_CONFIGFILE
   runloop_st->overrides_active          = false;
#endif
   global->cli_load_menu_on_error        = false;

   /* Make sure we can call retroarch_parse_input several times ... */
   optind    = 0;
   optstring = "hs:fvS:A:U:DN:d:"
      BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG CONFIG_FILE_ARG;

#if defined(ORBIS)
   argv      = &(argv[2]);
   argc      = argc - 2;
#elif defined(WEBOS)
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
               retroarch_print_help(argv[0]);
               retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
      config_load(&p_rarch->g_extern);
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
                     retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
                     retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
                     retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
               if (recording_state.enable)
                  recording_state.enable = true;
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
               {
                  int path_stats;

                  if (string_ends_with_size(optarg, "builtin",
                           strlen(optarg), STRLEN_CONST("builtin")))
                  {
                     RARCH_LOG("--libretro argument \"%s\" is a built-in core. Ignoring.\n",
                           optarg);
                     break;
                  }

                  path_stats = path_stat(optarg);

                  if ((path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
                  {
                     path_clear(RARCH_PATH_CORE);

                     configuration_set_string(settings,
                     settings->paths.directory_libretro, optarg);

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
                     runloop_set_current_core_type(CORE_TYPE_PLAIN, false);
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
                  retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
               retroarch_override_setting_set(
                     RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
               netplay_driver_ctl(RARCH_NETPLAY_CTL_ENABLE_CLIENT, NULL);
               configuration_set_string(settings,
                     settings->paths.netplay_server, optarg);
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
                  retroarch_fail(p_rarch, 1, "network_cmd_send()");
#endif
               break;
#endif

#endif

            case RA_OPT_BPS:
#ifdef HAVE_PATCH
               strlcpy(global->name.bps, optarg,
                     sizeof(global->name.bps));
               p_rarch->rarch_bps_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_BPS_PREF, NULL);
#endif
               break;

            case 'U':
#ifdef HAVE_PATCH
               strlcpy(global->name.ups, optarg,
                     sizeof(global->name.ups));
               p_rarch->rarch_ups_pref = true;
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_UPS_PREF, NULL);
#endif
               break;

            case RA_OPT_IPS:
#ifdef HAVE_PATCH
               strlcpy(global->name.ips, optarg,
                     sizeof(global->name.ips));
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
                        &recording_state.width,
                        &recording_state.height) != 2)
               {
                  RARCH_ERR("Wrong format for --size.\n");
                  retroarch_print_help(argv[0]);
                  retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
               }
               break;

            case RA_OPT_RECORDCONFIG:
               strlcpy(global->record.config, optarg,
                     sizeof(global->record.config));
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
               retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
            case RA_OPT_ACCESSIBILITY:
#ifdef HAVE_ACCESSIBILITY
               p_rarch->accessibility_enabled = true;
#endif
               break;
            case RA_OPT_LOAD_MENU_ON_ERROR:
               global->cli_load_menu_on_error = true;
               break;
            default:
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_ERROR_PARSING_ARGUMENTS));
               retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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
         retroarch_fail(p_rarch, 1, "retroarch_parse_input()");
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

   /* Check whether a core has been set via the
    * command line interface */
   cli_core_set = (runloop_st->current_core_type != CORE_TYPE_DUMMY);

   /* Update global 'content launched from command
    * line' status flag */
   global->launched_from_cli = cli_active && (cli_core_set || cli_content_set);

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_SAVE_PATH, NULL) &&
         path_is_directory(global->name.savefile))
      dir_set(RARCH_DIR_SAVEFILE, global->name.savefile);

   if (retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_STATE_PATH, NULL) &&
         path_is_directory(global->name.savestate))
      dir_set(RARCH_DIR_SAVESTATE, global->name.savestate);

   return verbosity_enabled;
}

/* Validates CPU features for given processor architecture.
 * Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support,
 * but this will do for now. */
static void retroarch_validate_cpu_features(struct rarch_state *p_rarch)
{
   uint64_t cpu = cpu_features_get();
   (void)cpu;

#ifdef __MMX__
   if (!(cpu & RETRO_SIMD_MMX))
      FAIL_CPU(p_rarch, "MMX");
#endif
#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU(p_rarch, "SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU(p_rarch, "SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU(p_rarch, "AVX");
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
   runloop_state_t *runloop_st  = &runloop_state;
   input_driver_state_t 
      *input_st                 = input_state_get_ptr();
   video_driver_state_t*video_st= video_state_get_ptr();
   settings_t *settings         = config_get_ptr();
   global_t            *global  = &p_rarch->g_extern;
#ifdef HAVE_ACCESSIBILITY
   bool accessibility_enable    = false;
   unsigned accessibility_narrator_speech_speed = 0;
#endif
#ifdef HAVE_MENU
   struct menu_state *menu_st   = menu_state_get_ptr();
#endif

   input_st->osk_idx            = OSK_LOWERCASE_LATIN;
   video_st->active             = true;
   audio_state_get_ptr()->active= true;

   if (setjmp(p_rarch->error_sjlj_context) > 0)
   {
      RARCH_ERR("%s: \"%s\"\n",
            msg_hash_to_str(MSG_FATAL_ERROR_RECEIVED_IN), p_rarch->error_string);
      goto error;
   }

   p_rarch->rarch_error_on_init = true;

   /* Have to initialise non-file logging once at the start... */
   retro_main_log_file_init(NULL, false);

   verbosity_enabled = retroarch_parse_input_and_config(p_rarch, &p_rarch->g_extern, argc, argv);

#ifdef HAVE_ACCESSIBILITY
   accessibility_enable                = settings->bools.accessibility_enable;
   accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
   /* State that the narrator is on, and also include the first menu
      item we're on at startup. */
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
      accessibility_speak_priority(p_rarch,
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

         RARCH_LOG_OUTPUT(str_output);
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
         RARCH_LOG_OUTPUT(str_output);
      }
   }

#if defined(DEBUG) && defined(HAVE_DRMINGW)
   RARCH_LOG_OUTPUT("Initializing Dr.MingW Exception handler\n");
   fill_str_dated_filename(log_file_name, "crash",
         "log", sizeof(log_file_name));
   ExcHndlInit();
   ExcHndlSetLogFileNameA(log_file_name);
#endif

   retroarch_validate_cpu_features(p_rarch);
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
#ifdef HAVE_GONG
            case RARCH_CONTENT_GONG:
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL);
               runloop_set_current_core_type(CORE_TYPE_GONG, false);
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
      retroarch_fail(p_rarch, 1, "audio_driver_find()");
   if (!video_driver_find_driver(settings,
         "video driver", verbosity_enabled))
      retroarch_fail(p_rarch, 1, "video_driver_find_driver()");
   if (!input_driver_find_driver(
            settings,
            "input driver", verbosity_enabled))
      retroarch_fail(p_rarch, 1, "input_driver_find_driver()");

   camera_driver_find_driver(p_rarch, settings,
         "camera driver", verbosity_enabled);
   bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_FIND_DRIVER, NULL);
   wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);
   location_driver_find_driver(settings,
         "location driver", verbosity_enabled);
#ifdef HAVE_MENU
   {
      if (!(menu_st->driver_ctx = menu_driver_find_driver(settings,
                  "menu driver", verbosity_enabled)))
         retroarch_fail(p_rarch, 1, "menu_driver_find_driver()");
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
          * - Disable any active config overrides
          * - Unload any active input remaps */
#ifdef HAVE_CONFIGFILE
         if (runloop_st->overrides_active)
         {
            /* Reload the original config */
            config_unload_override();
            runloop_st->overrides_active = false;
         }
#endif
         if (     runloop_st->remaps_core_active
               || runloop_st->remaps_content_dir_active
               || runloop_st->remaps_game_active
            )
         {
            input_remapping_deinit();
            input_remapping_set_defaults(true);
         }
         else
            input_remapping_restore_global_config(true);

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
   drivers_init(p_rarch, settings, DRIVERS_CMD_ALL, verbosity_enabled);
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
   if (!string_is_empty(global->record.path))
      command_event(CMD_EVENT_RECORD_INIT, NULL);

   path_init_savefile(runloop_st);

   command_event(CMD_EVENT_SET_PER_GAME_RESOLUTION, NULL);

   p_rarch->rarch_error_on_init     = false;
   runloop_st->is_inited            = true;

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

static void runloop_task_msg_queue_push(
      retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
#if defined(HAVE_GFX_WIDGETS)
   struct rarch_state *p_rarch = &rarch_st;
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();
#endif
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings        = config_get_ptr();
   bool accessibility_enable   = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   runloop_state_t *runloop_st = &runloop_state;
   bool widgets_active         = dispwidget_get_ptr()->active;

   if (widgets_active && task->title && !task->mute)
   {
      RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
      ui_companion_driver_msg_queue_push(p_rarch, msg,
            prio, task ? duration : duration * 60 / 1000, flush);
#ifdef HAVE_ACCESSIBILITY
      if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
         accessibility_speak_priority(p_rarch,
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
            menu_st->alive
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
         runloop_st->remaps_core_active = true;
         break;
      case RARCH_CTL_IS_REMAPS_CORE_ACTIVE:
         return runloop_st->remaps_core_active;
      case RARCH_CTL_SET_REMAPS_GAME_ACTIVE:
         runloop_st->remaps_game_active = true;
         break;
      case RARCH_CTL_IS_REMAPS_GAME_ACTIVE:
         return runloop_st->remaps_game_active;
      case RARCH_CTL_SET_REMAPS_CONTENT_DIR_ACTIVE:
         runloop_st->remaps_content_dir_active = true;
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
            runloop_fastmotion_override_free(&runloop_state);
            runloop_core_options_cb_free(&runloop_state);
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
   struct rarch_state            *p_rarch = &rarch_st;

   switch (enum_idx)
   {
      case RARCH_OVERRIDE_SETTING_LIBRETRO_DEVICE:
         {
            unsigned *val = (unsigned*)data;
            if (val)
            {
               unsigned                bit = *val;
	       runloop_state_t *runloop_st = &runloop_state;
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
         return p_rarch->has_set_netplay_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS:
         return p_rarch->has_set_netplay_ip_address;
      case RARCH_OVERRIDE_SETTING_NETPLAY_IP_PORT:
         return p_rarch->has_set_netplay_ip_port;
      case RARCH_OVERRIDE_SETTING_NETPLAY_STATELESS_MODE:
         return p_rarch->has_set_netplay_stateless_mode;
      case RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES:
         return p_rarch->has_set_netplay_check_frames;
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

void runloop_set_current_core_type(
      enum rarch_core_type type, bool explicitly_set)
{
   runloop_state_t *runloop_st                = &runloop_state;

   if (runloop_st->has_set_core)
      return;

   if (explicitly_set)
   {
      runloop_st->has_set_core                = true;
      runloop_st->explicit_current_core_type  = type;
   }
   runloop_st->current_core_type              = type;
}

/**
 * retroarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
static void retroarch_fail(struct rarch_state *p_rarch,
      int error_code, const char *error)
{
   /* We cannot longjmp unless we're in retroarch_main_init().
    * If not, something went very wrong, and we should
    * just exit right away. */
   retro_assert(p_rarch->rarch_error_on_init);

   strlcpy(p_rarch->error_string,
         error, sizeof(p_rarch->error_string));
   longjmp(p_rarch->error_sjlj_context, error_code);
}

bool retroarch_main_quit(void)
{
   runloop_state_t *runloop_st   = &runloop_state;
   video_driver_state_t*video_st = video_state_get_ptr();
   settings_t *settings          = config_get_ptr();
   global_t            *global   = global_get_ptr();
#ifdef HAVE_DISCORD
   struct rarch_state *p_rarch   = &rarch_st;
   discord_state_t *discord_st   = &p_rarch->discord_st;
   if (discord_is_inited)
   {
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_SHUTDOWN;
      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
   }
   if (discord_st->ready)
   {
      Discord_ClearPresence();
#ifdef DISCORD_DISABLE_IO_THREAD
      Discord_UpdateConnection();
#endif
      Discord_Shutdown();
      discord_st->ready       = false;
   }
   discord_is_inited          = false;
#endif

   /* Restore original refresh rate, if it has been changed
    * automatically in SET_SYSTEM_AV_INFO */
   if (video_st->video_refresh_rate_original)
      video_display_server_restore_refresh_rate();

   if (!runloop_st->shutdown_initiated)
   {
      command_event_save_auto_state(
            settings->bools.savestate_auto_save,
            global,
            runloop_st->current_core_type);

      /* If any save states are in progress, wait
       * until all tasks are complete (otherwise
       * save state file may be truncated) */
      content_wait_for_save_state_task();

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

      if (     runloop_st->remaps_core_active
            || runloop_st->remaps_content_dir_active
            || runloop_st->remaps_game_active
         )
      {
         input_remapping_deinit();
         input_remapping_set_defaults(true);
      }
      else
         input_remapping_restore_global_config(true);
   }

   runloop_st->shutdown_initiated = true;
#ifdef HAVE_MENU
   retroarch_menu_running_finished(true);
#endif

   return true;
}

void runloop_msg_queue_push(const char *msg,
      unsigned prio, unsigned duration,
      bool flush,
      char *title,
      enum message_queue_icon icon,
      enum message_queue_category category)
{
   struct rarch_state *p_rarch = &rarch_st;
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active         = dispwidget_get_ptr()->active;
#endif
#ifdef HAVE_ACCESSIBILITY
   settings_t *settings        = config_get_ptr();
   bool accessibility_enable   = settings->bools.accessibility_enable;
   unsigned accessibility_narrator_speech_speed = settings->uints.accessibility_narrator_speech_speed;
#endif
   runloop_state_t *runloop_st = &runloop_state;

   RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
#ifdef HAVE_ACCESSIBILITY
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
      accessibility_speak_priority(p_rarch,
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
            menu_state_get_ptr()->alive
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

   ui_companion_driver_msg_queue_push(p_rarch,
         msg,
         prio, duration, flush);

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
   bool runloop_idle             = runloop_st->idle;
   video_driver_state_t*video_st = video_state_get_ptr();

   if (     video_st->poke
         && video_st->poke->set_texture_enable)
      video_st->poke->set_texture_enable(video_st->data, true, false);

   if (libretro_running)
   {
      if (!input_st->block_libretro_input)
         input_st->block_libretro_input = true;

      core_run();
      runloop_st->core_runtime_usec       +=
         runloop_core_runtime_tick(runloop_st, slowmotion_ratio, current_time);
      input_st->block_libretro_input    = false;

      return false;
   }

   if (runloop_idle)
   {
#ifdef HAVE_DISCORD
      discord_userdata_t userdata;
      userdata.status = DISCORD_PRESENCE_GAME_PAUSED;

      command_event(CMD_EVENT_DISCORD_UPDATE, &userdata);
#endif
      return false;
   }

   return true;
}
#endif

static void runloop_apply_fastmotion_override(runloop_state_t *runloop_st, settings_t *settings)
{
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
   float fastforward_ratio_current;

   memcpy(&runloop_st->fastmotion_override.current,
         &runloop_st->fastmotion_override.next,
         sizeof(runloop_st->fastmotion_override.current));

   /* Check if 'fastmotion' state has changed */
   if (runloop_st->fastmotion !=
         runloop_st->fastmotion_override.current.fastforward)
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      runloop_st->fastmotion =
            runloop_st->fastmotion_override.current.fastforward;

      if (input_st)
      {
         if (runloop_st->fastmotion)
            input_st->nonblocking_flag = true;
         else
            input_st->nonblocking_flag = false;
      }

      if (!runloop_st->fastmotion)
         runloop_st->fastforward_after_frames = 1;

      driver_set_nonblock_state();

      /* Reset frame time counter when toggling
       * fast-forward off, if required */
      if (!runloop_st->fastmotion &&
          frame_time_counter_reset_after_fastforwarding)
         video_st->frame_time_count = 0;

      /* Ensure fast forward widget is disabled when
       * toggling fast-forward off
       * (required if RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE
       * is called during core de-initialisation) */
#if defined(HAVE_GFX_WIDGETS)
      if (dispwidget_get_ptr()->active && !runloop_st->fastmotion)
         video_st->widgets_fast_forward = false;
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

static enum runloop_state_enum runloop_check_state(
      struct rarch_state *p_rarch,
      settings_t *settings,
      retro_time_t current_time)
{
   input_bits_t current_bits;
#ifdef HAVE_MENU
   static input_bits_t last_input      = {{0}};
#endif
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
   bool runloop_paused                 = runloop_st->paused;
   bool pause_nonactive                = settings->bools.pause_nonactive;
   unsigned quit_gamepad_combo         = settings->uints.input_quit_gamepad_combo;
#ifdef HAVE_MENU
   struct menu_state *menu_st          = menu_state_get_ptr();
   menu_handle_t *menu                 = menu_st->driver_data;
   unsigned menu_toggle_gamepad_combo  = settings->uints.input_menu_toggle_gamepad_combo;
   bool menu_driver_binding_state      = menu_st->is_binding;
   bool menu_is_alive                  = menu_st->alive;
   bool display_kb                     = menu_input_dialog_get_display_kb();
#endif
#if defined(HAVE_GFX_WIDGETS)
   bool widgets_active                 = dispwidget_get_ptr()->active;
#endif
#ifdef HAVE_CHEEVOS
   bool cheevos_hardcore_active        = rcheevos_hardcore_active();
#endif

#if defined(HAVE_TRANSLATE) && defined(HAVE_GFX_WIDGETS)
   if (dispwidget_get_ptr()->ai_service_overlay_state == 3)
   {
      command_event(CMD_EVENT_PAUSE, NULL);
      dispwidget_get_ptr()->ai_service_overlay_state = 1;
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

   input_st->block_libretro_input     = false;
   input_st->block_hotkey             = false;

   if (input_st->keyboard_mapping_blocked)
      input_st->block_hotkey          = true;

   {
      rarch_joypad_info_t joypad_info;
      unsigned port                                = 0;
      int input_hotkey_block_delay                 = settings->uints.input_hotkey_block_delay;
      input_driver_t *current_input                = input_st->current_driver;
      const struct retro_keybind *binds_norm       = &input_config_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds_auto       = &input_autoconf_binds[port][RARCH_ENABLE_HOTKEY];
      const struct retro_keybind *binds            = input_config_binds[port];
      struct retro_keybind *auto_binds             = input_autoconf_binds[port];
      struct retro_keybind *general_binds          = input_config_binds[port];
#ifdef HAVE_MENU
      bool menu_input_active                       = menu_is_alive && !(settings->bools.menu_unified_controls && !display_kb);
#else
      bool menu_input_active                       = false;
#endif
   const input_device_driver_t *joypad             = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t
      *sec_joypad                                  = input_st->secondary_joypad;
#else
   const input_device_driver_t
      *sec_joypad                                  = NULL;
#endif

      joypad_info.joy_idx                          = settings->uints.input_joypad_index[port];
      joypad_info.auto_binds                       = input_autoconf_binds[joypad_info.joy_idx];
      joypad_info.axis_threshold                   = settings->floats.input_axis_threshold;

#ifdef HAVE_MENU
      if (menu_input_active)
      {
         unsigned k;
         unsigned x_plus      =  RARCH_ANALOG_LEFT_X_PLUS;
         unsigned y_plus      =  RARCH_ANALOG_LEFT_Y_PLUS;
         unsigned x_minus     =  RARCH_ANALOG_LEFT_X_MINUS;
         unsigned y_minus     =  RARCH_ANALOG_LEFT_Y_MINUS;

         /* Push analog to D-Pad mappings to binds. */

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
#endif

      input_keys_pressed(port,
            menu_input_active, input_hotkey_block_delay,
            &current_bits, &binds, binds_norm, binds_auto,
            joypad, sec_joypad,
            &joypad_info);

#ifdef HAVE_MENU
      if (menu_input_active)
      {
         unsigned j;

         /* Restores analog D-pad binds temporarily overridden. */

         for (j = RETRO_DEVICE_ID_JOYPAD_UP; j <= RETRO_DEVICE_ID_JOYPAD_RIGHT; j++)
         {
            (auto_binds)[j].joyaxis    = (auto_binds)[j].orig_joyaxis;
            (general_binds)[j].joyaxis = (general_binds)[j].orig_joyaxis;
         }

         if (     !display_kb
               &&  current_input->input_state)
         {
            unsigned i;
            unsigned ids[][2] =
            {
               {RETROK_SPACE,     RETRO_DEVICE_ID_JOYPAD_START   },
               {RETROK_SLASH,     RETRO_DEVICE_ID_JOYPAD_X       },
               {RETROK_RSHIFT,    RETRO_DEVICE_ID_JOYPAD_SELECT  },
               {RETROK_RIGHT,     RETRO_DEVICE_ID_JOYPAD_RIGHT   },
               {RETROK_LEFT,      RETRO_DEVICE_ID_JOYPAD_LEFT    },
               {RETROK_DOWN,      RETRO_DEVICE_ID_JOYPAD_DOWN    },
               {RETROK_UP,        RETRO_DEVICE_ID_JOYPAD_UP      },
               {RETROK_PAGEUP,    RETRO_DEVICE_ID_JOYPAD_L       },
               {RETROK_PAGEDOWN,  RETRO_DEVICE_ID_JOYPAD_R       },
               {0,                RARCH_QUIT_KEY                 },
               {0,                RARCH_FULLSCREEN_TOGGLE_KEY    },
               {RETROK_BACKSPACE, RETRO_DEVICE_ID_JOYPAD_B      },
               {RETROK_RETURN,    RETRO_DEVICE_ID_JOYPAD_A      },
               {RETROK_DELETE,    RETRO_DEVICE_ID_JOYPAD_Y      },
               {0,                RARCH_UI_COMPANION_TOGGLE     },
               {0,                RARCH_FPS_TOGGLE              },
               {0,                RARCH_SEND_DEBUG_INFO         },
               {0,                RARCH_NETPLAY_HOST_TOGGLE     },
               {0,                RARCH_MENU_TOGGLE             },
            };

            ids[9][0]  = input_config_binds[port][RARCH_QUIT_KEY].key;
            ids[10][0] = input_config_binds[port][RARCH_FULLSCREEN_TOGGLE_KEY].key;
            ids[14][0] = input_config_binds[port][RARCH_UI_COMPANION_TOGGLE].key;
            ids[15][0] = input_config_binds[port][RARCH_FPS_TOGGLE].key;
            ids[16][0] = input_config_binds[port][RARCH_SEND_DEBUG_INFO].key;
            ids[17][0] = input_config_binds[port][RARCH_NETPLAY_HOST_TOGGLE].key;
            ids[18][0] = input_config_binds[port][RARCH_MENU_TOGGLE].key;

            if (settings->bools.input_menu_swap_ok_cancel_buttons)
            {
               ids[11][1] = RETRO_DEVICE_ID_JOYPAD_A;
               ids[12][1] = RETRO_DEVICE_ID_JOYPAD_B;
            }

            for (i = 0; i < ARRAY_SIZE(ids); i++)
            {
               if (current_input->input_state(
                        input_st->current_data,
                        joypad,
                        sec_joypad,
                        &joypad_info, &binds,
                        input_st->keyboard_mapping_blocked,
                        port,
                        RETRO_DEVICE_KEYBOARD, 0, ids[i][0]))
                  BIT256_SET_PTR(&current_bits, ids[i][1]);
            }
         }
      }
      else
#endif
      {
#ifdef HAVE_ACCESSIBILITY
#ifdef HAVE_TRANSLATE
         if (settings->bools.ai_service_enable)
         {
            unsigned i;

            input_st->gamepad_input_override = 0;

            for (i = 0; i < MAX_USERS; i++)
            {
               /* Set gamepad input override */
               if (p_rarch->ai_gamepad_state[i] == 2)
                  input_st->gamepad_input_override |= (1 << i);
               p_rarch->ai_gamepad_state[i] = 0;
            }
         }
#endif
#endif
      }
   }

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

      menu_st->input_driver_flushing_input = input_active
         ? menu_st->input_driver_flushing_input
         : (menu_st->input_driver_flushing_input - 1);

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
      const ui_application_t *application = p_rarch->ui_companion
         ? p_rarch->ui_companion->application
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
      HOTKEY_CHECK(RARCH_FULLSCREEN_TOGGLE_KEY, CMD_EVENT_FULLSCREEN_TOGGLE,
            fullscreen_toggled, NULL);
   }

   /* Check mouse grab toggle */
   HOTKEY_CHECK(RARCH_GRAB_MOUSE_TOGGLE, CMD_EVENT_GRAB_MOUSE_TOGGLE, true, NULL);

   /* Automatic mouse grab on focus */
   if (settings->bools.input_auto_mouse_grab &&
         is_focused &&
         is_focused != runloop_st->focused &&
         !input_st->grab_mouse_state)
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
   runloop_st->focused = is_focused;

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
      if (input_st->keyboard_linefeed_enable)
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

            retroarch_environment_cb(
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
               && runloop_st->max_frames_screenshot)
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

         if (runloop_st->core_shutdown_initiated &&
               settings->bools.load_dummy_on_core_shutdown)
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
               runloop_st->shutdown_initiated      = false;
               runloop_st->core_shutdown_initiated = false;
            }
            else
               quit_runloop              = true;
         }
         else
            quit_runloop                 = true;

         runloop_st->core_running   = false;

         if (quit_runloop)
         {
            old_quit_key                 = quit_key;
            retroarch_main_quit();
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
      bool rarch_force_fullscreen = video_st->force_fullscreen;
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
      global_t *global              = global_get_ptr();
      unsigned screensaver_timeout  = settings->uints.menu_screensaver_timeout;

      /* Get current time */
      menu_st->current_time_us       = current_time;

      cbs->poll_cb();

      bits_clear_bits(trigger_input.data, old_input.data,
            ARRAY_SIZE(trigger_input.data));
      action                    = (enum menu_action)menu_event(
            settings,
            &current_bits, &trigger_input, display_kb);
      focused                   = pause_nonactive ? is_focused : true;
      focused                   = focused &&
         !p_rarch->main_ui_companion_is_on_foreground;

      if (global)
      {
         if (action == old_action)
         {
            retro_time_t press_time           = current_time;

            if (action == MENU_ACTION_NOOP)
               global->menu.noop_press_time   = press_time - global->menu.noop_start_time;
            else
               global->menu.action_press_time = press_time - global->menu.action_start_time;
         }
         else
         {
            if (action == MENU_ACTION_NOOP)
            {
               global->menu.noop_start_time      = current_time;
               global->menu.noop_press_time      = 0;

               if (global->menu_prev_action == old_action)
                  global->menu.action_start_time = global->menu.prev_start_time;
               else
                  global->menu.action_start_time = current_time;
            }
            else
            {
               if (  global->menu_prev_action == action &&
                     global->menu.noop_press_time < 200000) /* 250ms */
               {
                  global->menu.action_start_time = global->menu.prev_start_time;
                  global->menu.action_press_time = current_time - global->menu.action_start_time;
               }
               else
               {
                  global->menu.prev_start_time   = current_time;
                  global->menu_prev_action       = action;
                  global->menu.action_press_time = 0;
               }
            }
         }
      }

      /* Check whether menu screensaver should be enabled */
      if ((screensaver_timeout > 0) &&
          menu_st->screensaver_supported &&
          !menu_st->screensaver_active &&
          ((menu_st->current_time_us - menu_st->input_last_time_us) >
               ((retro_time_t)screensaver_timeout * 1000000)))
      {
         menu_ctx_environment_t menu_environ;
         menu_environ.type           = MENU_ENVIRON_ENABLE_SCREENSAVER;
         menu_environ.data           = NULL;
         menu_st->screensaver_active = true;
         menu_driver_ctl(RARCH_MENU_CTL_ENVIRONMENT, &menu_environ);
      }

      /* Iterate the menu driver for one frame. */

      if (menu_st->pending_quick_menu)
      {
         /* If the user had requested that the Quick Menu
          * be spawned during the previous frame, do this now
          * and exit the function to go to the next frame.
          */
         menu_entries_flush_stack(NULL, MENU_SETTINGS);
         p_disp->msg_force = true;

         generic_action_ok_displaylist_push("", NULL,
               "", 0, 0, 0, ACTION_OK_DL_CONTENT_SETTINGS);

         menu_st->selection_ptr      = 0;
         menu_st->pending_quick_menu = false;
      }
      else if (!menu_driver_iterate(p_rarch,
               menu_st,
               p_disp,
               anim_get_ptr(),
               settings,
               action, current_time))
      {
         if (p_rarch->rarch_error_on_init)
         {
            content_ctx_info_t content_info = {0};
            task_push_start_dummy_core(&content_info);
         }
         else
            retroarch_menu_running_finished(false);
      }

      if (focused || !runloop_st->idle)
      {
         bool runloop_is_inited      = runloop_st->is_inited;
         bool menu_pause_libretro    = settings->bools.menu_pause_libretro;
         bool libretro_running       = !menu_pause_libretro
            && runloop_is_inited
            && (runloop_st->current_core_type != CORE_TYPE_DUMMY);

         if (menu)
         {
            if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER)
                  != BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX))
               BIT64_SET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER);

            if (BIT64_GET(menu->state, MENU_STATE_RENDER_FRAMEBUFFER))
               p_disp->framebuf_dirty = true;

            if (BIT64_GET(menu->state, MENU_STATE_RENDER_MESSAGEBOX)
                  && !string_is_empty(menu->menu_state_msg))
            {
               if (menu->driver_ctx->render_messagebox)
                  menu->driver_ctx->render_messagebox(
                        menu->userdata,
                        menu->menu_state_msg);

               if (p_rarch->main_ui_companion_is_on_foreground)
               {
                  if (  p_rarch->ui_companion &&
                        p_rarch->ui_companion->render_messagebox)
                     p_rarch->ui_companion->render_messagebox(menu->menu_state_msg);
               }
            }

            if (BIT64_GET(menu->state, MENU_STATE_BLIT))
            {
               if (menu->driver_ctx->render)
                  menu->driver_ctx->render(
                        menu->userdata,
                        video_st->width,
                        video_st->height,
                        runloop_st->idle);
            }

            if (menu_st->alive && !runloop_st->idle)
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

      if (!focused || runloop_st->idle)
         return RUNLOOP_STATE_POLLED_AND_SLEEP;
   }
   else
#endif
#endif
   {
      if (runloop_st->idle)
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
         if (menu_st->alive)
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
         if (menu_st->alive)
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

   /* Check if we have pressed the FPS toggle button */
   HOTKEY_CHECK(RARCH_FPS_TOGGLE, CMD_EVENT_FPS_TOGGLE, true, NULL);

   /* Check if we have pressed the netplay host toggle button */
   HOTKEY_CHECK(RARCH_NETPLAY_HOST_TOGGLE, CMD_EVENT_NETPLAY_HOST_TOGGLE, true, NULL);

   if (menu_st->alive)
   {
      float fastforward_ratio = runloop_get_fastforward_ratio(settings,
            &runloop_st->fastmotion_override.current);

      if (!settings->bools.menu_throttle_framerate && !fastforward_ratio)
         return RUNLOOP_STATE_MENU_ITERATE;

      return RUNLOOP_STATE_END;
   }
#endif

   if (pause_nonactive)
      focused                = is_focused;

#ifdef HAVE_SCREENSHOTS
   /* Check if we have pressed the screenshot toggle button */
   HOTKEY_CHECK(RARCH_SCREENSHOT, CMD_EVENT_TAKE_SCREENSHOT, true, NULL);
#endif

   /* Check if we have pressed the audio mute toggle button */
   HOTKEY_CHECK(RARCH_MUTE, CMD_EVENT_AUDIO_MUTE_TOGGLE, true, NULL);

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

   if (BIT256_GET(current_bits, RARCH_VOLUME_UP))
      command_event(CMD_EVENT_VOLUME_UP, NULL);
   else if (BIT256_GET(current_bits, RARCH_VOLUME_DOWN))
      command_event(CMD_EVENT_VOLUME_DOWN, NULL);

#ifdef HAVE_NETWORKING
   /* Check Netplay */
   HOTKEY_CHECK(RARCH_NETPLAY_GAME_WATCH, CMD_EVENT_NETPLAY_GAME_WATCH, true, NULL);
#endif

   /* Check if we have pressed the pause button */
   {
      static bool old_frameadvance  = false;
      static bool old_pause_pressed = false;
      bool frameadvance_pressed     = false;
      bool trig_frameadvance        = false;
      bool pause_pressed            = BIT256_GET(current_bits, RARCH_PAUSE_TOGGLE);
#ifdef HAVE_CHEEVOS
      if (cheevos_hardcore_active)
      {
         static int unpaused_frames = 0;

         /* Frame advance is not allowed when achievement hardcore is active */
         if (!runloop_st->paused)
         {
            /* Limit pause to approximately three times per second (depending on core framerate) */
            if (unpaused_frames < 20)
            {
               ++unpaused_frames;
               pause_pressed        = false;
            }
         }
         else
            unpaused_frames         = 0;
      }
      else
#endif
      {
         frameadvance_pressed = BIT256_GET(current_bits, RARCH_FRAMEADVANCE);
         trig_frameadvance    = frameadvance_pressed && !old_frameadvance;

         /* FRAMEADVANCE will set us into pause mode. */
         pause_pressed       |= !runloop_st->paused
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

      if (runloop_st->paused)
      {
         bool toggle = !runloop_st->idle ? true : false;

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
      p_rarch->ai_gamepad_state[0]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_B);
      p_rarch->ai_gamepad_state[1]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_Y);
      p_rarch->ai_gamepad_state[2]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_SELECT);
      p_rarch->ai_gamepad_state[3]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_START);

      p_rarch->ai_gamepad_state[4]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_UP);
      p_rarch->ai_gamepad_state[5]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_DOWN);
      p_rarch->ai_gamepad_state[6]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_LEFT);
      p_rarch->ai_gamepad_state[7]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_RIGHT);

      p_rarch->ai_gamepad_state[8]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_A);
      p_rarch->ai_gamepad_state[9]  = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_X);
      p_rarch->ai_gamepad_state[10] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L);
      p_rarch->ai_gamepad_state[11] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R);

      p_rarch->ai_gamepad_state[12] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L2);
      p_rarch->ai_gamepad_state[13] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R2);
      p_rarch->ai_gamepad_state[14] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_L3);
      p_rarch->ai_gamepad_state[15] = BIT256_GET(current_bits, RETRO_DEVICE_ID_JOYPAD_R3);
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
         if (input_st->nonblocking_flag)
         {
            input_st->nonblocking_flag        = false;
            runloop_st->fastmotion            = false;
            runloop_st->fastforward_after_frames = 1;
         }
         else
         {
            input_st->nonblocking_flag        = true;
            runloop_st->fastmotion            = true;
         }

         driver_set_nonblock_state();

         /* Reset frame time counter when toggling
          * fast-forward off, if required */
         if (!runloop_st->fastmotion &&
             settings->bools.frame_time_counter_reset_after_fastforwarding)
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
         video_st->widgets_fast_forward =
               settings->bools.notification_show_fast_forward ?
                     runloop_st->fastmotion : false;
      else
#endif
      {
         /* > If widgets are disabled, display fast-forward
          *   status via OSD text for 1 frame every frame */
         if (runloop_st->fastmotion &&
             settings->bools.notification_show_fast_forward)
            runloop_msg_queue_push(
               msg_hash_to_str(MSG_FAST_FORWARD), 1, 1, false, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }
#if defined(HAVE_GFX_WIDGETS)
   else
      video_st->widgets_fast_forward = false;
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
         char msg[128];
         int cur_state_slot                = state_slot;
         if (check1)
            configuration_set_int(settings, settings->ints.state_slot,
                  cur_state_slot + addition);
         msg[0] = '\0';
         snprintf(msg, sizeof(msg), "%s: %d",
               msg_hash_to_str(MSG_STATE_SLOT),
               settings->ints.state_slot);
         runloop_msg_queue_push(msg, 2, 180, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         RARCH_LOG("%s\n", msg);
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
               BIT256_GET(current_bits, RARCH_REWIND),
               settings->uints.rewind_granularity,
               runloop_st->paused,
               s, sizeof(s), &t);

#if defined(HAVE_GFX_WIDGETS)
         if (widgets_active)
            video_st->widgets_rewinding = rewinding;
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
            runloop_st->slowmotion = !runloop_st->slowmotion;
         else if (old_slowmotion_hold_button_state != new_slowmotion_hold_button_state)
            runloop_st->slowmotion = new_slowmotion_hold_button_state;

         if (runloop_st->slowmotion)
         {
            if (settings->uints.video_black_frame_insertion)
               if (!runloop_st->idle)
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
            uint64_t current_usec = cpu_features_get_time_usec();
            RARCH_TIMER_BEGIN_NEW_TIME_USEC(timer,
                  current_usec,
                  SHADER_FILE_WATCH_DELAY_MSEC * 1000);
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
         RARCH_TIMER_TICK(timer, current_time);

         if (!timer.timer_end && RARCH_TIMER_HAS_EXPIRED(timer))
         {
            RARCH_TIMER_END(timer);
            need_to_apply = false;
            command_event(CMD_EVENT_SHADERS_APPLY_CHANGES, NULL);
         }
      }
   }

   if (      settings->uints.video_shader_delay
         && !runloop_st->shader_delay_timer.timer_end)
   {
      if (!runloop_st->shader_delay_timer.timer_begin)
      {
         uint64_t current_usec = cpu_features_get_time_usec();
         RARCH_TIMER_BEGIN_NEW_TIME_USEC(
               runloop_st->shader_delay_timer,
               current_usec,
               settings->uints.video_shader_delay * 1000);
         runloop_st->shader_delay_timer.timer_begin = true;
         runloop_st->shader_delay_timer.timer_end   = false;
      }
      else
      {
         RARCH_TIMER_TICK(runloop_st->shader_delay_timer, current_time);

         if (RARCH_TIMER_HAS_EXPIRED(runloop_st->shader_delay_timer))
         {
            RARCH_TIMER_END(runloop_st->shader_delay_timer);

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
   unsigned i;
   enum analog_dpad_mode dpad_mode[MAX_USERS];
   struct rarch_state                  *p_rarch = &rarch_st;
   input_driver_state_t               *input_st = input_state_get_ptr();
   audio_driver_state_t               *audio_st = audio_state_get_ptr();
   video_driver_state_t               *video_st = video_state_get_ptr();
   settings_t *settings                         = config_get_ptr();
   runloop_state_t *runloop_st                  = &runloop_state;
   unsigned video_frame_delay                   = settings->uints.video_frame_delay;
   bool vrr_runloop_enable                      = settings->bools.vrr_runloop_enable;
   unsigned max_users                           = settings->uints.input_max_users;
   retro_time_t current_time                    = cpu_features_get_time_usec();
#ifdef HAVE_MENU
   bool menu_pause_libretro                     = settings->bools.menu_pause_libretro;
   bool core_paused                             = runloop_st->paused || (menu_pause_libretro && menu_state_get_ptr()->alive);
#else
   bool core_paused                             = runloop_st->paused;
#endif
   float slowmotion_ratio                       = settings->floats.slowmotion_ratio;
#ifdef HAVE_CHEEVOS
   bool cheevos_enable                          = settings->bools.cheevos_enable;
#endif
   bool audio_sync                              = settings->bools.audio_sync;


#ifdef HAVE_DISCORD
   discord_state_t *discord_st                  = &p_rarch->discord_st;

   if (discord_is_inited)
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
      bool is_locked_fps                   = (runloop_st->paused
            || input_st->nonblocking_flag)
            | !!recording_state.data;
      retro_time_t delta                   = (!runloop_last_frame_time || is_locked_fps)
         ? runloop_st->frame_time.reference
         : (current - runloop_last_frame_time);

      if (is_locked_fps)
         runloop_st->frame_time_last  = 0;
      else
      {
         runloop_st->frame_time_last  = current;

         if (runloop_st->slowmotion)
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

      if (!(runloop_st->paused         ||
            !audio_st->active          ||
            !audio_st->output_samples_buf) &&
          audio_st->current_audio->write_avail &&
          audio_st->context_audio_data &&
          audio_st->buffer_size)
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

   switch ((enum runloop_state_enum)runloop_check_state(p_rarch,
            settings, current_time))
   {
      case RUNLOOP_STATE_QUIT:
         runloop_st->frame_limit_last_time = 0.0;
         runloop_st->core_running          = false;
         command_event(CMD_EVENT_QUIT, NULL);
         return -1;
      case RUNLOOP_STATE_POLLED_AND_SLEEP:
#ifdef HAVE_NETWORKING
         /* FIXME: This is an ugly way to tell Netplay this... */
         netplay_driver_ctl(RARCH_NETPLAY_CTL_PAUSE, NULL);
#endif
#if defined(HAVE_COCOATOUCH)
         if (!p_rarch->main_ui_companion_is_on_foreground)
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
         runloop_st->core_running = true;
         break;
   }

#ifdef HAVE_THREADS
   if (runloop_st->autosave)
      autosave_lock();
#endif

#ifdef HAVE_BSV_MOVIE
   /* Used for rewinding while playback/record. */
   if (input_st->bsv_movie_state_handle)
      input_st->bsv_movie_state_handle->frame_pos[input_st->bsv_movie_state_handle->frame_ptr]
         = intfstream_tell(input_st->bsv_movie_state_handle->file);
#endif

   if (  p_rarch->camera_cb.caps &&
         p_rarch->camera_driver  &&
         p_rarch->camera_driver->poll &&
         p_rarch->camera_data)
      p_rarch->camera_driver->poll(p_rarch->camera_data,
            p_rarch->camera_cb.frame_raw_framebuffer,
            p_rarch->camera_cb.frame_opengl_texture);

   /* Update binds for analog dpad modes. */
   for (i = 0; i < max_users; i++)
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

   if ((video_frame_delay > 0) && input_st && !input_st->nonblocking_flag)
      retro_sleep(video_frame_delay);

   {
#ifdef HAVE_RUNAHEAD
      bool run_ahead_enabled            = settings->bools.run_ahead_enabled;
      unsigned run_ahead_num_frames     = settings->uints.run_ahead_frames;
      bool run_ahead_hide_warnings      = settings->bools.run_ahead_hide_warnings;
      bool run_ahead_secondary_instance = settings->bools.run_ahead_secondary_instance;
      /* Run Ahead Feature replaces the call to core_run in this loop */
      bool want_runahead                = run_ahead_enabled && run_ahead_num_frames > 0;
#ifdef HAVE_NETWORKING
      want_runahead                     = want_runahead && !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif

      if (want_runahead)
         do_runahead(
               p_rarch,
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
#ifdef HAVE_DISCORD
   if (discord_is_inited && discord_st->ready)
      discord_update(DISCORD_PRESENCE_GAME);
#endif

   /* Restores analog D-pad binds temporarily overridden. */
   for (i = 0; i < max_users; i++)
   {
      if (dpad_mode[i] != ANALOG_DPAD_NONE)
      {
         unsigned j;
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
   if (runloop_st->autosave)
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
            if (audio_st->active &&
                  audio_st->context_audio_data)
               audio_st->current_audio->set_nonblock_state(
                     audio_st->context_audio_data, true);
            audio_st->chunk_size =
               audio_st->chunk_nonblock_size;
         }

         runloop_st->fastforward_after_frames++;

         if (runloop_st->fastforward_after_frames == 6)
         {
            /* Blocking audio */
            if (audio_st->active &&
                  audio_st->context_audio_data)
               audio_st->current_audio->set_nonblock_state(
                     audio_st->context_audio_data,
                     audio_sync ? false : true);

            audio_st->chunk_size = audio_st->chunk_block_size;
            runloop_st->fastforward_after_frames = 0;
         }
      }

      if (runloop_st->fastmotion)
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
            if (!p_rarch->main_ui_companion_is_on_foreground)
#endif
               retro_sleep(sleep_ms);
         }

         return 1;
      }

      runloop_st->frame_limit_last_time = end_frame_time;
   }

   return 0;
}

runloop_state_t *runloop_state_get_ptr(void)
{
   return &runloop_state;
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

   RARCH_LOG("[Playlist]: %s: [%s].\n",
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

/* Libretro core loader */

static void retro_run_null(void) { }
static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch) { }
static void retro_input_poll_null(void) { }

static int16_t core_input_state_poll_late(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   runloop_state_t     *runloop_st = &runloop_state;
   if (!runloop_st->current_core.input_polled)
      input_driver_poll();
   runloop_st->current_core.input_polled = true;

   return input_driver_state_wrapper(port, device, idx, id);
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

/**
 * core_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
static bool core_set_default_callbacks(struct retro_callbacks *cbs)
{
   retro_input_state_t state_cb = core_input_state_poll_return_cb();

   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = state_cb;
   cbs->poll_cb         = input_driver_poll;

   return true;
}



#ifdef HAVE_REWIND
/**
 * core_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
bool core_set_rewind_callbacks(void)
{
   runloop_state_t *runloop_st  = &runloop_state;
   struct state_manager_rewind_state
      *rewind_st                = &runloop_st->rewind_st;

   if (rewind_st->frame_is_reversed)
   {
      runloop_st->current_core.retro_set_audio_sample(audio_driver_sample_rewind);
      runloop_st->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      runloop_st->current_core.retro_set_audio_sample(audio_driver_sample);
      runloop_st->current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   }
   return true;
}
#endif

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
   struct rarch_state *p_rarch       = &rarch_st;
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
      want_runahead                  = run_ahead_enabled && (run_ahead_frames > 0);
#ifdef HAVE_NETWORKING
      if (want_runahead)
         want_runahead               = !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
   }
#endif

   runloop_st->current_core.retro_cheat_set(info->index, info->enabled, info->code);

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   if (     want_runahead
         && run_ahead_secondary_instance
         && runloop_st->runahead_secondary_core_available 
         && secondary_core_ensure_exists(p_rarch, runloop_st, settings)
         && runloop_st->secondary_core.retro_cheat_set)
      runloop_st->secondary_core.retro_cheat_set(
            info->index, info->enabled, info->code);
#endif

   return true;
}

bool core_reset_cheat(void)
{
   runloop_state_t *runloop_st       = &runloop_state;
#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   struct rarch_state *p_rarch       = &rarch_st;
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
      want_runahead                  = run_ahead_enabled && (run_ahead_frames > 0);
#ifdef HAVE_NETWORKING
      if (want_runahead)
         want_runahead               = !netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL);
#endif
   }
#endif

   runloop_st->current_core.retro_cheat_reset();

#if defined(HAVE_RUNAHEAD) && (defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
   if (   want_runahead
       && run_ahead_secondary_instance
       && runloop_st->runahead_secondary_core_available
       && secondary_core_ensure_exists(p_rarch, runloop_st, settings)
       && runloop_st->secondary_core.retro_cheat_reset)
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
   struct rarch_state   *p_rarch  = &rarch_st;
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
   remember_controller_port_device(p_rarch, pad->port, pad->device);
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
   bool             contentless = false;
   bool             is_inited   = false;
   bool             game_loaded = false;
   struct rarch_state *p_rarch  = &rarch_st;
   runloop_state_t *runloop_st  = &runloop_state;

   video_driver_set_cached_frame_ptr(NULL);

#ifdef HAVE_RUNAHEAD
   set_load_content_info(runloop_st, load_info);
   clear_controller_port_map(p_rarch);
#endif

   content_get_status(&contentless, &is_inited);
   set_save_state_in_background(false);

   if (load_info && load_info->special)
      game_loaded = runloop_st->current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      game_loaded = runloop_st->current_core.retro_load_game(load_info->info);
   else if (contentless)
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

#if HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!info || !runloop_st->current_core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   runloop_state_t *runloop_st  = &runloop_state;
   if (!info)
      return false;
   info->size = runloop_st->current_core.retro_serialize_size();
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

static bool core_verify_api_version(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   unsigned api_version        = runloop_st->current_core.retro_api_version();

   RARCH_LOG("%s: %u\n%s %s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version,
         FILE_PATH_LOG_INFO,
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION
         );

   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
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

   return true;
}

bool core_has_set_input_descriptor(void)
{
   runloop_state_t *runloop_st = &runloop_state;
   return runloop_st->current_core.has_set_input_descriptors;
}

unsigned int retroarch_get_rotation(void)
{
   settings_t     *settings    = config_get_ptr();
   unsigned     video_rotation = settings->uints.video_rotation;
   return video_rotation + runloop_state.system.rotation;
}

#ifdef HAVE_ACCESSIBILITY
static bool accessibility_speak_priority(
      struct rarch_state *p_rarch,
      bool accessibility_enable,
      unsigned accessibility_narrator_speech_speed,
      const char* speak_text, int priority)
{
   if (is_accessibility_enabled(
            accessibility_enable,
            p_rarch->accessibility_enabled))
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

/* Creates folder and core options stub file for subsequent runs */
bool core_options_create_override(bool game_specific)
{
   char options_path[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st = &runloop_state;
   config_file_t *conf         = NULL;

   options_path[0]             = '\0';

   if (!game_specific)
   {
      /* Sanity check - cannot create a folder-specific
       * override if a game-specific override is
       * already active */
      if (runloop_st->game_options_active)
         goto error;

      /* Get options file path (folder-specific) */
      if (!validate_folder_options(
               options_path,
               sizeof(options_path), true))
         goto error;
   }
   else
   {
      /* Get options file path (game-specific) */
      if (!validate_game_options(
               runloop_st->system.info.library_name,
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
   runloop_st->game_options_active   = game_specific;
   runloop_st->folder_options_active = !game_specific;

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

   new_options_path[0] = '\0';

   /* Sanity check 1 - if there are no core options
    * or no overrides are active, there is nothing to do */
   if (          !coreopts ||
         (       !runloop_st->game_options_active
              && !runloop_st->folder_options_active)
      )
      return true;

   /* Sanity check 2 - can only remove an override
    * if the specified type is currently active */
   if (game_specific && !runloop_st->game_options_active)
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
         struct core_option *option      = NULL;
         struct config_entry_list *entry = NULL;

         option = (struct core_option*)&coreopts->opts[i];
         if (!option)
            continue;

         entry = config_get_entry(conf, option->key);
         if (!entry || string_is_empty(entry->value))
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
      runloop_st->game_options_active   = false;
      runloop_st->folder_options_active = true;
   }
   else
   {
      path_clear(RARCH_PATH_CORE_OPTIONS);
      runloop_st->game_options_active   = false;
      runloop_st->folder_options_active = false;

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

      /* Attempt to load existing file */
      if (path_is_valid(path_core_options))
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
      core_options_file = path_basename(path_core_options);

   if (string_is_empty(core_options_file))
      core_options_file = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);

   /* Log result */
   RARCH_LOG(success ?
         "[Core Options]: Saved core options to \"%s\"\n" :
               "[Core Options]: Failed to save core options to \"%s\"\n",
            path_core_options ? path_core_options : "UNKNOWN");

   snprintf(msg, sizeof(msg), "%s \"%s\"",
         success ?
               msg_hash_to_str(MSG_CORE_OPTIONS_FLUSHED) :
                     msg_hash_to_str(MSG_CORE_OPTIONS_FLUSH_FAILED),
         core_options_file);

   runloop_msg_queue_push(
         msg, 1, 100, true,
         NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

void menu_content_environment_get(int *argc, char *argv[],
      void *args, void *params_data)
{
   struct rarch_state       *p_rarch = &rarch_st;
   struct rarch_main_wrap *wrap_args = (struct rarch_main_wrap*)params_data;
   runloop_state_t       *runloop_st = &runloop_state;
   rarch_system_info_t *sys_info     = &runloop_st->system;

   if (!wrap_args)
      return;

   wrap_args->no_content             = sys_info->load_no_content;

   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL))
      wrap_args->verbose       = verbosity_is_enabled();

   wrap_args->touched          = true;
   wrap_args->config_path      = NULL;
   wrap_args->sram_path        = NULL;
   wrap_args->state_path       = NULL;
   wrap_args->content_path     = NULL;

   if (!path_is_empty(RARCH_PATH_CONFIG))
      wrap_args->config_path   = path_get(RARCH_PATH_CONFIG);
   if (!string_is_empty(p_rarch->dir_savefile))
      wrap_args->sram_path     = p_rarch->dir_savefile;
   if (!string_is_empty(p_rarch->dir_savestate))
      wrap_args->state_path    = p_rarch->dir_savestate;
   if (!path_is_empty(RARCH_PATH_CONTENT))
      wrap_args->content_path  = path_get(RARCH_PATH_CONTENT);
   if (!retroarch_override_setting_is_set(RARCH_OVERRIDE_SETTING_LIBRETRO, NULL))
      wrap_args->libretro_path = string_is_empty(path_get(RARCH_PATH_CORE)) ? NULL :
         path_get(RARCH_PATH_CORE);
}
