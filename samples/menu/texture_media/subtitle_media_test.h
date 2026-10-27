#ifndef MEDIA_SUBTITLE_GLYPH_CHECK
#define MEDIA_SUBTITLE_GLYPH_CHECK() true
#endif

static bool media_subtitle_capture(int ordinal)
{
   static uint32_t visible_hash;
   uint32_t hash;
   bool captured;
   if (!getenv("MEDIA_TEST_GPU") || !getenv("MEDIA_TEST_CAPTURE_DIR"))
      return true;
   captured = media_native_capture(&hash, ordinal);
   if (ordinal == 20 && captured)
      visible_hash = hash;
   if (ordinal == 21 && captured && hash == visible_hash)
   {
      printf("subtitle output did not change when captions were delayed\n");
      return false;
   }
   return captured;
}

static const char *media_test_subtitle_text(const menu_media_controls_t *media)
{
   const gfx_media_subtitle_view_t *view = gfx_media_subtitle_view(media->subtitle);
   return view ? view->text : "";
}

static bool media_subtitle_wait(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *),
      const char *expected)
{
   int64_t deadline = cpu_features_get_time_usec() + 5000000;
   do
   {
      const gfx_media_subtitle_view_t *view;
      task_queue_check();
      draw(menu_st);
      view = gfx_media_subtitle_view(media->subtitle);
      if (view && (expected ? strstr(view->text, expected) != NULL : view->off))
         return true;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < deadline);
   return false;
}

static bool media_test_embedded_subtitles(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   unsigned choice, i;
   gfx_media_subtitle_t *owner;
   if (!media->controls.target.subtitle_tracks)
      return false;
   for (choice = 0; choice < 3; choice++)
   {
      menu_media_controls_action(media, MENU_ACTION_DOWN, menu_st->current_time_us);
      for (i = 0; i + 1 < media->controls.layout.button_count; i++)
         menu_media_controls_action(media, MENU_ACTION_RIGHT, menu_st->current_time_us);
      menu_media_controls_action(media, MENU_ACTION_OK, menu_st->current_time_us);
      if (!media_subtitle_wait(media, menu_st, draw,
               choice == 2 ? NULL : choice == 1 ? "Embedded second" : "Embedded first"))
         return false;
      if (choice != 2)
      {
         const gfx_media_subtitle_view_t *view = gfx_media_subtitle_view(media->subtitle);
         if (!view || view->track_count != 2 || !strstr(view->text, "UTF-8 \303\251"))
            return false;
         owner = media->subtitle;
         menu_media_controls_context_reset(media, true);
         if (!media_subtitle_wait(media, menu_st, draw,
                  choice == 1 ? "Embedded second" : "Embedded first")
               || media->subtitle != owner)
            return false;
         if (getenv("MEDIA_TEST_SUBTITLE_CONTEXT"))
            for (i = 0; i < 2; i++)
               if (!command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL)
                     || !media_subtitle_wait(media, menu_st, draw,
                        choice == 1 ? "Embedded second" : "Embedded first"))
                  return false;
      }
   }
   puts("embedded subtitles: both tracks, UTF-8, context retention and Off passed");
   return true;
}

/* Runs against the real menu adapter and asynchronous sidecar loader. */
static bool media_test_subtitles(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   settings_t *settings = config_get_ptr();
   int64_t deadline;
   bool valid;
   if (getenv("MEDIA_TEST_EMBEDDED_SUBTITLES"))
      return media_test_embedded_subtitles(media, menu_st, draw);
   if (!getenv("MEDIA_TEST_SUBTITLES"))
      return true;
   printf("subtitle integration: enabled=%u path=%s\n",
         settings->bools.media_player_subtitles,
         media->controls.target.source_path ? media->controls.target.source_path : "(none)");
   if (!settings->bools.media_player_subtitles)
      return false;
   deadline = cpu_features_get_time_usec() + 5000000;
   do
   {
      task_queue_check();
      draw(menu_st);
      if (strstr(media_test_subtitle_text(media), "Subtitle sidecar"))
         break;
      retro_sleep(1);
   } while (cpu_features_get_time_usec() < deadline);
   valid = strstr(media_test_subtitle_text(media), "Subtitle sidecar") != NULL
      && strstr(media_test_subtitle_text(media), "UTF-8 \303\251") != NULL;
   printf("subtitle integration loaded: %s\n", media_test_subtitle_text(media));
   valid = MEDIA_SUBTITLE_GLYPH_CHECK() && valid;
   valid = media_subtitle_capture(20) && valid;
   settings->ints.media_player_subtitle_delay = 600000;
   draw(menu_st);
   draw(menu_st);
   valid = valid && !media_test_subtitle_text(media)[0];
   valid = media_subtitle_capture(21) && valid;
   printf("subtitle integration delayed: %s\n", media_test_subtitle_text(media));
   settings->ints.media_player_subtitle_delay = 0;
   draw(menu_st);
   draw(menu_st);
   valid = valid && strstr(media_test_subtitle_text(media), "Subtitle sidecar");
   printf("subtitle integration restored: %s\n", media_test_subtitle_text(media));
   valid = media_subtitle_capture(22) && valid;
   valid = valid && media->controls.layout.button_count >= 5
      && media->controls.target.subtitle_tracks;
   menu_media_controls_action(media, MENU_ACTION_DOWN, menu_st->current_time_us);
   {
      unsigned i;
      gfx_media_subtitle_t *owner;
      const gfx_media_rect_t *button;
      for (i = 0; i + 1 < media->controls.layout.button_count; i++)
         menu_media_controls_action(media, MENU_ACTION_RIGHT, menu_st->current_time_us);
      valid = valid && media->controls.focus + 1 == media->controls.layout.button_count;
      menu_media_controls_action(media, MENU_ACTION_OK, menu_st->current_time_us);
      valid = media_subtitle_wait(media, menu_st, draw, NULL) && valid;
      valid = media_subtitle_capture(23) && valid;
      button = &media->controls.layout.buttons[media->controls.layout.button_count - 1];
      gfx_media_controls_pointer(&media->controls, button->x + 1, button->y + 1,
            false, true, menu_st->current_time_us);
      gfx_media_controls_pointer(&media->controls, button->x + 1, button->y + 1,
            true, true, menu_st->current_time_us);
      menu_media_controls_release(media, button->x + 1, button->y + 1,
            menu_st->current_time_us);
      valid = media_subtitle_wait(media, menu_st, draw, "English track") && valid;
      valid = media_subtitle_capture(24) && valid;
      owner = media->subtitle;
      menu_media_controls_context_reset(media, true);
      valid = media_subtitle_wait(media, menu_st, draw, "English track") && valid;
      valid = valid && media->subtitle == owner;
      if (getenv("MEDIA_TEST_SUBTITLE_CONTEXT"))
         for (i = 0; i < 2; i++)
         {
            valid = command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL) && valid;
            valid = media_subtitle_wait(media, menu_st, draw, "English track") && valid;
         }

      printf("subtitle track selection retained: %s\n", media_test_subtitle_text(media));
   }
   settings->bools.media_player_subtitles = false;
   draw(menu_st);
   draw(menu_st);
   valid = valid && !media_test_subtitle_text(media)[0];
   settings->bools.media_player_subtitles = true;
   return valid;
}
