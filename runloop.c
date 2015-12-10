/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <stdarg.h>
#include <math.h>

#include <file/file_path.h>
#include <retro_inline.h>
#include <retro_assert.h>
#include <queues/message_queue.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include <compat/strl.h>

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif
#include "autosave.h"
#include "cheats.h"
#include "configuration.h"
#include "performance.h"
#include "movie.h"
#include "retroarch.h"
#include "runloop.h"
#include "rewind.h"
#include "system.h"
#include "dir_list_special.h"
#include "audio/audio_driver.h"
#include "camera/camera_driver.h"
#include "record/record_driver.h"
#include "input/input_driver.h"
#include "ui/ui_companion_driver.h"
#include "libretro_version_1.h"

#include "msg_hash.h"

#include "tasks/tasks.h"
#include "input/input_keyboard.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#include "verbosity.h"

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

static rarch_system_info_t g_system;

#ifdef HAVE_MENU
struct retro_system_info g_system_menu;
#endif

typedef struct event_cmd_state
{
   retro_input_t state[3];
} event_cmd_state_t;

static rarch_dir_list_t runloop_shader_dir;

static unsigned runloop_pending_windowed_scale;

static msg_queue_t *g_msg_queue;

global_t *global_get_ptr(void)
{
   static struct global g_extern;
   return &g_extern;
}

rarch_system_info_t *rarch_system_info_get_ptr(void)
{
   return &g_system;
}

const char *runloop_msg_queue_pull(void)
{
   const char *ret = NULL;

   runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_LOCK, NULL);

   ret = msg_queue_pull(g_msg_queue);

   runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_UNLOCK, NULL);

   return ret;
}

void runloop_msg_queue_push_new(uint32_t hash, unsigned prio, unsigned duration,
      bool flush)
{
   const char *msg = msg_hash_to_str(hash);

   if (!msg)
      return;

   runloop_msg_queue_push(msg, prio, duration, flush);
}

void runloop_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   settings_t *settings = config_get_ptr();
   if(!settings->video.font_enable || !g_msg_queue)
      return;

   runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_LOCK, NULL);

   if (flush)
      msg_queue_clear(g_msg_queue);
   msg_queue_push(g_msg_queue, msg, prio, duration);

   runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_UNLOCK, NULL);

   if (ui_companion_is_on_foreground())
   {
      const ui_companion_driver_t *ui = ui_companion_get_ptr();
      if (ui->msg_queue_push)
         ui->msg_queue_push(msg, prio, duration, flush);
   }
}

#ifdef HAVE_MENU
static bool runloop_cmd_get_state_menu_toggle_button_combo(
      settings_t *settings,
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   switch (settings->input.menu_toggle_gamepad_combo)
   {
      case 0:
         return false;
      case 1:
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_DOWN))
            return false;
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_Y))
            return false;
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_L))
            return false;
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_R))
            return false;
         break;
      case 2:
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_L3))
            return false;
         if (!BIT64_GET(input, RETRO_DEVICE_ID_JOYPAD_R3))
            return false;
         break;
   }

   input_driver_ctl(RARCH_INPUT_CTL_SET_FLUSHING_INPUT, NULL);
   return true;
}
#endif

#define runloop_cmd_triggered(cmd, id) BIT64_GET(cmd->state[2], id) 

#define runloop_cmd_press(cmd, id)     BIT64_GET(cmd->state[0], id)
#define runloop_cmd_pressed(cmd, id)   BIT64_GET(cmd->state[1], id)
#ifdef HAVE_MENU
#define runloop_cmd_menu_press(cmd)   (BIT64_GET(cmd->state[2], RARCH_MENU_TOGGLE) || \
                                      runloop_cmd_get_state_menu_toggle_button_combo( \
                                            settings, cmd->state[0], \
                                            cmd->state[1], cmd->state[2]))
#endif

/**
 * check_pause:
 * @pressed              : was libretro pause key pressed?
 * @frameadvance_pressed : was frameadvance key pressed?
 *
 * Check if libretro pause key was pressed. If so, pause or
 * unpause the libretro core.
 *
 * Returns: true if libretro pause key was toggled, otherwise false.
 **/
static bool check_pause(settings_t *settings,
      bool focus, bool pause_pressed,
      bool frameadvance_pressed)
{
   static bool old_focus    = true;
   enum event_command cmd   = EVENT_CMD_NONE;
   bool old_is_paused       = runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);

   /* FRAMEADVANCE will set us into pause mode. */
   pause_pressed |= !old_is_paused && frameadvance_pressed;

   if (focus && pause_pressed)
      cmd = EVENT_CMD_PAUSE_TOGGLE;
   else if (focus && !old_focus)
      cmd = EVENT_CMD_UNPAUSE;
   else if (!focus && old_focus)
      cmd = EVENT_CMD_PAUSE;

   old_focus = focus;

   if (cmd != EVENT_CMD_NONE)
      event_command(cmd);

   if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL) == old_is_paused)
      return false;

   return true;
}

/**
 * check_fast_forward_button:
 * @fastforward_pressed  : is fastforward key pressed?
 * @hold_pressed         : is fastforward key pressed and held?
 * @old_hold_pressed     : was fastforward key pressed and held the last frame?
 *
 * Checks if the fast forward key has been pressed for this frame.
 *
 **/
