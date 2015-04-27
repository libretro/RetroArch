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
#include "dynamic.h"
#include "performance.h"
#include "retroarch_logger.h"
#include "intl/intl.h"
#include "retroarch.h"
#include "runloop.h"
#include "runloop_data.h"
#include "input/keyboard_line.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

static struct runloop *g_runloop;

static struct global *g_extern;

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
static bool check_pause(bool pause_pressed, bool frameadvance_pressed)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   static bool old_focus    = true;
   bool focus               = true;
   enum event_command cmd   = EVENT_CMD_NONE;
   bool old_is_paused       = runloop->is_paused;
   settings_t *settings     = config_get_ptr();

   /* FRAMEADVANCE will set us into pause mode. */
   pause_pressed |= !runloop->is_paused && frameadvance_pressed;

   if (settings->pause_nonactive)
      focus = video_driver_has_focus();

   if (focus && pause_pressed)
      cmd = EVENT_CMD_PAUSE_TOGGLE;
   else if (focus && !old_focus)
      cmd = EVENT_CMD_UNPAUSE;
   else if (!focus && old_focus)
      cmd = EVENT_CMD_PAUSE;

   old_focus = focus;

   if (cmd != EVENT_CMD_NONE)
      event_command(cmd);

   if (runloop->is_paused == old_is_paused)
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
   driver_t *driver = driver_get_ptr();

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
static void check_stateslots(bool pressed_increase, bool pressed_decrease)
{
   char msg[PATH_MAX_LENGTH];
   settings_t *settings     = config_get_ptr();

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

   snprintf(msg, sizeof(msg), "State slot: %d",
         settings->state_slot);

   rarch_main_msg_queue_push(msg, 1, 180, true);

   RARCH_LOG("%s\n", msg);
}

static INLINE void setup_rewind_audio(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   /* Push audio ready to be played. */
   global->audio_data.rewind_ptr = global->audio_data.rewind_size;

   for (i = 0; i < global->audio_data.data_ptr; i += 2)
   {
      global->audio_data.rewind_buf[--global->audio_data.rewind_ptr] =
         global->audio_data.conv_outsamples[i + 1];

      global->audio_data.rewind_buf[--global->audio_data.rewind_ptr] =
         global->audio_data.conv_outsamples[i + 0];
   }

   global->audio_data.data_ptr = 0;
}

/**
 * check_rewind:
 * @pressed              : was rewind key pressed or held?
 *
 * Checks if rewind toggle/hold was being pressed and/or held.
 **/
static void check_rewind(bool pressed)
{
   static bool first = true;
   global_t *global  = global_get_ptr();

   if (global->rewind.frame_is_reverse)
   {
      /* We just rewound. Flush rewind audio buffer. */
      retro_flush_audio(global->audio_data.rewind_buf
            + global->audio_data.rewind_ptr,
            global->audio_data.rewind_size - global->audio_data.rewind_ptr);

      global->rewind.frame_is_reverse = false;
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
      runloop_t *runloop = rarch_main_get_ptr();

      if (state_manager_pop(global->rewind.state, &buf))
      {
         global->rewind.frame_is_reverse = true;
         setup_rewind_audio();

         rarch_main_msg_queue_push(RETRO_MSG_REWINDING, 0,
               runloop->is_paused ? 1 : 30, true);
         pretro_unserialize(buf, global->rewind.size);

         if (global->bsv.movie)
            bsv_movie_frame_rewind(global->bsv.movie);
      }
      else
         rarch_main_msg_queue_push(RETRO_MSG_REWIND_REACHED_END,
               0, 30, true);
   }
   else
   {
      static unsigned cnt      = 0;
      settings_t *settings     = config_get_ptr();

      cnt = (cnt + 1) % (settings->rewind_granularity ?
            settings->rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if ((cnt == 0) || global->bsv.movie)
      {
         void *state = NULL;
         state_manager_push_where(global->rewind.state, &state);

         RARCH_PERFORMANCE_INIT(rewind_serialize);
         RARCH_PERFORMANCE_START(rewind_serialize);
         pretro_serialize(state, global->rewind.size);
         RARCH_PERFORMANCE_STOP(rewind_serialize);

         state_manager_push_do(global->rewind.state);
      }
   }

   retro_set_rewind_callbacks();
}

/**
 * check_slowmotion:
 * @slowmotion_pressed   : was slow motion key pressed or held?
 *
 * Checks if slowmotion toggle/hold was being pressed and/or held.
 **/
static void check_slowmotion(bool slowmotion_pressed)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t *global         = global_get_ptr();

   runloop->is_slowmotion   = slowmotion_pressed;

   if (!runloop->is_slowmotion)
      return;

   if (settings->video.black_frame_insertion)
      rarch_render_cached_frame();

   rarch_main_msg_queue_push(global->rewind.frame_is_reverse ?
         "Slow motion rewind." : "Slow motion.", 0, 30, true);
}

