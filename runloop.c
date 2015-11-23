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

#include <file/file_path.h>
#include <retro_inline.h>

#include <compat/strl.h>

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif
#include "configuration.h"
#include "performance.h"
#include "retroarch.h"
#include "runloop.h"
#include "runloop_data.h"

#include "msg_hash.h"

#include "input/keyboard_line.h"
#include "input/input_common.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#include "verbosity.h"

static struct global g_extern;

static bool main_is_idle;
static bool main_is_paused;
static bool main_is_slowmotion;

static unsigned main_max_frames;

static retro_time_t frame_limit_last_time;
static retro_time_t frame_limit_minimum_time;

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
static bool check_pause(driver_t *driver, settings_t *settings,
      bool pause_pressed, bool frameadvance_pressed)
{
   static bool old_focus    = true;
   bool focus               = true;
   enum event_command cmd   = EVENT_CMD_NONE;
   bool old_is_paused       = main_is_paused;
   const video_driver_t *video = driver ? (const video_driver_t*)driver->video :
      NULL;

   /* FRAMEADVANCE will set us into pause mode. */
   pause_pressed |= !main_is_paused && frameadvance_pressed;

   if (settings->pause_nonactive)
      focus = video->focus(driver->video_data);

   if (focus && pause_pressed)
      cmd = EVENT_CMD_PAUSE_TOGGLE;
   else if (focus && !old_focus)
      cmd = EVENT_CMD_UNPAUSE;
   else if (!focus && old_focus)
      cmd = EVENT_CMD_PAUSE;

   old_focus = focus;

   if (cmd != EVENT_CMD_NONE)
      event_command(cmd);

   if (main_is_paused == old_is_paused)
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
static void check_fast_forward_button(driver_t *driver,
      bool fastforward_pressed,
      bool hold_pressed, bool old_hold_pressed)
{
   /* To avoid continous switching if we hold the button down, we require
    * that the button must go from pressed to unpressed back to pressed
    * to be able to toggle between then.
    */
   if (fastforward_pressed)
      driver->nonblock_state = !driver->nonblock_state;
   else if (old_hold_pressed != hold_pressed)
      driver->nonblock_state = hold_pressed;
   else
      return;

   driver_set_nonblock_state(driver->nonblock_state);
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
   char msg[PATH_MAX_LENGTH];

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

   rarch_main_msg_queue_push(msg, 1, 180, true);

   RARCH_LOG("%s\n", msg);
}

/**
 * check_rewind:
 * @pressed              : was rewind key pressed or held?
 *
 * Checks if rewind toggle/hold was being pressed and/or held.
 **/
static void check_rewind(settings_t *settings,
      global_t *global, bool pressed)
{
   static bool first = true;

   if (state_manager_frame_is_reversed())
   {
      audio_driver_ctl(RARCH_AUDIO_CTL_FRAME_IS_REVERSE, NULL);
      state_manager_set_frame_is_reversed(false);
   }

   if (first)
   {
      first = false;
      return;
   }

   if (!global->rewind.state)
      return;

   if (pressed)
   {
      const void *buf    = NULL;

      if (state_manager_pop(global->rewind.state, &buf))
      {
         state_manager_set_frame_is_reversed(true);
         audio_driver_ctl(RARCH_AUDIO_CTL_SETUP_REWIND, NULL);

         rarch_main_msg_queue_push_new(MSG_REWINDING, 0,
               main_is_paused ? 1 : 30, true);
         core.retro_unserialize(buf, global->rewind.size);

         if (global->bsv.movie)
            bsv_movie_frame_rewind(global->bsv.movie);
      }
      else
         rarch_main_msg_queue_push_new(MSG_REWIND_REACHED_END,
               0, 30, true);
   }
   else
   {
      static unsigned cnt      = 0;

      cnt = (cnt + 1) % (settings->rewind_granularity ?
            settings->rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if ((cnt == 0) || global->bsv.movie)
      {
         static struct retro_perf_counter rewind_serialize = {0};
         void *state = NULL;

         state_manager_push_where(global->rewind.state, &state);

         rarch_perf_init(&rewind_serialize, "rewind_serialize");
         retro_perf_start(&rewind_serialize);
         core.retro_serialize(state, global->rewind.size);
         retro_perf_stop(&rewind_serialize);

         state_manager_push_do(global->rewind.state);
      }
   }

   retro_set_rewind_callbacks();
}

#define SHADER_EXT_GLSL      0x7c976537U
#define SHADER_EXT_GLSLP     0x0f840c87U
#define SHADER_EXT_CG        0x0059776fU
#define SHADER_EXT_CGP       0x0b8865bfU

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
static void check_shader_dir(global_t *global,
      bool pressed_next, bool pressed_prev)
{
   uint32_t ext_hash;
   char msg[PATH_MAX_LENGTH];
   const char *shader          = NULL;
   const char *ext             = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!global || !global->dir.shader_dir.list)
      return;

   if (pressed_next)
   {
      global->dir.shader_dir.ptr = (global->dir.shader_dir.ptr + 1) %
         global->dir.shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      if (global->dir.shader_dir.ptr == 0)
         global->dir.shader_dir.ptr = global->dir.shader_dir.list->size - 1;
      else
         global->dir.shader_dir.ptr--;
   }
   else
      return;

   shader   = global->dir.shader_dir.list->elems[global->dir.shader_dir.ptr].data;
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
         (unsigned)global->dir.shader_dir.ptr, shader);
   rarch_main_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_APPLYING_SHADER),
         shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER));
}

