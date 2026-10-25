/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIA_FOLDER_PLAYBACK_TEST_H
#define MEDIA_FOLDER_PLAYBACK_TEST_H
#include "../../../menu/menu_cbs.h"

static double media_folder_core_fps;
static float media_folder_core_hz;


static bool media_folder_install(struct menu_state *menu_st, file_list_t *list,
      const char *directory, const char *fixture)
{
   static const unsigned episodes[] = {10, 2, 1, 11};
   const char *extension = path_get_extension(fixture);
   char name[64];
   unsigned i;
   unsigned count = getenv("MEDIA_TEST_CONTINUOUS_ERROR") ? 4 : 3;
   media_folder_core_fps = video_state_get_ptr()->av_info.timing.fps;
   media_folder_core_hz = video_state_get_ptr()->core_hz;
   for (i = 0; i < count; i++)
   {
      snprintf(name, sizeof(name), "episode%u.%s", episodes[i], extension);
      if (!file_list_append(list, name, "", FILE_TYPE_MOVIE, 0, 0))
         return false;
      if (menu_st->driver_ctx->list_insert)
         menu_st->driver_ctx->list_insert(menu_st->userdata, list, name,
               directory, "", i, FILE_TYPE_MOVIE);
   }
   return true;
}

static bool media_folder_refresh_expected(void)
{
#ifdef _WIN32
   const char *value = getenv("MEDIA_TEST_REFRESH_RATE");
   DEVMODE mode;
   unsigned expected;
   if (!value)
      return true;
   expected = (unsigned)strtoul(value, NULL, 10);
   memset(&mode, 0, sizeof(mode));
   mode.dmSize = sizeof(mode);
   if (!EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &mode))
      return false;
   printf("fullscreen refresh: %lu Hz expected=%u\n",
         (unsigned long)mode.dmDisplayFrequency, expected);
   return mode.dmDisplayFrequency == expected;
#else
   return getenv("MEDIA_TEST_REFRESH_RATE") == NULL;
#endif
}

