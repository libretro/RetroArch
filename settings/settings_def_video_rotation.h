/* Single-source definitions: rotation group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(video_rotation, VIDEO_ROTATION,
      "video_rotation",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_rotation, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Video Rotation",
      "Forces a certain rotation of the video. The rotation is added to rotations which the core sets.")
S_UINT_EX(screen_orientation, SCREEN_ORIENTATION,
      "screen_orientation",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_screen_orientation, NULL, NULL, NULL, NULL, 0,
      "Screen Orientation",
      "Forces a certain orientation of the screen from the operating system.")
