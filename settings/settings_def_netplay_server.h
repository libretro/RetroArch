/* Single-source definitions: netplay server group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "netplay_custom_mitm_server" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(netplay_custom_mitm_server, NETPLAY_CUSTOM_MITM_SERVER,
      "netplay_custom_mitm_server",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Custom Relay Server Address",
      "Input the address of your custom relay server here. Format: address or address|port.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "netplay_ip_address" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P(netplay_server, NETPLAY_IP_ADDRESS,
      "netplay_ip_address",
      "", SD_FLAG_ALLOW_INPUT | SD_FLAG_ADVANCED, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Server Address",
      "The address of the host to connect to.")
#endif
#endif
