/* Single-source definitions: SMB client setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_SMBCLIENT
S_BOOL_EX(smb_client_enable, SMB_CLIENT_ENABLE,
      "smb_client_enable",
      false, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable SMB Client",
      "Enable SMB network share access. Ethernet is strongly recommended over Wi-Fi for a more reliable connection. Note: changing these settings requires a restart of RetroArch.")
#endif
