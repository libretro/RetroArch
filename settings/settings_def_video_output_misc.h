/* Single-source definitions: video output misc group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(video_scale_integer, VIDEO_SCALE_INTEGER,
      "video_scale_integer",
      DEFAULT_SCALE_INTEGER, SD_FLAG_NONE, 0, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Integer Scale",
      "Scale video in integer steps only. The base size depends on core-reported geometry and aspect ratio.")
S_UINT_EX(video_scale_integer_axis, VIDEO_SCALE_INTEGER_AXIS,
      "video_scale_integer_axis",
      DEFAULT_SCALE_INTEGER_AXIS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0, VIDEO_SCALE_INTEGER_AXIS_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_scale_integer_axis, NULL, NULL, NULL, NULL, 0,
      "Integer Scale Axis",
      "Scale either height or width, or both height and width. Half steps apply only to high resolution sources.")
S_UINT_EX(video_scale_integer_scaling, VIDEO_SCALE_INTEGER_SCALING,
      "video_scale_integer_scaling",
      DEFAULT_SCALE_INTEGER_SCALING, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0, VIDEO_SCALE_INTEGER_SCALING_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_scale_integer_scaling, NULL, NULL, NULL, NULL, 0,
      "Integer Scale Scaling",
      "Round down or up to the next integer. 'Smart' drops to underscale when image is cropped too much, and finally falls back to non-integer scaling if the underscale margins are too large.")
