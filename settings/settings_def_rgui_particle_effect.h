/* Single-source definitions: RGUI particle effect group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(menu_scale_factor, MENU_SCALE_FACTOR,
      "menu_scale_factor",
      DEFAULT_MENU_SCALE_FACTOR, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.2, 5.0, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Scale Factor",
      "Scale the size of user interface elements in the menu.")
#endif