static void check_fast_forward_button(bool fastforward_pressed,
      bool hold_pressed, bool old_hold_pressed)
{
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   if (fastforward_pressed)
   {
      if (input_driver_ctl(RARCH_INPUT_CTL_IS_NONBLOCK_STATE, NULL))
         input_driver_ctl(RARCH_INPUT_CTL_UNSET_NONBLOCK_STATE, NULL);
      else
         input_driver_ctl(RARCH_INPUT_CTL_SET_NONBLOCK_STATE, NULL);
   }
   else if (old_hold_pressed != hold_pressed)
   {
      if (hold_pressed)
         input_driver_ctl(RARCH_INPUT_CTL_SET_NONBLOCK_STATE, NULL);
      else
         input_driver_ctl(RARCH_INPUT_CTL_UNSET_NONBLOCK_STATE, NULL);
   }
   else
      return;

   driver_set_nonblock_state();
}

/**
 * check_stateslots:
 * @pressed_increase     : is state slot increase key pressed?
 * @pressed_decrease     : is state slot decrease key pressed?
 *
 * Checks if the state increase/decrease keys have been pressed
 * for this frame.
 **/
static void check_stateslots(settings_t *settings,
      bool pressed_increase, bool pressed_decrease)
{
   char msg[128];

   /* Save state slots */
   if (pressed_increase)
      settings->state_slot++;
   else if (pressed_decrease)
   {
      if (settings->state_slot > 0)
         settings->state_slot--;
   }
   else
      return;

   snprintf(msg, sizeof(msg), "%s: %d",
         msg_hash_to_str(MSG_STATE_SLOT),
         settings->state_slot);

   runloop_msg_queue_push(msg, 1, 180, true);

   RARCH_LOG("%s\n", msg);
}

#define SHADER_EXT_GLSL      0x7c976537U
#define SHADER_EXT_GLSLP     0x0f840c87U
#define SHADER_EXT_CG        0x0059776fU
#define SHADER_EXT_CGP       0x0b8865bfU

static void shader_dir_free(void)
{
   dir_list_free(runloop_shader_dir.list);
   runloop_shader_dir.list = NULL;
   runloop_shader_dir.ptr  = 0;
}

static bool shader_dir_init(void)
{
   unsigned i;
   settings_t *settings  = config_get_ptr();

   if (!*settings->video.shader_dir)
      return false;

   runloop_shader_dir.list = dir_list_new_special(NULL, DIR_LIST_SHADERS, NULL);

   if (!runloop_shader_dir.list || runloop_shader_dir.list->size == 0)
   {
      event_command(EVENT_CMD_SHADER_DIR_DEINIT);
      return false;
   }

   runloop_shader_dir.ptr  = 0;
   dir_list_sort(runloop_shader_dir.list, false);

   for (i = 0; i < runloop_shader_dir.list->size; i++)
      RARCH_LOG("%s \"%s\"\n",
            msg_hash_to_str(MSG_FOUND_SHADER),
            runloop_shader_dir.list->elems[i].data);
   return true;
}

/**
 * check_shader_dir:
 * @pressed_next         : was next shader key pressed?
 * @pressed_previous     : was previous shader key pressed?
 *
 * Checks if any one of the shader keys has been pressed for this frame:
 * a) Next shader index.
 * b) Previous shader index.
 *
 * Will also immediately apply the shader.
 **/
static void check_shader_dir(bool pressed_next, bool pressed_prev)
{
   uint32_t ext_hash;
   char msg[128];
   const char *shader          = NULL;
   const char *ext             = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!runloop_shader_dir.list)
      return;

   if (pressed_next)
   {
      runloop_shader_dir.ptr = (runloop_shader_dir.ptr + 1) %
         runloop_shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      if (runloop_shader_dir.ptr == 0)
         runloop_shader_dir.ptr = runloop_shader_dir.list->size - 1;
      else
         runloop_shader_dir.ptr--;
   }
   else
      return;

   shader   = runloop_shader_dir.list->elems[runloop_shader_dir.ptr].data;
   ext      = path_get_extension(shader);
   ext_hash = msg_hash_calculate(ext);

   switch (ext_hash)
   {
      case SHADER_EXT_GLSL:
      case SHADER_EXT_GLSLP:
         type = RARCH_SHADER_GLSL;
         break;
      case SHADER_EXT_CG:
      case SHADER_EXT_CGP:
         type = RARCH_SHADER_CG;
         break;
      default:
         return;
   }

   snprintf(msg, sizeof(msg), "%s #%u: \"%s\".",
         msg_hash_to_str(MSG_SHADER),
         (unsigned)runloop_shader_dir.ptr, shader);
   runloop_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_APPLYING_SHADER),
         shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER));
}

/**
 * rarch_game_specific_options:
 * @cmd                          : Output variable with path to core options file.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if a game specific core options path has been found,
 * otherwise false (0).
 **/
