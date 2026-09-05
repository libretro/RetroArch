/* Single-source definitions: frame time counter reset setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(frame_time_counter_auto_reset, FRAME_TIME_COUNTER_AUTO_RESET,
      "frame_time_counter_auto_reset",
      DEFAULT_FRAME_TIME_COUNTER_AUTO_RESET, SD_FLAG_NONE, 0, 0,
      "Auto-Reset After Disruptive Events",
      "Clear the 'Estimated Screen Refresh Rate' sample buffer after fast-forwarding, save state, or load state. These operations introduce timing samples that don't reflect normal frame cadence and would skew the deviation measurement. Best-effort cleanup; has no effect when 'Sample Frame Time Only In Stable State' is enabled (which prevents the contamination at the source).")
