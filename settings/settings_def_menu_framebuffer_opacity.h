/* Single-source definitions: framebuffer opacity group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(menu_framebuffer_opacity, MENU_FRAMEBUFFER_OPACITY,
      "menu_framebuffer_opacity",
      DEFAULT_MENU_FRAMEBUFFER_OPACITY, "%.3f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 1.0, 0.010, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Opacity",
      "Modify the opacity of the default menu background.")
#endif
