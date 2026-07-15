/* Single-source definitions: first video action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(VIDEO_FULLSCREEN_MODE_SETTINGS,
      "video_fullscreen_mode_settings",
      "Fullscreen Mode",
      "Change fullscreen mode settings.")
S_ACTION(VIDEO_WINDOWED_MODE_SETTINGS,
      "video_windowed_mode_settings",
      "Windowed Mode",
      "Change windowed mode settings.")
