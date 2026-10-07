/* Single-source definitions: SMB client authentication group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* String rows keep their configuration.c rows literal, as the cloud
 * sync ones do; the descriptor and string tables come from here. */
/* The translations name these two labels wherever this group's region
 * of msg_hash.h is compiled, which is with the SMB client or the NFS
 * one: intl/json2h.py drops a guard line that mentions a pass, the
 * feature in it included. So the labels exist with either client;
 * the rows themselves still need the SMB client. */
#if (defined(HAVE_SMBCLIENT) || (defined(SETTINGS_DEF_STRINGS_PASS) && defined(HAVE_NFSCLIENT))) && !defined(SETTINGS_DEF_CONFIG_PASS)
S_STRING(smb_client_realm, SMB_CLIENT_REALM,
      "smb_client_realm",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "SMB Kerberos Realm",
      "Kerberos realm for ticket authentication, such as the Active Directory domain in capitals (EXAMPLE.COM). Leave empty for password (NTLMSSP) authentication. The server must be given by host name.")
#endif
#if (defined(HAVE_SMBCLIENT) || (defined(SETTINGS_DEF_STRINGS_PASS) && defined(HAVE_NFSCLIENT))) && !defined(SETTINGS_DEF_CONFIG_PASS)
S_STRING(smb_client_kdc, SMB_CLIENT_KDC,
      "smb_client_kdc",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "SMB Kerberos KDC",
      "Host of the Kerberos key distribution center. Leave empty when the SMB server is the domain controller.")
#endif
#ifdef HAVE_SMBCLIENT
S_UINT_EX(smb_client_auth_mode, SMB_CLIENT_AUTH_MODE,
      "smb_client_auth_mode",
      DEFAULT_SMB_CLIENT_AUTH_MODE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RETRO_SMB2_SEC_KRB5, 1, 0, setting_action_ok_uint, setting_get_string_representation_smb_auth, NULL, NULL, NULL, NULL, 0,
      "SMB Authentication Mode",
      "Select the authentication used in your environment.")
#endif
#ifdef HAVE_SMBCLIENT
S_UINT_EX(smb_client_num_contexts, SMB_CLIENT_NUM_CONTEXTS,
      "smb_client_num_contexts",
      DEFAULT_SMB_CLIENT_NUM_CONTEXTS, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, DEFAULT_SMB_CLIENT_MAX_CONTEXTS, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "SMB Maximum connections",
      "Select the maximum connections used in your environment.")
#endif
#ifdef HAVE_SMBCLIENT
S_UINT_EX(smb_client_timeout, SMB_CLIENT_TIMEOUT,
      "smb_client_timeout",
      DEFAULT_SMB_CLIENT_TIMEOUT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, DEFAULT_SMB_CLIENT_MAX_TIMEOUT, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "SMB Timeout",
      "Select the default timeout in seconds.")
#endif
#ifdef HAVE_SMBCLIENT
S_UINT_EX(smb_client_readahead, SMB_CLIENT_READAHEAD,
      "smb_client_readahead",
      DEFAULT_SMB_CLIENT_READAHEAD, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, DEFAULT_SMB_CLIENT_MAX_READAHEAD, 64, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "SMB Read-Ahead (KiB)",
      "Data fetched ahead when a game reads a file in small pieces, with a background thread keeping the next window coming. Can smooth large disc images over a slow link; costs that much memory and one more connection per open file. 0 turns it off: each read is one request, as before read-ahead existed.")
#endif
