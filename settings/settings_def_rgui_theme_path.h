/* Single-source definitions: RGUI theme preset path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "rgui_menu_theme_preset" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_rgui_theme_preset, RGUI_MENU_THEME_PRESET,
      "rgui_menu_theme_preset",
      directory_assets, SD_FLAG_NONE, 0, "cfg", NULL, 0,
      "Custom Theme Preset",
      "Select a menu theme preset from File Browser.")
#endif
