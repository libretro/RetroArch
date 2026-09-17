/* Single-source definitions: auto mouse grab and background
 * controller input settings.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(input_auto_mouse_grab, INPUT_AUTO_MOUSE_GRAB,
      "input_auto_mouse_grab",
      DEFAULT_INPUT_AUTO_MOUSE_GRAB, SD_FLAG_NONE, 0, 0,
      "Automatic Mouse Grab",
      "Enable mouse grab on application focus.")
S_BOOL(input_joypad_background, INPUT_JOYPAD_BACKGROUND,
      "input_joypad_background",
      DEFAULT_INPUT_JOYPAD_BACKGROUND, SD_FLAG_NONE, 0, 0,
      "Background Controller Input",
      "Accept controller input while RetroArch is not the active window. When disabled, controllers are ignored while unfocused: the menu, hotkeys and running content do not react to them.")
