/* Single-source definitions: netplay visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_NS(netplay_show_only_connectable, NETPLAY_SHOW_ONLY_CONNECTABLE,
      "netplay_show_only_connectable",
      DEFAULT_NETPLAY_SHOW_ONLY_CONNECTABLE, SD_FLAG_NONE, 0, 0,
      "Only Connectable Rooms")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_NS(netplay_show_only_installed_cores, NETPLAY_SHOW_ONLY_INSTALLED_CORES,
      "netplay_show_only_installed_cores",
      DEFAULT_NETPLAY_SHOW_ONLY_INSTALLED_CORES, SD_FLAG_NONE, 0, 0,
      "Only Installed Cores")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_NS(netplay_show_passworded, NETPLAY_SHOW_PASSWORDED,
      "netplay_show_passworded",
      DEFAULT_NETPLAY_SHOW_PASSWORDED, SD_FLAG_NONE, 0, 0,
      "Passworded Rooms")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL(netplay_public_announce, NETPLAY_PUBLIC_ANNOUNCE,
      "netplay_public_announce",
      DEFAULT_NETPLAY_PUBLIC_ANNOUNCE, SD_FLAG_NONE, 0, 0,
      "Publicly Announce Netplay",
      "Whether to announce netplay games publicly. If unset, clients must manually connect rather than using the public lobby.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_EX(netplay_use_mitm_server, NETPLAY_USE_MITM_SERVER,
      "netplay_use_mitm_server",
      DEFAULT_NETPLAY_USE_MITM_SERVER, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Use Relay Server",
      "Forward netplay connections through a man-in-the-middle server. Useful if the host is behind a firewall or has NAT/UPnP problems.")
#endif
#endif
