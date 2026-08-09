/* Single-source definitions: reboot and shutdown visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_reboot, MENU_SHOW_REBOOT,
      "menu_show_reboot",
      DEFAULT_MENU_SHOW_REBOOT, SD_FLAG_NONE, 0, 0,
      "Show 'Reboot'",
      "Show the 'Reboot' option.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(HAVE_ODROIDGO2) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_shutdown, MENU_SHOW_SHUTDOWN,
      "menu_show_shutdown",
      DEFAULT_MENU_SHOW_SHUTDOWN, SD_FLAG_NONE, 0, 0,
      "Show 'Shutdown'",
      "Show the 'Shutdown' option.")
#endif
