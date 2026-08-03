/* Single-source definitions: restart visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if !(defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2)) #if !defined(IOS); the string
 * tables always carry this row via the strings pass. */
#if (!(defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2))) && !defined(IOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_restart_retroarch, MENU_SHOW_RESTART_RETROARCH,
      "menu_show_restart_retroarch",
      DEFAULT_MENU_SHOW_RESTART, SD_FLAG_NONE, 0, 0,
      "Show 'Restart RetroArch'",
      "Show the 'Restart RetroArch' option in the Main Menu.")
#endif
