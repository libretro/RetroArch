/* Single-source definitions: game AI group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_GAME_AI
S_BOOL(quick_menu_show_game_ai, QUICK_MENU_SHOW_GAME_AI,
      "quick_menu_show_game_ai",
      1, SD_FLAG_NONE, 0, 0,
      "Show 'Game AI'",
      "Show the 'Game AI' option.")
#endif
#ifdef HAVE_GAME_AI
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(game_ai_override_p1, GAME_AI_OVERRIDE_P1,
      "game_ai_override_p1",
      1, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Override p1",
      "Override player 01")
#endif
#endif
#ifdef HAVE_GAME_AI
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(game_ai_override_p2, GAME_AI_OVERRIDE_P2,
      "game_ai_override_p2",
      1, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Override p2",
      "Override player 02")
#endif
#endif
#ifdef HAVE_GAME_AI
/* Persistence lives in custom configuration code; no config
 * row is emitted. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(game_ai_show_debug, GAME_AI_SHOW_DEBUG,
      "game_ai_show_debug",
      1, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Show Debug",
      "Show Debug")
#endif
#endif
