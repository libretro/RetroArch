/* Single-source definitions: turbo fire group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(input_auto_game_focus, INPUT_AUTO_GAME_FOCUS,
      "input_auto_game_focus",
      DEFAULT_INPUT_AUTO_GAME_FOCUS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, AUTO_GAME_FOCUS_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_input_auto_game_focus, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Auto Enable 'Game Focus' Mode",
      "Always enable 'Game Focus' mode when launching and resuming content. When set to 'Detect', option will be enabled if current core implements frontend keyboard callback functionality.")
S_BOOL(pause_on_disconnect, PAUSE_ON_DISCONNECT,
      "pause_on_disconnect",
      DEFAULT_PAUSE_ON_DISCONNECT, SD_FLAG_NONE, 0, 0,
      "Pause Content on Controller Disconnect",
      "Pause content when any controller is disconnected. Resume with Start.")
