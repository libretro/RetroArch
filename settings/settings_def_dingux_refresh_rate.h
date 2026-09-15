/* Single-source definitions: Dingux refresh rate group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if defined(DINGUX) && defined(DINGUX_BETA)
S_UINT_EX(video_dingux_refresh_rate, VIDEO_DINGUX_REFRESH_RATE,
      "video_dingux_refresh_rate",
      DEFAULT_DINGUX_REFRESH_RATE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, DINGUX_REFRESH_RATE_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_dingux_refresh_rate, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Vertical Refresh Rate",
      "Set vertical refresh rate of the display. '50 Hz' will enable smooth video when running PAL content.")
#endif
