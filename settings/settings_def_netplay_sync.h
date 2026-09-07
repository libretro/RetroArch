/* Single-source definitions: netplay sync group.
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
S_BOOL(netplay_nat_traversal, NETPLAY_NAT_TRAVERSAL,
      "netplay_nat_traversal",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Netplay NAT Traversal",
      "When hosting, attempt to listen for connections from the public Internet, using UPnP or similar technologies to escape LANs.")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_UINT_EX_NS(netplay_share_digital, NETPLAY_SHARE_DIGITAL,
      "netplay_share_digital",
      DEFAULT_NETPLAY_SHARE_DIGITAL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RARCH_NETPLAY_SHARE_DIGITAL_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_netplay_share_digital, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Digital Input Sharing")
#endif
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_UINT_EX_NS(netplay_share_analog, NETPLAY_SHARE_ANALOG,
      "netplay_share_analog",
      DEFAULT_NETPLAY_SHARE_ANALOG, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RARCH_NETPLAY_SHARE_ANALOG_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_netplay_share_analog, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Analog Input Sharing")
#endif
#endif
