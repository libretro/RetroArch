/* Single-source definitions: XMB animation group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_XMB) || defined (HAVE_OZONE); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_XMB) || defined (HAVE_OZONE)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_xmb_animation_horizontal_highlight" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(menu_xmb_animation_horizontal_highlight, MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "xmb_menu_animation_horizontal_highlight",
      DEFAULT_XMB_ANIMATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_xmb_animation_horizontal_highlight, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Animation Horizontal Icon Highlight",
      "The animation that triggers when scrolling between tabs.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_XMB) || defined (HAVE_OZONE); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_XMB) || defined (HAVE_OZONE)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_xmb_animation_move_up_down" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(menu_xmb_animation_move_up_down, MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "xmb_menu_animation_move_up_down",
      DEFAULT_XMB_ANIMATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_xmb_animation_move_up_down, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Animation Move Up/Down",
      "The animation that triggers when moving up or down.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_XMB) || defined (HAVE_OZONE); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_XMB) || defined (HAVE_OZONE)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_xmb_animation_opening_main_menu" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(menu_xmb_animation_opening_main_menu, MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "xmb_menu_animation_opening_main_menu",
      DEFAULT_XMB_ANIMATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_menu_xmb_animation_opening_main_menu, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Animation Main Menu Opens/Closes",
      "The animation that triggers when opening a submenu.")
#endif
#endif