static bool rarch_game_specific_options(char **output)
{
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   const char *core_name                  = NULL;
   const char *game_name                  = NULL;
   config_file_t *option_file             = NULL;
   char game_path[PATH_MAX_LENGTH]        = {0};
   char config_directory[PATH_MAX_LENGTH] = {0};

   core_name = system ? system->info.library_name : NULL;
   game_name = global ? path_basename(global->name.base) : NULL;

   if (!core_name || !game_name)
      return false;
   if (core_name[0] == '\0' || game_name[0] == '\0')
      return false;

   RARCH_LOG("Per-Game Options: core name: %s\n", core_name);
   RARCH_LOG("Per-Game Options: game name: %s\n", game_name);

   /* Config directory: config_directory.
   * Try config directory setting first,
   * fallback to the location of the current configuration file. */
   if (settings->menu_config_directory[0] != '\0')
      strlcpy(config_directory, settings->menu_config_directory, sizeof(config_directory));
   else if (global->path.config[0] != '\0')
      fill_pathname_basedir(config_directory, global->path.config, sizeof(config_directory));
   else
   {
      RARCH_WARN("Per-Game Options: no config directory set\n");
      return false;
   }

   /* Concatenate strings into full paths for game_path */
   fill_pathname_join(game_path, config_directory, core_name, sizeof(game_path));
   fill_pathname_join(game_path, game_path, game_name, sizeof(game_path));
   strlcat(game_path, ".opt", sizeof(game_path));

   option_file = config_file_new(game_path);
   if (!option_file)
      return false;

   config_file_free(option_file);
   
   RARCH_LOG("Per-Game Options: game-specific core options found at %s\n", game_path);
   *output = strdup(game_path);
   return true;
}

static void runloop_data_clear_state(void)
{
   runloop_ctl(RUNLOOP_CTL_DATA_DEINIT, NULL);
   rarch_task_init();
}

bool runloop_ctl(enum runloop_ctl_state state, void *data)
{
   static char runloop_fullpath[PATH_MAX_LENGTH];
   static unsigned runloop_max_frames          = false;
   static bool runloop_frame_time_last         = false;
   static bool runloop_set_frame_limit         = false;
   static bool runloop_paused                  = false;
   static bool runloop_idle                    = false;
   static bool runloop_exec                    = false;
   static bool runloop_slowmotion              = false;
   static bool runloop_shutdown_initiated      = false;
   static bool runloop_core_shutdown_initiated = false;
   static bool runloop_perfcnt_enable          = false;
   static bool runloop_overrides_active        = false;
#ifdef HAVE_THREADS
   static slock_t *runloop_msg_queue_lock      = NULL;
#endif
   settings_t *settings                        = config_get_ptr();
   rarch_system_info_t *system                 = rarch_system_info_get_ptr();

   switch (state)
   {
      case RUNLOOP_CTL_SHADER_DIR_DEINIT:
         shader_dir_free();
         return true;
      case RUNLOOP_CTL_SHADER_DIR_INIT:
         return shader_dir_init();
      case RUNLOOP_CTL_SYSTEM_INFO_INIT:
         core.retro_get_system_info(&system->info);

         if (!system->info.library_name)
            system->info.library_name = msg_hash_to_str(MSG_UNKNOWN);
         if (!system->info.library_version)
            system->info.library_version = "v0";

#ifndef RARCH_CONSOLE
         strlcpy(system->title_buf, 
               msg_hash_to_str(MSG_PROGRAM), sizeof(system->title_buf));
         strlcat(system->title_buf, " : ", sizeof(system->title_buf));
#endif
         strlcat(system->title_buf, system->info.library_name, sizeof(system->title_buf));
         strlcat(system->title_buf, " ", sizeof(system->title_buf));
         strlcat(system->title_buf, system->info.library_version, sizeof(system->title_buf));
         strlcpy(system->valid_extensions, system->info.valid_extensions ?
               system->info.valid_extensions : DEFAULT_EXT,
               sizeof(system->valid_extensions));
         system->block_extract = system->info.block_extract;
         break;
      case RUNLOOP_CTL_GET_CORE_OPTION_SIZE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            *idx = core_option_size(system->core_options);
         }
         return true;
      case RUNLOOP_CTL_HAS_CORE_OPTIONS:
         return system && system->core_options;
      case RUNLOOP_CTL_SYSTEM_INFO_FREE:
         if (system->core_options)
         {
            core_option_flush(system->core_options);
            core_option_free(system->core_options);
         }

         system->core_options = NULL;

         /* No longer valid. */
         if (system->special)
            free(system->special);
         system->special = NULL;
         if (system->ports)
            free(system->ports);
         system->ports   = NULL;

         memset(&g_system, 0, sizeof(rarch_system_info_t));
         break;
      case RUNLOOP_CTL_IS_FRAME_COUNT_END:
         {
            uint64_t *frame_count         = NULL;
            video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);
            return runloop_max_frames && (*frame_count >= runloop_max_frames);
         }
      case RUNLOOP_CTL_SET_FRAME_TIME_LAST:
         runloop_frame_time_last = true;
         break;
      case RUNLOOP_CTL_UNSET_FRAME_TIME_LAST:
         runloop_frame_time_last = false;
         break;
      case RUNLOOP_CTL_SET_OVERRIDES_ACTIVE:
         runloop_overrides_active = true;
         break;
      case RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE:
         runloop_overrides_active = false; 
         break;
      case RUNLOOP_CTL_IS_OVERRIDES_ACTIVE:
         return runloop_overrides_active;
      case RUNLOOP_CTL_IS_FRAME_TIME_LAST:
         return runloop_frame_time_last;
      case RUNLOOP_CTL_SET_FRAME_LIMIT:
         runloop_set_frame_limit = true;
         break;
      case RUNLOOP_CTL_UNSET_FRAME_LIMIT:
         runloop_set_frame_limit = false;
         break;
      case RUNLOOP_CTL_SHOULD_SET_FRAME_LIMIT:
         return runloop_set_frame_limit;
      case RUNLOOP_CTL_GET_PERFCNT:
         {
            bool **perfcnt = (bool**)data;
            if (!perfcnt)
               return false;
            *perfcnt = &runloop_perfcnt_enable;
         }
         return true;
      case RUNLOOP_CTL_SET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = true;
         break;
      case RUNLOOP_CTL_UNSET_PERFCNT_ENABLE:
         runloop_perfcnt_enable = false;
         break;
      case RUNLOOP_CTL_IS_PERFCNT_ENABLE:
         return runloop_perfcnt_enable;
      case RUNLOOP_CTL_GET_WINDOWED_SCALE:
         {
            unsigned **scale = (unsigned**)data;
            if (!scale)
               return false;
            *scale       = (unsigned*)&runloop_pending_windowed_scale;
         }
         break;
      case RUNLOOP_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            runloop_pending_windowed_scale = *idx;
         }
         break;
      case RUNLOOP_CTL_SET_LIBRETRO_PATH:
         {
            const char *fullpath = (const char*)data;
            if (!fullpath)
               return false;
            strlcpy(settings->libretro, fullpath, sizeof(settings->libretro));
         }
         break;
      case RUNLOOP_CTL_CLEAR_CONTENT_PATH:
         *runloop_fullpath = '\0';
         break;
      case RUNLOOP_CTL_GET_CONTENT_PATH:
         {
            char **fullpath = (char**)data;
            if (!fullpath)
               return false;
            *fullpath       = (char*)runloop_fullpath;
         }
         break;
      case RUNLOOP_CTL_SET_CONTENT_PATH:
         {
            const char *fullpath = (const char*)data;
            if (!fullpath)
               return false;
            strlcpy(runloop_fullpath, fullpath, sizeof(runloop_fullpath));
         }
         break;
      case RUNLOOP_CTL_CHECK_FOCUS:
         if (settings->pause_nonactive)
            return video_driver_ctl(RARCH_DISPLAY_CTL_IS_FOCUSED, NULL);
         return true;
      case RUNLOOP_CTL_CHECK_IDLE_STATE:
         {
            event_cmd_state_t *cmd    = (event_cmd_state_t*)data;
            bool focused              = runloop_ctl(RUNLOOP_CTL_CHECK_FOCUS, NULL);

            check_pause(settings, focused,
                  runloop_cmd_triggered(cmd, RARCH_PAUSE_TOGGLE),
                  runloop_cmd_triggered(cmd, RARCH_FRAMEADVANCE));

            if (!runloop_ctl(RUNLOOP_CTL_CHECK_PAUSE_STATE, cmd) || !focused)
               return false;
            break;
         }
      case RUNLOOP_CTL_CHECK_STATE:
         {
            bool tmp                  = false;
            event_cmd_state_t *cmd    = (event_cmd_state_t*)data;

            if (!cmd || runloop_idle)
               return false;

            if (runloop_cmd_triggered(cmd, RARCH_SCREENSHOT))
               event_command(EVENT_CMD_TAKE_SCREENSHOT);

            if (runloop_cmd_triggered(cmd, RARCH_MUTE))
               event_command(EVENT_CMD_AUDIO_MUTE_TOGGLE);

            if (runloop_cmd_triggered(cmd, RARCH_OSK))
            {
               if (input_driver_ctl(RARCH_INPUT_CTL_IS_KEYBOARD_LINEFEED_ENABLED, NULL))
                  input_driver_ctl(RARCH_INPUT_CTL_UNSET_KEYBOARD_LINEFEED_ENABLED, NULL);
               else
                  input_driver_ctl(RARCH_INPUT_CTL_SET_KEYBOARD_LINEFEED_ENABLED, NULL);
            }

            if (runloop_cmd_press(cmd, RARCH_VOLUME_UP))
               event_command(EVENT_CMD_VOLUME_UP);
            else if (runloop_cmd_press(cmd, RARCH_VOLUME_DOWN))
               event_command(EVENT_CMD_VOLUME_DOWN);

#ifdef HAVE_NETPLAY
            tmp = runloop_cmd_triggered(cmd, RARCH_NETPLAY_FLIP);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_FLIP_PLAYERS, &tmp);
            tmp = runloop_cmd_triggered(cmd, RARCH_FULLSCREEN_TOGGLE_KEY);
            netplay_driver_ctl(RARCH_NETPLAY_CTL_FULLSCREEN_TOGGLE, &tmp);
