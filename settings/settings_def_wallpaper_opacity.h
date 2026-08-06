/* Single-source definitions: wallpaper opacity setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(menu_wallpaper_opacity, MENU_WALLPAPER_OPACITY,
      "menu_wallpaper_opacity",
      DEFAULT_MENU_WALLPAPER_OPACITY, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 1.0, 0.010, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Background Image Opacity",
      "Modify the opacity level of the background image.")
