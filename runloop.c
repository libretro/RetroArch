/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include "general.h"
#include "performance.h"
#include "input/input_common.h"
#include "intl/intl.h"

#ifdef HAVE_MENU
#include "frontend/menu/menu_common.h"
#endif

static void set_volume(float gain)
{
   char msg[256];

   g_settings.audio.volume += gain;
   g_settings.audio.volume = max(g_settings.audio.volume, -80.0f);
   g_settings.audio.volume = min(g_settings.audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", g_settings.audio.volume);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_extern.audio_data.volume_gain = db_to_gain(g_settings.audio.volume);
}

static void check_grab_mouse_toggle(void)
{
   static bool grab_mouse_state  = false;

   if (!driver.input->grab_mouse)
      return;

   grab_mouse_state = !grab_mouse_state;
   RARCH_LOG("Grab mouse state: %s.\n", grab_mouse_state ? "yes" : "no");
   driver.input->grab_mouse(driver.input_data, grab_mouse_state);

   if (driver.video_poke && driver.video_poke->show_mouse)
      driver.video_poke->show_mouse(driver.video_data, !grab_mouse_state);
}

#ifdef HAVE_NETPLAY
static void check_netplay_flip(bool pressed, bool fullscreen_toggle_pressed)
{
   netplay_t *netplay = (netplay_t*)driver.netplay_data;
   if (pressed && netplay)
      netplay_flip_players(netplay);

   rarch_check_fullscreen(fullscreen_toggle_pressed);
}
#endif

static void check_pause(bool pressed, bool frameadvance_pressed)
{
   static bool old_focus    = true;
   bool focus               = true;
   bool old_is_paused       = g_extern.is_paused;

   /* FRAMEADVANCE will set us into pause mode. */
   pressed |= !g_extern.is_paused && frameadvance_pressed;

   if (g_settings.pause_nonactive)
      focus = driver.video->focus(driver.video_data);

   if (focus && pressed)
      g_extern.is_paused  = !g_extern.is_paused;
   else if (focus && !old_focus)
      g_extern.is_paused  = false;
   else if (!focus && old_focus)
      g_extern.is_paused  = true;

   old_focus = focus;

   if (g_extern.is_paused == old_is_paused)
      return;

   if (g_extern.is_paused)
   {
      RARCH_LOG("Paused.\n");
      rarch_main_command(RARCH_CMD_AUDIO_STOP);

      if (g_settings.video.black_frame_insertion)
         rarch_render_cached_frame();
   }
   else
   {
      RARCH_LOG("Unpaused.\n");
      rarch_main_command(RARCH_CMD_AUDIO_START);
   }
}

/* Rewind buttons works like FRAMEREWIND when paused.
 * We will one-shot in that case. */
static inline bool check_is_oneshot(bool oneshot_pressed, bool rewind_pressed)
{
   return (oneshot_pressed | rewind_pressed);
}

/* To avoid continous switching if we hold the button down, we require
 * that the button must go from pressed to unpressed back to pressed 
 * to be able to toggle between then.
 */
static void check_fast_forward_button(bool fastforward_pressed,
      bool hold_pressed, bool old_hold_pressed)
{
   if (fastforward_pressed)
      driver.nonblock_state = !driver.nonblock_state;
   else if (old_hold_pressed != hold_pressed)
      driver.nonblock_state = hold_pressed;
   else
      return;

   driver_set_nonblock_state(driver.nonblock_state);
}

static void check_stateslots(bool pressed_increase, bool pressed_decrease)
{
   char msg[PATH_MAX];

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


   if (g_extern.msg_queue)
      msg_queue_clear(g_extern.msg_queue);

   snprintf(msg, sizeof(msg), "State slot: %d",
         g_settings.state_slot);

   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   RARCH_LOG("%s\n", msg);
}

static inline void setup_rewind_audio(void)
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

static void check_rewind(bool pressed)
{
   static bool first = true;

   if (g_extern.frame_is_reverse)
   {
      /* We just rewound. Flush rewind audio buffer. */
      retro_flush_audio(g_extern.audio_data.rewind_buf
            + g_extern.audio_data.rewind_ptr,
            g_extern.audio_data.rewind_size - g_extern.audio_data.rewind_ptr);

      g_extern.frame_is_reverse = false;
   }

   if (first)
   {
      first = false;
      return;
   }

   if (!g_extern.state_manager)
      return;

   if (pressed)
   {
      const void *buf = NULL;

      msg_queue_clear(g_extern.msg_queue);
      if (state_manager_pop(g_extern.state_manager, &buf))
      {
         g_extern.frame_is_reverse = true;
         setup_rewind_audio();

         msg_queue_push(g_extern.msg_queue, RETRO_MSG_REWINDING, 0,
               g_extern.is_paused ? 1 : 30);
         pretro_unserialize(buf, g_extern.state_size);

         if (g_extern.bsv.movie)
            bsv_movie_frame_rewind(g_extern.bsv.movie);
      }
      else
         msg_queue_push(g_extern.msg_queue,
               RETRO_MSG_REWIND_REACHED_END, 0, 30);
   }
   else
   {
      static unsigned cnt = 0;

      cnt = (cnt + 1) % (g_settings.rewind_granularity ?
            g_settings.rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if ((cnt == 0) || g_extern.bsv.movie)
      {
         void *state = NULL;
         state_manager_push_where(g_extern.state_manager, &state);

         RARCH_PERFORMANCE_INIT(rewind_serialize);
         RARCH_PERFORMANCE_START(rewind_serialize);
         pretro_serialize(state, g_extern.state_size);
         RARCH_PERFORMANCE_STOP(rewind_serialize);

         state_manager_push_do(g_extern.state_manager);
      }
   }

   retro_set_rewind_callbacks();
}

static void check_slowmotion(bool pressed)
{
   g_extern.is_slowmotion = pressed;

   if (!g_extern.is_slowmotion)
      return;

   if (g_settings.video.black_frame_insertion)
      rarch_render_cached_frame();

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, g_extern.frame_is_reverse ?
         "Slow motion rewind." : "Slow motion.", 0, 30);
}

static bool check_movie_init(void)
{
   char path[PATH_MAX], msg[PATH_MAX];
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

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, g_extern.bsv.movie ?
         msg : "Failed to start movie record.", 1, 180);

   if (g_extern.bsv.movie)
      RARCH_LOG("Starting movie record to \"%s\".\n", path);
   else
      RARCH_ERR("Failed to start movie record.\n");

   return ret;
}

