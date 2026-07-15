/* Single-source definitions: ASIO device action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_ASIO; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ASIO) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(AUDIO_ASIO_CONTROL_PANEL,
      "audio_asio_control_panel", SD_FLAG_NONE, setting_action_asio_control_panel, NULL, 0,
      "Open ASIO Control Panel",
      "Open the ASIO driver control panel to configure device routing and buffer settings.")
#endif
