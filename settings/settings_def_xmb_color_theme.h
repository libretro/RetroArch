/* Single-source definitions: XMB color theme setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration.c row stays literal for this setting. */
#if !defined(SETTINGS_DEF_CONFIG_PASS)
S_STRING(menu_xmb_color_theme, XMB_MENU_COLOR_THEME,
      "xmb_menu_color_theme",
      DEFAULT_XMB_THEME, SD_FLAG_NONE, 0, setting_action_ok_uint, setting_get_string_representation_menu_theme, setting_generic_action_start_default, NULL, setting_string_action_left_menu_theme, setting_string_action_right_menu_theme, ST_UI_TYPE_STRING_COMBOBOX,
      "Color Theme",
      "Select a different background color theme.")
#endif
#endif
