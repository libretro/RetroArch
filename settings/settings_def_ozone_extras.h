/* Single-source definitions: Ozone extras group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL_EX(menu_materialui_icons_enable, MATERIALUI_ICONS_ENABLE,
      "materialui_icons_enable",
      DEFAULT_MATERIALUI_ICONS_ENABLE, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Icons",
      "Show icons to the left of the menu entries.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_materialui_switch_icons, MATERIALUI_SWITCH_ICONS,
      "materialui_switch_icons",
      DEFAULT_MATERIALUI_SWITCH_ICONS, SD_FLAG_NONE, 0, 0,
      "Switch Icons",
      "Use icons instead of ON/OFF text to represent 'toggle switch' menu settings entries.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_materialui_playlist_icons_enable, MATERIALUI_PLAYLIST_ICONS_ENABLE,
      "materialui_playlist_icons_enable",
      DEFAULT_MATERIALUI_PLAYLIST_ICONS_ENABLE, SD_FLAG_NONE, 0, 0,
      "Playlist Icons (Restart required)",
      "Show system-specific icons in the playlists.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_materialui_color_theme, MATERIALUI_MENU_COLOR_THEME,
      "materialui_menu_color_theme",
      DEFAULT_MATERIALUI_THEME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MATERIALUI_THEME_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_materialui_menu_color_theme, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Color Theme",
      "Select a different background color theme.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_materialui_transition_animation, MATERIALUI_MENU_TRANSITION_ANIMATION,
      "materialui_menu_transition_animation",
      DEFAULT_MATERIALUI_TRANSITION_ANIM, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MATERIALUI_TRANSITION_ANIM_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_materialui_menu_transition_animation, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Transition Animation",
      "Enable smooth animation effects when navigating between different levels of the menu.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_materialui_landscape_layout_optimization, MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
      "materialui_landscape_layout_optimization",
      DEFAULT_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_materialui_landscape_layout_optimization, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Optimize Landscape Layout",
      "Automatically adjust menu layout to better fit the screen when using landscape display orientations.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL_EX(menu_materialui_show_nav_bar, MATERIALUI_SHOW_NAV_BAR,
      "materialui_show_nav_bar",
      DEFAULT_MATERIALUI_SHOW_NAV_BAR, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Show Navigation Bar",
      "Display permanent on-screen menu navigation shortcuts. Enables fast switching between menu categories. Recommended for touchscreen devices.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_materialui_auto_rotate_nav_bar, MATERIALUI_AUTO_ROTATE_NAV_BAR,
      "materialui_auto_rotate_nav_bar",
      DEFAULT_MATERIALUI_AUTO_ROTATE_NAV_BAR, SD_FLAG_NONE, 0, 0,
      "Automatically Rotate Navigation Bar",
      "Automatically move the navigation bar to the right hand side of the screen when using landscape display orientations.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_materialui_thumbnail_view_portrait, MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
      "materialui_thumbnail_view_portrait",
      DEFAULT_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_materialui_menu_thumbnail_view_portrait, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Portrait Thumbnail View",
      "Specify playlist thumbnail view mode when using portrait display orientations.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_materialui_thumbnail_view_landscape, MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
      "materialui_thumbnail_view_landscape",
      DEFAULT_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_materialui_menu_thumbnail_view_landscape, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Landscape Thumbnail View",
      "Specify playlist thumbnail view mode when using landscape display orientations.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_materialui_dual_thumbnail_list_view_enable, MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
      "materialui_dual_thumbnail_list_view_enable",
      DEFAULT_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE, SD_FLAG_NONE, 0, 0,
      "Show Secondary Thumbnail In List Views",
      "Displays a secondary thumbnail when using 'List'-type playlist thumbnail view modes. This setting only applies when the screen has sufficient physical width to show two thumbnails.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_MATERIALUI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_MATERIALUI) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(menu_materialui_thumbnail_background_enable, MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
      "materialui_thumbnail_background_enable",
      DEFAULT_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE, SD_FLAG_NONE, 0, 0,
      "Thumbnail Backgrounds",
      "Enables padding of unused space in thumbnail images with a solid background. This ensures a uniform display size for all images, improving menu appearance when viewing mixed content thumbnails with varying base dimensions.")
#endif
#endif
