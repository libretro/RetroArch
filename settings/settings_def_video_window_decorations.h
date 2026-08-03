/* Single-source definitions: window decorations setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Everywhere but macOS, toggling decorations needs a video reinit;
 * two row variants keep that in the row instead of builder code. */
#if defined(OSX) && !defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(video_window_show_decorations, VIDEO_WINDOW_SHOW_DECORATIONS,
      "video_window_show_decorations",
      DEFAULT_WINDOW_DECORATIONS, SD_FLAG_NONE, 0, 0,
      "Show Window Decorations",
      "Show window title bar and borders.")
#else
S_BOOL(video_window_show_decorations, VIDEO_WINDOW_SHOW_DECORATIONS,
      "video_window_show_decorations",
      DEFAULT_WINDOW_DECORATIONS, SD_FLAG_NONE, 0, CMD_EVENT_REINIT,
      "Show Window Decorations",
      "Show window title bar and borders.")
#endif
