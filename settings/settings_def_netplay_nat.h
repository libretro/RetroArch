/* Single-source definitions: netplay NAT traversal setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) #if defined(HAVE_NETWORK_CMD); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX_NS(network_cmd_enable, NETWORK_CMD_ENABLE,
      "network_cmd_enable",
      DEFAULT_NETWORK_CMD_ENABLE, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Network Commands")
#endif
