/* Single-source definitions: netplay action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_SMBCLIENT
S_ACTION(SMB_CLIENT_SETTINGS,
      "smb_client_settings",
      "SMB Network Settings",
      "Configure SMB network share settings.")
#endif