global_t *global_get_ptr(void)
{
   return &g_extern;
}

bool rarch_main_ctl(enum rarch_main_ctl_state state, void *data)
{
   driver_t     *driver  = driver_get_ptr();
   settings_t *settings  = config_get_ptr();
   global_t     *global  = global_get_ptr();

   switch (state)
   {
      case RARCH_MAIN_CTL_SET_WINDOWED_SCALE:
         {
            unsigned *idx = (unsigned*)data;
            if (!idx)
               return false;
            global->pending.windowed_scale = *idx;
         }
         break;
      case RARCH_MAIN_CTL_SET_LIBRETRO_PATH:
         {
            const char *fullpath = (const char*)data;
            if (!fullpath)
               return false;
            strlcpy(settings->libretro, fullpath, sizeof(settings->libretro));
         }
         break;
      case RARCH_MAIN_CTL_CLEAR_CONTENT_PATH:
         *global->path.fullpath = '\0';
         break;
      case RARCH_MAIN_CTL_GET_CONTENT_PATH:
         {
            char **fullpath = (char**)data;
            if (!fullpath)
               return false;
            *fullpath       = (char*)global->path.fullpath;
         }
         break;
      case RARCH_MAIN_CTL_SET_CONTENT_PATH:
         {
            const char *fullpath = (const char*)data;
            if (!fullpath)
               return false;
            strlcpy(global->path.fullpath, fullpath, sizeof(global->path.fullpath));
         }
         break;
      case RARCH_MAIN_CTL_CHECK_STATE:
         {
            event_cmd_state_t *cmd    = (event_cmd_state_t*)data;

            if (!cmd || main_is_idle)
               return false;

            if (cmd->screenshot_pressed)
               event_command(EVENT_CMD_TAKE_SCREENSHOT);

            if (cmd->mute_pressed)
               event_command(EVENT_CMD_AUDIO_MUTE_TOGGLE);

            if (cmd->osk_pressed)
               driver->keyboard_linefeed_enable = !driver->keyboard_linefeed_enable;

            if (cmd->volume_up_pressed)
               event_command(EVENT_CMD_VOLUME_UP);
            else if (cmd->volume_down_pressed)
               event_command(EVENT_CMD_VOLUME_DOWN);

#ifdef HAVE_NETPLAY
            if (driver->netplay_data)
            {
               if (cmd->netplay_flip_pressed)
                  event_command(EVENT_CMD_NETPLAY_FLIP_PLAYERS);

               if (cmd->fullscreen_toggle)
                  event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
               break;
            }
#endif

            check_pause(driver, settings,
                  cmd->pause_pressed, cmd->frameadvance_pressed);

            if (!rarch_main_ctl(RARCH_MAIN_CTL_CHECK_PAUSE_STATE, cmd))
               return false;

            check_fast_forward_button(driver,
                  cmd->fastforward_pressed,
                  cmd->hold_pressed, cmd->old_hold_pressed);
            check_stateslots(settings, cmd->state_slot_increase,
                  cmd->state_slot_decrease);

            if (cmd->save_state_pressed)
               event_command(EVENT_CMD_SAVE_STATE);
            else if (cmd->load_state_pressed)
               event_command(EVENT_CMD_LOAD_STATE);

            check_rewind(settings, global, cmd->rewind_pressed);

            rarch_main_ctl(RARCH_MAIN_CTL_CHECK_SLOWMOTION, &cmd->slowmotion_pressed);

            if (cmd->movie_record)
               rarch_main_ctl(RARCH_MAIN_CTL_CHECK_MOVIE, NULL);

            check_shader_dir(global, cmd->shader_next_pressed,
                  cmd->shader_prev_pressed);

            if (cmd->disk_eject_pressed)
               event_command(EVENT_CMD_DISK_EJECT_TOGGLE);
            else if (cmd->disk_next_pressed)
               event_command(EVENT_CMD_DISK_NEXT);
            else if (cmd->disk_prev_pressed)
               event_command(EVENT_CMD_DISK_PREV);

            if (cmd->reset_pressed)
               event_command(EVENT_CMD_RESET);

            if (global->cheat)
            {
               if (cmd->cheat_index_plus_pressed)
                  cheat_manager_index_next(global->cheat);
               else if (cmd->cheat_index_minus_pressed)
                  cheat_manager_index_prev(global->cheat);
               else if (cmd->cheat_toggle_pressed)
                  cheat_manager_toggle(global->cheat);
            }
         }
         break;
      case RARCH_MAIN_CTL_CHECK_PAUSE_STATE:
         {
            bool check_is_oneshot;
            event_cmd_state_t *cmd    = (event_cmd_state_t*)data;

            if (!cmd)
               return false;

            check_is_oneshot     = cmd->frameadvance_pressed || cmd->rewind_pressed;

            if (!main_is_paused)
               return true;

            if (cmd->fullscreen_toggle)
            {
               event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
            }

            if (!check_is_oneshot)
               return false;
         }
         break;
      case RARCH_MAIN_CTL_CHECK_SLOWMOTION:
         {
            bool *ptr            = (bool*)data;

            if (!ptr)
               return false;

            main_is_slowmotion   = *ptr;

            if (!main_is_slowmotion)
               return false;

            if (settings->video.black_frame_insertion)
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);

            if (state_manager_frame_is_reversed())
               rarch_main_msg_queue_push_new(MSG_SLOW_MOTION_REWIND, 0, 30, true);
            else
               rarch_main_msg_queue_push_new(MSG_SLOW_MOTION, 0, 30, true);
         }
         break;
      case RARCH_MAIN_CTL_CHECK_MOVIE:
         if (global->bsv.movie_playback)
            return rarch_main_ctl(RARCH_MAIN_CTL_CHECK_MOVIE_PLAYBACK, NULL);
         if (!global->bsv.movie)
            return rarch_main_ctl(RARCH_MAIN_CTL_CHECK_MOVIE_INIT, NULL);
         return rarch_main_ctl(RARCH_MAIN_CTL_CHECK_MOVIE_RECORD, NULL);
      case RARCH_MAIN_CTL_CHECK_MOVIE_RECORD:
         if (!global->bsv.movie)
            return false;

         rarch_main_msg_queue_push_new(
               MSG_MOVIE_RECORD_STOPPED, 2, 180, true);
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_RECORD_STOPPED));

         event_command(EVENT_CMD_BSV_MOVIE_DEINIT);
         break;
      case RARCH_MAIN_CTL_CHECK_MOVIE_INIT:
         if (global->bsv.movie)
            return false;
         {
            char path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];

            settings->rewind_granularity = 1;

            if (settings->state_slot > 0)
               snprintf(path, sizeof(path), "%s%d",
                     global->bsv.movie_path, settings->state_slot);
            else
               strlcpy(path, global->bsv.movie_path, sizeof(path));

            strlcat(path, ".bsv", sizeof(path));

            snprintf(msg, sizeof(msg), "%s \"%s\".",
                  msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
                  path);

            global->bsv.movie = bsv_movie_init(path, RARCH_MOVIE_RECORD);

            if (!global->bsv.movie)
               return false;
            else if (global->bsv.movie)
            {
               rarch_main_msg_queue_push(msg, 1, 180, true);
               RARCH_LOG("%s \"%s\".\n",
                     msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
                     path);
            }
            else
            {
               rarch_main_msg_queue_push_new(
                     MSG_FAILED_TO_START_MOVIE_RECORD,
                     1, 180, true);
               RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
            }
         }
         break;
      case RARCH_MAIN_CTL_CHECK_MOVIE_PLAYBACK:
         if (!global->bsv.movie_end)
            return false;

         rarch_main_msg_queue_push_new(
               MSG_MOVIE_PLAYBACK_ENDED, 1, 180, false);
         RARCH_LOG("%s\n", msg_hash_to_str(MSG_MOVIE_PLAYBACK_ENDED));

         event_command(EVENT_CMD_BSV_MOVIE_DEINIT);

         global->bsv.movie_end      = false;
         global->bsv.movie_playback = false;
         break;
      case RARCH_MAIN_CTL_STATE_FREE:
         main_is_idle               = false;
         main_is_paused             = false;
         main_is_slowmotion         = false;
         frame_limit_last_time      = 0.0;
         main_max_frames            = 0;
         break;
      case RARCH_MAIN_CTL_GLOBAL_FREE:
         event_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
         event_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
         event_command(EVENT_CMD_RECORD_DEINIT);
         event_command(EVENT_CMD_LOG_FILE_DEINIT);

         memset(&g_extern, 0, sizeof(g_extern));
         break;
      case RARCH_MAIN_CTL_CLEAR_STATE:
         driver_clear_state();
         rarch_main_ctl(RARCH_MAIN_CTL_STATE_FREE,  NULL);
         rarch_main_ctl(RARCH_MAIN_CTL_GLOBAL_FREE, NULL);
         break;
      case RARCH_MAIN_CTL_SET_MAX_FRAMES:
         {
            unsigned *ptr = (unsigned*)data;
            if (!ptr)
               return false;
            main_max_frames = *ptr;
         }
         break;
      case RARCH_MAIN_CTL_SET_FRAME_LIMIT_LAST_TIME:
         {
            struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
            float fastforward_ratio              = settings->fastforward_ratio;

            if (fastforward_ratio == 0.0f)
               fastforward_ratio = 1.0f;

            frame_limit_last_time    = retro_get_time_usec();
            frame_limit_minimum_time = (retro_time_t)roundf(1000000.0f / (av_info->timing.fps * fastforward_ratio));
         }
         break;
      case RARCH_MAIN_CTL_IS_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = main_is_idle;
         }
         break;
      case RARCH_MAIN_CTL_SET_IDLE:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            main_is_idle = *ptr;
         }
         break;
      case RARCH_MAIN_CTL_IS_SLOWMOTION:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = main_is_slowmotion;
         }
         break;
      case RARCH_MAIN_CTL_SET_SLOWMOTION:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            main_is_slowmotion = *ptr;
         }
         break;
      case RARCH_MAIN_CTL_SET_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            main_is_paused = *ptr;
         }
         break;
      case RARCH_MAIN_CTL_IS_PAUSED:
         {
            bool *ptr = (bool*)data;
            if (!ptr)
               return false;
            *ptr = main_is_paused;
         }
         break;
      default:
         return false;
   }

   return true;
}

