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

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

struct runloop g_runloop;

/* Convenience macros. */
#define check_oneshot_func(trigger_input) (check_is_oneshot(BIT64_GET(trigger_input, RARCH_FRAMEADVANCE), BIT64_GET(trigger_input, RARCH_REWIND)))
#define check_slowmotion_func(input)      (check_slowmotion(BIT64_GET(input, RARCH_SLOWMOTION)))
#define check_quit_key_func(input)        (BIT64_GET(input, RARCH_QUIT_KEY))
#define check_pause_func(input)           (check_pause(BIT64_GET(input, RARCH_PAUSE_TOGGLE), BIT64_GET(input, RARCH_FRAMEADVANCE)))
#define check_stateslots_func(trigger_input) (check_stateslots(BIT64_GET(trigger_input, RARCH_STATE_SLOT_PLUS), BIT64_GET(trigger_input, RARCH_STATE_SLOT_MINUS)))
#define check_rewind_func(input)          (check_rewind(BIT64_GET(input, RARCH_REWIND)))
#define check_fast_forward_button_func(input, old_input, trigger_input) (check_fast_forward_button(BIT64_GET(trigger_input, RARCH_FAST_FORWARD_KEY), BIT64_GET(input, RARCH_FAST_FORWARD_HOLD_KEY), BIT64_GET(old_input, RARCH_FAST_FORWARD_HOLD_KEY)))
#define check_enter_menu_func(input)      (BIT64_GET(input, RARCH_MENU_TOGGLE))
#define check_shader_dir_func(trigger_input) (check_shader_dir(BIT64_GET(trigger_input, RARCH_SHADER_NEXT), BIT64_GET(trigger_input, RARCH_SHADER_PREV)))

/**
 * set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void set_volume(float gain)
{
   char msg[PATH_MAX_LENGTH];

   g_settings.audio.volume += gain;
   g_settings.audio.volume = max(g_settings.audio.volume, -80.0f);
   g_settings.audio.volume = min(g_settings.audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", g_settings.audio.volume);
   rarch_main_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);

   g_extern.audio_data.volume_gain = db_to_gain(g_settings.audio.volume);
}

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
static bool check_pause(bool pressed, bool frameadvance_pressed)
{
   static bool old_focus    = true;
   bool focus               = true;
   bool old_is_paused       = g_runloop.is_paused;
   unsigned cmd             = RARCH_CMD_NONE;

   /* FRAMEADVANCE will set us into pause mode. */
   pressed |= !g_runloop.is_paused && frameadvance_pressed;

   if (g_settings.pause_nonactive)
      focus = video_driver_has_focus();

   if (focus && pressed)
      cmd = RARCH_CMD_PAUSE_TOGGLE;
   else if (focus && !old_focus)
      cmd = RARCH_CMD_UNPAUSE;
   else if (!focus && old_focus)
      cmd = RARCH_CMD_PAUSE;

   old_focus = focus;

   if (cmd != RARCH_CMD_NONE)
      rarch_main_command(cmd);

   if (g_runloop.is_paused == old_is_paused)
      return false;

   return true;
}

/**
 * check_is_oneshot:
 * @oneshot_pressed      : is this a oneshot frame?
 * @rewind_pressed       : was rewind key pressed?
 *
 * Checks if the current frame is one-shot frame type.
 *
 * Rewind buttons works like FRAMEREWIND when paused.
 * We will one-shot in that case.
 *
 * Returns: true if libretro frame is one-shot, otherwise false..
 *
 **/
static INLINE bool check_is_oneshot(bool oneshot_pressed, bool rewind_pressed)
{
   return (oneshot_pressed | rewind_pressed);
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

   /* Save state slots */
   if (pressed_increase)
      g_settings.state_slot++;
   else if (pressed_decrease)
   {
      if (g_settings.state_slot > 0)
         g_settings.state_slot--;
   }
   else
      return;

   snprintf(msg, sizeof(msg), "State slot: %d",
         g_settings.state_slot);

   rarch_main_msg_queue_push(msg, 1, 180, true);

   RARCH_LOG("%s\n", msg);
}

