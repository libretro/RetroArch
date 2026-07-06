/* Single-source definitions: SMB client authentication group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

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