static bool check_movie_init(void)
{
   char path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   bool ret                     = true;
   settings_t *settings         = config_get_ptr();
   global_t *global             = global_get_ptr();
   
   if (global->bsv.movie)
      return false;

   settings->rewind_granularity = 1;

   if (settings->state_slot > 0)
   {
      snprintf(path, sizeof(path), "%s%d.bsv",
            global->bsv.movie_path, settings->state_slot);
   }
   else
   {
      snprintf(path, sizeof(path), "%s.bsv",
            global->bsv.movie_path);
   }

   snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".", path);

   global->bsv.movie = bsv_movie_init(path, RARCH_MOVIE_RECORD);

   if (!global->bsv.movie)
      ret = false;

   rarch_main_msg_queue_push(global->bsv.movie ?
         msg : "Failed to start movie record.", 1, 180, true);

   if (global->bsv.movie)
      RARCH_LOG("Starting movie record to \"%s\".\n", path);
   else
      RARCH_ERR("Failed to start movie record.\n");

   return ret;
}

/**
 * check_movie_record:
 *
 * Checks if movie is being recorded.
 *
 * Returns: true (1) if movie is being recorded, otherwise false (0).
 **/
static bool check_movie_record(void)
{
   global_t *global = global_get_ptr();
   if (!global->bsv.movie)
      return false;

   rarch_main_msg_queue_push(
         RETRO_MSG_MOVIE_RECORD_STOPPING, 2, 180, true);
   RARCH_LOG(RETRO_LOG_MOVIE_RECORD_STOPPING);

   event_command(EVENT_CMD_BSV_MOVIE_DEINIT);

   return true;
}

/**
 * check_movie_playback:
 *
 * Checks if movie is being played.
 *
 * Returns: true (1) if movie is being played, otherwise false (0).
 **/
static bool check_movie_playback(void)
{
   global_t *global = global_get_ptr();
   if (!global->bsv.movie_end)
      return false;

   rarch_main_msg_queue_push(
         RETRO_MSG_MOVIE_PLAYBACK_ENDED, 1, 180, false);
   RARCH_LOG(RETRO_LOG_MOVIE_PLAYBACK_ENDED);

   event_command(EVENT_CMD_BSV_MOVIE_DEINIT);

   global->bsv.movie_end      = false;
   global->bsv.movie_playback = false;

   return true;
}