#endif
            if (!runloop_ctl(RUNLOOP_CTL_CHECK_IDLE_STATE, data))
               return false;

            check_fast_forward_button(
                  runloop_cmd_triggered(cmd, RARCH_FAST_FORWARD_KEY),
                  runloop_cmd_press    (cmd, RARCH_FAST_FORWARD_HOLD_KEY),
                  runloop_cmd_pressed  (cmd, RARCH_FAST_FORWARD_HOLD_KEY));
            check_stateslots(settings,
                  runloop_cmd_triggered(cmd, RARCH_STATE_SLOT_PLUS),
                  runloop_cmd_triggered(cmd, RARCH_STATE_SLOT_MINUS)
                  );

            if (runloop_cmd_triggered(cmd, RARCH_SAVE_STATE_KEY))
               event_command(EVENT_CMD_SAVE_STATE);
            else if (runloop_cmd_triggered(cmd, RARCH_LOAD_STATE_KEY))
               event_command(EVENT_CMD_LOAD_STATE);

            state_manager_check_rewind(runloop_cmd_press(cmd, RARCH_REWIND));

            tmp = runloop_cmd_press(cmd, RARCH_SLOWMOTION);

            runloop_ctl(RUNLOOP_CTL_CHECK_SLOWMOTION, &tmp);

            if (runloop_cmd_triggered(cmd, RARCH_MOVIE_RECORD_TOGGLE))
               runloop_ctl(RUNLOOP_CTL_CHECK_MOVIE, NULL);

            check_shader_dir(
                  runloop_cmd_triggered(cmd, RARCH_SHADER_NEXT),
                  runloop_cmd_triggered(cmd, RARCH_SHADER_PREV));

            if (runloop_cmd_triggered(cmd, RARCH_DISK_EJECT_TOGGLE))
               event_command(EVENT_CMD_DISK_EJECT_TOGGLE);
            else if (runloop_cmd_triggered(cmd, RARCH_DISK_NEXT))
               event_command(EVENT_CMD_DISK_NEXT);
            else if (runloop_cmd_triggered(cmd, RARCH_DISK_PREV))
               event_command(EVENT_CMD_DISK_PREV);

            if (runloop_cmd_triggered(cmd, RARCH_RESET))
               event_command(EVENT_CMD_RESET);

            cheat_manager_state_checks(
                  runloop_cmd_triggered(cmd, RARCH_CHEAT_INDEX_PLUS),
                  runloop_cmd_triggered(cmd, RARCH_CHEAT_INDEX_MINUS),
                  runloop_cmd_triggered(cmd, RARCH_CHEAT_TOGGLE));
         }
         break;
      case RUNLOOP_CTL_CHECK_PAUSE_STATE:
         {
            bool check_is_oneshot;
            event_cmd_state_t *cmd    = (event_cmd_state_t*)data;

            if (!cmd)
               return false;

            check_is_oneshot     = runloop_cmd_triggered(cmd, RARCH_FRAMEADVANCE) 
               || runloop_cmd_press(cmd, RARCH_REWIND);

            if (!runloop_paused)
               return true;

            if (runloop_cmd_triggered(cmd, RARCH_FULLSCREEN_TOGGLE_KEY))
            {
               event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
            }

            if (!check_is_oneshot)
               return false;
         }
         break;
      case RUNLOOP_CTL_CHECK_SLOWMOTION:
         {
            bool *ptr            = (bool*)data;

            if (!ptr)
               return false;

            runloop_slowmotion   = *ptr;

            if (!runloop_slowmotion)
               return false;

            if (settings->video.black_frame_insertion)
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);

            if (state_manager_frame_is_reversed())
               runloop_msg_queue_push_new(MSG_SLOW_MOTION_REWIND, 0, 30, true);
            else
               runloop_msg_queue_push_new(MSG_SLOW_MOTION, 0, 30, true);
         }
         break;
      case RUNLOOP_CTL_CHECK_MOVIE:
         if (bsv_movie_ctl(BSV_MOVIE_CTL_PLAYBACK_ON, NULL))
            return runloop_ctl(RUNLOOP_CTL_CHECK_MOVIE_PLAYBACK, NULL);
         if (!bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            return runloop_ctl(RUNLOOP_CTL_CHECK_MOVIE_INIT, NULL);
         return runloop_ctl(RUNLOOP_CTL_CHECK_MOVIE_RECORD, NULL);
      case RUNLOOP_CTL_CHECK_MOVIE_RECORD:
         if (!bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            return false;

         runloop_msg_queue_push_new(
               MSG_MOVIE_RECORD_STOPPED, 2, 180, true);
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

         event_command(EVENT_CMD_BSV_MOVIE_DEINIT);
         break;
      case RUNLOOP_CTL_CHECK_MOVIE_INIT:
         if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            return false;
         {
            char msg[128];
            char path[PATH_MAX_LENGTH];

            settings->rewind_granularity = 1;

            if (settings->state_slot > 0)
               snprintf(path, sizeof(path), "%s%d",
                     bsv_movie_get_path(), settings->state_slot);
            else
               strlcpy(path, bsv_movie_get_path(), sizeof(path));

            strlcat(path, ".bsv", sizeof(path));

            snprintf(msg, sizeof(msg), "%s \"%s\".",
                  msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
                  path);

            bsv_movie_init_handle(path, RARCH_MOVIE_RECORD);

            if (!bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
               return false;
            else if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            {
               runloop_msg_queue_push(msg, 1, 180, true);
               RARCH_LOG("%s \"%s\".\n",
                     msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
                     path);
            }
            else
            {
               runloop_msg_queue_push_new(
                     MSG_FAILED_TO_START_MOVIE_RECORD,
                     1, 180, true);
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
            }
         }
         break;
      case RUNLOOP_CTL_CHECK_MOVIE_PLAYBACK:
         if (!bsv_movie_ctl(BSV_MOVIE_CTL_END, NULL))
            return false;

         runloop_msg_queue_push_new(
               MSG_MOVIE_PLAYBACK_ENDED, 1, 180, false);
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

         event_command(EVENT_CMD_BSV_MOVIE_DEINIT);

         bsv_movie_ctl(BSV_MOVIE_CTL_UNSET_END, NULL);
         bsv_movie_ctl(BSV_MOVIE_CTL_UNSET_PLAYBACK, NULL);
         break;
      case RUNLOOP_CTL_STATE_FREE:
         runloop_perfcnt_enable     = false;
         runloop_idle               = false;
         runloop_paused             = false;
         runloop_slowmotion         = false;
         runloop_frame_time_last    = false;
         runloop_set_frame_limit    = false;
         runloop_overrides_active   = false;
         runloop_max_frames         = 0;
         break;
      case RUNLOOP_CTL_GLOBAL_FREE:
         {
            global_t *global;
            event_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
            event_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
            event_command(EVENT_CMD_RECORD_DEINIT);
            event_command(EVENT_CMD_LOG_FILE_DEINIT);

            rarch_ctl(RARCH_CTL_UNSET_BLOCK_CONFIG_READ, NULL);
            runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH,  NULL);
            runloop_overrides_active   = false;

            global = global_get_ptr();
            memset(global, 0, sizeof(struct global));
         }
         break;
      case RUNLOOP_CTL_CLEAR_STATE:
         driver_free();
         runloop_ctl(RUNLOOP_CTL_STATE_FREE,  NULL);
         runloop_ctl(RUNLOOP_CTL_GLOBAL_FREE, NULL);
         break;
      case RUNLOOP_CTL_SET_MAX_FRAMES:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            runloop_max_frames = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_IDLE:
         return runloop_idle;
      case RUNLOOP_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_idle = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_SLOWMOTION:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = runloop_slowmotion;
         }
         break;
      case RUNLOOP_CTL_SET_SLOWMOTION:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_slowmotion = *ptr;
         }
         break;
      case RUNLOOP_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            runloop_paused = *ptr;
         }
         break;
      case RUNLOOP_CTL_IS_PAUSED:
         return runloop_paused;
      case RUNLOOP_CTL_MSG_QUEUE_FREE:
