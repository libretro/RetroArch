/* Single-source definitions: MIDI volume setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if !defined(RARCH_CONSOLE); the string
 * tables always carry this row via the strings pass. */
#if !defined(RARCH_CONSOLE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(midi_volume, MIDI_VOLUME,
      "midi_volume",
      DEFAULT_MIDI_VOLUME, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0f, 100.0f, 1.0f, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Volume",
      "Set output volume (%).")
#endif