static INLINE void setup_rewind_audio(void)
{
   unsigned i;

   /* Push audio ready to be played. */
   g_extern.audio_data.rewind_ptr = g_extern.audio_data.rewind_size;

   for (i = 0; i < g_extern.audio_data.data_ptr; i += 2)
   {
      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] =
         g_extern.audio_data.conv_outsamples[i + 1];

      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] =
         g_extern.audio_data.conv_outsamples[i + 0];
   }

   g_extern.audio_data.data_ptr = 0;
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

   if (g_extern.rewind.frame_is_reverse)
   {
      /* We just rewound. Flush rewind audio buffer. */
      retro_flush_audio(g_extern.audio_data.rewind_buf
            + g_extern.audio_data.rewind_ptr,
            g_extern.audio_data.rewind_size - g_extern.audio_data.rewind_ptr);

      g_extern.rewind.frame_is_reverse = false;
   }

   if (first)
   {
      first = false;
      return;
   }

   if (!g_extern.rewind.state)
      return;

   if (pressed)
   {
      const void *buf = NULL;

      if (state_manager_pop(g_extern.rewind.state, &buf))
      {
         g_extern.rewind.frame_is_reverse = true;
         setup_rewind_audio();

         rarch_main_msg_queue_push(RETRO_MSG_REWINDING, 0,
               g_runloop.is_paused ? 1 : 30, true);
         pretro_unserialize(buf, g_extern.rewind.size);

         if (g_extern.bsv.movie)
            bsv_movie_frame_rewind(g_extern.bsv.movie);
      }
      else
         rarch_main_msg_queue_push(RETRO_MSG_REWIND_REACHED_END,
               0, 30, true);
   }
   else
   {
      static unsigned cnt = 0;

      cnt = (cnt + 1) % (g_settings.rewind_granularity ?
            g_settings.rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if ((cnt == 0) || g_extern.bsv.movie)
      {
         void *state = NULL;
         state_manager_push_where(g_extern.rewind.state, &state);

         RARCH_PERFORMANCE_INIT(rewind_serialize);
         RARCH_PERFORMANCE_START(rewind_serialize);
         pretro_serialize(state, g_extern.rewind.size);
         RARCH_PERFORMANCE_STOP(rewind_serialize);

         state_manager_push_do(g_extern.rewind.state);
      }
   }

   retro_set_rewind_callbacks();
}

/**
 * check_slowmotion:
 * @pressed              : was slow motion key pressed or held?
 *
 * Checks if slowmotion toggle/hold was being pressed and/or held.
 **/
static void check_slowmotion(bool pressed)
{
   g_runloop.is_slowmotion = pressed;

   if (!g_runloop.is_slowmotion)
      return;

   if (g_settings.video.black_frame_insertion)
      rarch_render_cached_frame();

   rarch_main_msg_queue_push(g_extern.rewind.frame_is_reverse ?
         "Slow motion rewind." : "Slow motion.", 0, 30, true);
}

static bool check_movie_init(void)
{
   char path[PATH_MAX_LENGTH], msg[PATH_MAX_LENGTH];
   bool ret = true;
   
   if (g_extern.bsv.movie)
      return false;

   g_settings.rewind_granularity = 1;

   if (g_settings.state_slot > 0)
   {
      snprintf(path, sizeof(path), "%s%d.bsv",
            g_extern.bsv.movie_path, g_settings.state_slot);
   }
   else
   {
      snprintf(path, sizeof(path), "%s.bsv",
            g_extern.bsv.movie_path);
   }

   snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".", path);

   g_extern.bsv.movie = bsv_movie_init(path, RARCH_MOVIE_RECORD);

   if (!g_extern.bsv.movie)
      ret = false;

   rarch_main_msg_queue_push(g_extern.bsv.movie ?
         msg : "Failed to start movie record.", 1, 180, true);

   if (g_extern.bsv.movie)
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
   if (!g_extern.bsv.movie)
      return false;

   rarch_main_msg_queue_push(
         RETRO_MSG_MOVIE_RECORD_STOPPING, 2, 180, true);
   RARCH_LOG(RETRO_LOG_MOVIE_RECORD_STOPPING);

   rarch_main_command(RARCH_CMD_BSV_MOVIE_DEINIT);

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
   if (!g_extern.bsv.movie_end)
      return false;

   rarch_main_msg_queue_push(
         RETRO_MSG_MOVIE_PLAYBACK_ENDED, 1, 180, false);
   RARCH_LOG(RETRO_LOG_MOVIE_PLAYBACK_ENDED);

   rarch_main_command(RARCH_CMD_BSV_MOVIE_DEINIT);

   g_extern.bsv.movie_end = false;
   g_extern.bsv.movie_playback = false;

   return true;
}

