/* Single-source definitions: RGUI appearance group.
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
S_UINT_EX_NS(menu_font_color_red, MENU_FONT_COLOR_RED,
      "menu_font_color_red",
      DEFAULT_MENU_FONT_COLOR_RED, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Red")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX_NS(menu_font_color_green, MENU_FONT_COLOR_GREEN,
      "menu_font_color_green",
      DEFAULT_MENU_FONT_COLOR_GREEN, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Green")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX_NS(menu_font_color_blue, MENU_FONT_COLOR_BLUE,
      "menu_font_color_blue",
      DEFAULT_MENU_FONT_COLOR_BLUE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Blue")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_xmb_layout, XMB_LAYOUT,
      "xmb_layout",
      DEFAULT_XMB_MENU_LAYOUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_xmb_layout, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Layout",
      "Select a different layout for the XMB interface.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_xmb_theme, XMB_THEME,
      "xmb_theme",
      DEFAULT_XMB_ICON_THEME, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, XMB_ICON_THEME_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_xmb_icon_theme, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Icon Theme",
      "Select a different icon theme for RetroArch.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_xmb_entry_icons, XMB_ENTRY_ICONS,
      "xmb_entry_icons",
      DEFAULT_XMB_ENTRY_ICONS, SD_FLAG_NONE, 0, 0,
      "Entry Icons",
      "Draw icons for menu entries.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_xmb_switch_icons, XMB_SWITCH_ICONS,
      "xmb_switch_icons",
      DEFAULT_XMB_SWITCH_ICONS, SD_FLAG_NONE, 0, 0,
      "Switch Icons",
      "Use icons instead of ON/OFF text to represent 'toggle switch' menu settings entries.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_XMB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_xmb_shadows_enable, XMB_SHADOWS_ENABLE,
      "xmb_shadows_enable",
      DEFAULT_XMB_SHADOWS_ENABLE, SD_FLAG_NONE, 0, 0,
      "Shadow Effects",
      "Draw drop shadows for icons, thumbnails and letters. This will have a minor performance hit.")
#endif
#endif