static bool check_movie_record(void)
{
   if (!g_extern.bsv.movie)
      return false;

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue,
         RETRO_MSG_MOVIE_RECORD_STOPPING, 2, 180);
   RARCH_LOG(RETRO_LOG_MOVIE_RECORD_STOPPING);

   rarch_main_command(RARCH_CMD_BSV_MOVIE_DEINIT);

   return true;
}

static bool check_movie_playback(void)
{
   if (!g_extern.bsv.movie_end)
      return false;

   msg_queue_push(g_extern.msg_queue,
         RETRO_MSG_MOVIE_PLAYBACK_ENDED, 1, 180);
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

static void check_shader_dir(bool pressed_next, bool pressed_prev)
{
   char msg[PATH_MAX];
   const char *shader = NULL, *ext = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;

   if (!g_extern.shader_dir.list || !driver.video->set_shader)
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

   msg_queue_clear(g_extern.msg_queue);

   snprintf(msg, sizeof(msg), "Shader #%u: \"%s\".",
         (unsigned)g_extern.shader_dir.ptr, shader);
   msg_queue_push(g_extern.msg_queue, msg, 1, 120);
   RARCH_LOG("Applying shader \"%s\".\n", shader);

   if (!driver.video->set_shader(driver.video_data, type, shader))
      RARCH_WARN("Failed to apply shader.\n");
}

/* 
 * Checks for stuff like fullscreen, save states, etc.
 *
 * Returns:
 * 0 - normal operation.
 * 1 - when RetroArch is paused.
 */

static int do_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (BIT64_GET(trigger_input, RARCH_SCREENSHOT))
      rarch_main_command(RARCH_CMD_TAKE_SCREENSHOT);

   if (BIT64_GET(trigger_input, RARCH_MUTE))
      rarch_main_command(RARCH_CMD_AUDIO_MUTE_TOGGLE);

   if (BIT64_GET(input, RARCH_VOLUME_UP))
      set_volume(0.5f);
   else if (BIT64_GET(input, RARCH_VOLUME_DOWN))
      set_volume(-0.5f);

   if (BIT64_GET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE))
      check_grab_mouse_toggle();

#ifdef HAVE_OVERLAY
   if (BIT64_GET(trigger_input, RARCH_OVERLAY_NEXT))
      input_overlay_next(driver.overlay);
#endif

   if (!g_extern.is_paused)
      check_fullscreen_func(trigger_input);

