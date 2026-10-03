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
/* The MCP server: the commands, as tools for AI assistants over the
 * Model Context Protocol. Off unless chosen; the token is registered
 * with the configuration as a secret, so its row is menu-only. */
#if defined(HAVE_MCP) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(mcp_server_enable, MCP_SERVER_ENABLE,
      "mcp_server_enable",
      DEFAULT_MCP_SERVER_ENABLE, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "MCP Server",
      "Let AI assistants control RetroArch through the Model Context Protocol: every network command becomes a tool. The server listens on this machine only, at http://127.0.0.1:<port>/mcp, and accepts only clients that send the token below. Takes effect on restart.")
S_UINT_EX(mcp_server_port, MCP_SERVER_PORT,
      "mcp_server_port",
      DEFAULT_MCP_SERVER_PORT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, 65535, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "MCP Server Port",
      "TCP port of the MCP server.")
#endif
#if defined(HAVE_MCP) && !defined(SETTINGS_DEF_CONFIG_PASS)
S_STRING(mcp_server_token, MCP_SERVER_TOKEN,
      "mcp_server_token",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "MCP Server Token",
      "The bearer token an MCP client must send. Created when the server first starts; enter it in the client's configuration.")
#endif
