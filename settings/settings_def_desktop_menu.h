/* Single-source definitions: desktop menu group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_NS_H
#define S_BOOL_NS_H S_BOOL_NS
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(menu_scroll_delay, MENU_SCROLL_DELAY,
      "menu_scroll_delay",
      DEFAULT_MENU_SCROLL_DELAY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 999, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Scroll Delay",
      "Initial delay in milliseconds when holding a direction to scroll.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_QT) || defined(HAVE_COCOA); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(ui_companion_enable, UI_COMPANION_ENABLE,
      "ui_companion_enable",
      DEFAULT_UI_COMPANION_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "UI Companion")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_QT) || defined(HAVE_COCOA); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS_H(ui_companion_start_on_boot, UI_COMPANION_START_ON_BOOT,
      "ui_companion_start_on_boot",
      DEFAULT_UI_COMPANION_START_ON_BOOT, SD_FLAG_ADVANCED, 0, 0,
      "Start UI Companion on Boot")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_QT) || defined(HAVE_COCOA); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX_NS(desktop_menu_enable, DESKTOP_MENU_ENABLE,
      "desktop_menu_enable",
      DEFAULT_DESKTOP_MENU_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Desktop Menu (Restart required)")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_QT) || defined(HAVE_COCOA); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_QT) || defined(HAVE_COCOA)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(ui_companion_toggle, UI_COMPANION_TOGGLE,
      "ui_companion_toggle",
      DEFAULT_UI_COMPANION_TOGGLE, SD_FLAG_NONE, 0, 0,
      "Open Desktop Menu on Startup")
#endif