#ifdef HAVE_THREADS
         slock_free(runloop_msg_queue_lock);
         runloop_msg_queue_lock = NULL;
#endif
         break;
      case RUNLOOP_CTL_MSG_QUEUE_DEINIT:
         if (!g_msg_queue)
            return true;

         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_LOCK, NULL);

         msg_queue_free(g_msg_queue);

         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_UNLOCK, NULL);
         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_FREE, NULL);

         g_msg_queue = NULL;
         break;
      case RUNLOOP_CTL_MSG_QUEUE_INIT:
         runloop_ctl(RUNLOOP_CTL_MSG_QUEUE_DEINIT, NULL);
         g_msg_queue = msg_queue_new(8);
         retro_assert(g_msg_queue);

#ifdef HAVE_THREADS
         runloop_msg_queue_lock = slock_new();
         retro_assert(runloop_msg_queue_lock);
#endif
         break;
      case RUNLOOP_CTL_MSG_QUEUE_LOCK:
#ifdef HAVE_THREADS
         slock_lock(runloop_msg_queue_lock);
#endif
         break;
      case RUNLOOP_CTL_MSG_QUEUE_UNLOCK:
#ifdef HAVE_THREADS
         slock_unlock(runloop_msg_queue_lock);
#endif
         break;
      case RUNLOOP_CTL_PREPARE_DUMMY:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_UNSET_LOAD_NO_CONTENT, NULL);
