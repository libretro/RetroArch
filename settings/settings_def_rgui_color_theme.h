/* Single-source definitions: RGUI color theme setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(menu_rgui_color_theme, RGUI_MENU_COLOR_THEME,
      "rgui_menu_color_theme",
      DEFAULT_RGUI_COLOR_THEME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_THEME_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_menu_color_theme, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Color Theme",
      "Select a different color theme. Choosing 'Custom' enables the use of menu theme preset files.")
