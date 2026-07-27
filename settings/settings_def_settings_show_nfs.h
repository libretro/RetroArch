/* Single-source definitions: NFS client settings visibility setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if defined(HAVE_MENU) && defined(HAVE_NFSCLIENT)
S_BOOL(settings_show_nfs_client, SETTINGS_SHOW_NFS_CLIENT,
      "settings_show_nfs_client",
      DEFAULT_SETTINGS_SHOW_NFS_CLIENT, SD_FLAG_NONE, 0, 0,
      "Show 'NFS Client'",
      "Show 'NFS Client' settings.")
#endif
