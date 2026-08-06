/* Single-source definitions: CRT SwitchRes group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(crt_switch_resolution, CRT_SWITCH_RESOLUTION,
      "crt_switch_resolution",
      DEFAULT_CRT_SWITCH_RESOLUTION, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, CRT_SWITCH_NONE, CRT_SWITCH_INI, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_crt_switch_resolutions, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "CRT SwitchRes",
      "For CRT displays only. Attempts to use exact core/game resolution and refresh rate.")
S_UINT_EX(crt_switch_resolution_super, CRT_SWITCH_RESOLUTION_SUPER,
      "crt_switch_resolution_super",
      DEFAULT_CRT_SWITCH_RESOLUTION_SUPER, SD_FLAG_ADVANCED, 0, 0, 0, 0, 0, 0, NULL, setting_get_string_representation_crt_switch_resolution_super, NULL, NULL, setting_uint_action_left_crt_switch_resolution_super, setting_uint_action_right_crt_switch_resolution_super, 0,
      "CRT Super Resolution",
      "Switch among native and ultrawide super resolutions.")
/* config key "crt_switch_center_adjust" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_INT_EX(crt_switch_center_adjust, CRT_SWITCH_X_AXIS_CENTERING,
      "crt_switch_horizontal_shift",
      DEFAULT_CRT_SWITCH_CENTER_ADJUST, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, -50, 50, 1.0, -50, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_SPINBOX,
      "Horizontal Centering",
      "Cycle through these options if the image is not centered properly on the display.")
#endif
/* config key "crt_switch_porch_adjust" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_INT_EX(crt_switch_porch_adjust, CRT_SWITCH_PORCH_ADJUST,
      "crt_switch_horizontal_size",
      DEFAULT_CRT_SWITCH_PORCH_ADJUST, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, -50, 100, 2.0, -50, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_SPINBOX,
      "Horizontal Size",
      "Cycle through these options to adjust the horizontal settings to change the image size.")
#endif
/* config key "crt_switch_vertical_adjust" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_INT_EX(crt_switch_vertical_adjust, CRT_SWITCH_VERTICAL_ADJUST,
      "crt_switch_vertical_size",
      DEFAULT_CRT_SWITCH_VERTICAL_ADJUST, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, -20, 20, 1.0, -20, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_SPINBOX,
      "Vertical Centering",
      "Cycle through these options if the image is not centered properly on the display.")
#endif
S_BOOL(crt_switch_custom_refresh_enable, CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "crt_switch_resolution_use_custom_refresh_rate",
      false, SD_FLAG_NONE, 0, 0,
      "Custom Refresh Rate",
      "Use a custom refresh rate specified in the configuration file if needed.")
S_BOOL(crt_switch_hires_menu, CRT_SWITCH_HIRES_MENU,
      "crt_switch_hires_menu",
      false, SD_FLAG_NONE, 0, 0,
      "Use High Resolution Menu",
      "Switch to high resolution modeline for use with high-resolution menus when no content is loaded.")
