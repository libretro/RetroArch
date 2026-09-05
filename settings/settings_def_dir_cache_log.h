/* Single-source definitions: cache and log directory group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "cache_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_cache, CACHE_DIRECTORY,
      "cache_directory",
      g_defaults.dirs[DEFAULT_DIR_CACHE], DIRECTORY_NONE, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Cache",
      "Archived content will be temporarily extracted to this directory.")
#endif
/* config key "log_dir" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(log_dir, LOG_DIR,
      "log_dir",
      g_defaults.dirs[DEFAULT_DIR_LOGS], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "System Event Logs",
      "System event logs are stored in this directory.")
#endif
