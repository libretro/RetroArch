/* Single-source definitions: tenth main menu action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if !defined(IOS) && !defined(HAVE_LAKKA); the string
 * tables always carry this row via the strings pass. */
#if (!defined(IOS) && !defined(HAVE_LAKKA)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(RESTART_RETROARCH,
      "restart_retroarch", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_RESTART_RETROARCH,
      "Restart",
      "Restart RetroArch application.")
#endif
