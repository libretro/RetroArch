/* Single-source definitions: settings tab visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_content_show_settings, CONTENT_SHOW_SETTINGS,
      "content_show_settings",
      DEFAULT_CONTENT_SHOW_SETTINGS, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'Settings'",
      "Show the 'Settings' menu.")