#endif
         runloop_data_clear_state();

         runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);

         rarch_ctl(RARCH_CTL_LOAD_CONTENT, NULL);
         break;
      case RUNLOOP_CTL_SET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = true;
         break;
      case RUNLOOP_CTL_UNSET_CORE_SHUTDOWN:
         runloop_core_shutdown_initiated = false;
         break;
      case RUNLOOP_CTL_IS_CORE_SHUTDOWN:
         return runloop_core_shutdown_initiated;
      case RUNLOOP_CTL_SET_SHUTDOWN:
         runloop_shutdown_initiated = true;
         break;
      case RUNLOOP_CTL_UNSET_SHUTDOWN:
         runloop_shutdown_initiated = false;
         break;
      case RUNLOOP_CTL_IS_SHUTDOWN:
         return runloop_shutdown_initiated;
      case RUNLOOP_CTL_SET_EXEC:
         runloop_exec = true;
         break;
      case RUNLOOP_CTL_UNSET_EXEC:
         runloop_exec = false;
         break;
      case RUNLOOP_CTL_IS_EXEC:
         return runloop_exec;
      case RUNLOOP_CTL_DATA_DEINIT:
         rarch_task_deinit();
         break;
      case RUNLOOP_CTL_IS_CORE_OPTION_UPDATED:
         return system->core_options ?
            core_option_updated(system->core_options) : false;
      case RUNLOOP_CTL_CORE_OPTION_PREV:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_prev(system->core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         return true;
      case RUNLOOP_CTL_CORE_OPTION_NEXT:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            core_option_next(system->core_options, *idx);
            if (ui_companion_is_on_foreground())
               ui_companion_driver_notify_refresh();
         }
         return true;
      case RUNLOOP_CTL_CORE_OPTIONS_GET:
         {
            struct retro_variable *var = (struct retro_variable*)data;

            if (!system || !system->core_options || !var)
               return false;

            RARCH_LOG("Environ GET_VARIABLE %s:\n", var->key);
            core_option_get(system->core_options, var);
            RARCH_LOG("\t%s\n", var->value ? var->value : "N/A");
         }
         return true;
      case RUNLOOP_CTL_CORE_OPTIONS_INIT:
         {
            char *game_options_path           = NULL;
            bool ret                          = false;
            const struct retro_variable *vars = (const struct retro_variable*)data;
            char buf[PATH_MAX_LENGTH]         = {0};
            global_t *global                  = global_get_ptr();
            const char *options_path          = settings->core_options_path;

            if (!*options_path && *global->path.config)
            {
               fill_pathname_resolve_relative(buf, global->path.config,
                     "retroarch-core-options.cfg", sizeof(buf));
               options_path = buf;
            }


            if (settings->game_specific_options)
               ret = rarch_game_specific_options(&game_options_path);

            if(ret)
            {
               system->core_options = core_option_new(game_options_path, vars);
               free(game_options_path);
            }
            else
               system->core_options = core_option_new(options_path, vars);

         }
         break;
      case RUNLOOP_CTL_CORE_OPTIONS_DEINIT:
         if (!system->core_options)
            return false;

         core_option_flush(system->core_options);
         core_option_free(system->core_options);

         system->core_options = NULL;
         return true;
      case RUNLOOP_CTL_NONE:
      default:
         return false;
   }

   return true;
}


