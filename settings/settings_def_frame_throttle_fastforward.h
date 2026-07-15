/* Single-source definitions: fast-forward ratio setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(slowmotion_ratio, SLOWMOTION_RATIO,
      "slowmotion_ratio",
      DEFAULT_SLOWMOTION_RATIO, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 10, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Slow-Motion Rate",
      "The rate that content will play when using slow-motion.")
