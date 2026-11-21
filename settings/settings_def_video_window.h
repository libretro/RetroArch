/* Single-source definitions: windowed-mode scale and geometry group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_NS(video_scale, VIDEO_SCALE,
      "video_scale",
      DEFAULT_SCALE, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 1, 10, 1, 1, setting_action_ok_uint, NULL,
      "Windowed Scale")
/* config key "video_windowed_position_width" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_AT_EX(offsetof(settings_t, uints.window_position_dims)
      | SDESC_OFF_HALF_HI, VIDEO_WINDOW_WIDTH,
      "video_window_width",
      DEFAULT_WINDOW_WIDTH, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 7680, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Window Width",
      "Set the custom width for the display window.")
#endif
/* config key "video_windowed_position_height" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_AT_EX(offsetof(settings_t, uints.window_position_dims)
      | SDESC_OFF_HALF_LO, VIDEO_WINDOW_HEIGHT,
      "video_window_height",
      DEFAULT_WINDOW_HEIGHT, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 4320, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Window Height",
      "Set the custom height for the display window.")
#endif
S_UINT_AT_EX(offsetof(settings_t, uints.window_auto_dims_max)
      | SDESC_OFF_HALF_HI, VIDEO_WINDOW_AUTO_WIDTH_MAX,
      "video_window_auto_width_max",
      DEFAULT_WINDOW_AUTO_WIDTH_MAX, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 7680, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Maximum Window Width",
      "Set the maximum width of the display window when automatically resizing based on 'Windowed Scale'.")
S_UINT_AT_EX(offsetof(settings_t, uints.window_auto_dims_max)
      | SDESC_OFF_HALF_LO, VIDEO_WINDOW_AUTO_HEIGHT_MAX,
      "video_window_auto_height_max",
      DEFAULT_WINDOW_AUTO_HEIGHT_MAX, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 4320, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Maximum Window Height",
      "Set the maximum height of the display window when automatically resizing based on 'Windowed Scale'.")
S_BOOL_EX(video_window_fit_display, VIDEO_WINDOW_FIT_DISPLAY,
      "video_window_fit_display",
      DEFAULT_WINDOW_FIT_DISPLAY, SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_REINIT, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Fit Window to Display",
      "Size the window to the largest that fits the display, less 'Window Margin', at 'Window Aspect Ratio'. Takes priority over 'Windowed Scale' and custom window sizes.")
S_UINT_EX(video_window_fit_aspect_ratio_idx, VIDEO_WINDOW_FIT_ASPECT_RATIO_INDEX,
      "video_window_fit_aspect_ratio_index",
      DEFAULT_WINDOW_FIT_ASPECT_RATIO_IDX, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, LAST_ASPECT_RATIO, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_aspect_ratio_index, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Window Aspect Ratio",
      "Set the aspect ratio of the window when fitting it to the display. 'Full' fills the space left by 'Window Margin'.")
S_UINT(video_window_fit_margin, VIDEO_WINDOW_FIT_MARGIN,
      "video_window_fit_margin",
      DEFAULT_WINDOW_FIT_MARGIN, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 1024, 8, 0, setting_action_ok_uint_special, NULL,
      "Window Margin",
      "Set the least space left between the window and the edges of the display when fitting the window to the display.")
S_UINT(video_window_opacity, VIDEO_WINDOW_OPACITY,
      "video_window_opacity",
      DEFAULT_WINDOW_OPACITY, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 1, 100, 1, 1, setting_action_ok_uint, NULL,
      "Window Opacity",
      "Set the window transparency.")
