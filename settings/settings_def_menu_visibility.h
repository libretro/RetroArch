/* Single-source definitions: menu visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_remember_selection, MENU_REMEMBER_SELECTION,
      "menu_remember_selection",
      DEFAULT_MENU_REMEMBER_SELECTION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_REMEMBER_SELECTION_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_remember_selection, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Remember Selection When Changing Tabs",
      "Remember previous cursor position in tabs. RGUI does not have tabs, but Playlists and Settings behave as such.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_startup_page, MENU_STARTUP_PAGE,
      "menu_startup_page",
      DEFAULT_MENU_STARTUP_PAGE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_STARTUP_PAGE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_startup_page, NULL, NULL, setting_uint_action_left_default, setting_uint_action_right_default, 0,
      "Startup Page",
      "Initial menu page on startup.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_mouse_enable, MOUSE_ENABLE,
      "menu_mouse_enable",
      DEFAULT_MOUSE_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "Mouse Support",
      "Allow the menu to be controlled with a mouse.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_pointer_enable, POINTER_ENABLE,
      "menu_pointer_enable",
      DEFAULT_POINTER_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "Touch Support",
      "Allow the menu to be controlled with a touchscreen.")
#endif
