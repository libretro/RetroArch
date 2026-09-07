/* Single-source definitions: horizontal animation setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_XMB) || defined (HAVE_OZONE); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined (HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_horizontal_animation, MENU_HORIZONTAL_ANIMATION,
      "menu_horizontal_animation",
      DEFAULT_MENU_HORIZONTAL_ANIMATION, SD_FLAG_ADVANCED, 0, 0,
      "Horizontal Animation",
      "Enable horizontal animation for the menu. This will have a performance hit.")
#endif
