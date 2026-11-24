/* Single-source definitions: twelfth main menu action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION_EX(MIDI_SETTINGS,
      "midi_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "MIDI",
      "Change MIDI settings.")

S_ACTION_EX(CAMERA_SETTINGS,
      "camera_settings", SD_FLAG_NONE, NULL, NULL, 0,
      "Camera",
      "Change camera settings and check the camera's picture.")

S_ACTION_EX(CAMERA_PREVIEW,
      "camera_preview", SD_FLAG_NONE, NULL, NULL, 0,
      "Preview",
      "Show the camera's live picture while this entry is selected. A running core's camera is shown as the core uses it.")