static bool check_movie(void)
{
   global_t *global = global_get_ptr();

   if (global->bsv.movie_playback)
      return check_movie_playback();
   if (!global->bsv.movie)
      return check_movie_init();
   return check_movie_record();
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
   char msg[PATH_MAX_LENGTH];
   const char *shader = NULL, *ext = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;
   global_t *global            = global_get_ptr();

   if (!global->shader_dir.list)
      return;

   if (pressed_next)
   {
      global->shader_dir.ptr = (global->shader_dir.ptr + 1) %
         global->shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      if (global->shader_dir.ptr == 0)
         global->shader_dir.ptr = global->shader_dir.list->size - 1;
      else
         global->shader_dir.ptr--;
   }
   else
      return;

   shader = global->shader_dir.list->elems[global->shader_dir.ptr].data;
   ext    = path_get_extension(shader);

   if (strcmp(ext, "glsl") == 0 || strcmp(ext, "glslp") == 0)
      type = RARCH_SHADER_GLSL;
   else if (strcmp(ext, "cg") == 0 || strcmp(ext, "cgp") == 0)
      type = RARCH_SHADER_CG;
   else
      return;

   snprintf(msg, sizeof(msg), "Shader #%u: \"%s\".",
         (unsigned)global->shader_dir.ptr, shader);
   rarch_main_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("Applying shader \"%s\".\n", shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("Failed to apply shader.\n");
}

#ifdef HAVE_MENU
static void do_state_check_menu_toggle(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   global_t *global   = global_get_ptr();

   if (runloop->is_menu)
   {
      if (global->main_is_init && !global->libretro_dummy)
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
      return;
   }

   rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
}
#endif

/**
 * do_pre_state_checks:
 *
 * Checks for state changes in this frame.
 *
 * Unlike do_state_checks(), this is performed for both
 * the menu and the regular loop.
 *
 * Returns: 0.
 **/
static int do_pre_state_checks(event_cmd_state_t *cmd)
{
   runloop_t *runloop        = rarch_main_get_ptr();
   global_t *global          = global_get_ptr();

   if (cmd->overlay_next_pressed)
      event_command(EVENT_CMD_OVERLAY_NEXT);

   if (!runloop->is_paused || runloop->is_menu)
   {
      if (cmd->fullscreen_toggle)
         event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
   }

   if (cmd->grab_mouse_pressed)
      event_command(EVENT_CMD_GRAB_MOUSE_TOGGLE);

#ifdef HAVE_MENU
   if (cmd->menu_pressed || (global->libretro_dummy))
      do_state_check_menu_toggle();
#endif

   return 0;
}

#ifdef HAVE_NETPLAY
static int do_netplay_state_checks(
      bool netplay_flip_pressed,
      bool fullscreen_toggle)
{
   if (netplay_flip_pressed)
      event_command(EVENT_CMD_NETPLAY_FLIP_PLAYERS);

   if (fullscreen_toggle)
      event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
   return 0;
}
#endif

static int do_pause_state_checks(
      bool pause_pressed,
      bool frameadvance_pressed,
      bool fullscreen_toggle_pressed,
      bool rewind_pressed)
{
   runloop_t *runloop        = rarch_main_get_ptr();
   bool check_is_oneshot     = frameadvance_pressed || rewind_pressed;

   if (!runloop || !runloop->is_paused)
      return 0;

   if (fullscreen_toggle_pressed)
   {
      event_command(EVENT_CMD_FULLSCREEN_TOGGLE);
      rarch_render_cached_frame();
   }

   if (!check_is_oneshot)
      return 1;

   return 0;
}

/**
 * do_state_checks:
 *
 * Checks for state changes in this frame.
 *
 * Returns: 1 if RetroArch is in pause mode, 0 otherwise.
 **/
static int do_state_checks(event_cmd_state_t *cmd)
{
   driver_t  *driver         = driver_get_ptr();
   runloop_t *runloop        = rarch_main_get_ptr();
   global_t  *global         = global_get_ptr();

   (void)driver;

   if (runloop->is_idle)
      return 1;

   if (cmd->screenshot_pressed)
      event_command(EVENT_CMD_TAKE_SCREENSHOT);

   if (cmd->mute_pressed)
      event_command(EVENT_CMD_AUDIO_MUTE_TOGGLE);

   if (cmd->osk_pressed)
   {
        driver_t *driver     = driver_get_ptr();

        if (driver)
           driver->keyboard_linefeed_enable = !driver->keyboard_linefeed_enable;
   }
      
   if (cmd->volume_up_pressed)
      event_command(EVENT_CMD_VOLUME_UP);
   else if (cmd->volume_down_pressed)
      event_command(EVENT_CMD_VOLUME_DOWN);

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      return do_netplay_state_checks(cmd->netplay_flip_pressed, cmd->fullscreen_toggle);
#endif

   check_pause(cmd->pause_pressed, cmd->frameadvance_pressed);

   if (do_pause_state_checks(
            cmd->pause_pressed,
            cmd->frameadvance_pressed,
            cmd->fullscreen_toggle,
            cmd->rewind_pressed))
      return 1;

   check_fast_forward_button(cmd->fastforward_pressed, cmd->hold_pressed, cmd->old_hold_pressed);
   check_stateslots(cmd->state_slot_increase, cmd->state_slot_decrease);

   if (cmd->save_state_pressed)
      event_command(EVENT_CMD_SAVE_STATE);
   else if (cmd->load_state_pressed)
      event_command(EVENT_CMD_LOAD_STATE);

   check_rewind(cmd->rewind_pressed);
   check_slowmotion(cmd->slowmotion_pressed);

   if (cmd->movie_record)
      check_movie();

   check_shader_dir(cmd->shader_next_pressed, cmd->shader_prev_pressed);

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

   return 0;
}

/**
 * time_to_exit:
 *
 * rarch_main_iterate() checks this to see if it's time to
 * exit out of the main loop.
 *
 * Reasons for exiting:
 * a) Shutdown environment callback was invoked.
 * b) Quit key was pressed.
 * c) Frame count exceeds or equals maximum amount of frames to run.
 * d) Video driver no longer alive.
 * e) End of BSV movie and BSV EOF exit is true. (TODO/FIXME - explain better)
 *
 * Returns: 1 if any of the above conditions are true, otherwise 0.
 **/
static INLINE int time_to_exit(event_cmd_state_t *cmd)
{
   runloop_t *runloop            = rarch_main_get_ptr();
   global_t  *global             = global_get_ptr();
   bool shutdown_pressed         = global->system.shutdown;
   bool video_alive              = video_driver_is_alive();
   bool movie_end                = (global->bsv.movie_end && global->bsv.eof_exit);
   bool frame_count_end          = (runloop->frames.video.max && 
         runloop->frames.video.count >= runloop->frames.video.max);

   if (shutdown_pressed || cmd->quit_key_pressed || frame_count_end || movie_end
         || !video_alive)
      return 1;
   return 0;
}

/**
 * rarch_update_frame_time:
 *
 * Updates frame timing if frame timing callback is in use by the core.
 **/
static void rarch_update_frame_time(void)
{
   runloop_t *runloop       = rarch_main_get_ptr();
   driver_t *driver         = driver_get_ptr();
   settings_t *settings     = config_get_ptr();
   retro_time_t curr_time   = rarch_get_time_usec();
   global_t  *global        = global_get_ptr();
   retro_time_t delta       = curr_time - global->system.frame_time_last;
   bool is_locked_fps       = runloop->is_paused || driver->nonblock_state;

   is_locked_fps         |= !!driver->recording_data;

   if (!global->system.frame_time_last || is_locked_fps)
      delta = global->system.frame_time.reference;

   if (!is_locked_fps && runloop->is_slowmotion)
      delta /= settings->slowmotion_ratio;

   global->system.frame_time_last = curr_time;

   if (is_locked_fps)
      global->system.frame_time_last = 0;

   global->system.frame_time.callback(delta);
}


/**
 * rarch_limit_frame_time:
 *
 * Limit frame time if fast forward ratio throttle is enabled.
 **/
static void rarch_limit_frame_time(void)
{
   retro_time_t target      = 0;
   retro_time_t to_sleep_ms = 0;
   runloop_t *runloop       = rarch_main_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t  *global        = global_get_ptr();
   retro_time_t current     = rarch_get_time_usec();
   double effective_fps     = global->system.av_info.timing.fps 
      * settings->fastforward_ratio;
   double mft_f             = 1000000.0f / effective_fps;

   runloop->frames.limit.minimum_time = (retro_time_t) roundf(mft_f);

   target        = runloop->frames.limit.last_time + 
                   runloop->frames.limit.minimum_time;
   to_sleep_ms   = (target - current) / 1000;

   if (to_sleep_ms <= 0)
   {
      runloop->frames.limit.last_time = rarch_get_time_usec();
      return;
   }

   rarch_sleep((unsigned int)to_sleep_ms);

   /* Combat jitter a bit. */
   runloop->frames.limit.last_time += 
      runloop->frames.limit.minimum_time;
}

/**
 * check_block_hotkey:
 * @enable_hotkey        : Is hotkey enable key enabled?
 *
 * Checks if 'hotkey enable' key is pressed.
 **/
static bool check_block_hotkey(bool enable_hotkey)
{
   bool use_hotkey_enable;
   settings_t *settings             = config_get_ptr();
   driver_t *driver                 = driver_get_ptr();
   const struct retro_keybind *bind = 
      &settings->input.binds[0][RARCH_ENABLE_HOTKEY];
   const struct retro_keybind *autoconf_bind = 
      &settings->input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   driver->block_hotkey             = driver->block_input;

   /* If we haven't bound anything to this, 
    * always allow hotkeys. */
   use_hotkey_enable                = 
      bind->key != RETROK_UNKNOWN ||
      bind->joykey != NO_BTN ||
      bind->joyaxis != AXIS_NONE ||
      autoconf_bind->key != RETROK_UNKNOWN ||
      autoconf_bind->joykey != NO_BTN ||
      autoconf_bind->joyaxis != AXIS_NONE;

   driver->block_hotkey             = driver->block_input ||
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
static INLINE retro_input_t input_keys_pressed(void)
{
   unsigned i;
   retro_input_t ret        = 0;
   driver_t *driver         = driver_get_ptr();
   settings_t *settings     = config_get_ptr();
   global_t   *global       = global_get_ptr();
   const struct retro_keybind *binds[MAX_USERS] = {
      settings->input.binds[0],
      settings->input.binds[1],
      settings->input.binds[2],
      settings->input.binds[3],
      settings->input.binds[4],
      settings->input.binds[5],
      settings->input.binds[6],
      settings->input.binds[7],
      settings->input.binds[8],
      settings->input.binds[9],
      settings->input.binds[10],
      settings->input.binds[11],
      settings->input.binds[12],
      settings->input.binds[13],
      settings->input.binds[14],
      settings->input.binds[15],
   };

   if (!driver->input || !driver->input_data)
      return 0;

   global->turbo_count++;

   driver->block_libretro_input = check_block_hotkey(
         input_driver_key_pressed(RARCH_ENABLE_HOTKEY));

   for (i = 0; i < settings->input.max_users; i++)
   {
      input_push_analog_dpad(settings->input.binds[i],
            settings->input.analog_dpad_mode[i]);
      input_push_analog_dpad(settings->input.autoconf_binds[i],
            settings->input.analog_dpad_mode[i]);

      global->turbo_frame_enable[i] = 0;
   }

   if (!driver->block_libretro_input)
   {
      for (i = 0; i < settings->input.max_users; i++)
         global->turbo_frame_enable[i] = input_driver_state(binds, 
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

/**
 * input_flush:
 * @input                : input sample for this frame
 *
 * Resets input sample.
 *
 * Returns: always true (1).
 **/
static bool input_flush(retro_input_t *input)
{
   runloop_t *runloop = rarch_main_get_ptr();

   *input = 0;

   /* If core was paused before entering menu, evoke
    * pause toggle to wake it up. */
   if (runloop->is_paused)
      BIT64_SET(*input, RARCH_PAUSE_TOGGLE);

   return true;
}

/**
 * rarch_main_load_dummy_core:
 *
 * Quits out of RetroArch main loop.
 *
 * On special case, loads dummy core 
 * instead of exiting RetroArch completely.
 * Aborts core shutdown if invoked.
 *
 * Returns: -1 if we are about to quit, otherwise 0.
 **/
static int rarch_main_iterate_quit(void)
{
   settings_t *settings     = config_get_ptr();
   global_t   *global       = global_get_ptr();

   if (global->core_shutdown_initiated
         && settings->load_dummy_on_core_shutdown)
   {
      if (!event_command(EVENT_CMD_PREPARE_DUMMY))
         return -1;

      global->core_shutdown_initiated = false;

      return 0;
   }

   return -1;
}

#ifdef HAVE_OVERLAY
static void rarch_main_iterate_linefeed_overlay(void)
{
   static char prev_overlay_restore = false;
   driver_t *driver                 = driver_get_ptr();

   if (driver->osk_enable && !driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = false;
      prev_overlay_restore  = true;
      event_command(EVENT_CMD_OVERLAY_DEINIT);
      return;
   }
   else if (!driver->osk_enable && driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = true;
      prev_overlay_restore  = false;
      event_command(EVENT_CMD_OVERLAY_INIT);
      return;
   }
   else if (prev_overlay_restore)
   {
      event_command(EVENT_CMD_OVERLAY_INIT);
      prev_overlay_restore = false;
   }
}
#endif

const char *rarch_main_msg_queue_pull(void)
{
   runloop_t *runloop = rarch_main_get_ptr();

   if (!runloop)
      return NULL;
   return msg_queue_pull(runloop->msg_queue);
}

void rarch_main_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop->msg_queue)
      return;

   if (flush)
      msg_queue_clear(runloop->msg_queue);
   msg_queue_push(runloop->msg_queue, msg, prio, duration);
}

void rarch_main_msg_queue_free(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return;

   if (runloop->msg_queue)
      msg_queue_free(runloop->msg_queue);
   runloop->msg_queue = NULL;
}

void rarch_main_msg_queue_init(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return;

   if (!runloop->msg_queue)
      rarch_assert(runloop->msg_queue = msg_queue_new(8));
}

global_t *global_get_ptr(void)
{
   return g_extern;
}

runloop_t *rarch_main_get_ptr(void)
{
   return g_runloop;
}

void rarch_main_state_free(void)
{
   runloop_t *runloop = rarch_main_get_ptr();

   if (!runloop)
      return;

   free(runloop);
}

void rarch_main_global_free(void)
{
   global_t *global = NULL;
   
   event_command(EVENT_CMD_TEMPORARY_CONTENT_DEINIT);
   event_command(EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT);
   event_command(EVENT_CMD_RECORD_DEINIT);
   event_command(EVENT_CMD_LOG_FILE_DEINIT);

   global = global_get_ptr();

   if (!global)
      return;

   free(global);
}

bool rarch_main_verbosity(void)
{
   global_t *global = global_get_ptr();
   if (!global)
      return false;
   return global->verbosity;
}

FILE *rarch_main_log_file(void)
{
   global_t *global = global_get_ptr();
   if (!global)
      return NULL;
   return global->log_file;
}

static global_t *rarch_main_global_new(void)
{
   global_t *global = (global_t*)calloc(1, sizeof(global_t));

   if (!global)
      return NULL;

   return global;
}

static runloop_t *rarch_main_state_init(void)
{
   runloop_t *runloop = (runloop_t*)calloc(1, sizeof(runloop_t));

   if (!runloop)
      return NULL;

   return runloop;
}

void rarch_main_clear_state(void)
{
   driver_clear_state();

   rarch_main_state_free();
   g_runloop = rarch_main_state_init();

   rarch_main_global_free();
   g_extern  = rarch_main_global_new();
}

bool rarch_main_is_idle(void)
{
   runloop_t *runloop = rarch_main_get_ptr();
   if (!runloop)
      return false;
   return runloop->is_idle;
}

static void rarch_main_cmd_get_state(event_cmd_state_t *cmd,
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (!cmd)
      return;

   cmd->fullscreen_toggle           = BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY);
   cmd->overlay_next_pressed        = BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT);
   cmd->grab_mouse_pressed          = BIT64_GET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE);
#ifdef HAVE_MENU
   cmd->menu_pressed                = BIT64_GET(trigger_input, RARCH_MENU_TOGGLE);
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

/**
 * rarch_main_iterate:
 *
 * Run Libretro core in RetroArch for one frame.
 *
 * Returns: 0 on success, 1 if we have to wait until button input in order
 * to wake up the loop, -1 if we forcibly quit out of the RetroArch iteration loop. 
 **/
int rarch_main_iterate(void)
{
   unsigned i;
   retro_input_t trigger_input;
   event_cmd_state_t    cmd        = {0};
   runloop_t *runloop              = rarch_main_get_ptr();
   int ret                         = 0;
   static retro_input_t last_input = 0;
   retro_input_t old_input         = last_input;
   retro_input_t input             = input_keys_pressed();
   last_input                      = input;
   driver_t *driver                = driver_get_ptr();
   settings_t *settings            = config_get_ptr();
   global_t   *global              = global_get_ptr();

   if (driver->flushing_input)
      driver->flushing_input = (input) ? input_flush(&input) : false;

   trigger_input = input & ~old_input;

   rarch_main_cmd_get_state(&cmd, input, old_input, trigger_input);

   if (time_to_exit(&cmd))
      return rarch_main_iterate_quit();

   if (global->system.frame_time.callback)
      rarch_update_frame_time();

   do_pre_state_checks(&cmd);

#ifdef HAVE_OVERLAY
   rarch_main_iterate_linefeed_overlay();
#endif
   
   rarch_main_data_iterate();

#ifdef HAVE_MENU
   if (runloop->is_menu)
   {
      menu_handle_t *menu = menu_driver_get_ptr();
      if (menu)
         if (menu_iterate(input, old_input, trigger_input) == -1)
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);

      if (!input && settings->menu.pause_libretro)
        ret = 1;
      goto success;
   }
#endif

   if (global->exec)
   {
      global->exec = false;
      return rarch_main_iterate_quit();
   }

   if (do_state_checks(&cmd))
   {
      /* RetroArch has been paused */
      driver->retro_ctx.poll_cb();
      rarch_sleep(10);

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

   if (global->system.camera_callback.caps)
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
      rarch_sleep(settings->video.frame_delay);


   /* Run libretro for one frame. */
   pretro_run();

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

success:
   if (settings->fastforward_ratio_throttle_enable)
      rarch_limit_frame_time();

   return ret;
}
