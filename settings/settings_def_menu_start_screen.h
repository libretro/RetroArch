/* Single-source definitions: start screen setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* FIXME Not RGUI specific */ /* FIXME Not RGUI specific */
/* The configuration table registers this row by hand in
 * configuration.c because it carries no default there; the
 * generated row is for the other passes. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_show_start_screen, RGUI_SHOW_START_SCREEN,
      "rgui_show_start_screen",
      DEFAULT_MENU_SHOW_START_SCREEN, SD_FLAG_ADVANCED, 0, 0,
      "Display Start Screen",
      "Show startup screen in menu. This is automatically set to false after the program starts for the first time.")
#endif
