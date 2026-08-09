/* Single-source definitions: netplay stateless mode setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _ND are registered in the configuration table without a
 * default applied; outside the configuration pass they behave exactly
 * like the base row. */
#ifndef S_BOOL_EX_NS_ND
#define S_BOOL_EX_NS_ND S_BOOL_EX_NS
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) #if defined(HAVE_NETWORK_CMD); the string
 * tables always carry this row via the strings pass, and the
 * configuration row lives under HAVE_NETWORKGAMEPAD as it did before
 * the migration. */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) || defined(SETTINGS_DEF_STRINGS_PASS) || (defined(SETTINGS_DEF_CONFIG_PASS) && defined(HAVE_NETWORKGAMEPAD))
S_BOOL_EX_NS_ND(network_remote_enable, NETWORK_REMOTE_ENABLE,
      "network_remote_enable",
      false, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Network RetroPad")
#endif
