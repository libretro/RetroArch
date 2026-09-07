/* Single-source definitions: menu wallpaper path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "menu_wallpaper" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_menu_wallpaper, MENU_WALLPAPER,
      "menu_wallpaper",
      directory_assets, SD_FLAG_NONE, 0, "png", NULL, 0,
      "Background Image",
      "Select an image to set as menu background. Manual and dynamic images will override 'Color Theme'.")
#endif
