/* Single-source definitions: RGUI color theme setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration.c row stays literal for this setting. */
#if !defined(SETTINGS_DEF_CONFIG_PASS)
S_STRING(menu_rgui_color_theme, RGUI_MENU_COLOR_THEME,
      "rgui_menu_color_theme",
      DEFAULT_RGUI_COLOR_THEME, SD_FLAG_NONE, 0, setting_action_ok_uint, setting_get_string_representation_menu_theme, setting_generic_action_start_default, NULL, setting_string_action_left_menu_theme, setting_string_action_right_menu_theme, ST_UI_TYPE_STRING_COMBOBOX,
      "Color Theme",
      "Select a different color theme. Choosing 'Custom' enables the use of menu theme preset files.")
#endif
