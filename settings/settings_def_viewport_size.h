/* Single-source definitions: custom viewport size group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_AT_EX(offsetof(settings_t, video_vp_custom.width), VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "video_viewport_custom_width",
      0, SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 1, 9999, 1, 0, NULL, setting_get_string_representation_uint_custom_vp_width, setting_action_start_custom_vp_width, NULL, setting_uint_action_left_custom_vp_width, setting_uint_action_right_custom_vp_width, 0,
      "Custom Aspect Ratio (Width)",
      "Custom viewport width that is used if the Aspect Ratio is set to 'Custom Aspect Ratio'.")
S_UINT_AT_EX(offsetof(settings_t, video_vp_custom.height), VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "video_viewport_custom_height",
      0, SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 1, 9999, 1, 0, NULL, setting_get_string_representation_uint_custom_vp_height, setting_action_start_custom_vp_height, NULL, setting_uint_action_left_custom_vp_height, setting_uint_action_right_custom_vp_height, 0,
      "Custom Aspect Ratio (Height)",
      "Custom viewport height that is used if the Aspect Ratio is set to 'Custom Aspect Ratio'.")
