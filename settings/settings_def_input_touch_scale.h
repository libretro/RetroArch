/* Single-source definitions: touch scale setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(input_touch_scale, INPUT_TOUCH_SCALE,
      "input_touch_scale",
      DEFAULT_TOUCH_SCALE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 4, 1, 1, setting_action_ok_uint, setting_get_string_representation_input_touch_scale, NULL, NULL, NULL, NULL, 0,
      "Touch Scale",
      "Adjust x/y scale of touchscreen coordinates to accommodate OS-level display scaling.")
