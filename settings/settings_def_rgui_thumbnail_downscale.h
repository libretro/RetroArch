/* Single-source definitions: RGUI thumbnail downscale group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(menu_rgui_thumbnail_downscaler, MENU_RGUI_THUMBNAIL_DOWNSCALER,
      "rgui_thumbnail_downscaler",
      DEFAULT_RGUI_THUMBNAIL_DOWNSCALER, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_THUMB_SCALE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_thumbnail_scaler, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Thumbnail Downscaling Method",
      "Resampling method used when shrinking large thumbnails to fit the display.")
S_UINT_EX(menu_rgui_thumbnail_delay, MENU_RGUI_THUMBNAIL_DELAY,
      "rgui_thumbnail_delay",
      DEFAULT_RGUI_THUMBNAIL_DELAY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0f, 1024.0f, 64.0f, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Thumbnail Delay (ms)",
      "Applies a time delay between selecting a playlist entry and loading its associated thumbnails. Setting this to a value of at least 256 ms enables fast lag-free scrolling on even the slowest devices.")
