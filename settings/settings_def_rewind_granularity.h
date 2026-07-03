/* Single-source definitions: rewind granularity setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _EX carry the extended descriptor arguments
 * (callbacks and ui type); every pass except the menu table
 * drops them and behaves exactly like the base row. */
#ifndef S_UINT_EX
#define S_UINT_EX(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, stax, selx, lfx, rtx, uix, us, sub) S_UINT(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub)
#endif
S_UINT_EX(rewind_buffer_size_step, REWIND_BUFFER_SIZE_STEP,
      "rewind_buffer_size_step",
      DEFAULT_REWIND_BUFFER_SIZE_STEP, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 100, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Rewind Buffer Size Step (MB)",
      "Each time the rewind buffer size value is increased or decreased, it will change by this amount.")