/**
 * check_block_hotkey:
 * @enable_hotkey        : Is hotkey enable key enabled?
 *
 * Checks if 'hotkey enable' key is pressed.
 **/
static bool check_block_hotkey(driver_t *driver, settings_t *settings,
      bool enable_hotkey)
{
   bool use_hotkey_enable;
   const struct retro_keybind *bind =
      &settings->input.binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *autoconf_bind =
      &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   driver->block_hotkey             =
      input_driver_keyboard_mapping_is_blocked();

   /* If we haven't bound anything to this,
    * always allow hotkeys. */
   use_hotkey_enable                =
         (bind->key != RETROK_UNKNOWN)
      || (bind->joykey != NO_BTN)
      || (bind->joyaxis != AXIS_NONE)
      || (autoconf_bind->key != RETROK_UNKNOWN )
      || (autoconf_bind->joykey != NO_BTN)
      || (autoconf_bind->joyaxis != AXIS_NONE);

   driver->block_hotkey             =
      input_driver_keyboard_mapping_is_blocked() ||
      (use_hotkey_enable && !enable_hotkey);

   /* If we hold ENABLE_HOTKEY button, block all libretro input to allow
    * hotkeys to be bound to same keys as RetroPad. */
   return (use_hotkey_enable && enable_hotkey);
}

