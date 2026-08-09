/* Single-source definitions: sustained performance setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef ANDROID; the string
 * tables always carry this row via the strings pass. */
#if defined(ANDROID) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(sustained_performance_mode, SUSTAINED_PERFORMANCE_MODE,
      "sustained_performance_mode",
      DEFAULT_SUSTAINED_PERFORMANCE_MODE, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Sustained Performance Mode")
#endif
