/* Single-source definitions: NFS client setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_NFSCLIENT
S_BOOL_EX(nfs_client_enable, NFS_CLIENT_ENABLE,
      "nfs_client_enable",
      false, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable NFS Client",
      "Enable NFS network share access. Ethernet is strongly recommended over Wi-Fi for a more reliable connection. Note: changing these settings requires a restart of RetroArch.")
#endif