static bool check_movie(void)
{
   if (g_extern.bsv.movie_playback)
      return check_movie_playback();
   if (!g_extern.bsv.movie)
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

   if (!g_extern.shader_dir.list)
      return;

   if (pressed_next)
   {
      g_extern.shader_dir.ptr = (g_extern.shader_dir.ptr + 1) %
         g_extern.shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      if (g_extern.shader_dir.ptr == 0)
         g_extern.shader_dir.ptr = g_extern.shader_dir.list->size - 1;
      else
         g_extern.shader_dir.ptr--;
   }
   else
      return;

   shader = g_extern.shader_dir.list->elems[g_extern.shader_dir.ptr].data;
   ext    = path_get_extension(shader);

   if (strcmp(ext, "glsl") == 0 || strcmp(ext, "glslp") == 0)
      type = RARCH_SHADER_GLSL;
   else if (strcmp(ext, "cg") == 0 || strcmp(ext, "cgp") == 0)
      type = RARCH_SHADER_CG;
   else
      return;

   snprintf(msg, sizeof(msg), "Shader #%u: \"%s\".",
         (unsigned)g_extern.shader_dir.ptr, shader);
   rarch_main_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("Applying shader \"%s\".\n", shader);

   if (!video_driver_set_shader(type, shader))
      RARCH_WARN("Failed to apply shader.\n");
}

/**
 * check_cheats:
 * @trigger_input        : difference' input sample - difference
 *
 * Checks if any one of the cheat keys has been pressed for this frame: 
 * a) Next cheat index.
 * b) Previous cheat index.
 * c) Toggle on/off of current cheat index.
 **/
static void check_cheats(retro_input_t trigger_input)
{
   if (BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_PLUS))
      cheat_manager_index_next(g_extern.cheat);
   else if (BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_MINUS))
      cheat_manager_index_prev(g_extern.cheat);
   else if (BIT64_GET(trigger_input, RARCH_CHEAT_TOGGLE))
      cheat_manager_toggle(g_extern.cheat);
}

#ifdef HAVE_MENU
static void do_state_check_menu_toggle(void)
{
   if (g_runloop.is_menu)
   {
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
      return;
   }

   rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
}
#endif

/**
 * do_pre_state_checks:
 * @input                : input sample for this frame
 * @old_input            : input sample of the previous frame
 * @trigger_input        : difference' input sample - difference
 *                         between 'input' and 'old_input'
 *
 * Checks for state changes in this frame.
 *
 * Unlike do_state_checks(), this is performed for both
 * the menu and the regular loop.
 *
 * Returns: 0.
 **/
static int do_pre_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT))
      rarch_main_command(RARCH_CMD_OVERLAY_NEXT);

   if (!g_runloop.is_paused || g_runloop.is_menu)
   {
      if (BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
         rarch_main_command(RARCH_CMD_FULLSCREEN_TOGGLE);
   }

   if (BIT64_GET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE))
      rarch_main_command(RARCH_CMD_GRAB_MOUSE_TOGGLE);

#ifdef HAVE_MENU
   if (check_enter_menu_func(trigger_input) || (g_extern.libretro_dummy))
      do_state_check_menu_toggle();
#endif

   return 0;
}

#ifdef HAVE_NETPLAY
static int do_netplay_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (BIT64_GET(trigger_input, RARCH_NETPLAY_FLIP))
      rarch_main_command(RARCH_CMD_NETPLAY_FLIP_PLAYERS);

   if (BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
      rarch_main_command(RARCH_CMD_FULLSCREEN_TOGGLE);
   return 0;
}
#endif

