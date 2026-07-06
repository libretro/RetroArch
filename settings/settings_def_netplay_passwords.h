/* Single-source definitions: netplay passwords group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "netplay_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(netplay_password, NETPLAY_PASSWORD,
      "netplay_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Server Password",
      "The password used by clients connecting to the host.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "netplay_spectate_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(netplay_spectate_password, NETPLAY_SPECTATE_PASSWORD,
      "netplay_spectate_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Server Spectate-Only Password",
      "The password used by clients connecting to the host as a spectator.")
#endif
#endif
