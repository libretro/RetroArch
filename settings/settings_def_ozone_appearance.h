/* Single-source definitions: Ozone appearance group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_favorites, CONTENT_SHOW_FAVORITES,
      "content_show_favorites",
      DEFAULT_CONTENT_SHOW_FAVORITES, SD_FLAG_NONE, 0, 0,
      "Show 'Favorites'",
      "Show the 'Favorites' menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_favorites_first, CONTENT_SHOW_FAVORITES_FIRST,
      "content_show_favorites_first",
      DEFAULT_CONTENT_SHOW_FAVORITES_FIRST, SD_FLAG_NONE, 0, 0,
      "Show Favorites First",
      "Show 'Favorites' before 'History'.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_IMAGEVIEWER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_IMAGEVIEWER) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_IMAGEVIEWER); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_IMAGEVIEWER))
S_BOOL(menu_content_show_images, CONTENT_SHOW_IMAGES,
      "content_show_images",
      DEFAULT_CONTENT_SHOW_IMAGES, SD_FLAG_NONE, 0, 0,
      "Show 'Images'",
      "Show the 'Images' menu.")
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_music, CONTENT_SHOW_MUSIC,
      "content_show_music",
      DEFAULT_CONTENT_SHOW_MUSIC, SD_FLAG_NONE, 0, 0,
      "Show 'Music'",
      "Show the 'Music' menu.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_FFMPEG) || defined(HAVE_MPV); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_FFMPEG) || defined(HAVE_MPV)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && (defined(HAVE_FFMPEG) || defined(HAVE_MPV)); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && (defined(HAVE_FFMPEG) || defined(HAVE_MPV)))
S_BOOL(menu_content_show_video, CONTENT_SHOW_VIDEO,
      "content_show_video",
      DEFAULT_CONTENT_SHOW_VIDEO, SD_FLAG_NONE, 0, 0,
      "Show 'Videos'",
      "Show the 'Videos' menu.")
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_history, CONTENT_SHOW_HISTORY,
      "content_show_history",
      DEFAULT_CONTENT_SHOW_HISTORY, SD_FLAG_NONE, 0, 0,
      "Show 'History'",
      "Show the recent history menu.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_NETWORKING))
S_UINT_EX(menu_content_show_netplay, CONTENT_SHOW_NETPLAY,
      "content_show_netplay",
      DEFAULT_CONTENT_SHOW_NETPLAY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_ADD_CONTENT_ENTRY_DISPLAY_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_add_content_entry_display_type, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Show 'Netplay'",
      "Show the 'Netplay' entry inside the Main Menu or Playlists.")
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_content_show_add_entry, CONTENT_SHOW_ADD_ENTRY,
      "content_show_add_entry",
      DEFAULT_MENU_CONTENT_SHOW_ADD_ENTRY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MENU_ADD_CONTENT_ENTRY_DISPLAY_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_add_content_entry_display_type, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Show 'Import Content'",
      "Show the 'Import Content' entry inside the Main Menu or Playlists.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_playlists, CONTENT_SHOW_PLAYLISTS,
      "content_show_playlists",
      DEFAULT_CONTENT_SHOW_PLAYLISTS, SD_FLAG_NONE, 0, 0,
      "Show 'Playlists'",
      "Show the playlists in Main Menu. Ignored in GLUI if playlist tabs and navbar are enabled.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_content_show_playlist_tabs, CONTENT_SHOW_PLAYLIST_TABS,
      "content_show_playlist_tabs",
      DEFAULT_CONTENT_SHOW_PLAYLIST_TABS, SD_FLAG_NONE, 0, 0,
      "Show Playlist Tabs",
      "Show the playlist tabs. Does not affect RGUI. Navbar must be enabled in GLUI.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_LIBRETRODB); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LIBRETRODB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU) && (defined(HAVE_LIBRETRODB)); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && (defined(HAVE_LIBRETRODB)))
S_BOOL(menu_content_show_explore, CONTENT_SHOW_EXPLORE,
      "content_show_explore",
      DEFAULT_MENU_CONTENT_SHOW_EXPLORE, SD_FLAG_NONE, 0, 0,
      "Show 'Explore'",
      "Show the content explorer option.")
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_content_show_contentless_cores, CONTENT_SHOW_CONTENTLESS_CORES,
      "content_show_contentless_cores",
      DEFAULT_MENU_CONTENT_SHOW_CONTENTLESS_CORES, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, MENU_CONTENTLESS_CORES_DISPLAY_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_contentless_cores_display_type, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Show 'Contentless Cores'",
      "Specify the type of core (if any) to show in the 'Contentless Cores' menu. When set to 'Custom', individual core visibility may be toggled via the 'Manage Cores' menu.")
#endif