static int do_pause_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   check_pause_func(trigger_input);

   if (!g_runloop.is_paused)
      return 0;

   if (BIT64_GET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
   {
      rarch_main_command(RARCH_CMD_FULLSCREEN_TOGGLE);
      rarch_render_cached_frame();
   }

   if (!check_oneshot_func(trigger_input))
      return 1;

   return 0;
}

/**
 * do_state_checks:
 * @input                : input sample for this frame
 * @old_input            : input sample of the previous frame
 * @trigger_input        : difference' input sample - difference
 *                         between 'input' and 'old_input'
 *
 * Checks for state changes in this frame.
 *
 * Returns: 1 if RetroArch is in pause mode, 0 otherwise.
 **/
static int do_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   driver_t *driver = driver_get_ptr();

   (void)driver;

   if (g_runloop.is_idle)
      return 1;

   if (BIT64_GET(trigger_input, RARCH_SCREENSHOT))
      rarch_main_command(RARCH_CMD_TAKE_SCREENSHOT);

   if (BIT64_GET(trigger_input, RARCH_MUTE))
      rarch_main_command(RARCH_CMD_AUDIO_MUTE_TOGGLE);

   if (BIT64_GET(input, RARCH_VOLUME_UP))
      set_volume(0.5f);
   else if (BIT64_GET(input, RARCH_VOLUME_DOWN))
      set_volume(-0.5f);

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      return do_netplay_state_checks(input,
            old_input, trigger_input);
#endif

   if (do_pause_state_checks(input, old_input,
            trigger_input))
      return 1;

   check_fast_forward_button_func(input, old_input, trigger_input);

   check_stateslots_func(trigger_input);

   if (BIT64_GET(trigger_input, RARCH_SAVE_STATE_KEY))
      rarch_main_command(RARCH_CMD_SAVE_STATE);
   else if (BIT64_GET(trigger_input, RARCH_LOAD_STATE_KEY))
      rarch_main_command(RARCH_CMD_LOAD_STATE);

   check_rewind_func(input);

   check_slowmotion_func(input);

   if (BIT64_GET(trigger_input, RARCH_MOVIE_RECORD_TOGGLE))
      check_movie();

   check_shader_dir_func(trigger_input);

   if (BIT64_GET(trigger_input, RARCH_DISK_EJECT_TOGGLE))
      rarch_main_command(RARCH_CMD_DISK_EJECT_TOGGLE);
   else if (BIT64_GET(trigger_input, RARCH_DISK_NEXT))
      rarch_main_command(RARCH_CMD_DISK_NEXT);
   else if (BIT64_GET(trigger_input, RARCH_DISK_PREV))
      rarch_main_command(RARCH_CMD_DISK_PREV);	  

   if (BIT64_GET(trigger_input, RARCH_RESET))
      rarch_main_command(RARCH_CMD_RESET);

   if (g_extern.cheat)
      check_cheats(trigger_input);

   return 0;
}

/**
 * time_to_exit:
 * @input                : input sample for this frame
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
static INLINE int time_to_exit(retro_input_t input)
{
   if (
         g_extern.system.shutdown
         || check_quit_key_func(input)
         || (g_runloop.frames.video.max && 
            g_runloop.frames.video.count >= g_runloop.frames.video.max)
         || (g_extern.bsv.movie_end && g_extern.bsv.eof_exit)
         || !video_driver_is_alive()
      )
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
   driver_t *driver = driver_get_ptr();
   retro_time_t curr_time = rarch_get_time_usec();
   retro_time_t delta     = curr_time - g_extern.system.frame_time_last;
   bool is_locked_fps     = g_runloop.is_paused || driver->nonblock_state;
   is_locked_fps         |= !!driver->recording_data;

   if (!g_extern.system.frame_time_last || is_locked_fps)
      delta = g_extern.system.frame_time.reference;

   if (!is_locked_fps && g_runloop.is_slowmotion)
      delta /= g_settings.slowmotion_ratio;

   g_extern.system.frame_time_last = curr_time;

   if (is_locked_fps)
      g_extern.system.frame_time_last = 0;

   g_extern.system.frame_time.callback(delta);
}


/**
 * rarch_limit_frame_time:
 *
 * Limit frame time if fast forward ratio throttle is enabled.
 **/
