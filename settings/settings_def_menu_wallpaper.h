/* Single-source definitions: dynamic wallpaper setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_dynamic_wallpaper_enable, DYNAMIC_WALLPAPER,
      "menu_dynamic_wallpaper_enable",
      DEFAULT_MENU_DYNAMIC_WALLPAPER_ENABLE, SD_FLAG_NONE, 0, 0,
      "Dynamic Background",
      "Dynamically load a new wallpaper depending on context.")