#ifdef HAVE_OVERLAY
static void runloop_iterate_linefeed_overlay(settings_t *settings)
{
   static char prev_overlay_restore = false;
   bool osk_enable = input_driver_ctl(RARCH_INPUT_CTL_IS_OSK_ENABLED, NULL);

   if (osk_enable && !input_driver_ctl(RARCH_INPUT_CTL_IS_KEYBOARD_LINEFEED_ENABLED, NULL))
   {
      input_driver_ctl(RARCH_INPUT_CTL_UNSET_OSK_ENABLED, NULL);
      prev_overlay_restore  = true;
      event_command(EVENT_CMD_OVERLAY_DEINIT);
   }
   else if (!osk_enable && input_driver_ctl(RARCH_INPUT_CTL_IS_KEYBOARD_LINEFEED_ENABLED, NULL))
   {
      input_driver_ctl(RARCH_INPUT_CTL_SET_OSK_ENABLED, NULL);
      prev_overlay_restore  = false;
      event_command(EVENT_CMD_OVERLAY_INIT);
   }
   else if (prev_overlay_restore)
   {
      if (!settings->input.overlay_hide_in_menu)
         event_command(EVENT_CMD_OVERLAY_INIT);
      prev_overlay_restore = false;
   }
}
#endif



/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
static INLINE int runloop_iterate_time_to_exit(bool quit_key_pressed)
{
   bool time_to_exit             = runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL) || quit_key_pressed;
   time_to_exit                  = time_to_exit || (video_driver_ctl(RARCH_DISPLAY_CTL_IS_ALIVE, NULL) == false);
   time_to_exit                  = time_to_exit || bsv_movie_ctl(BSV_MOVIE_CTL_END_EOF, NULL);
   time_to_exit                  = time_to_exit || runloop_ctl(RUNLOOP_CTL_IS_FRAME_COUNT_END, NULL);
   time_to_exit                  = time_to_exit || runloop_ctl(RUNLOOP_CTL_IS_EXEC, NULL);

   if (!time_to_exit)
      return 1;

   if (runloop_ctl(RUNLOOP_CTL_IS_EXEC, NULL))
      runloop_ctl(RUNLOOP_CTL_UNSET_EXEC, NULL);

   if (runloop_ctl(RUNLOOP_CTL_IS_CORE_SHUTDOWN, NULL))
   {
      /* Quits out of RetroArch main loop.
       * On special case, loads dummy core
       * instead of exiting RetroArch completely.
       * Aborts core shutdown if invoked.
       */

      settings_t *settings          = config_get_ptr();
      if (!settings->load_dummy_on_core_shutdown)
         return -1;
      if (!runloop_ctl(RUNLOOP_CTL_PREPARE_DUMMY, NULL))
         return -1;

      runloop_ctl(RUNLOOP_CTL_UNSET_SHUTDOWN,      NULL);
      runloop_ctl(RUNLOOP_CTL_UNSET_CORE_SHUTDOWN, NULL);

      return 0;
   }

   return -1;
}

/**
 * runloop_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int runloop_iterate(unsigned *sleep_ms)
{
   unsigned i;
   event_cmd_state_t    cmd;
   event_cmd_state_t   *cmd_ptr                 = &cmd;
   retro_time_t current, target, to_sleep_ms;
   static retro_usec_t frame_time_last          = 0;
   static retro_time_t frame_limit_minimum_time = 0.0;
   static retro_time_t frame_limit_last_time    = 0.0;
   static retro_input_t last_input              = 0;
   settings_t *settings                         = config_get_ptr();
   global_t   *global                           = global_get_ptr();
   rarch_system_info_t *system                  = rarch_system_info_get_ptr();

   cmd.state[1]                                 = last_input;
   cmd.state[0]                                 = input_keys_pressed();
   last_input                                   = cmd.state[0];

   if (runloop_ctl(RUNLOOP_CTL_IS_FRAME_TIME_LAST, NULL))
   {
      frame_time_last = 0;
      runloop_ctl(RUNLOOP_CTL_UNSET_FRAME_TIME_LAST, NULL);
   }

   if (runloop_ctl(RUNLOOP_CTL_SHOULD_SET_FRAME_LIMIT, NULL))
   {
      struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
      float fastforward_ratio              = (settings->fastforward_ratio == 0.0f) 
         ? 1.0f : settings->fastforward_ratio;

      frame_limit_last_time    = retro_get_time_usec();
      frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f 
            / (av_info->timing.fps * fastforward_ratio));

      runloop_ctl(RUNLOOP_CTL_UNSET_FRAME_LIMIT, NULL);
   }

   if (input_driver_ctl(RARCH_INPUT_CTL_IS_FLUSHING_INPUT, NULL))
   {
      input_driver_ctl(RARCH_INPUT_CTL_UNSET_FLUSHING_INPUT, NULL);
      if (cmd.state[0])
      {
         cmd.state[0] = 0;

         /* If core was paused before entering menu, evoke
          * pause toggle to wake it up. */
         if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
            BIT64_SET(cmd.state[0], RARCH_PAUSE_TOGGLE);
         input_driver_ctl(RARCH_INPUT_CTL_SET_FLUSHING_INPUT, NULL);
      }
   }

   if (system->frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */

      bool is_slowmotion;
      retro_time_t current     = retro_get_time_usec();
      retro_time_t delta       = current - frame_time_last;
      bool is_locked_fps       = (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL) ||
            input_driver_ctl(RARCH_INPUT_CTL_IS_NONBLOCK_STATE, NULL)) |
         !!recording_driver_get_data_ptr();

      runloop_ctl(RUNLOOP_CTL_IS_SLOWMOTION, &is_slowmotion);

      if (!frame_time_last || is_locked_fps)
         delta = system->frame_time.reference;

      if (!is_locked_fps && is_slowmotion)
         delta /= settings->slowmotion_ratio;

      frame_time_last = current;

      if (is_locked_fps)
         frame_time_last = 0;

      system->frame_time.callback(delta);
   }

   cmd.state[2]      = cmd.state[0] & ~cmd.state[1];  /* trigger  */

   if (runloop_cmd_triggered(cmd_ptr, RARCH_OVERLAY_NEXT))
      event_command(EVENT_CMD_OVERLAY_NEXT);

   if (runloop_cmd_triggered(cmd_ptr, RARCH_FULLSCREEN_TOGGLE_KEY))
   {
      bool fullscreen_toggled = !runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL);
