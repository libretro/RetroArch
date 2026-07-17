/* Single-source definitions: playlist management group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_INT_EX(content_favorites_size, CONTENT_FAVORITES_SIZE,
      "content_favorites_size",
      DEFAULT_CONTENT_FAVORITES_SIZE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -1.0f, 9999.0f, 1.0f, -1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Favorites Size",
      "Limit the number of entries in the 'Favorites' playlist. Once the limit is reached, new additions will be prevented until old entries are removed. Setting a value of -1 allows 'unlimited' entries.\nWARNING: Reducing the value will delete existing entries!")
S_BOOL(playlist_entry_rename, PLAYLIST_ENTRY_RENAME,
      "playlist_entry_rename",
      DEFAULT_PLAYLIST_ENTRY_RENAME, SD_FLAG_NONE, 0, 0,
      "Allow to Rename Entries",
      "Allow playlist entries to be renamed.")
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(playlist_entry_remove_enable, PLAYLIST_ENTRY_REMOVE,
      "playlist_entry_remove_enable",
      DEFAULT_PLAYLIST_ENTRY_REMOVE_ENABLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_ENTRY_REMOVE_ENABLE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_playlist_entry_remove_enable, NULL, NULL, NULL, NULL, 0,
      "Allow to Remove Entries",
      "Allow playlist entries to be removed.")
#endif
S_BOOL(playlist_sort_alphabetical, PLAYLIST_SORT_ALPHABETICAL,
      "playlist_sort_alphabetical",
      DEFAULT_PLAYLIST_SORT_ALPHABETICAL, SD_FLAG_NONE, 0, 0,
      "Sort Playlists Alphabetically",
      "Sort content playlists in alphabetical order, excluding the 'History', 'Images', 'Music' and 'Videos' playlists.")
S_BOOL(playlist_use_old_format, PLAYLIST_USE_OLD_FORMAT,
      "playlist_use_old_format",
      DEFAULT_PLAYLIST_USE_OLD_FORMAT, SD_FLAG_NONE, 0, 0,
      "Save Playlists Using Old Format",
      "Write playlists using depreciated plain-text format. When disabled, playlists are formatted using JSON.")
/* Descriptor and configuration rows are #if defined(HAVE_COMPRESSION); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_COMPRESSION) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(playlist_compression, PLAYLIST_COMPRESSION,
      "playlist_compression",
      DEFAULT_PLAYLIST_COMPRESSION, SD_FLAG_NONE, 0, 0,
      "Compress Playlists",
      "Archive playlist data when writing to disk. Reduces file size and loading times at the expense of (negligibly) increased CPU usage. May be used with either old or new format playlists.")
#endif
S_BOOL_EX(playlist_show_sublabels, PLAYLIST_SHOW_SUBLABELS,
      "playlist_show_sublabels",
      DEFAULT_PLAYLIST_SHOW_SUBLABELS, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Show Playlist Sub-Labels",
      "Show additional information for each playlist entry, such as current core association and runtime (if available). Has a variable performance impact.")
