/* Single-source definitions: restart visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_LAKKA; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_show_quit_retroarch" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_LV(menu_show_quit_retroarch, MENU_SHOW_QUIT_RETROARCH, MENU_SHOW_RESTART_RETROARCH,
      "menu_show_quit_retroarch",
      DEFAULT_MENU_SHOW_RESTART, SD_FLAG_NONE, 0, 0,
      "",
      "Show the 'Quit RetroArch' option in the Main Menu.")
#endif
#endif
