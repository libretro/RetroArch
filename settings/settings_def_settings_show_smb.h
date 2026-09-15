/* Single-source definitions: SMB client settings visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_SMBCLIENT
S_BOOL(settings_show_smb_client, SETTINGS_SHOW_SMB_CLIENT,
      "settings_show_smb_client",
      DEFAULT_SETTINGS_SHOW_SMB_CLIENT, SD_FLAG_NONE, 0, 0,
      "Show 'SMB Client'",
      "Show 'SMB Client' settings.")
#endif
