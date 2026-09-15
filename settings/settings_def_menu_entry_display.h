/* Single-source definitions: menu entry display group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_xmb_alpha_factor, XMB_ALPHA_FACTOR,
      "xmb_alpha_factor",
      DEFAULT_XMB_ALPHA_FACTOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 100, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Color Theme Opacity Factor",
      "Modify the opacity percent of the color theme.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_xmb_vertical_fade_factor, MENU_XMB_VERTICAL_FADE_FACTOR,
      "menu_xmb_vertical_fade_factor",
      DEFAULT_XMB_VERTICAL_FADE_FACTOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 500, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Vertical Fade Factor",
      "Adjust the fade level of visible items near screen edges.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_xmb_current_menu_icon, XMB_CURRENT_MENU_ICON,
      "xmb_current_menu_icon",
      DEFAULT_XMB_CURRENT_MENU_ICON, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, XMB_CURRENT_MENU_ICON_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_xmb_current_menu_icon, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Current Menu Icon",
      "Current menu icon can be hidden, under the horizontal menu or in header title.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_xmb_show_horizontal_list, MENU_XMB_SHOW_HORIZONTAL_LIST,
      "menu_xmb_show_horizontal_list",
      DEFAULT_XMB_SHOW_HORIZONTAL_LIST, SD_FLAG_NONE, 0, 0,
      "Show Horizontal List",
      "Enable the main horizontal tab list for navigation.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_xmb_show_title_header, MENU_XMB_SHOW_TITLE_HEADER,
      "menu_xmb_show_title_header",
      DEFAULT_XMB_SHOW_TITLE_HEADER, SD_FLAG_NONE, 0, 0,
      "Show Title Header",
      "Show the current menu location in the header.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(menu_xmb_title_margin, MENU_XMB_TITLE_MARGIN,
      "menu_xmb_title_margin",
      DEFAULT_XMB_TITLE_MARGIN, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -MAXIMUM_XMB_TITLE_MARGIN, MAXIMUM_XMB_TITLE_MARGIN, 1, -MAXIMUM_XMB_TITLE_MARGIN, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Title Margin",
      "Adjust the title header distance from the screen edge.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(menu_xmb_title_margin_horizontal_offset, MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
      "menu_xmb_title_margin_horizontal_offset",
      DEFAULT_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -MAXIMUM_XMB_TITLE_MARGIN, MAXIMUM_XMB_TITLE_MARGIN, 1, -MAXIMUM_XMB_TITLE_MARGIN, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Title Margin Horizontal Offset",
      "Adjust the horizontal distance of the title header.")
#endif
