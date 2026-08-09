/* Single-source definitions: screensaver suspend group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_H
#define S_BOOL_H S_BOOL
#endif
#endif
/* Descriptor and configuration rows are #if (!defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)) || (defined(IOS) && TARGET_OS_TV); the string
 * tables always carry this row via the strings pass. */
#if (!defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)) || (defined(IOS) && TARGET_OS_TV) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_H(ui_suspend_screensaver_enable, SUSPEND_SCREENSAVER_ENABLE,
      "suspend_screensaver_enable",
      true, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Suspend Screensaver",
      "Prevent your system's screensaver from becoming active.")
#endif
