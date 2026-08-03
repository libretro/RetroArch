/* Single-source definitions: slow-motion ratio setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef ANDROID; the string
 * tables always carry this row via the strings pass. */
#if defined(ANDROID) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_block_timeout, INPUT_BLOCK_TIMEOUT,
      "input_block_timeout",
      DEFAULT_INPUT_BLOCK_TIMEOUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 4, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Input Block Timeout",
      "The number of milliseconds to wait to get a complete input sample. Use it if you have issues with simultaneous button presses (Android only).")
#endif
