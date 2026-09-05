/* Single-source definitions: netplay stateless mode setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) #if defined(HAVE_NETWORK_CMD); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) || defined(SETTINGS_DEF_STRINGS_PASS) || (defined(SETTINGS_DEF_CONFIG_PASS) && defined(HAVE_NETWORKGAMEPAD))
/* The configuration table registers this row by hand in
 * configuration.c because it carries no default there; the
 * generated row is for the other passes. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_EX_NS(network_remote_enable, NETWORK_REMOTE_ENABLE,
      "network_remote_enable",
      false, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Network RetroPad")
#endif
#endif
