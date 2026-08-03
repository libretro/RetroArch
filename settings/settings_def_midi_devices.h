/* Single-source definitions: MIDI input/output device selectors.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if !defined(RARCH_CONSOLE)
/* Device pickers: start/left/right/ok cycle the enumerated MIDI
 * devices, so those four handlers ride in the row. */
S_STRING(midi_input, MIDI_INPUT,
      "midi_input",
      DEFAULT_MIDI_INPUT, SD_FLAG_NONE, 0, setting_string_action_ok_midi_device, NULL, setting_string_action_start_midi_device, NULL, setting_string_action_left_midi_input, setting_string_action_right_midi_input, 0,
      "Input",
      "Select input device.")
S_STRING(midi_output, MIDI_OUTPUT,
      "midi_output",
      DEFAULT_MIDI_OUTPUT, SD_FLAG_NONE, 0, setting_string_action_ok_midi_device, NULL, setting_string_action_start_midi_device, NULL, setting_string_action_left_midi_output, setting_string_action_right_midi_output, 0,
      "Output",
      "Select output device.")
#endif
