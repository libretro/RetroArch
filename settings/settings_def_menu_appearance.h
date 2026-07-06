/* Single-source definitions: menu appearance group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_pause_libretro, PAUSE_LIBRETRO,
      "menu_pause_libretro",
      true, SD_FLAG_CMD_APPLY_AUTO, 0, CMD_EVENT_MENU_PAUSE_LIBRETRO,
      "Pause Content When Menu Is Active",
      "Pause the content if the menu is active.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_savestate_resume, MENU_SAVESTATE_RESUME,
      "menu_savestate_resume",
      DEFAULT_MENU_SAVESTATE_RESUME, SD_FLAG_ADVANCED, 0, 0,
      "Resume Content After Using Save States",
      "Automatically close the menu and resume content after saving or loading a state. Disabling this can improve save state performance on very slow devices.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_insert_disk_resume, MENU_INSERT_DISK_RESUME,
      "menu_insert_disk_resume",
      DEFAULT_MENU_INSERT_DISK_RESUME, SD_FLAG_ADVANCED, 0, 0,
      "Resume Content After Changing Discs",
      "Automatically close the menu and resume content after inserting or loading a new disc.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(quit_on_close_content, QUIT_ON_CLOSE_CONTENT,
      "quit_on_close_content",
      DEFAULT_QUIT_ON_CLOSE_CONTENT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, QUIT_ON_CLOSE_CONTENT_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_quit_on_close_content, NULL, NULL, NULL, NULL, 0,
      "Quit on Close Content",
      "Automatically quit RetroArch when closing content. 'CLI' quits only when content is launched via command line.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_screensaver_timeout, MENU_SCREENSAVER_TIMEOUT,
      "menu_screensaver_timeout",
      DEFAULT_MENU_SCREENSAVER_TIMEOUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1800, 10, 0, setting_action_ok_uint_special, setting_get_string_representation_uint_menu_screensaver_timeout, NULL, NULL, NULL, NULL, 0,
      "Menu Screensaver Timeout",
      "While menu is active, a screensaver will be displayed after the specified period of inactivity.")
#endif
