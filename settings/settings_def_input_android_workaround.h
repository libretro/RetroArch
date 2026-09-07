/* Single-source definitions: Android disconnect workaround setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef ANDROID; the string
 * tables always carry this row via the strings pass. */
#if defined(ANDROID) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(android_input_disconnect_workaround, ANDROID_INPUT_DISCONNECT_WORKAROUND,
      "android_input_disconnect_workaround",
      false, SD_FLAG_NONE, 0, 0,
      "Android disconnect workaround",
      "Workaround for controllers disconnecting and reconnecting. Impedes 2 players with the identical controllers.")
#endif