/**
 * input_keys_pressed:
 *
 * Grab an input sample for this frame.
 *
 * TODO: In case RARCH_BIND_LIST_END starts exceeding 64,
 * and you need a bitmask of more than 64 entries, reimplement
 * it to use something like rarch_bits_t.
 *
 * Returns: Input sample containg a mask of all pressed keys.
 */
static INLINE retro_input_t input_keys_pressed(driver_t *driver,
      settings_t *settings, global_t *global)
{
   unsigned i;
   const struct retro_keybind *binds[MAX_USERS];
   retro_input_t             ret = 0;

   for (i = 0; i < MAX_USERS; i++)
      binds[i] = settings->input.binds[i];

   if (!driver->input || !driver->input_data)
      return 0;

   global->turbo.count++;

   driver->block_libretro_input = check_block_hotkey(driver,
         settings, driver->input->key_pressed(
            driver->input_data, RARCH_ENABLE_HOTKEY));


   for (i = 0; i < settings->input.max_users; i++)
   {
      input_push_analog_dpad(settings->input.binds[i],
            settings->input.analog_dpad_mode[i]);
      input_push_analog_dpad(settings->input.autoconf_binds[i],
            settings->input.analog_dpad_mode[i]);

      global->turbo.frame_enable[i] = 0;
   }

   if (!driver->block_libretro_input)
   {
      for (i = 0; i < settings->input.max_users; i++)
         global->turbo.frame_enable[i] = input_driver_state(binds,
               i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
   }

   ret = input_driver_keys_pressed();

   for (i = 0; i < settings->input.max_users; i++)
   {
      input_pop_analog_dpad(settings->input.binds[i]);
      input_pop_analog_dpad(settings->input.autoconf_binds[i]);
   }

   return ret;
}

#ifdef HAVE_OVERLAY
static void rarch_main_iterate_linefeed_overlay(driver_t *driver,
      settings_t *settings)
{
   static char prev_overlay_restore = false;

   if (driver->osk_enable && !driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = false;
      prev_overlay_restore  = true;
      event_command(EVENT_CMD_OVERLAY_DEINIT);
   }
   else if (!driver->osk_enable && driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = true;
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

FILE *retro_main_log_file(void)
{
   global_t *global = global_get_ptr();
   if (!global)
      return NULL;
   return global->log_file;
}

#ifdef HAVE_MENU
static bool rarch_main_cmd_get_state_menu_toggle_button_combo(
      driver_t *driver, settings_t *settings,
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

   driver->flushing_input = true;
   return true;
}
#endif

static void rarch_main_cmd_get_state(driver_t *driver,
      settings_t *settings, event_cmd_state_t *cmd,
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (!cmd)
      return;

   cmd->overlay_next_pressed        = BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT);
   cmd->grab_mouse_pressed          = BIT64_GET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE);
#ifdef HAVE_MENU
   cmd->menu_pressed                = BIT64_GET(trigger_input, RARCH_MENU_TOGGLE) ||
                                      rarch_main_cmd_get_state_menu_toggle_button_combo(driver,
                                            settings, input,
                                            old_input, trigger_input);
#endif
   cmd->quit_key_pressed            = BIT64_GET(input, RARCH_QUIT_KEY);
   cmd->screenshot_pressed          = BIT64_GET(trigger_input, RARCH_SCREENSHOT);
   cmd->mute_pressed                = BIT64_GET(trigger_input, RARCH_MUTE);
   cmd->osk_pressed                 = BIT64_GET(trigger_input, RARCH_OSK);
   cmd->volume_up_pressed           = BIT64_GET(input, RARCH_VOLUME_UP);
   cmd->volume_down_pressed         = BIT64_GET(input, RARCH_VOLUME_DOWN);
   cmd->reset_pressed               = BIT64_GET(trigger_input, RARCH_RESET);
   cmd->disk_prev_pressed           = BIT64_GET(trigger_input, RARCH_DISK_PREV);
   cmd->disk_next_pressed           = BIT64_GET(trigger_input, RARCH_DISK_NEXT);
   cmd->disk_eject_pressed          = BIT64_GET(trigger_input, RARCH_DISK_EJECT_TOGGLE);
   cmd->movie_record                = BIT64_GET(trigger_input, RARCH_MOVIE_RECORD_TOGGLE);
   cmd->save_state_pressed          = BIT64_GET(trigger_input, RARCH_SAVE_STATE_KEY);
   cmd->load_state_pressed          = BIT64_GET(trigger_input, RARCH_LOAD_STATE_KEY);
   cmd->slowmotion_pressed          = BIT64_GET(input, RARCH_SLOWMOTION);
   cmd->shader_next_pressed         = BIT64_GET(trigger_input, RARCH_SHADER_NEXT);
   cmd->shader_prev_pressed         = BIT64_GET(trigger_input, RARCH_SHADER_PREV);
   cmd->fastforward_pressed         = BIT64_GET(trigger_input, RARCH_FAST_FORWARD_KEY);
   cmd->hold_pressed                = BIT64_GET(input, RARCH_FAST_FORWARD_HOLD_KEY);
   cmd->old_hold_pressed            = BIT64_GET(old_input, RARCH_FAST_FORWARD_HOLD_KEY);
   cmd->state_slot_increase         = BIT64_GET(trigger_input, RARCH_STATE_SLOT_PLUS);
   cmd->state_slot_decrease         = BIT64_GET(trigger_input, RARCH_STATE_SLOT_MINUS);
   cmd->pause_pressed               = BIT64_GET(trigger_input, RARCH_PAUSE_TOGGLE);
   cmd->frameadvance_pressed        = BIT64_GET(trigger_input, RARCH_FRAMEADVANCE);
   cmd->rewind_pressed              = BIT64_GET(input,         RARCH_REWIND);
   cmd->netplay_flip_pressed        = BIT64_GET(trigger_input, RARCH_NETPLAY_FLIP);
   cmd->fullscreen_toggle           = BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY);
   cmd->cheat_index_plus_pressed    = BIT64_GET(trigger_input,
         RARCH_CHEAT_INDEX_PLUS);
   cmd->cheat_index_minus_pressed   = BIT64_GET(trigger_input,
         RARCH_CHEAT_INDEX_MINUS);
   cmd->cheat_toggle_pressed        = BIT64_GET(trigger_input,
         RARCH_CHEAT_TOGGLE);
}

