/* Single-source definitions: playlist flag group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_OZONE) || defined(HAVE_XMB); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_OZONE) || defined(HAVE_XMB)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL_EX(ozone_truncate_playlist_name, OZONE_TRUNCATE_PLAYLIST_NAME,
      "ozone_truncate_playlist_name",
      DEFAULT_OZONE_TRUNCATE_PLAYLIST_NAME, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Truncate Playlist Names (Restart required)",
      "Remove the manufacturer names from the playlists. For example, 'Sony - PlayStation' becomes 'PlayStation'.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_OZONE) || defined(HAVE_XMB); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_OZONE) || defined(HAVE_XMB)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(ozone_sort_after_truncate_playlist_name, OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
      "ozone_sort_after_truncate_playlist_name",
      DEFAULT_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME, SD_FLAG_NONE, 0, 0,
      "Sort Playlists After Name Truncation (Restart required)",
      "Playlists will be re-sorted in alphabetical order after removing the manufacturer component of their names.")
#endif
#endif
