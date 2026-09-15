/* Single-source definitions: Ozone sidebar group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_ozone_color_theme, OZONE_MENU_COLOR_THEME,
      "ozone_menu_color_theme",
      DEFAULT_OZONE_COLOR_THEME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OZONE_COLOR_THEME_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ozone_menu_color_theme, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Color Theme",
      "Select a different color theme.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(ozone_show_sidebar, OZONE_SHOW_SIDEBAR,
      "ozone_show_sidebar",
      DEFAULT_OZONE_SHOW_SIDEBAR, SD_FLAG_NONE, 0, 0,
      "Show the Sidebar",
      "Allow left sidebar navigation and playlists.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(ozone_collapse_sidebar, OZONE_COLLAPSE_SIDEBAR,
      "ozone_collapse_sidebar",
      DEFAULT_OZONE_COLLAPSE_SIDEBAR, SD_FLAG_NONE, 0, 0,
      "Collapse the Sidebar",
      "Have the left sidebar always collapsed.")
#endif
#endif
