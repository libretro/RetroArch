/* Single-source definitions: Steam core manager visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_MIST
S_BOOL(menu_show_core_manager_steam, MENU_SHOW_CORE_MANAGER_STEAM,
      "menu_show_core_manager_steam",
      DEFAULT_MENU_SHOW_CORE_MANAGER_STEAM, SD_FLAG_NONE, 0, 0,
      "Show 'Manage cores'",
      "Show the 'Manage cores' option in the Main Menu.")
#endif