static void rarch_limit_frame_time(void)
{
   double effective_fps, mft_f;
   retro_time_t current, target = 0, to_sleep_ms = 0;

   current       = rarch_get_time_usec();
   effective_fps = g_extern.system.av_info.timing.fps 
      * g_settings.fastforward_ratio;
   mft_f         = 1000000.0f / effective_fps;

   g_runloop.frames.limit.minimum_time = (retro_time_t) roundf(mft_f);

   target        = g_runloop.frames.limit.last_time + 
                   g_runloop.frames.limit.minimum_time;
   to_sleep_ms   = (target - current) / 1000;

   if (to_sleep_ms <= 0)
   {
      g_runloop.frames.limit.last_time = rarch_get_time_usec();
      return;
   }

   rarch_sleep((unsigned int)to_sleep_ms);

   /* Combat jitter a bit. */
   g_runloop.frames.limit.last_time += 
      g_runloop.frames.limit.minimum_time;
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
   static const struct retro_keybind *bind = 
      &g_settings.input.binds[0][RARCH_ENABLE_HOTKEY];
   static const struct retro_keybind *autoconf_bind = 
      &g_settings.input.autoconf_binds[0][RARCH_ENABLE_HOTKEY];
   driver_t *driver = driver_get_ptr();

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   driver->block_hotkey = driver->block_input;

   // If we haven't bound anything to this, always allow hotkeys.
   use_hotkey_enable = bind->key != RETROK_UNKNOWN ||
      bind->joykey != NO_BTN ||
      bind->joyaxis != AXIS_NONE ||
      autoconf_bind->key != RETROK_UNKNOWN ||
      autoconf_bind->joykey != NO_BTN ||
      autoconf_bind->joyaxis != AXIS_NONE;

   driver->block_hotkey = driver->block_input ||
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
   static const struct retro_keybind *binds[MAX_USERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
      g_settings.input.binds[5],
      g_settings.input.binds[6],
      g_settings.input.binds[7],
      g_settings.input.binds[8],
      g_settings.input.binds[9],
      g_settings.input.binds[10],
      g_settings.input.binds[11],
      g_settings.input.binds[12],
      g_settings.input.binds[13],
      g_settings.input.binds[14],
      g_settings.input.binds[15],
   };
   retro_input_t ret = 0;
   driver_t *driver = driver_get_ptr();

   if (!driver->input || !driver->input_data)
      return 0;

   g_extern.turbo_count++;

   driver->block_libretro_input = check_block_hotkey(
         driver->input->key_pressed(driver->input_data,
            RARCH_ENABLE_HOTKEY));

   for (i = 0; i < g_settings.input.max_users; i++)
   {
      input_push_analog_dpad(g_settings.input.binds[i],
            g_settings.input.analog_dpad_mode[i]);
      input_push_analog_dpad(g_settings.input.autoconf_binds[i],
            g_settings.input.analog_dpad_mode[i]);

      g_extern.turbo_frame_enable[i] = 0;
   }

   if (!driver->block_libretro_input)
   {
      for (i = 0; i < g_settings.input.max_users; i++)
      {
         g_extern.turbo_frame_enable[i] = 
            driver->input->input_state(driver->input_data, binds, i,
                  RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
      }
   }

   ret = input_driver_keys_pressed();

   for (i = 0; i < g_settings.input.max_users; i++)
   {
      input_pop_analog_dpad(g_settings.input.binds[i]);
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);
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
   *input = 0;

   /* If core was paused before entering menu, evoke
    * pause toggle to wake it up. */
   if (g_runloop.is_paused)
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
   if (g_extern.core_shutdown_initiated
         && g_settings.load_dummy_on_core_shutdown)
   {
      if (!rarch_main_command(RARCH_CMD_PREPARE_DUMMY))
         return -1;

      g_extern.core_shutdown_initiated = false;

      return 0;
   }

   return -1;
}

#ifdef HAVE_OVERLAY
static void rarch_main_iterate_linefeed_overlay(void)
{
   static char prev_overlay_restore = false;
   driver_t *driver = driver_get_ptr();

   if (driver->osk_enable && !driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = false;
      prev_overlay_restore  = true;
      rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
      return;
   }
   else if (!driver->osk_enable && driver->keyboard_linefeed_enable)
   {
      driver->osk_enable    = true;
      prev_overlay_restore  = false;
      rarch_main_command(RARCH_CMD_OVERLAY_INIT);
      return;
   }
   else if (prev_overlay_restore)
   {
      rarch_main_command(RARCH_CMD_OVERLAY_INIT);
      prev_overlay_restore = false;
   }
}
#endif

const char *rarch_main_msg_queue_pull(void)
{
   return msg_queue_pull(g_runloop.msg_queue);
}

void rarch_main_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   if (!g_runloop.msg_queue)
      return;

   if (flush)
      msg_queue_clear(g_runloop.msg_queue);
   msg_queue_push(g_runloop.msg_queue, msg, prio, duration);
}

