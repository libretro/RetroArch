/* Single-source definitions: GameMode setting.
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
/* Descriptor and configuration rows are #ifndef HAVE_LAKKA; the string
 * tables always carry this row via the strings pass. */
#if !defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "gamemode_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS_H(gamemode_enable, GAMEMODE_ENABLE,
      "game_mode_enable",
      DEFAULT_GAMEMODE_ENABLE, SD_FLAG_NONE, 0, 0,
      "Game Mode")
#endif
#endif
