/* Single-source definitions: thumbnail upscale threshold setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(gfx_thumbnail_upscale_threshold, MENU_THUMBNAIL_UPSCALE_THRESHOLD,
      "menu_thumbnail_upscale_threshold",
      DEFAULT_GFX_THUMBNAIL_UPSCALE_THRESHOLD, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1024, 256, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Thumbnail Upscaling Threshold",
      "Automatically upscale thumbnail images with a width/height smaller than the specified value. Improves picture quality. Has a moderate performance impact.")
