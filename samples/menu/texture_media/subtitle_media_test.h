#ifndef MEDIA_SUBTITLE_GLYPH_CHECK
#define MEDIA_SUBTITLE_GLYPH_CHECK() true
#endif

static bool media_subtitle_capture(int ordinal)
{
   uint32_t hash;
   if (!getenv("MEDIA_TEST_GPU") || !getenv("MEDIA_TEST_CAPTURE_DIR"))
      return true;
   return media_native_capture(&hash, ordinal);
}

static const char *media_test_subtitle_text(const menu_media_controls_t *media)
{
   const gfx_media_subtitle_view_t *view = gfx_media_subtitle_view(media->subtitle);
   return view ? view->text : "";
}

/* Runs against the real menu adapter and asynchronous sidecar loader. */
static bool media_test_subtitles(menu_media_controls_t *media,
      struct menu_state *menu_st, void (*draw)(struct menu_state *))
{
   settings_t *settings = config_get_ptr();
   int64_t deadline;
   bool valid;
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
   settings->bools.media_player_subtitles = false;
   draw(menu_st);
   draw(menu_st);
   valid = valid && !media_test_subtitle_text(media)[0];
   settings->bools.media_player_subtitles = true;
   return valid;
}
