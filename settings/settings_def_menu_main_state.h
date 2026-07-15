/* Single-source definitions: main menu state group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_INT_EX(state_slot, STATE_SLOT,
      "state_slot",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -1, 999, 1, -1, setting_action_ok_uint, setting_get_string_representation_state_slot, NULL, NULL, NULL, NULL, 0,
      "State Slot",
      "Change the currently selected state slot.")
/* Descriptor and configuration rows are #ifdef HAVE_BSV_MOVIE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_BSV_MOVIE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(replay_slot, REPLAY_SLOT,
      "replay_slot",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -1, 999, 1, -1, setting_action_ok_uint, setting_get_string_representation_state_slot, NULL, NULL, NULL, NULL, 0,
      "Replay Slot",
      "Change the currently selected state slot.")
#endif
S_ACTION(START_CORE,
      "start_core",
      "Start Core",
      "Start core without content.")
/* Descriptor and configuration rows are #if defined(HAVE_VIDEOPROCESSOR); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_VIDEOPROCESSOR) || defined(SETTINGS_DEF_STRINGS_PASS)
/* FIXME Maybe add a description? */
S_ACTION_NS(START_VIDEO_PROCESSOR,
      "menu_start_video_processor",
      "Start Video Processor")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* FIXME Maybe add a description? */
S_ACTION_NS(START_NET_RETROPAD,
      "menu_start_net_retropad",
      "Start Remote RetroPad")
#endif
S_ACTION(CONTENT_SETTINGS,
      "quick_menu",
      "Quick Menu",
      "Quickly access all relevant in-game settings.")
S_ACTION(XMB_MAIN_MENU_ENABLE_SETTINGS,
      "xmb_main_menu_enable_settings",
      "Enable Settings Tab",
      "Show the Settings tab containing program settings.")
S_ACTION(MENU_DISABLE_KIOSK_MODE,
      "menu_disable_kiosk_mode",
      "Disable Kiosk Mode",
      "Show all configuration related settings.")
