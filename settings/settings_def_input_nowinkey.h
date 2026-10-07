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
#if defined(_WIN32) && defined(HAVE_WINRAWINPUT)
S_BOOL(input_winraw_xinput_enable, INPUT_WINRAW_XINPUT_ENABLE,
      "input_winraw_xinput_enable",
      false, SD_FLAG_NONE, 0, 0,
      "XInput for Xbox Controllers (Restart required)",
      "With the 'raw' controller driver, read Xbox-compatible controllers through XInput: separate triggers and the XInput button layout. Other controllers are still read by raw input.")
#endif
#if defined(_WIN32) && defined(HAVE_WINRAWINPUT)
S_BOOL(input_winraw_player_lights, INPUT_WINRAW_PLAYER_LIGHTS,
      "input_winraw_player_lights",
      false, SD_FLAG_NONE, 0, 0,
      "Controller Player Lights",
      "With the 'raw' controller driver, show each controller's port on its own lights: the player lights of a DualSense, the light bar's colour of a DualShock 4 (blue, red, green, pink). They follow the controller when its port changes, and go out when this is turned off.")
#endif
