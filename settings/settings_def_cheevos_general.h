/* Single-source definitions: achievements general group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(cheevos_visibility_summary, CHEEVOS_VISIBILITY_SUMMARY,
      "cheevos_visibility_summary",
      DEFAULT_CHEEVOS_VISIBILITY_SUMMARY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RCHEEVOS_SUMMARY_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_cheevos_visibility_summary, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Startup Summary",
      "Shows information about the game being loaded and the user's current progress. 'All Identified Games' will show summary for games with no published achievements.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_unlock, CHEEVOS_VISIBILITY_UNLOCK,
      "cheevos_visibility_unlock",
      DEFAULT_CHEEVOS_VISIBILITY_UNLOCK, SD_FLAG_NONE, 0, 0,
      "Unlock Notifications",
      "Shows a notification when an achievement is unlocked.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_mastery, CHEEVOS_VISIBILITY_MASTERY,
      "cheevos_visibility_mastery",
      DEFAULT_CHEEVOS_VISIBILITY_MASTERY, SD_FLAG_NONE, 0, 0,
      "Mastery Notifications",
      "Shows a notification when all achievements for a game are unlocked.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_challenge_indicators, CHEEVOS_CHALLENGE_INDICATORS,
      "cheevos_challenge_indicators",
      true, SD_FLAG_NONE, 0, 0,
      "Active Challenge Indicators",
      "Shows on-screen indicators while certain achievements can be earned.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_progress_tracker, CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
      "cheevos_visibility_progress_tracker",
      DEFAULT_CHEEVOS_VISIBILITY_PROGRESS_TRACKER, SD_FLAG_NONE, 0, 0,
      "Progress Indicator",
      "Shows an on-screen indicator when progress is made towards certain achievements.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_lboard_start, CHEEVOS_VISIBILITY_LBOARD_START,
      "cheevos_visibility_lboard_start",
      DEFAULT_CHEEVOS_VISIBILITY_LBOARD_START, SD_FLAG_NONE, 0, 0,
      "Leaderboard Start Messages",
      "Shows a description of a leaderboard when it becomes active.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_lboard_submit, CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
      "cheevos_visibility_lboard_submit",
      DEFAULT_CHEEVOS_VISIBILITY_LBOARD_SUBMIT, SD_FLAG_NONE, 0, 0,
      "Leaderboard Submit Messages",
      "Shows a message with the value being submitted when a leaderboard attempt is completed.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_lboard_cancel, CHEEVOS_VISIBILITY_LBOARD_CANCEL,
      "cheevos_visibility_lboard_cancel",
      DEFAULT_CHEEVOS_VISIBILITY_LBOARD_CANCEL, SD_FLAG_NONE, 0, 0,
      "Leaderboard Failed Messages",
      "Shows a message when a leaderboard attempt fails.")
#endif
