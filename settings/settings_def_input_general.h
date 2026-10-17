/* Single-source definitions: general input group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
S_UINT_EX(input_max_users, INPUT_MAX_USERS,
      "input_max_users",
      DEFAULT_INPUT_MAX_USERS, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, MAX_USERS, 1, 1, setting_action_ok_uint, setting_get_string_representation_max_users, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Maximum Users",
      "Maximum amount of users supported by RetroArch. (Restart required)")
/* config key "menu_unified_controls" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_unified_controls, INPUT_UNIFIED_MENU_CONTROLS,
      "unified_menu_controls",
      false, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Unified Menu Controls",
      "Use the same controls for both the menu and the game. Applies to the keyboard.")
#endif
/* config key "menu_disable_info_button" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_disable_info_button, INPUT_DISABLE_INFO_BUTTON,
      "disable_info_button",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Disable Info Button",
      "Prevent menu info function.")
#endif
/* config key "menu_disable_search_button" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_disable_search_button, INPUT_DISABLE_SEARCH_BUTTON,
      "disable_search_button",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Disable Search Button",
      "Prevent menu search function.")
#endif
/* config key "menu_disable_left_analog" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_disable_left_analog, INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
      "disable_left_analog_in_menu",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Disable Left Analog in Menu",
      "Prevent menu left analog stick input.")
#endif
/* config key "menu_disable_right_analog" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_disable_right_analog, INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
      "disable_right_analog_in_menu",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Disable Right Analog in Menu",
      "Prevent menu right analog stick input. Right analog stick cycles thumbnails in playlists.")
#endif
S_BOOL(confirm_quit, CONFIRM_QUIT,
      "confirm_quit",
      DEFAULT_CONFIRM_QUIT, SD_FLAG_NONE, 0, 0,
      "Confirm Quit",
      "Require the Quit hotkey to be pressed twice.")
S_BOOL(confirm_close, CONFIRM_CLOSE,
      "confirm_close",
      DEFAULT_CONFIRM_CLOSE, SD_FLAG_NONE, 0, 0,
      "Confirm Close Content",
      "Require the Close Content hotkey to be pressed twice.")
S_BOOL(confirm_reset, CONFIRM_RESET,
      "confirm_reset",
      DEFAULT_CONFIRM_RESET, SD_FLAG_NONE, 0, 0,
      "Confirm Reset Content",
      "Require the Reset Content hotkey to be pressed twice.")
S_BOOL_NS(vibrate_on_keypress, VIBRATE_ON_KEYPRESS,
      "vibrate_on_keypress",
      DEFAULT_VIBRATE_ON_KEYPRESS, SD_FLAG_NONE, 0, 0,
      "Vibrate on Key Press")
S_BOOL_NS(enable_device_vibration, ENABLE_DEVICE_VIBRATION,
      "enable_device_vibration",
      DEFAULT_ENABLE_DEVICE_VIBRATION, SD_FLAG_NONE, 0, 0,
      "Enable Device Vibration (For Supported Cores)")
S_UINT_EX(input_rumble_gain, INPUT_RUMBLE_GAIN,
      "input_rumble_gain",
      DEFAULT_RUMBLE_GAIN, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 100, 5, 0, setting_action_ok_uint_special, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Vibration Strength",
      "Specify the magnitude of haptic feedback effects.")
S_UINT_EX_H(input_poll_type_behavior, INPUT_POLL_TYPE_BEHAVIOR,
      "input_poll_type_behavior",
      DEFAULT_INPUT_POLL_TYPE_BEHAVIOR, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_poll_type_behavior, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Polling Behavior (Restart required)",
      "Influences how input polling is done in RetroArch. Setting it to 'Early' or 'Late' can result in less latency, depending on your configuration.")
