/* Single-source definitions: desktop menu group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_NS_H
#define S_BOOL_NS_H S_BOOL_NS
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_scroll_delay, MENU_SCROLL_DELAY,
      "menu_scroll_delay",
      DEFAULT_MENU_SCROLL_DELAY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 999, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Scroll Delay",
      "Initial delay in milliseconds when holding a direction to scroll.")
#endif
/* Descriptor and configuration rows are guarded by the same condition
 * as HAVE_COMPANION_WIMP in ui/ui_companion_driver.h (Qt, Cocoa, or
 * desktop Win32); the string tables always carry this row via the
 * strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(ui_companion_enable, UI_COMPANION_ENABLE,
      "ui_companion_enable",
      DEFAULT_UI_COMPANION_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "UI Companion")
#endif
/* Descriptor and configuration rows are guarded by the same condition
 * as HAVE_COMPANION_WIMP in ui/ui_companion_driver.h (Qt, Cocoa, or
 * desktop Win32); the string tables always carry this row via the
 * strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS_H(ui_companion_start_on_boot, UI_COMPANION_START_ON_BOOT,
      "ui_companion_start_on_boot",
      DEFAULT_UI_COMPANION_START_ON_BOOT, SD_FLAG_ADVANCED, 0, 0,
      "Start UI Companion on Boot")
#endif
/* Descriptor and configuration rows are guarded by the same condition
 * as HAVE_COMPANION_WIMP in ui/ui_companion_driver.h (Qt, Cocoa, or
 * desktop Win32); the string tables always carry this row via the
 * strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX_NS(desktop_menu_enable, DESKTOP_MENU_ENABLE,
      "desktop_menu_enable",
      DEFAULT_DESKTOP_MENU_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Desktop Menu (Restart required)")
#endif
/* Descriptor and configuration rows are guarded by the same condition
 * as HAVE_COMPANION_WIMP in ui/ui_companion_driver.h (Qt, Cocoa, or
 * desktop Win32); the string tables always carry this row via the
 * strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration table registers this row by hand in
 * configuration.c because it carries no default there; the
 * generated row is for the other passes. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(ui_companion_toggle, UI_COMPANION_TOGGLE,
      "ui_companion_toggle",
      DEFAULT_UI_COMPANION_TOGGLE, SD_FLAG_NONE, 0, 0,
      "Open Desktop Menu on Startup")
#endif
#endif

/* Desktop companion presentation settings shared by every companion
 * backend. Same guard as HAVE_COMPANION_WIMP. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(desktop_menu_suggest_loaded_core_first, DESKTOP_MENU_SUGGEST_LOADED_CORE_FIRST,
      "desktop_menu_suggest_loaded_core_first",
      DEFAULT_DESKTOP_MENU_SUGGEST_LOADED_CORE_FIRST, SD_FLAG_ADVANCED, 0, 0,
      "Desktop Menu: Suggest Loaded Core First")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(desktop_menu_save_last_tab, DESKTOP_MENU_SAVE_LAST_TAB,
      "desktop_menu_save_last_tab",
      DEFAULT_DESKTOP_MENU_SAVE_LAST_TAB, SD_FLAG_ADVANCED, 0, 0,
      "Desktop Menu: Remember Last Tab")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(desktop_menu_save_geometry, DESKTOP_MENU_SAVE_GEOMETRY,
      "desktop_menu_save_geometry",
      DEFAULT_DESKTOP_MENU_SAVE_GEOMETRY, SD_FLAG_ADVANCED, 0, 0,
      "Desktop Menu: Remember Window Geometry")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(desktop_menu_show_welcome_screen, DESKTOP_MENU_SHOW_WELCOME_SCREEN,
      "desktop_menu_show_welcome_screen",
      DEFAULT_DESKTOP_MENU_SHOW_WELCOME_SCREEN, SD_FLAG_ADVANCED, 0, 0,
      "Desktop Menu: Show Welcome Screen")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(desktop_menu_scan_finish_confirm, DESKTOP_MENU_SCAN_FINISH_CONFIRM,
      "desktop_menu_scan_finish_confirm",
      DEFAULT_DESKTOP_MENU_SCAN_FINISH_CONFIRM, SD_FLAG_ADVANCED, 0, 0,
      "Desktop Menu: Confirm When Scan Finishes")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_view_type, DESKTOP_MENU_VIEW_TYPE,
      "desktop_menu_view_type",
      DEFAULT_DESKTOP_MENU_VIEW_TYPE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: View Type",
      "0 = list, 1 = icons.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_thumbnail_type, DESKTOP_MENU_THUMBNAIL_TYPE,
      "desktop_menu_thumbnail_type",
      DEFAULT_DESKTOP_MENU_THUMBNAIL_TYPE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Thumbnail Type",
      "0 = boxart, 1 = screenshot, 2 = title screen, 3 = logo.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_last_tab, DESKTOP_MENU_LAST_TAB,
      "desktop_menu_last_tab",
      DEFAULT_DESKTOP_MENU_LAST_TAB, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Last Tab",
      "0 = playlists, 1 = file browser.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_thumbnail_cache_limit, DESKTOP_MENU_THUMBNAIL_CACHE_LIMIT,
      "desktop_menu_thumbnail_cache_limit",
      DEFAULT_DESKTOP_MENU_THUMBNAIL_CACHE_LIMIT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 100000, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Thumbnail Cache Limit",
      "Number of thumbnails kept in memory.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_thumbnail_max_size, DESKTOP_MENU_THUMBNAIL_MAX_SIZE,
      "desktop_menu_thumbnail_max_size",
      DEFAULT_DESKTOP_MENU_THUMBNAIL_MAX_SIZE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 4096, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Thumbnail Max Size",
      "Longest edge in pixels; 0 = unlimited.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_thumbnail_quality, DESKTOP_MENU_THUMBNAIL_QUALITY,
      "desktop_menu_thumbnail_quality",
      DEFAULT_DESKTOP_MENU_THUMBNAIL_QUALITY, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 100, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Thumbnail Quality",
      "0 = default.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_icon_view_zoom, DESKTOP_MENU_ICON_VIEW_ZOOM,
      "desktop_menu_icon_view_zoom",
      DEFAULT_DESKTOP_MENU_ICON_VIEW_ZOOM, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 100, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Icon View Zoom",
      "Zoom level of the icon view.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_all_playlists_list_max_count, DESKTOP_MENU_ALL_PLAYLISTS_LIST_MAX_COUNT,
      "desktop_menu_all_playlists_list_max_count",
      DEFAULT_DESKTOP_MENU_ALL_PLAYLISTS_LIST_MAX_COUNT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 100000, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: All Playlists List Max Count",
      "Entries shown for All Playlists in list view; 0 = unlimited.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_all_playlists_grid_max_count, DESKTOP_MENU_ALL_PLAYLISTS_GRID_MAX_COUNT,
      "desktop_menu_all_playlists_grid_max_count",
      DEFAULT_DESKTOP_MENU_ALL_PLAYLISTS_GRID_MAX_COUNT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 100000, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: All Playlists Grid Max Count",
      "Entries shown for All Playlists in icon view; 0 = unlimited.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_theme, DESKTOP_MENU_THEME,
      "desktop_menu_theme",
      DEFAULT_DESKTOP_MENU_THEME, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Theme",
      "0 = system default, 1 = dark, 2 = custom stylesheet.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_window_x, DESKTOP_MENU_WINDOW_X,
      "desktop_menu_window_x",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32767, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Window X",
      "Saved window position; used when Remember Window Geometry is on.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_window_y, DESKTOP_MENU_WINDOW_Y,
      "desktop_menu_window_y",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32767, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Window Y",
      "Saved window position; used when Remember Window Geometry is on.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_window_width, DESKTOP_MENU_WINDOW_WIDTH,
      "desktop_menu_window_width",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32767, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Window Width",
      "Saved window size; used when Remember Window Geometry is on.")
#endif
#if (defined(HAVE_QT) || defined(HAVE_COCOA) || (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(desktop_menu_window_height, DESKTOP_MENU_WINDOW_HEIGHT,
      "desktop_menu_window_height",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32767, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Desktop Menu: Window Height",
      "Saved window size; used when Remember Window Geometry is on.")
#endif
