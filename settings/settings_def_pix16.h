/* Single-source definitions: pix16 menu group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_BOOL(menu_pix16_widescreen, MENU_PIX16_WIDESCREEN,
      "menu_pix16_widescreen",
      DEFAULT_PIX16_WIDESCREEN, SD_FLAG_NONE, 0, 0,
      "Widescreen Layout",
      "Draw the menu at 480x270 for 16:9 displays instead of 320x240. The pause menu of a game shown at 4:3 keeps the 4:3 layout.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_BOOL(menu_pix16_boot_screen, MENU_PIX16_BOOT_SCREEN,
      "menu_pix16_boot_screen",
      DEFAULT_PIX16_BOOT_SCREEN, SD_FLAG_NONE, 0, 0,
      "Boot Screen",
      "Show a BIOS-style boot screen when RetroArch starts.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_BOOL(menu_pix16_start_screen, MENU_PIX16_START_SCREEN,
      "menu_pix16_start_screen",
      DEFAULT_PIX16_START_SCREEN, SD_FLAG_NONE, 0, 0,
      "Start Screen",
      "Show the logo and a PRESS ANY BUTTON prompt after the boot screen.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_BOOL(menu_pix16_screen_asked, MENU_PIX16_SCREEN_ASKED,
      "menu_pix16_screen_asked",
      DEFAULT_PIX16_SCREEN_ASKED, SD_FLAG_NONE, 0, 0,
      "Screen Prompt Answered",
      "Internal: the first-run screen question has been answered.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_UINT_EX(menu_pix16_button_style, MENU_PIX16_BUTTON_STYLE,
      "menu_pix16_button_style",
      DEFAULT_PIX16_BUTTON_STYLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PIX16_BUTTONS_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_pix16_button_style, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Button Icons",
      "Controller style for the button icons in the on-screen guides.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_UINT_EX(menu_pix16_transition, MENU_PIX16_TRANSITION,
      "menu_pix16_transition",
      DEFAULT_PIX16_TRANSITION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PIX16_TRANSITION_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_pix16_transition, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Launch Effect",
      "Animation played when a game starts.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_UINT_EX(menu_pix16_palette, MENU_PIX16_PALETTE,
      "menu_pix16_palette",
      DEFAULT_PIX16_PALETTE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, PIX16_PALETTE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_pix16_palette, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Color Theme",
      "Overall color scheme of the pix16 menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU) && defined(HAVE_PIX16); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU) && defined(HAVE_PIX16))
S_BOOL(menu_pix16_reduced_motion, MENU_PIX16_REDUCED_MOTION,
      "menu_pix16_reduced_motion",
      DEFAULT_PIX16_REDUCED_MOTION, SD_FLAG_NONE, 0, 0,
      "Reduced Motion",
      "Turn off sliding, growing, scrolling and launch animations. Transitions become instant.")
#endif
