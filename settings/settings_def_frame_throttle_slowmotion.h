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
      "How long, in milliseconds, a controller's press is held back for the rest of a press of several buttons at once to arrive with it. Use it if a pad's simultaneous presses are taken one frame apart (Android only).")
#endif
