/* Single-source definitions: online updater visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_online_updater, MENU_SHOW_ONLINE_UPDATER,
      "menu_show_online_updater",
      DEFAULT_MENU_SHOW_ONLINE_UPDATER, SD_FLAG_NONE, 0, 0,
      "Show 'Online Updater'",
      "Show the 'Online Updater' option in the Main Menu.")
#endif
