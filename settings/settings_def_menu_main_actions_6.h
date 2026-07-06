/* Single-source definitions: sixth main menu action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(NETPLAY,
      "netplay",
      "Netplay",
      "Join or host a netplay session.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(ONLINE_UPDATER,
      "online_updater",
      "Online Updater",
      "Download add-ons, components, and content for RetroArch.")
#endif
#ifdef HAVE_MIST
S_ACTION(CORE_MANAGER_STEAM_LIST,
      "core_manager_steam_list",
      "Manage Cores",
      "Install or uninstall cores distributed through Steam.")
#endif
S_ACTION(SETTINGS,
      "settings",
      "Settings",
      "Configure the program.")
S_ACTION_NS(INFORMATION_LIST,
      "information_list",
      "Information")