#ifdef HAVE_MENU
      fullscreen_toggled = fullscreen_toggled || menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL);
#endif

      if (fullscreen_toggled)
         event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
   }

   if (runloop_cmd_triggered(cmd_ptr, RARCH_GRAB_MOUSE_TOGGLE))
      event_command(EVENT_CMD_GRAB_MOUSE_TOGGLE);

#ifdef HAVE_MENU
   if (runloop_cmd_menu_press(cmd_ptr) || (global->inited.core.type == CORE_TYPE_DUMMY))
   {
      if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
      {
         if (global->inited.main && (global->inited.core.type != CORE_TYPE_DUMMY))
            rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
      }
      else
         rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   runloop_iterate_linefeed_overlay(settings);
#endif

   if (runloop_iterate_time_to_exit(runloop_cmd_press(cmd_ptr, RARCH_QUIT_KEY)) != 1)
   {
      frame_limit_last_time = 0.0;
      return -1;
   }


#ifdef HAVE_MENU
   if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
   {
      bool focused = runloop_ctl(RUNLOOP_CTL_CHECK_FOCUS, NULL) && !ui_companion_is_on_foreground();
      bool is_idle = runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL);

      if (menu_driver_iterate((enum menu_action)menu_input_frame_retropad(cmd.state[0], cmd.state[2])) == -1)
         rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);

      if (focused || !is_idle)
         menu_driver_ctl(RARCH_MENU_CTL_RENDER, NULL);

      if (!focused || is_idle)
      {
         *sleep_ms = 10;
         return 1;
      }

      goto end;
   }
#endif

   if (!runloop_ctl(RUNLOOP_CTL_CHECK_STATE, &cmd))
   {
      /* RetroArch has been paused. */
      retro_ctx.poll_cb();
      *sleep_ms = 10;
      return 1;
   }

#if defined(HAVE_THREADS)
   lock_autosave();
#endif

#ifdef HAVE_NETPLAY
   netplay_driver_ctl(RARCH_NETPLAY_CTL_PRE_FRAME, NULL);
#endif

   if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_FRAME_START, NULL);

   if (system->camera_callback.caps)
      driver_camera_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!settings->input.analog_dpad_mode[i])
         continue;

      input_push_analog_dpad(settings->input.binds[i],
            settings->input.analog_dpad_mode[i]);
      input_push_analog_dpad(settings->input.autoconf_binds[i],
            settings->input.analog_dpad_mode[i]);
   }

   if ((settings->video.frame_delay > 0) && 
         !input_driver_ctl(RARCH_INPUT_CTL_IS_NONBLOCK_STATE, NULL))
      retro_sleep(settings->video.frame_delay);

   /* Run libretro for one frame. */
   core.retro_run();

#ifdef HAVE_CHEEVOS
   /* Test the achievements. */
   cheevos_test();
#endif

   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!settings->input.analog_dpad_mode[i])
         continue;

      input_pop_analog_dpad(settings->input.binds[i]);
      input_pop_analog_dpad(settings->input.autoconf_binds[i]);
   }

   if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
      bsv_movie_ctl(BSV_MOVIE_CTL_SET_FRAME_END, NULL);

#ifdef HAVE_NETPLAY
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

#ifdef HAVE_MENU
end:
#endif
   if (!settings->fastforward_ratio)
      return 0;

   current                        = retro_get_time_usec();
   target                         = frame_limit_last_time + frame_limit_minimum_time;
   to_sleep_ms                    = (target - current) / 1000;

   if (to_sleep_ms > 0)
   {
      *sleep_ms = (unsigned)to_sleep_ms;
      /* Combat jitter a bit. */
      frame_limit_last_time += frame_limit_minimum_time;
      return 1;
   }

   frame_limit_last_time  = retro_get_time_usec();

   return 0;
}

void runloop_data_iterate(void)
{
   rarch_task_check();
}


void data_runloop_osd_msg(const char *msg, size_t len)
{
   runloop_msg_queue_push(msg, 1, 10, true);
}

