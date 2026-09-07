/* Single-source definitions: achievement visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_verbose_enable, CHEEVOS_VERBOSE_ENABLE,
      "cheevos_verbose_enable",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Verbose Messages",
      "Shows additional diagnostic and error messages.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cheevos_visibility_account, CHEEVOS_VISIBILITY_ACCOUNT,
      "cheevos_visibility_account",
      DEFAULT_CHEEVOS_VISIBILITY_ACCOUNT, SD_FLAG_ADVANCED, 0, 0,
      "Login Messages",
      "Shows messages related to RetroAchievements account login.")
#endif
