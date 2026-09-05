/* Single-source definitions: netplay port group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "netplay_ip_port" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(netplay_port, NETPLAY_TCP_UDP_PORT,
      "netplay_tcp_udp_port",
      RARCH_DEFAULT_PORT, SD_FLAG_ALLOW_INPUT | SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 65535, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Netplay TCP Port",
      "The port of the host IP address. Can be either a TCP or UDP port.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(netplay_max_connections, NETPLAY_MAX_CONNECTIONS,
      "netplay_max_connections",
      DEFAULT_NETPLAY_MAX_CONNECTIONS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 31, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_SPINBOX,
      "Max Simultaneous Connections",
      "The maximum number of active connections that the host will accept before refusing new ones.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(netplay_max_ping, NETPLAY_MAX_PING,
      "netplay_max_ping",
      DEFAULT_NETPLAY_MAX_PING, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 500, 10, 0, NULL, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_SPINBOX,
      "Ping Limiter",
      "The maximum connection latency (ping) that the host will accept. Set it to 0 for no limit.")
#endif
