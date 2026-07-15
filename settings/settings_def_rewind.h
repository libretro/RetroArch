/* Single-source definitions: rewind group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(rewind_enable, REWIND_ENABLE,
      "rewind_enable",
      DEFAULT_REWIND_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_REWIND_TOGGLE,
      "Rewind Support",
      "Return to a previous point in recent gameplay. This causes a severe performance hit when playing.")
S_UINT_EX(rewind_granularity, REWIND_GRANULARITY,
      "rewind_granularity",
      DEFAULT_REWIND_GRANULARITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 32768, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Rewind Frames",
      "The number of frames to rewind per step. Higher values increase the rewind speed.")
