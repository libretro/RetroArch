/* Single-source definitions: kiosk mode password setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "kiosk_mode_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(kiosk_mode_password, MENU_KIOSK_MODE_PASSWORD,
      "menu_disable_kiosk_mode_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Set Password for Disabling Kiosk Mode",
      "Supplying a password when enabling kiosk mode makes it possible to later disable it from the menu, by going to the Main Menu, selecting Disable Kiosk Mode and entering the password.")
#endif
