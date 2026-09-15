/* Single-source definitions: achievements account action.
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
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_H(ACCOUNTS_RETRO_ACHIEVEMENTS,
      "retro_achievements",
      "RetroAchievements",
      "Earn achievements in classic games. For more information, visit 'https://retroachievements.org'.")
#endif
