/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <string.h>

#include <file/config_file.h>
#include <queues/task_queue.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <retro_timers.h>
#include <gfx/video_frame.h>
#include <glsym/glsym.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend.h"
#include "../frontend_driver.h"
#include "../../configuration.h"
#include "../../content.h"
#include "../../command.h"
#include "../../defaults.h"
#include "../../file_path_special.h"
#include "../../paths.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"
#include "../../cheat_manager.h"
#include "../../audio/audio_driver.h"

void emscripten_mainloop(void);
void PlatformEmscriptenWatchCanvasSize(void);
void PlatformEmscriptenPowerStateInit(void);
bool PlatformEmscriptenPowerStateGetSupported(void);
int PlatformEmscriptenPowerStateGetDischargeTime(void);
float PlatformEmscriptenPowerStateGetLevel(void);
bool PlatformEmscriptenPowerStateGetCharging(void);
uint64_t PlatformEmscriptenGetTotalMem(void);
uint64_t PlatformEmscriptenGetFreeMem(void);

//// begin exported functions

// saves and states

void cmd_savefiles(void)
{
   command_event(CMD_EVENT_SAVE_FILES, NULL);
}

void cmd_save_state(void)
{
   command_event(CMD_EVENT_SAVE_STATE, NULL);
}

void cmd_load_state(void)
{
   command_event(CMD_EVENT_LOAD_STATE, NULL);
}

void cmd_undo_save_state(void)
{
   command_event(CMD_EVENT_UNDO_SAVE_STATE, NULL);
}

void cmd_undo_load_state(void)
{
   command_event(CMD_EVENT_UNDO_LOAD_STATE, NULL);
}

// misc

void cmd_take_screenshot(void)
{
   command_event(CMD_EVENT_TAKE_SCREENSHOT, NULL);
}

void cmd_toggle_menu(void)
{
   command_event(CMD_EVENT_MENU_TOGGLE, NULL);
}

void cmd_reload_config(void)
{
   command_event(CMD_EVENT_RELOAD_CONFIG, NULL);
}

void cmd_toggle_grab_mouse(void)
{
   command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
}

void cmd_toggle_game_focus(void)
{
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, NULL);
}

void cmd_reset(void)
{
   command_event(CMD_EVENT_RESET, NULL);
}

void cmd_toggle_pause(void)
{
   command_event(CMD_EVENT_PAUSE_TOGGLE, NULL);
}

void cmd_pause(void)
{
   command_event(CMD_EVENT_PAUSE, NULL);
}

void cmd_unpause(void)
{
   command_event(CMD_EVENT_UNPAUSE, NULL);
}

void cmd_set_volume(float volume) {
   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, volume);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool cmd_set_shader(const char *path)
{
   return command_set_shader(NULL, path);
}
#endif

// cheats

void cmd_cheat_set_code(unsigned index, const char *str)
{
   cheat_manager_set_code(index, str);
}

const char *cmd_cheat_get_code(unsigned index)
{
   return cheat_manager_get_code(index);
}

void cmd_cheat_toggle_index(bool apply_cheats_after_toggle, unsigned index)
{
   cheat_manager_toggle_index(apply_cheats_after_toggle, index);
}

bool cmd_cheat_get_code_state(unsigned index)
{
   return cheat_manager_get_code_state(index);
}

bool cmd_cheat_realloc(unsigned new_size)
{
   return cheat_manager_realloc(new_size, CHEAT_HANDLER_TYPE_EMU);
}

unsigned cmd_cheat_get_size(void)
{
   return cheat_manager_get_size();
}

void cmd_cheat_apply_cheats(void)
{
   cheat_manager_apply_cheats();
}

//// end exported functions

