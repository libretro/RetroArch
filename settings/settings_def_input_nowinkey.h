/* Single-source definitions: Windows hotkey passthrough setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
S_BOOL(input_nowinkey_enable, INPUT_NOWINKEY_ENABLE,
      "input_nowinkey_enable",
      false, SD_FLAG_NONE, 0, 0,
      "Disable Windows Hotkeys (Restart required)",
      "Keep Win-key combinations inside the application.")
#endif
