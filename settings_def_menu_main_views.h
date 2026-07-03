/* Single-source definitions: main menu visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_show_load_core, MENU_SHOW_LOAD_CORE,
      "menu_show_load_core",
      DEFAULT_MENU_SHOW_LOAD_CORE, SD_FLAG_NONE, 0, 0,
      "Show 'Load Core'",
      "Show the 'Load Core' option in the Main Menu.")
S_BOOL(menu_show_load_content, MENU_SHOW_LOAD_CONTENT,
      "menu_show_load_content",
      DEFAULT_MENU_SHOW_LOAD_CONTENT, SD_FLAG_NONE, 0, 0,
      "Show 'Load Content'",
      "Show the 'Load Content' option in the Main Menu.")
/* Descriptor and configuration rows are #ifdef HAVE_CDROM; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CDROM) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_load_disc, MENU_SHOW_LOAD_DISC,
      "menu_show_load_disc",
      DEFAULT_MENU_SHOW_LOAD_DISC, SD_FLAG_NONE, 0, 0,
      "Show 'Load Disc'",
      "Show the 'Load Disc' option in the Main Menu.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CDROM; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CDROM) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_dump_disc, MENU_SHOW_DUMP_DISC,
      "menu_show_dump_disc",
      DEFAULT_MENU_SHOW_DUMP_DISC, SD_FLAG_NONE, 0, 0,
      "Show 'Dump Disc'",
      "Show the 'Dump Disc' option in the Main Menu.")
#endif
#ifdef HAVE_CDROM
S_BOOL(menu_show_eject_disc, MENU_SHOW_EJECT_DISC,
      "menu_show_eject_disc",
      DEFAULT_MENU_SHOW_EJECT_DISC, SD_FLAG_NONE, 0, 0,
      "Show 'Eject Disc'",
      "Show the 'Eject Disc' option in the Main Menu.")
#endif
S_BOOL(menu_show_information, MENU_SHOW_INFORMATION,
      "menu_show_information",
      DEFAULT_MENU_SHOW_INFORMATION, SD_FLAG_NONE, 0, 0,
      "Show 'Information'",
      "Show the 'Information' option in the Main Menu.")
S_BOOL(menu_show_configurations, MENU_SHOW_CONFIGURATIONS,
      "menu_show_configurations",
      DEFAULT_MENU_SHOW_CONFIGURATIONS, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'Configuration File'",
      "Show the 'Configuration File' option in the Main Menu.")
/* config key "menu_show_overlays" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_show_overlays, CONTENT_SHOW_OVERLAYS,
      "menu_show_overlay_settings",
      DEFAULT_QUICK_MENU_SHOW_OVERLAYS, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'On-Screen Overlay'",
      "Show the 'On-Screen Overlay' option.")
#endif
/* config key "menu_show_latency" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_show_latency, CONTENT_SHOW_LATENCY,
      "menu_show_latency_settings",
      DEFAULT_QUICK_MENU_SHOW_LATENCY, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'Latency'",
      "Show the 'Latency' option.")
#endif
/* config key "menu_show_rewind" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_show_rewind, CONTENT_SHOW_REWIND,
      "menu_show_rewind_settings",
      DEFAULT_QUICK_MENU_SHOW_REWIND, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'Rewind'",
      "Show the 'Rewind' option.")
#endif
S_BOOL(menu_show_help, MENU_SHOW_HELP,
      "menu_show_help",
      DEFAULT_MENU_SHOW_HELP, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Show 'Help'",
      "Show the 'Help' option in the Main Menu.")
