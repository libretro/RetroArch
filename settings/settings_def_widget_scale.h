/* Single-source definitions: widget scale setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor rows are #ifdef HAVE_GFX_WIDGETS #if (defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) && ((defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))) || defined(SETTINGS_DEF_STRINGS_PASS) || defined(SETTINGS_DEF_CONFIG_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT(menu_widget_scale_factor, MENU_WIDGET_SCALE_FACTOR,
      "menu_widget_scale_factor",
      DEFAULT_MENU_WIDGET_SCALE_FACTOR, "%.2fx", SD_FLAG_NONE, 0, 0, 0, 0, 0, NULL, NULL,
      "Graphics Widgets Scale Override",
      "Apply a manual scaling factor override when drawing display widgets. Only applies when 'Scale Graphics Widgets Automatically' is disabled. Can be used to increase or decrease the size of decorated notifications, indicators and controls independently from the menu itself.")
#endif
#endif
