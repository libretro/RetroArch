/* Single-source definitions: run-ahead frame count.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_RUNAHEAD
S_UINT_CH(run_ahead_frames, RUN_AHEAD_FRAMES,
      "run_ahead_frames",
      1, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, MAX_RUNAHEAD_FRAMES, 1, 1, setting_action_ok_uint, NULL, ST_UI_TYPE_UINT_COMBOBOX, runahead_change_handler,
      "Number of Frames to Run Ahead",
      "The number of frames to run ahead of the emulated system. Reduces perceived latency of your controller inputs.")
#endif