/* Time to exit out of the main loop?
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 */
static INLINE int rarch_main_iterate_time_to_exit(event_cmd_state_t *cmd)
{
   uint64_t *frame_count         = NULL;
   settings_t *settings          = config_get_ptr();
   global_t   *global            = global_get_ptr();
   driver_t *driver              = driver_get_ptr();
   rarch_system_info_t *system   = rarch_system_info_get_ptr();
   video_driver_t *video         = driver ? (video_driver_t*)driver->video : NULL;
   bool shutdown_pressed         = (system && system->shutdown) || cmd->quit_key_pressed;
   bool video_alive              = video && video->alive(driver->video_data);
   bool movie_end                = (global->bsv.movie_end && global->bsv.eof_exit);
   bool frame_count_end          = false;
   
   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);
   frame_count_end               = main_max_frames && (*frame_count >= main_max_frames);

   if (shutdown_pressed || frame_count_end || movie_end || !video_alive || global->exec)
   {
      if (global->exec)
         global->exec = false;

      /* Quits out of RetroArch main loop.
       * On special case, loads dummy core
       * instead of exiting RetroArch completely.
       * Aborts core shutdown if invoked.
       */
      if (global->core_shutdown_initiated
            && settings->load_dummy_on_core_shutdown)
      {
         if (!event_command(EVENT_CMD_PREPARE_DUMMY))
            return -1;

         system->shutdown = false;
         global->core_shutdown_initiated = false;

         return 0;
      }

      return -1;
   }

   return 1;
}

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop.
 **/
