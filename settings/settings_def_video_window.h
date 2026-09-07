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
S_UINT(window_position_width, VIDEO_WINDOW_WIDTH,
      "video_window_width",
      DEFAULT_WINDOW_WIDTH, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 7680, 8, 0, setting_action_ok_uint_special, NULL,
      "Window Width",
      "Set the custom width for the display window.")
#endif
/* config key "video_windowed_position_height" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT(window_position_height, VIDEO_WINDOW_HEIGHT,
      "video_window_height",
      DEFAULT_WINDOW_HEIGHT, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 4320, 8, 0, setting_action_ok_uint_special, NULL,
      "Window Height",
      "Set the custom height for the display window.")
#endif
S_UINT(window_auto_width_max, VIDEO_WINDOW_AUTO_WIDTH_MAX,
      "video_window_auto_width_max",
      DEFAULT_WINDOW_AUTO_WIDTH_MAX, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 7680, 8, 0, setting_action_ok_uint_special, NULL,
      "Maximum Window Width",
      "Set the maximum width of the display window when automatically resizing based on 'Windowed Scale'.")
S_UINT(window_auto_height_max, VIDEO_WINDOW_AUTO_HEIGHT_MAX,
      "video_window_auto_height_max",
      DEFAULT_WINDOW_AUTO_HEIGHT_MAX, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 4320, 8, 0, setting_action_ok_uint_special, NULL,
      "Maximum Window Height",
      "Set the maximum height of the display window when automatically resizing based on 'Windowed Scale'.")
S_UINT(video_window_opacity, VIDEO_WINDOW_OPACITY,
      "video_window_opacity",
      DEFAULT_WINDOW_OPACITY, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 1, 100, 1, 1, setting_action_ok_uint, NULL,
      "Window Opacity",
      "Set the window transparency.")
