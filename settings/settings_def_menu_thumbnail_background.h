/* Single-source definitions: thumbnail display settings.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_thumbnail_background_enable, MENU_THUMBNAIL_BACKGROUND_ENABLE,
      "menu_thumbnail_background_enable",
      DEFAULT_MENU_THUMBNAIL_BACKGROUND_ENABLE, SD_FLAG_NONE, 0, 0,
      "Thumbnail Backgrounds",
      "Enables padding of unused space in thumbnail images with a solid background. This ensures a uniform display size for all images, improving menu appearance when viewing mixed content thumbnails with varying base dimensions.")

S_BOOL(menu_thumbnail_preview_audio, MENU_THUMBNAIL_PREVIEW_AUDIO,
      "menu_thumbnail_preview_audio",
      DEFAULT_MENU_THUMBNAIL_PREVIEW_AUDIO, SD_FLAG_NONE, 0, 0,
      "Animated Thumbnail Audio",
      "Plays the audio track of animated WebM thumbnails while they are shown. Vorbis and Opus audio is supported. The audio loops with the animation and stops when the thumbnail is closed.")

S_UINT(menu_thumbnail_preview_threads, MENU_THUMBNAIL_PREVIEW_THREADS,
      "menu_thumbnail_preview_threads",
      DEFAULT_MENU_THUMBNAIL_PREVIEW_THREADS, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, 8, 1, 0,
      setting_action_ok_uint, NULL,
      "Animated Thumbnail Threads",
      "How many threads an animated thumbnail may decode on. One keeps the whole decode on its own thread. More spread it across cores, which is what lets a large preview play at speed. While a core is running the preview keeps to one thread, so the game keeps its cores.")
