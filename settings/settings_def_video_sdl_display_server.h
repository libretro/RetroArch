/* Single-source definitions: SDL display server setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(video_sdl_display_server, VIDEO_SDL_DISPLAY_SERVER,
      "video_sdl_display_server",
      DEFAULT_VIDEO_SDL_DISPLAY_SERVER, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, VIDEO_SDL_DISPLAY_SERVER_OFF, VIDEO_SDL_DISPLAY_SERVER_ALWAYS, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_sdl_display_server, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "SDL Display Mode Switching",
      "Let the SDL window switch among the display modes it lists. 'Auto' uses it only when the native display server cannot switch modes. 'Always' overrides the native server: CRT SwitchRes then picks from the listed modes, including ones without known timings, and custom CRT timings are unavailable.")
