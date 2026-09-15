/* Single-source definitions: extended-row pilot setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(menu_use_preferred_system_color_theme, MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "menu_use_preferred_system_color_theme",
      DEFAULT_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Use Preferred System Color Theme",
      "Use operating system's color theme (if any). Overrides theme settings.")
