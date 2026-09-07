/* Single-source definitions: savestate resume group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_show_advanced_settings, SHOW_ADVANCED_SETTINGS,
      "menu_show_advanced_settings",
      DEFAULT_SHOW_ADVANCED_SETTINGS, SD_FLAG_NONE, 0, 0,
      "Show Advanced Settings",
      "Show advanced settings for power users.")
#endif
/* config key "kiosk_mode_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_EX(kiosk_mode_enable, MENU_ENABLE_KIOSK_MODE,
      "menu_enable_kiosk_mode",
      DEFAULT_KIOSK_MODE_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Kiosk Mode",
      "Protects the setup by hiding all configuration related settings.")
#endif
