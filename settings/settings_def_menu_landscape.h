/* Single-source definitions: landscape layout group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_BOOL(menu_rgui_shadows, MENU_RGUI_SHADOWS,
      "menu_rgui_shadows",
      DEFAULT_RGUI_SHADOWS, SD_FLAG_NONE, 0, 0,
      "Shadow Effects",
      "Enable drop shadows for menu text, borders and thumbnails. Has a modest performance impact.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_UINT_EX(menu_rgui_particle_effect, MENU_RGUI_PARTICLE_EFFECT,
      "rgui_particle_effect",
      DEFAULT_RGUI_PARTICLE_EFFECT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_PARTICLE_EFFECT_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_particle_effect, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Background Animation",
      "Enable background particle animation effect. Has a significant performance impact.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_FLOAT_EX(menu_rgui_particle_effect_speed, MENU_RGUI_PARTICLE_EFFECT_SPEED,
      "rgui_particle_effect_speed",
      DEFAULT_RGUI_PARTICLE_EFFECT_SPEED, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.1, 10.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Background Animation Speed",
      "Adjust speed of background particle animation effects.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_BOOL(menu_rgui_particle_effect_screensaver, MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
      "rgui_particle_effect_screensaver",
      DEFAULT_RGUI_PARTICLE_EFFECT_SCREENSAVER, SD_FLAG_NONE, 0, 0,
      "Screensaver Background Animation",
      "Display background particle animation effect while menu screensaver is active.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_BOOL(menu_rgui_extended_ascii, MENU_RGUI_EXTENDED_ASCII,
      "rgui_extended_ascii",
      DEFAULT_RGUI_EXTENDED_ASCII, SD_FLAG_NONE, 0, 0,
      "Extended ASCII Support",
      "Enable display of non-standard ASCII characters. Required for compatibility with certain non-English Western languages. Has a moderate performance impact.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_RGUI); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_RGUI))
S_BOOL(menu_rgui_switch_icons, MENU_RGUI_SWITCH_ICONS,
      "rgui_switch_icons",
      DEFAULT_RGUI_SWITCH_ICONS, SD_FLAG_NONE, 0, 0,
      "Switch Icons",
      "Use icons instead of ON/OFF text to represent 'toggle switch' menu settings entries.")
#endif
