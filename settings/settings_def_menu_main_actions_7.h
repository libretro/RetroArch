/* Single-source definitions: seventh main menu action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_ACTION_H
#define S_ACTION_H S_ACTION
#endif
#ifndef S_ACTION_EX_H
#define S_ACTION_EX_H S_ACTION_EX
#endif
#endif
/* Descriptor and configuration rows are #if !defined(IOS) #if !defined(HAVE_LAKKA); the string
 * tables always carry this row via the strings pass. */
#if !defined(IOS) && !defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX_H(QUIT_RETROARCH,
      "quit_retroarch",
      SD_FLAG_NONE, NULL, NULL, CMD_EVENT_QUIT,
      "Quit",
      "Quit RetroArch application. Configuration save on exit is enabled.")
#endif
