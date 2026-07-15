/* Single-source definitions: auto mouse grab setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(input_auto_mouse_grab, INPUT_AUTO_MOUSE_GRAB,
      "input_auto_mouse_grab",
      DEFAULT_INPUT_AUTO_MOUSE_GRAB, SD_FLAG_NONE, 0, 0,
      "Automatic Mouse Grab",
      "Enable mouse grab on application focus.")
