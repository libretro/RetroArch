/* Single-source definitions: file browser group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_navigation_browser_filter_supported_extensions_enable, NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "menu_navigation_browser_filter_supported_extensions_enable",
      true, SD_FLAG_NONE, 0, 0,
      "Filter Unknown Extensions",
      "Filter files being shown in File Browser by supported extensions.")
/* config key "filter_by_current_core" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(filter_by_current_core, FILTER_BY_CURRENT_CORE,
      "filter_by_current_Core",
      DEFAULT_FILTER_BY_CURRENT_CORE, SD_FLAG_NONE, 0, 0,
      "Filter by Current Core",
      "Filter files being shown in File Browser by current core.")
#endif
S_BOOL(use_last_start_directory, USE_LAST_START_DIRECTORY,
      "use_last_start_directory",
      DEFAULT_USE_LAST_START_DIRECTORY, SD_FLAG_NONE, 0, 0,
      "Remember Last Used Start Directory",
      "Open File Browser at the last used location when loading content from the Start Directory. Note: Location will be reset to default upon restarting RetroArch.")
/* Descriptor and configuration rows are #ifdef HAVE_DYNAMIC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_DYNAMIC) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(core_suggest_always, CORE_SUGGEST_ALWAYS,
      "core_suggest_always",
      DEFAULT_CORE_SUGGEST_ALWAYS, SD_FLAG_NONE, 0, 0,
      "Always Suggest Cores",
      "Suggest available cores even when a core is manually loaded.")
#endif