static bool media_folder_drive(struct menu_state *menu_st,
      menu_media_controls_t *media, void (*draw)(struct menu_state *),
      int (*stage)(void *), bool (*shown)(void *), void *viewer)
{
   struct menu_media_queue *identity = media->queue;
   bool next_loaded = false, ended = false;
   bool check_pixels = getenv("MEDIA_TEST_REFRESH_PIXELS") != NULL;
   uint32_t picture = 0;
   int64_t capture_time = 0;
   unsigned captures = 0;
   unsigned tick;
   if (!identity || menu_st->selection_ptr != 1)
   {
      printf("folder setup: queue=%u selection=%lu enabled=%u\n",
            identity != NULL, (unsigned long)menu_st->selection_ptr,
            config_get_ptr()->bools.media_player_continuous);
      return false;
   }
   if (getenv("MEDIA_TEST_CONTINUOUS_ERROR"))
   {
      unsigned settled = 0;
      for (tick = 0; tick < 16000; tick++)
      {
         draw(menu_st);
         if (!shown(viewer) || media->queue != identity)
            return false;
         if (menu_st->selection_ptr != 1 && menu_st->selection_ptr != 0)
            return false;
         if (menu_st->selection_ptr == 0 && !media->controls.target.user)
         {
            if (++settled == 1000)
            {
               puts("folder error: invalid next movie does not skip to episode11");
               return true;
            }
         }
         else
            settled = 0;
         retro_sleep(1);
      }
      return false;
   }
   for (tick = 0; tick < 16000; tick++)
   {
      int status;
      draw(menu_st);
      if (!shown(viewer) || media->queue != identity)
         break;
      if (menu_st->selection_ptr != 1 && menu_st->selection_ptr != 0)
         break;
      status = stage(viewer);
      if (check_pixels && captures < 3
            && menu_media_controls_display(NULL, NULL)
            && media->controls.playback.cadence_num
            && cpu_features_get_time_usec() >= capture_time)
      {
         uint32_t hash;
         if (!media_native_capture_region(&hash, 3000 + (int)captures, true))
            return false;
         if (!captures && !media_folder_refresh_expected())
            return false;
         printf("fullscreen refresh pixels: capture=%u hash=%08lx changed=%u\n",
               captures, (unsigned long)hash, !captures || hash != picture);
         if (captures && hash == picture)
            return false;
         picture = hash;
         captures++;
         capture_time = cpu_features_get_time_usec() + 250000;
      }
      if (!next_loaded && menu_st->selection_ptr == 0 && status == 1)
      {
         if (!media_folder_refresh_expected())
            return false;
         next_loaded = true;
      }
      if (next_loaded && status == 2)
      {
         ended = true;
         break;
      }
      retro_sleep(1);
   }
   printf("folder playback: ticks=%u selection=%lu next=%u ended=%u "
          "fullscreen=%u stable=%u\n",
         tick, (unsigned long)menu_st->selection_ptr, next_loaded, ended,
         shown(viewer), media->queue == identity);
   if (!ended || !shown(viewer) || media->queue != identity
         || menu_st->selection_ptr != 0 || (check_pixels && captures != 3))
   {
      printf("folder state: stage=%d position=%.6f duration=%.6f paused=%u seeking=%u\n",
            stage(viewer), (double)media->controls.playback.position_us / 1000000.0,
            (double)media->controls.playback.duration_us / 1000000.0,
            media->controls.playback.paused, media->controls.playback.seeking);
      return false;
   }
   {
      int64_t until = cpu_features_get_time_usec() + 500000;
      for (tick = 0; cpu_features_get_time_usec() < until; tick++)
      {
         draw(menu_st);
         if (menu_st->selection_ptr != 0 || stage(viewer) != 2)
            return false;
         retro_sleep(1);
      }
   }

   if (getenv("MEDIA_TEST_CADENCE"))
   {
      printf("movie cadence: %u/%u Hz\n", media->controls.playback.cadence_num,
            media->controls.playback.cadence_den);
      if (!media->controls.playback.cadence_num
            || !media->controls.playback.cadence_den)
         return false;
      {
         unsigned dims = 0;
         float hz = 0.0f;
         double expected = (double)media->controls.playback.cadence_num
            / media->controls.playback.cadence_den;
         if (getenv("MEDIA_TEST_CRT_ENGINE"))
         {
#ifdef HAVE_MODELINE
            video_driver_state_t *video = video_state_get_ptr();
            dims = video->crt_switch_st.ra_core_dims;
            hz = video->crt_switch_st.ra_core_hz;
            printf("CRT owner request: %ux%u %.6f Hz; core %.6f/%.6f Hz\n",
                  VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), hz,
                  video->av_info.timing.fps, (double)video->core_hz);
            if (video->av_info.timing.fps != media_folder_core_fps
                  || video->core_hz != media_folder_core_hz) return false;
#else
            return false;
#endif
         }
         else if (!menu_media_controls_display(&dims, &hz))
            return false;
         if (dims != media->controls.playback.dims
               || hz < expected - 0.001 || hz > expected + 0.001)
            return false;
         printf("fullscreen display source: %ux%u %.6f Hz\n",
               VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims), hz);
      }
   }
   if (!media->controls.target.user || !media->controls.playback.paused
         || media->controls.layout.button_count
            != 7u + (media->controls.target.subtitle_tracks ? 1u : 0u)
         || media->controls.target.directions != 1)
      return false;
   if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_PREVIOUS_FILE,
             menu_st->current_time_us))
      return false;
   for (tick = 0; tick < 2000 && stage(viewer) != 3; tick++)
   {
      draw(menu_st);
      retro_sleep(1);
   }
   if (stage(viewer) != 3 || menu_st->selection_ptr != 1 || !shown(viewer))
      return false;
   if (!gfx_media_controls_action(
             &media->controls, GFX_MEDIA_STOP, menu_st->current_time_us))
      return false;
   for (tick = 0; tick < 2000; tick++)
   {
      draw(menu_st);
      if (!media->controls.playback.seeking && media->controls.playback.paused
            && !media->controls.playback.seek_failed
            && !media->controls.playback.position_us)
         break;
      retro_sleep(1);
   }
   if (tick == 2000)
      return false;
   {
      int64_t until = cpu_features_get_time_usec() + 4000000;
      for (tick = 0; cpu_features_get_time_usec() < until; tick++)
      {
         draw(menu_st);
         if (menu_st->selection_ptr != 1 || !media->controls.playback.paused)
            return false;
         retro_sleep(1);
      }
   }
   puts("folder controls: previous works after EOF; Stop does not advance");
   if (getenv("MEDIA_TEST_SETTINGS"))
   {
      file_list_t *stack = MENU_LIST_GET(menu_st->entries.list, 0);
      size_t depth = stack->size;
      size_t selected = menu_st->selection_ptr;
      file_list_t *entries;
      size_t row;
      bool found = false, valid;
      gfx_media_target_t playing = media->controls.target;
      gfx_media_playback_state_t state;
      int64_t position;
      if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_PLAY,
               menu_st->current_time_us))
         return false;
      draw(menu_st);
      if (!playing.state(playing.user, menu_st->current_time_us, &state) || state.paused)
         return false;
      position = state.position_us;
      if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_SETTINGS,
               menu_st->current_time_us))
         return false;
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
      draw(menu_st);
      stack = MENU_LIST_GET(menu_st->entries.list, 0);
      if (!stack || !stack->size || !stack->list[stack->size - 1].label
            || strcmp(stack->list[stack->size - 1].label,
                  MENU_ENUM_LABEL_DEFERRED_MEDIA_PLAYER_SETTINGS_LIST_STR))
         return false;
      entries = MENU_LIST_GET_SELECTION(menu_st->entries.list, 0);
      for (row = 0; entries && row < entries->size; row++)
         if (entries->list[row].label
               && !strcmp(entries->list[row].label, "media_player_continuous"))
            found = true;
      if (!found) return false;
      printf("fullscreen settings: showing=%u\n", shown(viewer));
      if (shown(viewer) || !media->settings_open)
         return false;
      for (tick = 0; tick < 1000; tick++)
      {
         draw(menu_st);
         if (!playing.state(playing.user, menu_st->current_time_us, &state) || state.paused)
            return false;
         if (state.position_us > position + 100000)
            break;
         retro_sleep(1);
      }
      if (tick == 1000)
         return false;
      if ((getenv("MEDIA_TEST_CAPTURE_DIR") || getenv("MEDIA_TEST_SETTINGS_ORANGE"))
            && getenv("MEDIA_TEST_GPU")
            && !media_native_capture(&picture, 50))
         return false;
      puts("fullscreen settings: original media keeps playing in preview");
      if (getenv("MEDIA_TEST_CADENCE"))
      {
         bool crt_found = false, refresh_found = false;
         unsigned dims;
         float hz;
         if (getenv("MEDIA_TEST_CRT_ENGINE"))
         {
#ifdef HAVE_MODELINE
            video_driver_state_t *video = video_state_get_ptr();
            if (video->crt_switch_st.ra_core_hz != media_folder_core_hz
                  || video->av_info.timing.fps != media_folder_core_fps
                  || video->core_hz != media_folder_core_hz)
               return false;
#else
            return false;
#endif
         }
         else if (menu_media_controls_display(&dims, &hz)) return false;
         for (row = 0; row < entries->size; row++)
         {
            if (entries->list[row].label
                  && !strcmp(entries->list[row].label, "media_player_refresh_rate"))
               refresh_found = true;
            if (entries->list[row].label
                  && !strcmp(entries->list[row].label, "media_player_crt_switch"))
               crt_found = true;
         }
         if (!refresh_found) return false;
         if (getenv("MEDIA_TEST_DESKTOP_REFRESH"))
         {
            video_driver_state_t *video = video_state_get_ptr();
            if (!config_get_ptr()->bools.media_player_refresh_rate
                  || config_get_ptr()->bools.media_player_crt_switch
                  || video->av_info.timing.fps != media_folder_core_fps
                  || video->core_hz != media_folder_core_hz)
               return false;
            puts("desktop refresh: source cleared on settings exit; core timing unchanged");
         }
#ifdef HAVE_MODELINE
         if (!crt_found) return false;
#else
         if (crt_found) return false;
#endif
      }

      if (action_cancel_pop_default(NULL, NULL, 0, 0) != 0)
         return false;
      for (tick = 0; tick < 1000; tick++)
      {
         task_queue_check();
         menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
               config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
         draw(menu_st);
         if (!media->settings_open && shown(viewer)) break;
         retro_sleep(1);
      }
      if (tick == 1000) return false;
      stack = MENU_LIST_GET(menu_st->entries.list, 0);
      valid = playing.state(playing.user, menu_st->current_time_us, &state);
      if (stack->size != depth || menu_st->selection_ptr != selected
            || !shown(viewer) || media->settings_open || !valid || state.paused
            || state.position_us <= position)
      {
         printf("settings return: depth=%lu/%lu selection=%lu/%lu fullscreen=%u open=%u returning=%u valid=%u paused=%u\n",
               (unsigned long)stack->size, (unsigned long)depth,
               (unsigned long)menu_st->selection_ptr, (unsigned long)selected,
               shown(viewer), media->settings_open, media->settings_returning,
               valid, state.paused);
         return false;
      }
      if (!gfx_media_controls_action(&media->controls, GFX_MEDIA_PAUSE,
               menu_st->current_time_us)
            || !gfx_media_controls_action(&media->controls, GFX_MEDIA_SETTINGS,
               menu_st->current_time_us))
         return false;
      menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
            config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
      draw(menu_st);
      if (shown(viewer) || !playing.state(playing.user, menu_st->current_time_us, &state) || !state.paused)
         return false;
      if (action_cancel_pop_default(NULL, NULL, 0, 0) != 0)
         return false;
      for (tick = 0; tick < 1000; tick++)
      {
         task_queue_check();
         menu_driver_iterate(menu_st, disp_get_ptr(), anim_get_ptr(),
               config_get_ptr(), MENU_ACTION_NOOP, menu_st->current_time_us);
         draw(menu_st);
         if (!media->settings_open && shown(viewer)) break;
         retro_sleep(1);
      }
      if (tick == 1000) return false;
      if (!shown(viewer) || !playing.state(playing.user, menu_st->current_time_us, &state) || !state.paused)
         return false;
      puts("fullscreen settings: Back restores viewer and preserves pause state");
   }
   return true;
}
#endif
