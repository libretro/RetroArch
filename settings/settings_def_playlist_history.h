/* Single-source definitions: history size group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(playlist_show_history_icons, PLAYLIST_SHOW_HISTORY_ICONS,
      "playlist_show_history_icons",
      DEFAULT_PLAYLIST_SHOW_HISTORY_ICONS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_SHOW_HISTORY_ICONS_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_playlist_show_history_icons, NULL, NULL, NULL, NULL, 0,
      "Show Content Specific Icons in History and Favorites",
      "Show specific icons for each history and favorites playlist entry. Has a variable performance impact.")
#endif
