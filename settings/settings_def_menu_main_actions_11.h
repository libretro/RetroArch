/* Single-source definitions: eleventh main menu action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION_EX(RECORDING_SETTINGS,
      "recording_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Recording",
      "Change recording settings.")
