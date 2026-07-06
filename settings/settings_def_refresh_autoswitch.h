/* Single-source definitions: refresh rate autoswitch group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(video_autoswitch_refresh_rate, VIDEO_AUTOSWITCH_REFRESH_RATE,
      "video_autoswitch_refresh_rate",
      DEFAULT_AUTOSWITCH_REFRESH_RATE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, AUTOSWITCH_REFRESH_RATE_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_autoswitch_refresh_rate, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Automatic Refresh Rate Switch",
      "Switch screen refresh rate automatically based on current content.")
S_FLOAT(video_autoswitch_pal_threshold, VIDEO_AUTOSWITCH_PAL_THRESHOLD,
      "video_autoswitch_pal_threshold",
      DEFAULT_AUTOSWITCH_PAL_THRESHOLD, "%.3f Hz", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 50, 56, 0.1, NULL, NULL,
      "Automatic Refresh Rate PAL Threshold",
      "Maximum refresh rate to be considered PAL.")