int rarch_main_iterate(unsigned *sleep_ms)
{
   int ret;
   unsigned i;
   retro_input_t trigger_input;
   event_cmd_state_t    cmd;
   retro_time_t current, target, to_sleep_ms;
   static retro_input_t last_input = 0;
   driver_t *driver                = driver_get_ptr();
   settings_t *settings            = config_get_ptr();
   global_t   *global              = global_get_ptr();
   retro_input_t input             = input_keys_pressed(driver, settings, global);
   rarch_system_info_t *system     = rarch_system_info_get_ptr();
   retro_input_t old_input         = last_input;
   last_input                      = input;

   if (driver->flushing_input)
   {
      driver->flushing_input = false;
      if (input)
      {
         input = 0;

         /* If core was paused before entering menu, evoke
          * pause toggle to wake it up. */
         if (main_is_paused)
            BIT64_SET(input, RARCH_PAUSE_TOGGLE);
         driver->flushing_input = true;
      }
   }

   if (system->frame_time.callback)
   {
      /* Updates frame timing if frame timing callback is in use by the core.
       * Limits frame time if fast forward ratio throttle is enabled. */

      retro_time_t current     = retro_get_time_usec();
      retro_time_t delta       = current - system->frame_time_last;
      bool is_locked_fps       = (main_is_paused || driver->nonblock_state) |
         !!driver->recording_data;

      if (!system->frame_time_last || is_locked_fps)
         delta = system->frame_time.reference;

      if (!is_locked_fps && main_is_slowmotion)
         delta /= settings->slowmotion_ratio;

      system->frame_time_last = current;

      if (is_locked_fps)
         system->frame_time_last = 0;

      system->frame_time.callback(delta);
   }

   trigger_input = input & ~old_input;
   rarch_main_cmd_get_state(driver, settings, &cmd, input, old_input, trigger_input);

   if (cmd.overlay_next_pressed)
      event_command(EVENT_CMD_OVERLAY_NEXT);

   if (!main_is_paused
#ifdef HAVE_MENU
         || menu_driver_alive()
#endif
         )
   {
      if (cmd.fullscreen_toggle)
         event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
   }

   if (cmd.grab_mouse_pressed)
      event_command(EVENT_CMD_GRAB_MOUSE_TOGGLE);

#ifdef HAVE_MENU
   if (cmd.menu_pressed || (global->inited.core.type == CORE_TYPE_DUMMY))
   {
      if (menu_driver_alive())
      {
         if (global->inited.main && (global->inited.core.type != CORE_TYPE_DUMMY))
            rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED, NULL);
      }
      else
         rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING, NULL);
   }
