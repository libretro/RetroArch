/* Single-source definitions: NFS client options group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_NFSCLIENT
S_UINT_EX(nfs_client_num_contexts, NFS_CLIENT_NUM_CONTEXTS,
      "nfs_client_num_contexts",
      DEFAULT_NFS_CLIENT_NUM_CONTEXTS, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, DEFAULT_NFS_CLIENT_MAX_CONTEXTS, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "NFS Maximum connections",
      "Select the maximum connections used in your environment.")
#endif
#ifdef HAVE_NFSCLIENT
S_UINT_EX(nfs_client_timeout, NFS_CLIENT_TIMEOUT,
      "nfs_client_timeout",
      DEFAULT_NFS_CLIENT_TIMEOUT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, DEFAULT_NFS_CLIENT_MAX_TIMEOUT, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "NFS Timeout",
      "Select the default timeout in seconds.")
#endif
