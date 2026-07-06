/* Single-source definitions: second main menu list actions.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(CLOUD_SYNC_SYNC_NOW,
      "cloud_sync_sync_now", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_CLOUD_SYNC,
      "Sync Now",
      "Manually trigger cloud synchronization.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
      "cloud_sync_resolve_keep_local", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
      "Resolve Conflicts: Keep Local",
      "Resolve all conflicts by uploading local files to the server.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(CLOUD_SYNC_RESOLVE_KEEP_SERVER,
      "cloud_sync_resolve_keep_server", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
      "Resolve Conflicts: Keep Server",
      "Resolve all conflicts by downloading server files, replacing local copies.")
#endif
S_ACTION_EX(CONFIGURATIONS_LIST,
      "configurations_list", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Configuration File",
      "Manage and create configuration files.")
S_ACTION_EX(CONFIGURATIONS,
      "configurations", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Load Configuration",
      "Load existing configuration and replace current values.")
S_ACTION_EX(SAVE_CURRENT_CONFIG,
      "save_current_config", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG,
      "Save Current Configuration",
      "Overwrite current configuration file.")
S_ACTION_EX(SAVE_NEW_CONFIG,
      "save_new_config", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_SAVE_CONFIG,
      "Save New Configuration",
      "Save current configuration to separate file.")
S_ACTION_EX(RESET_TO_DEFAULT_CONFIG,
      "reset_to_default_config", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_RESET_TO_DEFAULT_CONFIG,
      "Reset to Defaults",
      "Reset current configuration to default values.")
S_ACTION_EX(SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "save_current_config_override_core", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Save Core Overrides",
      "Save an override configuration file which will apply for all content loaded with this core. Will take precedence over the main configuration.")
S_ACTION_EX(SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "save_current_config_override_content_dir", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "Save Content Directory Overrides",
      "Save an override configuration file which will apply for all content loaded from the same directory as the current file. Will take precedence over the main configuration.")
S_ACTION_EX(SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "save_current_config_override_game", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Save Game Overrides",
      "Save an override configuration file which will apply for the current content only. Will take precedence over the main configuration.")
S_ACTION_EX(REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "remove_current_config_override_core", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Remove Core Overrides",
      "Delete the override configuration file which will apply for all content loaded with this core.")
S_ACTION_EX(REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "remove_current_config_override_content_dir", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "Remove Content Directory Overrides",
      "Delete the override configuration file which will apply for all content loaded from the same directory as the current file.")
S_ACTION_EX(REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "remove_current_config_override_game", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, CMD_EVENT_MENU_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Remove Game Overrides",
      "Delete the override configuration file which will apply for the current content only.")
S_ACTION_EX(HELP_LIST,
      "help_list", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Help",
      "Learn more about how the program works.")
/* Descriptor and configuration rows are #ifdef HAVE_QT; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_QT) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(SHOW_WIMP,
      "show_wimp", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_UI_COMPANION_TOGGLE,
      "Show Desktop Menu",
      "Open the traditional desktop menu.")
#endif
