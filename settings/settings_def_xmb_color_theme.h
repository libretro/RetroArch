/* Single-source definitions: XMB color theme setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(menu_xmb_color_theme, XMB_MENU_COLOR_THEME,
      "xmb_menu_color_theme",
      DEFAULT_XMB_THEME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, XMB_THEME_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_xmb_menu_color_theme, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Color Theme",
      "Select a different background color theme.")
#endif
