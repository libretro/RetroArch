/* Single-source definitions: cloud sync general group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(cloud_sync_enable, CLOUD_SYNC_ENABLE,
      "cloud_sync_enable",
      false, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable Cloud Sync",
      "Attempt to sync configs, sram, and states to a cloud storage provider.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cloud_sync_destructive, CLOUD_SYNC_DESTRUCTIVE,
      "cloud_sync_destructive",
      false, SD_FLAG_NONE, 0, 0,
      "Destructive Cloud Sync",
      "When disabled, files are moved to a backup folder before being overwritten or deleted.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cloud_sync_sync_saves, CLOUD_SYNC_SYNC_SAVES,
      "cloud_sync_sync_saves",
      false, SD_FLAG_NONE, 0, 0,
      "Sync: Saves/States",
      "When enabled, saves/states will be synced to cloud.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cloud_sync_sync_configs, CLOUD_SYNC_SYNC_CONFIGS,
      "cloud_sync_sync_configs",
      false, SD_FLAG_NONE, 0, 0,
      "Sync: Configuration Files",
      "When enabled, configuration files will be synced to cloud.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cloud_sync_sync_thumbs, CLOUD_SYNC_SYNC_THUMBS,
      "cloud_sync_sync_thumbs",
      false, SD_FLAG_NONE, 0, 0,
      "Sync: Thumbnail Images",
      "When enabled, thumbnail images will be synced to cloud. Not generally recommended except for large collections of custom thumbnail images; otherwise the thumbnail downloader is a better choice.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(cloud_sync_sync_system, CLOUD_SYNC_SYNC_SYSTEM,
      "cloud_sync_sync_system",
      false, SD_FLAG_NONE, 0, 0,
      "Sync: System Files",
      "When enabled, system files will be synced to cloud. This can significantly increase the time it takes to sync; use with caution.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(cloud_sync_sync_mode, CLOUD_SYNC_SYNC_MODE,
      "cloud_sync_sync_mode",
      CLOUD_SYNC_MODE_AUTOMATIC, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, CLOUD_SYNC_MODE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_cloud_sync_sync_mode, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Sync Mode",
      "Automatic: Sync on RetroArch startup and when cores are unloaded. Manual: Only sync when 'Sync Now' button is manually triggered.")
#endif
