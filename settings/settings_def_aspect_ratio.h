/* Single-source definitions: aspect ratio group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(video_aspect_ratio_idx, VIDEO_ASPECT_RATIO_INDEX,
      "aspect_ratio_index",
      DEFAULT_ASPECT_RATIO_IDX, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_SET_ASPECT_RATIO, 0, LAST_ASPECT_RATIO, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_aspect_ratio_index, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Aspect Ratio",
      "Set display aspect ratio.")
S_FLOAT(video_aspect_ratio, VIDEO_ASPECT_RATIO,
      "video_aspect_ratio",
      DEFAULT_ASPECT_RATIO, "%.2f", SD_FLAG_NONE, SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN, CMD_EVENT_VIDEO_SET_ASPECT_RATIO, 0.1, 16.0, 0.01, NULL, NULL,
      "Config Aspect Ratio",
      "Floating point value for video aspect ratio (width / height).")
S_INT_AT(offsetof(settings_t, video_vp_custom.x), VIDEO_VIEWPORT_CUSTOM_X,
      "video_viewport_custom_x",
      0, SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, -9999, 9999, 1, -9999, NULL, NULL,
      "Custom Aspect Ratio (X Position)",
      "Custom viewport offset used for defining the X-axis position of the viewport.")
S_INT_AT(offsetof(settings_t, video_vp_custom.y), VIDEO_VIEWPORT_CUSTOM_Y,
      "video_viewport_custom_y",
      0, SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, -9999, 9999, 1, -9999, NULL, NULL,
      "Custom Aspect Ratio (Y Position)",
      "Custom viewport offset used for defining the Y-axis position of the viewport.")
