/* Single-source definitions: notification enable group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_GFX_WIDGETS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_enable_widgets" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_EX(menu_enable_widgets, MENU_WIDGETS_ENABLE,
      "menu_widgets_enable",
      DEFAULT_MENU_ENABLE_WIDGETS, SD_FLAG_NONE, 0, CMD_EVENT_OSD_NOTIFICATION_TOGGLE, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Graphics Widgets",
      "Use decorated animations, notifications, indicators and controls.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_GFX_WIDGETS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(menu_widget_scale_auto, MENU_WIDGET_SCALE_AUTO,
      "menu_widget_scale_auto",
      DEFAULT_MENU_WIDGET_SCALE_AUTO, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Scale Graphics Widgets Automatically",
      "Automatically resize decorated notifications, indicators and controls based on current menu scale.")
#endif
