/* Single-source definitions: settings tab password setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "content_show_settings_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(menu_content_show_settings_password, CONTENT_SHOW_SETTINGS_PASSWORD,
      "content_show_settings_password",
      "", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Set Password For Enabling 'Settings'",
      "Supplying a password when hiding the settings tab makes it possible to later restore it from the menu, by going to the Main Menu tab, selecting 'Enable Settings Tab' and entering the password.")
#endif
