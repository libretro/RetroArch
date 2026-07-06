/* Single-source definitions: playlist display group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(playlist_sublabel_runtime_type, PLAYLIST_SUBLABEL_RUNTIME_TYPE,
      "playlist_sublabel_runtime_type",
      DEFAULT_PLAYLIST_SUBLABEL_RUNTIME_TYPE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_RUNTIME_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_playlist_sublabel_runtime_type, NULL, NULL, NULL, NULL, 0,
      "Playlist Sub-Label Runtime",
      "Select which type of runtime log record to display on playlist sublabels. The corresponding runtime log must be enabled via the 'Saving' options menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(playlist_sublabel_last_played_style, PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
      "playlist_sublabel_last_played_style",
      DEFAULT_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_LAST_PLAYED_STYLE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_playlist_sublabel_last_played_style, NULL, NULL, NULL, NULL, 0,
      "'Last Played' Date and Time Style",
      "Set the style of the date and time displayed for 'Last Played' timestamp information. '(AM/PM)' options will have a small performance impact on some platforms.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(playlist_show_inline_core_name, PLAYLIST_SHOW_INLINE_CORE_NAME,
      "playlist_show_inline_core_name",
      DEFAULT_PLAYLIST_SHOW_INLINE_CORE_NAME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PLAYLIST_INLINE_CORE_DISPLAY_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_playlist_inline_core_display_type, NULL, NULL, NULL, NULL, 0,
      "Show Associated Cores in Playlists",
      "Specify when to tag playlist entries with the currently associated core (if any). This setting is ignored when playlist sublabels are enabled.")
#endif
S_BOOL(playlist_fuzzy_archive_match, PLAYLIST_FUZZY_ARCHIVE_MATCH,
      "playlist_fuzzy_archive_match",
      DEFAULT_PLAYLIST_FUZZY_ARCHIVE_MATCH, SD_FLAG_NONE, 0, 0,
      "Fuzzy Archive Matching",
      "When searching playlists for entries associated with compressed files, match only the archive file name instead of [file name]+[content]. Enable this to avoid duplicate content history entries when loading compressed files.")
S_BOOL(scan_without_core_match, SCAN_WITHOUT_CORE_MATCH,
      "scan_without_core_match",
      DEFAULT_SCAN_WITHOUT_CORE_MATCH, SD_FLAG_NONE, 0, 0,
      "Scan Without Core Match",
      "Allow content to be scanned and added to a playlist without a core installed that supports it.")
S_BOOL(scan_serial_and_crc, SCAN_SERIAL_AND_CRC,
      "scan_serial_and_crc",
      DEFAULT_SCAN_SERIAL_AND_CRC, SD_FLAG_NONE, 0, 0,
      "Scan Checks CRC on Possible Duplicates",
      "Sometimes ISOs duplicate serials, particularly with PSP/PSN titles. Relying solely on the serial can sometimes cause the scanner to put content in the wrong system. This adds a CRC check, which slows down scanning considerably, but may be more accurate.")
S_BOOL(playlist_portable_paths, PLAYLIST_PORTABLE_PATHS,
      "playlist_portable_paths",
      DEFAULT_PLAYLIST_PORTABLE_PATHS, SD_FLAG_NONE, 0, 0,
      "Portable Playlists",
      "When enabled, and 'File Browser' directory is also selected, the current value of parameter 'File Browser' is saved in the playlist. When the playlist is loaded on another system where the same option is enabled, the value of parameter 'File Browser' is compared with the playlist value; if different, the playlist entries' paths are automatically fixed.")
S_BOOL(playlist_use_filename, PLAYLIST_USE_FILENAME,
      "playlist_use_filename",
      DEFAULT_PLAYLIST_USE_FILENAME, SD_FLAG_NONE, 0, 0,
      "Use Filenames for Thumbnail Matching",
      "When enabled, will find thumbnails by the entry's filename, rather than its label.")
S_BOOL(playlist_allow_non_png, PLAYLIST_ALLOW_NON_PNG,
      "playlist_allow_non_png",
      DEFAULT_PLAYLIST_ALLOW_NON_PNG, SD_FLAG_NONE, 0, 0,
      "Allow All Supported Image Types for Thumbnails",
      "When enabled, local thumbnails can be added in all image types supported by RetroArch (such as jpeg). May have a minor performance impact.")
