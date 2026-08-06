/* Single-source definitions: run-ahead warnings setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_RUNAHEAD; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_RUNAHEAD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(run_ahead_hide_warnings, RUN_AHEAD_HIDE_WARNINGS,
      "run_ahead_hide_warnings",
      DEFAULT_RUN_AHEAD_HIDE_WARNINGS, SD_FLAG_ADVANCED, 0, 0,
      "Hide Run-Ahead Warnings",
      "Hide the warning message that appears when using Run-Ahead and the core does not support save states.")
#endif