#ifdef HAVE_NETPLAY
   if (driver.netplay_data)
   {
      check_netplay_flip_func(trigger_input);
      return 0;
   }
#endif
   check_pause_func(trigger_input);

   if (g_extern.is_paused)
   {
      if (check_fullscreen_func(trigger_input))
         rarch_render_cached_frame();
      if (!check_oneshot_func(trigger_input))
         return 1;
   }

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

   if (BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_PLUS))
      cheat_manager_index_next(g_extern.cheat);
   else if (BIT64_GET(trigger_input, RARCH_CHEAT_INDEX_MINUS))
      cheat_manager_index_prev(g_extern.cheat);
   else if (BIT64_GET(trigger_input, RARCH_CHEAT_TOGGLE))
      cheat_manager_toggle(g_extern.cheat);

   if (BIT64_GET(trigger_input, RARCH_DISK_EJECT_TOGGLE))
      rarch_main_command(RARCH_CMD_DISK_EJECT_TOGGLE);
   else if (BIT64_GET(trigger_input, RARCH_DISK_NEXT))
      rarch_main_command(RARCH_CMD_DISK_NEXT);
   else if (BIT64_GET(trigger_input, RARCH_DISK_PREV))
      rarch_main_command(RARCH_CMD_DISK_PREV);	  

   if (BIT64_GET(trigger_input, RARCH_RESET))
      rarch_main_command(RARCH_CMD_RESET);

   return 0;
}

static inline int time_to_exit(retro_input_t input)
{
   if (
         g_extern.system.shutdown
         || check_quit_key_func(input)
         || (g_extern.max_frames && g_extern.frame_count >= 
            g_extern.max_frames)
         || (g_extern.bsv.movie_end && g_extern.bsv.eof_exit)
         || !driver.video->alive(driver.video_data)
      )
      return 1;
   return 0;
}

static void update_frame_time(void)
{
   retro_time_t time = rarch_get_time_usec();
   retro_time_t delta = time - g_extern.system.frame_time_last;
   bool is_locked_fps = g_extern.is_paused || driver.nonblock_state;
   is_locked_fps |= !!driver.recording_data;

   if (!g_extern.system.frame_time_last || is_locked_fps)
      delta = g_extern.system.frame_time.reference;

   if (!is_locked_fps && g_extern.is_slowmotion)
      delta /= g_settings.slowmotion_ratio;

   g_extern.system.frame_time_last = is_locked_fps ? 0 : time;
   g_extern.system.frame_time.callback(delta);
}

#ifdef HAVE_MENU
static void do_state_check_menu_toggle(void)
{
   if (g_extern.is_menu)
   {
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
      return;
   }

   rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
}
#endif

static void limit_frame_time(void)
{
   retro_time_t current = rarch_get_time_usec();
   retro_time_t target  = 0, to_sleep_ms = 0;
   double effective_fps = g_extern.system.av_info.timing.fps 
      * g_settings.fastforward_ratio;
   double mft_f = 1000000.0f / effective_fps;

   g_extern.frame_limit.minimum_frame_time = (retro_time_t) roundf(mft_f);

   target = g_extern.frame_limit.last_frame_time + 
      g_extern.frame_limit.minimum_frame_time;
   to_sleep_ms = (target - current) / 1000;

   if (to_sleep_ms > 0)
   {
      rarch_sleep((unsigned int)to_sleep_ms);

      /* Combat jitter a bit. */
      g_extern.frame_limit.last_frame_time += 
         g_extern.frame_limit.minimum_frame_time;
   }
   else
      g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
}

static void check_block_hotkey(bool enable_hotkey)
{
   bool use_hotkey_enable;
   static const struct retro_keybind *bind = 
      &g_settings.input.binds[0][RARCH_ENABLE_HOTKEY];

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   driver.block_hotkey = driver.block_input;

   // If we haven't bound anything to this, always allow hotkeys.
   use_hotkey_enable = bind->key != RETROK_UNKNOWN ||
      bind->joykey != NO_BTN ||
      bind->joyaxis != AXIS_NONE;

   driver.block_hotkey = driver.block_input ||
      (use_hotkey_enable && !enable_hotkey);

   /* If we hold ENABLE_HOTKEY button, block all libretro input to allow 
    * hotkeys to be bound to same keys as RetroPad. */
   driver.block_libretro_input = use_hotkey_enable && enable_hotkey;
}

