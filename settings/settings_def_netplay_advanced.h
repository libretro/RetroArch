/* Single-source definitions: netplay advanced group.
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
#ifndef S_INT_EX_H
#define S_INT_EX_H S_INT_EX
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_H(netplay_start_as_spectator, NETPLAY_START_AS_SPECTATOR,
      "netplay_start_as_spectator",
      DEFAULT_NETPLAY_START_AS_SPECTATOR, SD_FLAG_NONE, 0, 0,
      "Netplay Spectator Mode",
      "Start netplay in spectator mode.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL(netplay_fade_chat, NETPLAY_FADE_CHAT,
      "netplay_fade_chat",
      DEFAULT_NETPLAY_FADE_CHAT, SD_FLAG_NONE, 0, 0,
      "Fade Chat",
      "Fade chat messages over time.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_UINT_EX(netplay_chat_color_name, NETPLAY_CHAT_COLOR_NAME,
      "netplay_chat_color_name",
      DEFAULT_NETPLAY_CHAT_COLOR_NAME, SD_FLAG_NONE, 0, 0, 0, 0, 0, 0, setting_action_ok_color_rgb, setting_get_string_representation_color_rgb, NULL, setting_action_ok_color_rgb, NULL, NULL, 0,
      "Chat Color (Nickname)",
      "Format: #RRGGBB or RRGGBB")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_UINT_EX(netplay_chat_color_msg, NETPLAY_CHAT_COLOR_MSG,
      "netplay_chat_color_msg",
      DEFAULT_NETPLAY_CHAT_COLOR_MSG, SD_FLAG_NONE, 0, 0, 0, 0, 0, 0, setting_action_ok_color_rgb, setting_get_string_representation_color_rgb, NULL, setting_action_ok_color_rgb, NULL, NULL, 0,
      "Chat Color (Message)",
      "Format: #RRGGBB or RRGGBB")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL(netplay_allow_pausing, NETPLAY_ALLOW_PAUSING,
      "netplay_allow_pausing",
      DEFAULT_NETPLAY_ALLOW_PAUSING, SD_FLAG_NONE, 0, 0,
      "Allow Pausing",
      "Allow players to pause during netplay.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_EX(netplay_allow_slaves, NETPLAY_ALLOW_SLAVES,
      "netplay_allow_slaves",
      DEFAULT_NETPLAY_ALLOW_SLAVES, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Allow Slave-Mode Clients",
      "Allow connections in slave mode. Slave-mode clients require very little processing power on either side, but will suffer significantly from network latency.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL(netplay_require_slaves, NETPLAY_REQUIRE_SLAVES,
      "netplay_require_slaves",
      DEFAULT_NETPLAY_REQUIRE_SLAVES, SD_FLAG_ADVANCED, 0, 0,
      "Disallow Non-Slave-Mode Clients",
      "Disallow connections not in slave mode. Not recommended except for very fast networks with very weak machines.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_INT_EX_H(netplay_check_frames, NETPLAY_CHECK_FRAMES,
      "netplay_check_frames",
      DEFAULT_NETPLAY_CHECK_FRAMES, SD_FLAG_ALLOW_INPUT | SD_FLAG_ADVANCED, (SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN), 0, 0, 5184000, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Netplay Check Frames",
      "The frequency (in frames) that netplay will verify that the host and client are in sync.")
#endif
#endif
