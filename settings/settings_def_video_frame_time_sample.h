/* Single-source definitions: frame time sampling setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Toggling hides/shows 'frame_time_counter_auto_reset', so the
 * menu needs a rebuild on change: refresh handlers on all three. */
S_BOOL_EX(video_frame_time_sample_gated, VIDEO_FRAME_TIME_SAMPLE_GATED,
      "video_frame_time_sample_gated",
      DEFAULT_FRAME_TIME_SAMPLE_GATED, SD_FLAG_NONE, 0, 0,
      setting_bool_action_left_with_refresh, NULL, NULL, NULL,
      setting_bool_action_left_with_refresh,
      setting_bool_action_right_with_refresh, 0,
      "Sample Frame Time Only In Stable State",
      "Restrict 'Estimated Screen Refresh Rate' sampling to frames where content is running cleanly (not menu, not paused, not fast-forwarding, frame time within a sanity envelope). The diagnostic readout becomes a real signal at the cost of slower convergence after content load.")
