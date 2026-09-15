/* Single-source definitions: Steam settings visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_MIST; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MIST) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(settings_show_steam, SETTINGS_SHOW_STEAM,
      "settings_show_steam",
      DEFAULT_SETTINGS_SHOW_STEAM, SD_FLAG_NONE, 0, 0,
      "Show 'Steam'",
      "Show 'Steam' settings.")
#endif