#endif

#ifdef HAVE_OVERLAY
   rarch_main_iterate_linefeed_overlay(driver, settings);
#endif

   ret = rarch_main_iterate_time_to_exit(&cmd);

   if (ret != 1)
      return -1;


#ifdef HAVE_MENU
   if (menu_driver_alive())
   {
      if (menu_driver_iterate((enum menu_action)menu_input_frame_retropad(input, trigger_input)) == -1)
         rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED, NULL);

      if (!input && settings->menu.pause_libretro)
        return 1;
      goto end;
   }
#endif

   if (!rarch_main_ctl(RARCH_MAIN_CTL_CHECK_STATE, &cmd))
   {
      /* RetroArch has been paused. */
      driver->retro_ctx.poll_cb();
      *sleep_ms = 10;
      return 1;
   }

#if defined(HAVE_THREADS)
   lock_autosave();
#endif

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      netplay_pre_frame((netplay_t*)driver->netplay_data);
#endif

   if (global->bsv.movie)
      bsv_movie_set_frame_start(global->bsv.movie);

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

   if ((settings->video.frame_delay > 0) && !driver->nonblock_state)
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

   if (global->bsv.movie)
      bsv_movie_set_frame_end(global->bsv.movie);

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      netplay_post_frame((netplay_t*)driver->netplay_data);
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
