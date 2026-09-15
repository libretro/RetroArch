/* Single-source definitions: header and footer opacity group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS); the string
 * tables always carry this row via the strings pass. */
#if ((defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)))
S_UINT_EX(menu_screensaver_animation, MENU_SCREENSAVER_ANIMATION,
      "menu_screensaver_animation",
      DEFAULT_MENU_SCREENSAVER_ANIMATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_SCREENSAVER_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_screensaver_animation, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Menu Screensaver Animation",
      "Enable an animation effect while the menu screensaver is active. Has a modest performance impact.")
#endif
#endif
/* Descriptor and configuration rows are #if (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS); the string
 * tables always carry this row via the strings pass. */
#if ((defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)) && !defined(_3DS)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && (defined(HAVE_MATERIALUI) || defined(HAVE_XMB) || defined(HAVE_OZONE)))
S_FLOAT_EX(menu_screensaver_animation_speed, MENU_SCREENSAVER_ANIMATION_SPEED,
      "menu_screensaver_animation_speed",
      DEFAULT_MENU_SCREENSAVER_ANIMATION_SPEED, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.1, 10.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Menu Screensaver Animation Speed",
      "Adjust speed of menu screensaver animation effect.")
#endif
#endif