/* We query all known keys per frame. Returns a 64-bit mask 
 * of all pressed keys.
 *
 * TODO: In case RARCH_BIND_LIST_END starts exceeding 64,
 * and you need a bitmask of more than 64 entries, reimplement
 * it to use something like rarch_bits_t.
 */

static inline retro_input_t input_keys_pressed(void)
{
   static const struct retro_keybind *binds[MAX_PLAYERS] = {
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
   int i, key;

   g_extern.turbo_count++;

   check_block_hotkey(driver.input->key_pressed(driver.input_data,
            RARCH_ENABLE_HOTKEY));

   input_push_analog_dpad((struct retro_keybind*)binds[0],
         (g_settings.input.analog_dpad_mode[0] == ANALOG_DPAD_NONE) ?
         ANALOG_DPAD_LSTICK : g_settings.input.analog_dpad_mode[0]);

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      input_push_analog_dpad(g_settings.input.autoconf_binds[i],
            g_settings.input.analog_dpad_mode[i]);

      g_extern.turbo_frame_enable[i] = driver.block_libretro_input ? 0 :
         driver.input->input_state(driver.input_data, binds, i,
               RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
   }

   for (key = 0; key < RARCH_BIND_LIST_END; key++)
   {
      bool state = false;

      if (
            (!driver.block_libretro_input && (key < RARCH_FIRST_META_KEY)) ||
            !driver.block_hotkey)
         state = driver.input->key_pressed(driver.input_data, key);

#ifdef HAVE_OVERLAY
      state = state || (driver.overlay_state.buttons & (1ULL << key));
#endif

#ifdef HAVE_COMMAND
      if (driver.command)
         state = state || rarch_cmd_get(driver.command, key);
#endif

      if (state)
         ret |= (1ULL << key);
   }

   input_pop_analog_dpad((struct retro_keybind*)binds[0]);
   for (i = 0; i < MAX_PLAYERS; i++)
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);

   return ret;
}

static bool input_flush(retro_input_t *input)
{
   *input = 0;

   /* If core was paused before entering menu, evoke
    * pause toggle to wake it up. */
   if (g_extern.is_paused)
      BIT64_SET(*input, RARCH_PAUSE_TOGGLE);

   return true;
}

/*
 * RetroArch's main iteration loop.
 *
 * Returns:
 *  0  -  Forcibly wake up the loop.
 *  1  -  Wait until input to wake up the loop
 * -1  -  Quit out of iteration loop.
 */

int rarch_main_iterate(void)
{
   unsigned i;
   retro_input_t trigger_input;
   int ret = 0;
   static retro_input_t last_input = 0;
   retro_input_t old_input = last_input;
   retro_input_t input = input_keys_pressed();

   last_input = input;

   if (driver.flushing_input)
      driver.flushing_input = (input) ? input_flush(&input) : false;

   trigger_input = input & ~old_input;

   if (time_to_exit(input))
      return -1;

   if (g_extern.system.frame_time.callback)
      update_frame_time();

#ifdef HAVE_MENU
   if (check_enter_menu_func(trigger_input) || (g_extern.libretro_dummy))
      do_state_check_menu_toggle();

   if (g_extern.is_menu)
   {
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
      return -1;
   }

   if (do_state_checks(input, old_input, trigger_input))
   {
      /* RetroArch has been paused */
      driver.retro_ctx.poll_cb();
      rarch_sleep(10);

      return 1;
   }

#if defined(HAVE_THREADS)
   lock_autosave();
#endif

#ifdef HAVE_NETPLAY
   if (driver.netplay_data)
      netplay_pre_frame((netplay_t*)driver.netplay_data);
#endif

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_start(g_extern.bsv.movie);

   if (g_extern.system.camera_callback.caps)
      driver_camera_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_push_analog_dpad(g_settings.input.binds[i],
            g_settings.input.analog_dpad_mode[i]);
      input_push_analog_dpad(g_settings.input.autoconf_binds[i],
            g_settings.input.analog_dpad_mode[i]);
   }

   if ((g_settings.video.frame_delay > 0) && !driver.nonblock_state)
      rarch_sleep(g_settings.video.frame_delay);


   /* Run libretro for one frame. */
   pretro_run();

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_pop_analog_dpad(g_settings.input.binds[i]);
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);
   }

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_end(g_extern.bsv.movie);

#ifdef HAVE_NETPLAY
   if (driver.netplay_data)
      netplay_post_frame((netplay_t*)driver.netplay_data);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

success:
   if (g_settings.fastforward_ratio_throttle_enable)
      limit_frame_time();

   return ret;
}
