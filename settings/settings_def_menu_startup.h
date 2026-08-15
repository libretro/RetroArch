/* Single-source definitions: menu startup group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_ticker_type, MENU_TICKER_TYPE,
      "menu_ticker_type",
      DEFAULT_MENU_TICKER_TYPE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, TICKER_TYPE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_ticker_type, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Ticker Text Animation",
      "Select horizontal scrolling method used to display long menu text.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_FLOAT_EX(menu_ticker_speed, MENU_TICKER_SPEED,
      "menu_ticker_speed",
      DEFAULT_MENU_TICKER_SPEED, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.1, 10.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Ticker Text Speed",
      "The animation speed when scrolling long menu text.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_ticker_smooth, MENU_TICKER_SMOOTH,
      "menu_ticker_smooth",
      DEFAULT_MENU_TICKER_SMOOTH, SD_FLAG_NONE, 0, 0,
      "Smooth Ticker Text",
      "Use smooth scrolling animation when displaying long menu text. Has a small performance impact.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(ozone_metadata_scroll_style, OZONE_METADATA_SCROLL_STYLE,
      "ozone_metadata_scroll_style",
      DEFAULT_OZONE_METADATA_SCROLL_STYLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OZONE_METADATA_SCROLL_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_ozone_metadata_scroll_style, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Metadata Text Animation",
      "Whether a scraped synopsis too long for the content metadata panel animates, and which way it moves. 'Horizontal' moves it sideways along a single line. 'Vertical' wraps it to the width of the sidebar and moves the whole block upwards, the way the XMB metadata panel does. 'Off' leaves it static, wrapped over as many lines as the panel has room for. Speed, smoothness, and whether the movement scrolls or bounces, all come from the ticker text settings alongside this one.")
#endif
#endif