void rarch_main_msg_queue_free(void)
{
   if (g_runloop.msg_queue)
      msg_queue_free(g_runloop.msg_queue);
   g_runloop.msg_queue = NULL;
}

void rarch_main_msg_queue_init(void)
{
   if (!g_runloop.msg_queue)
      rarch_assert(g_runloop.msg_queue = msg_queue_new(8));
}

runloop_t *rarch_main_get_ptr(void)
{
   return &g_runloop;
}

void rarch_main_clear_state(void)
{
   memset(&g_runloop, 0, sizeof(g_runloop));
}

bool rarch_main_is_idle(void)
{
   return g_runloop.is_idle;
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
   int ret                         = 0;
   static retro_input_t last_input = 0;
   retro_input_t old_input         = last_input;
   retro_input_t input             = input_keys_pressed();
   last_input                      = input;
   driver_t *driver                = driver_get_ptr();

   if (driver->flushing_input)
      driver->flushing_input = (input) ? input_flush(&input) : false;

   trigger_input = input & ~old_input;

   if (time_to_exit(input))
      return rarch_main_iterate_quit();

   if (g_extern.system.frame_time.callback)
      rarch_update_frame_time();

   do_pre_state_checks(input, old_input, trigger_input);

#ifdef HAVE_OVERLAY
   rarch_main_iterate_linefeed_overlay();
#endif
   
   rarch_main_data_iterate();

#ifdef HAVE_MENU
   if (g_runloop.is_menu)
   {
      menu_handle_t *menu = menu_driver_resolve();
      if (menu)
         if (menu_iterate(input, old_input, trigger_input) == -1)
            rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);

      if (!input && g_settings.menu.pause_libretro)
        ret = 1;
      goto success;
   }
#endif

   if (g_extern.exec)
   {
      g_extern.exec = false;
      return rarch_main_iterate_quit();
   }

   if (do_state_checks(input, old_input, trigger_input))
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

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_start(g_extern.bsv.movie);

   if (g_extern.system.camera_callback.caps)
      driver_camera_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < g_settings.input.max_users; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_push_analog_dpad(g_settings.input.binds[i],
            g_settings.input.analog_dpad_mode[i]);
      input_push_analog_dpad(g_settings.input.autoconf_binds[i],
            g_settings.input.analog_dpad_mode[i]);
   }

   if ((g_settings.video.frame_delay > 0) && !driver->nonblock_state)
      rarch_sleep(g_settings.video.frame_delay);


   /* Run libretro for one frame. */
   pretro_run();

   for (i = 0; i < g_settings.input.max_users; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_pop_analog_dpad(g_settings.input.binds[i]);
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);
   }

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_end(g_extern.bsv.movie);

#ifdef HAVE_NETPLAY
   if (driver->netplay_data)
      netplay_post_frame((netplay_t*)driver->netplay_data);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

success:
   if (g_settings.fastforward_ratio_throttle_enable)
      rarch_limit_frame_time();

   return ret;
}
