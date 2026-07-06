/* Single-source definitions: Material UI appearance group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_ozone_font_scale, OZONE_FONT_SCALE,
      "ozone_font_scale",
      DEFAULT_OZONE_FONT_SCALE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OZONE_FONT_SCALE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ozone_font_scale, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Font Scale",
      "Define whether the font size in the menu should have its own scaling, and if it should be scaled globally or with separate values for each part of the menu.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_global, OZONE_FONT_SCALE_FACTOR_GLOBAL,
      "ozone_font_scale_factor_global",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_GLOBAL, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Scale Factor",
      "Scale the font size linearly across the menu.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_title, OZONE_FONT_SCALE_FACTOR_TITLE,
      "ozone_font_scale_factor_title",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_TITLE, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Title Font Scale Factor",
      "Scale the font size for the title text in the menu header.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_sidebar, OZONE_FONT_SCALE_FACTOR_SIDEBAR,
      "ozone_font_scale_factor_sidebar",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_SIDEBAR, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Left Sidebar Font Scale Factor",
      "Scale the font size for the text in the left sidebar.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_label, OZONE_FONT_SCALE_FACTOR_LABEL,
      "ozone_font_scale_factor_label",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_LABEL, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Labels Font Scale Factor",
      "Scale the font size for the labels of menu options and playlist entries. Also affects text size in the help boxes.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_sublabel, OZONE_FONT_SCALE_FACTOR_SUBLABEL,
      "ozone_font_scale_factor_sublabel",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_SUBLABEL, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Sublabels Font Scale Factor",
      "Scale the font size for the sublabels of menu options and playlist entries.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_time, OZONE_FONT_SCALE_FACTOR_TIME,
      "ozone_font_scale_factor_time",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_TIME, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Timedate Font Scale Factor",
      "Scale the font size of the time and date indicator in the top-right corner of the menu.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_font_scale_factor_footer, OZONE_FONT_SCALE_FACTOR_FOOTER,
      "ozone_font_scale_factor_footer",
      DEFAULT_OZONE_FONT_SCALE_FACTOR_FOOTER, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 3.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Footer Font Scale Factor",
      "Scale the font size of the text in the menu footer. Also affects text size in the right thumbnail sidebar.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(ozone_scroll_content_metadata, OZONE_SCROLL_CONTENT_METADATA,
      "ozone_scroll_content_metadata",
      DEFAULT_OZONE_SCROLL_CONTENT_METADATA, SD_FLAG_NONE, 0, 0,
      "Use Ticker Text for Content Metadata",
      "When enabled, each item of content metadata shown on the right sidebar of playlists (associated core, play time) will occupy a single line; strings exceeding the width of the sidebar will be displayed as scrolling ticker text. When disabled, each item of content metadata will be displayed statically, wrapped to occupy as many lines as required.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "ozone_thumbnail_scale_factor" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_FLOAT_EX(ozone_thumbnail_scale_factor, OZONE_THUMBNAIL_SCALE_FACTOR,
      "ozone_menu_thumbnail_scale_factor",
      DEFAULT_OZONE_THUMBNAIL_SCALE_FACTOR, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1.0, 2.0, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Thumbnail Scale Factor",
      "Scale the size of the thumbnail bar.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(ozone_padding_factor, OZONE_PADDING_FACTOR,
      "ozone_padding_factor",
      DEFAULT_OZONE_PADDING_FACTOR, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 2.0, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Padding Factor",
      "Scale the horizontal padding size.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_ozone_header_icon, OZONE_HEADER_ICON,
      "ozone_header_icon",
      DEFAULT_OZONE_HEADER_ICON, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OZONE_HEADER_ICON_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ozone_header_icon, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, 0,
      "Header Icon",
      "Header logo can be hidden, dynamic depending on navigation or fixed to classic invader.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_ozone_header_separator, OZONE_HEADER_SEPARATOR,
      "ozone_header_separator",
      DEFAULT_OZONE_HEADER_SEPARATOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OZONE_HEADER_SEPARATOR_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ozone_header_separator, NULL, NULL, NULL, NULL, 0,
      "Header Separator",
      "Alternative width for header and footer separators.")
#endif
#endif
