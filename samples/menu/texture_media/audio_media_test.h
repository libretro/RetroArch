/* Real frontend/menu adapters; use a checked-in two-audio-track movie. */
#ifdef HAVE_AUDIOMIXER
static bool media_audio_failure(unsigned line)
{
   printf("audio selector failure at line %u\n", line);
   return false;
}

static bool media_test_audio_tracks(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   unsigned pass, supported = 0, i, rate = 0;
   bool pcm16 = false, ready;
   gfx_media_target_t target = media->controls.target;
   gfx_media_playback_state_t before, after;
   for (i = 0; i < target.audio_track_count; i++)
      if (target.audio_tracks[i].supported) supported++;
   ready = audio_driver_mixer_prepare_format(&rate, &pcm16);
   printf("audio device ready=%u rate=%u s16=%u\n", ready, rate, pcm16);
   printf("audio selector: tracks=%u supported=%u callback=%u buttons=%u requested=%d\n",
         target.audio_track_count, supported, target.select_audio != NULL,
         media->controls.layout.button_count, target.audio_track_index);
   if (!ready || supported < 2 || !target.select_audio
         || !target.pause(target.user, true, menu_st->current_time_us))
      return media_audio_failure(__LINE__);
   draw(menu_st);
   for (pass = 0; pass < 3; pass++)
   {
      /* Before CC and Shaders, where those are offered */
      unsigned button = media->controls.layout.button_count
         - (media->controls.target.subtitle_tracks ? 2 : 1)
         - (media->controls.target.shaders ? 1 : 0);
      int previous = media->controls.target.audio_track_index;
      int64_t deadline;
      if (!target.state(target.user, menu_st->current_time_us, &before))
         return media_audio_failure(__LINE__);
      if (!pass)
      {
         menu_media_controls_action(media, MENU_ACTION_DOWN, menu_st->current_time_us);
         for (i = 0; i < media->controls.layout.button_count
               && media->controls.focus != button; i++)
            menu_media_controls_action(media, MENU_ACTION_RIGHT, menu_st->current_time_us);
         if (media->controls.focus != button
               || !menu_media_controls_action(media, MENU_ACTION_OK, menu_st->current_time_us))
            return media_audio_failure(__LINE__);
      }
      else
      {
         const gfx_media_rect_t *rect = &media->controls.layout.buttons[button];
         int x = rect->x + 1, y = rect->y + 1;
         gfx_media_controls_pointer(&media->controls, x, y, false, true, menu_st->current_time_us);
         gfx_media_controls_pointer(&media->controls, x, y, true, true, menu_st->current_time_us);
         if (!menu_media_controls_release(media, (unsigned)x, (unsigned)y,
                  menu_st->current_time_us))
            return media_audio_failure(__LINE__);
      }
      deadline = cpu_features_get_time_usec() + 5000000;
      do
      {
         task_queue_check();
         draw(menu_st);
         if (!target.state(target.user, menu_st->current_time_us, &after))
            return media_audio_failure(__LINE__);
         if (!after.seeking) break;
         retro_sleep(1);
      } while (cpu_features_get_time_usec() < deadline);
      printf("audio selector pass %u: old=%d new=%d position=%ld/%ld paused=%u seeking=%u failed=%u view=%u/%u\n",
            pass, previous, media->controls.target.audio_track_index, (long)before.position_us,
            (long)after.position_us, after.paused, after.seeking, after.seek_failed,
            media->view.audio_tracks, media->view.audio_track_number);
      if (getenv("MEDIA_TEST_GPU") && getenv("MEDIA_TEST_CAPTURE_DIR"))
      {
         uint32_t hash;
         if (!media_native_capture(&hash, 30 + (int)pass))
            return media_audio_failure(__LINE__);
      }
      if (after.seeking || after.seek_failed || !after.paused
            || before.position_us != after.position_us
            || media->controls.target.audio_track_index < 0
            || media->controls.target.audio_track_index == previous
            || !media->view.audio_tracks || !media->view.audio_track_number)
         return media_audio_failure(__LINE__);
      for (i = 0; i < target.audio_track_count; i++)
         if (target.audio_tracks[i].info.index == media->controls.target.audio_track_index)
            break;
      if (i == target.audio_track_count || media->view.audio_track_number != i + 1
            || strcmp(media->view.audio_language, target.audio_tracks[i].language))
         return media_audio_failure(__LINE__);
   }
   return true;
}

static bool media_test_audio_restored(menu_media_controls_t *media, int index,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   int64_t deadline = cpu_features_get_time_usec() + 5000000;
   do
   {
      gfx_media_playback_state_t state;
      task_queue_check();
      draw(menu_st);
      if (media->controls.target.audio_track_index == index
            && media->controls.target.state
            && media->controls.target.state(media->controls.target.user,
               menu_st->current_time_us, &state) && state.paused
            && !state.seeking && !state.seek_failed) return true;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < deadline);
   return false;
}
#else
static bool media_test_audio_tracks(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   (void)media; (void)menu_st; (void)draw;
   return false;
}
static bool media_test_audio_restored(menu_media_controls_t *media, int index,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   (void)media; (void)index; (void)menu_st; (void)draw;
   return false;
}
#endif
