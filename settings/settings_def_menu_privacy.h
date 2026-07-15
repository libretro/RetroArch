/* Single-source definitions: menu privacy group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_timedate_enable, TIMEDATE_ENABLE,
      "menu_timedate_enable",
      DEFAULT_MENU_TIMEDATE_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "Show Date and Time",
      "Show current date and/or time inside the menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_timedate_style, TIMEDATE_STYLE,
      "menu_timedate_style",
      DEFAULT_MENU_TIMEDATE_STYLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_TIMEDATE_STYLE_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_timedate_style, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Style of Date and Time",
      "Change the style current date and/or time is shown inside the menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_timedate_date_separator, TIMEDATE_DATE_SEPARATOR,
      "menu_timedate_date_separator",
      DEFAULT_MENU_TIMEDATE_DATE_SEPARATOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_TIMEDATE_DATE_SEPARATOR_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_timedate_date_separator, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Date Separator",
      "Specify character to use as a separator between year/month/day components when current date is shown inside the menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_battery_level_enable, BATTERY_LEVEL_ENABLE,
      "menu_battery_level_enable",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Show Battery Level",
      "Show current battery level inside the menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_core_enable, CORE_ENABLE,
      "menu_core_enable",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Show Core Name",
      "Show current core name inside menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL_EX(menu_show_sublabels, MENU_SHOW_SUBLABELS,
      "menu_show_sublabels",
      DEFAULT_MENU_SHOW_SUBLABELS, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Show Menu Sub-Labels",
      "Show additional information for menu items.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_show_confirm, MENU_SHOW_CONFIRM,
      "menu_show_confirm",
      DEFAULT_MENU_SHOW_CONFIRM, SD_FLAG_NONE, 0, 0,
      "Show Confirmation Boxes",
      "Ask for confirmation before quitting, resetting or closing content. When disabled these actions happen immediately.")
#endif