static void frontend_emscripten_get_env(int *argc, char *argv[],
      void *args, void *params_data)
{
   char base_path[PATH_MAX];
   char user_path[PATH_MAX];
   const char *home         = getenv("HOME");

   if (home)
   {
      size_t _len = strlcpy(base_path, home, sizeof(base_path));
      strlcpy(base_path + _len, "/retroarch", sizeof(base_path) - _len);
      _len = strlcpy(user_path, home, sizeof(user_path));
      strlcpy(user_path + _len, "/retroarch/userdata", sizeof(user_path) - _len);
   }
   else
   {
      strlcpy(base_path, "retroarch", sizeof(base_path));
      strlcpy(user_path, "retroarch/userdata", sizeof(user_path));
   }

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], base_path,
         "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));

   /* bundle data */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], base_path,
         "bundle/assets", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], base_path,
         "bundle/autoconfig", sizeof(g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], base_path,
         "bundle/database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], base_path,
         "bundle/info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OVERLAY], base_path,
         "bundle/overlays", sizeof(g_defaults.dirs[DEFAULT_DIR_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY], base_path,
         "bundle/overlays/keyboards", sizeof(g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SHADER], base_path,
         "bundle/shaders", sizeof(g_defaults.dirs[DEFAULT_DIR_SHADER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], base_path,
         "bundle/filters/audio", sizeof(g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], base_path,
         "bundle/filters/video", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));

   /* user data dirs */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CHEATS], user_path,
         "cheats", sizeof(g_defaults.dirs[DEFAULT_DIR_CHEATS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], user_path,
         "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT], user_path,
         "content", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], user_path,
         "content/downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], user_path,
         "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         "remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], user_path,
         "saves", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], user_path,
         "screenshots", sizeof(g_defaults.dirs[DEFAULT_DIR_SCREENSHOT]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], user_path,
         "states", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], user_path,
         "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], user_path,
         "thumbnails", sizeof(g_defaults.dirs[DEFAULT_DIR_THUMBNAILS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], user_path,
         "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));

   /* cache dir */
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CACHE], "/tmp/",
         "retroarch", sizeof(g_defaults.dirs[DEFAULT_DIR_CACHE]));

   /* history and main config */
   strlcpy(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY],
         user_path, sizeof(g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY]));
   fill_pathname_join(g_defaults.path_config, user_path,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));

#ifndef IS_SALAMANDER
   dir_check_defaults("custom.ini");
#endif
}

static enum frontend_powerstate frontend_emscripten_get_powerstate(int *seconds, int *percent)
{
   enum frontend_powerstate ret = FRONTEND_POWERSTATE_NONE;

   if (!PlatformEmscriptenPowerStateGetSupported())
      return ret;

   if (!PlatformEmscriptenPowerStateGetCharging())
      ret = FRONTEND_POWERSTATE_ON_POWER_SOURCE;
   else if (PlatformEmscriptenPowerStateGetLevel() == 1)
      ret = FRONTEND_POWERSTATE_CHARGED;
   else
      ret = FRONTEND_POWERSTATE_CHARGING;

   *seconds = PlatformEmscriptenPowerStateGetDischargeTime();
   *percent = (int)(PlatformEmscriptenPowerStateGetLevel() * 100);

   return ret;
}

static uint64_t frontend_emscripten_get_total_mem(void)
{
   return PlatformEmscriptenGetTotalMem();
}

static uint64_t frontend_emscripten_get_free_mem(void)
{
   return PlatformEmscriptenGetFreeMem();
}

int main(int argc, char *argv[])
{
   PlatformEmscriptenWatchCanvasSize();
   PlatformEmscriptenPowerStateInit();

   EM_ASM({
      specialHTMLTargets["!canvas"] = Module.canvas;
   });

   emscripten_set_main_loop(emscripten_mainloop, 0, 0);
   rarch_main(argc, argv, NULL);

   return 0;
}

frontend_ctx_driver_t frontend_ctx_emscripten = {
   frontend_emscripten_get_env,         /* environment_get */
   NULL,                                /* init */
   NULL,                                /* deinit */
   NULL,                                /* exitspawn */
   NULL,                                /* process_args */
   NULL,                                /* exec */
   NULL,                                /* set_fork */
   NULL,                                /* shutdown */
   NULL,                                /* get_name */
   NULL,                                /* get_os */
   NULL,                                /* get_rating */
   NULL,                                /* load_content */
   NULL,                                /* get_architecture */
   frontend_emscripten_get_powerstate,  /* get_powerstate */
   NULL,                                /* parse_drive_list */
   frontend_emscripten_get_total_mem,   /* get_total_mem */
   frontend_emscripten_get_free_mem,    /* get_free_mem  */
   NULL,                                /* install_sighandlers */
   NULL,                                /* get_signal_handler_state */
   NULL,                                /* set_signal_handler_state */
   NULL,                                /* destroy_signal_handler_state */
   NULL,                                /* attach_console */
   NULL,                                /* detach_console */
   NULL,                                /* get_lakka_version */
   NULL,                                /* set_screen_brightness */
   NULL,                                /* watch_path_for_changes */
   NULL,                                /* check_for_path_changes */
   NULL,                                /* set_sustained_performance_mode */
   NULL,                                /* get_cpu_model_name */
   NULL,                                /* get_user_language */
   NULL,                                /* is_narrator_running */
   NULL,                                /* accessibility_speak */
   NULL,                                /* set_gamemode        */
   "emscripten",                        /* ident               */
   NULL                                 /* get_video_driver    */
};
