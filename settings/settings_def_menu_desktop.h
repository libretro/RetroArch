/* Single-source definitions: desktop menu group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(quick_menu_show_save_core_overrides, QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "quick_menu_show_save_core_overrides",
      DEFAULT_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES, SD_FLAG_NONE, 0, 0,
      "Show 'Save Core Overrides'",
      "Show the 'Save Core Overrides' option in the 'Overrides' menu.")
S_BOOL(quick_menu_show_save_content_dir_overrides, QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
      "quick_menu_show_save_content_dir_overrides",
      DEFAULT_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES, SD_FLAG_NONE, 0, 0,
      "Show 'Save Content Directory Overrides'",
      "Show the 'Save Content Directory Overrides' option in the 'Overrides' menu.")
S_BOOL(quick_menu_show_save_game_overrides, QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "quick_menu_show_save_game_overrides",
      DEFAULT_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES, SD_FLAG_NONE, 0, 0,
      "Show 'Save Game Overrides'",
      "Show the 'Save Game Overrides' option in the 'Overrides' menu.")
S_BOOL(quick_menu_show_information, QUICK_MENU_SHOW_INFORMATION,
      "quick_menu_show_information",
      DEFAULT_QUICK_MENU_SHOW_INFORMATION, SD_FLAG_NONE, 0, 0,
      "Show 'Information'",
      "Show the 'Information' option.")
/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(quick_menu_show_download_thumbnails, QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
      "quick_menu_show_download_thumbnails",
      DEFAULT_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS, SD_FLAG_NONE, 0, 0,
      "Show 'Download Thumbnails'",
      "Show the 'Download Thumbnails' option when content is not running.")
#endif
