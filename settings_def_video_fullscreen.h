/* Single-source definitions: video fullscreen and windowed-fullscreen group.
 * Grammar identical to settings_def_video_sync.h: fixed-arity C89
 * S_BOOL/S_UINT/S_INT rows; the descriptor argument span matches
 * SDESC_<kind>_ROW; row order is menu display order; h2json.py
 * parses these rows for the Crowdin source upload. */

S_BOOL(video_fullscreen, VIDEO_FULLSCREEN,
      "video_fullscreen",
      DEFAULT_FULLSCREEN, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_REINIT_FROM_TOGGLE,
      "Fullscreen Display",
      "Display in fullscreen. Can be changed at runtime. Can be overridden by a command line switch.")
S_BOOL(video_windowed_fullscreen, VIDEO_WINDOWED_FULLSCREEN,
      "video_windowed_fullscreen",
      DEFAULT_WINDOWED_FULLSCREEN, SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_NONE,
      "Windowed Fullscreen Mode",
      "If fullscreen, prefer using a fullscreen window to prevent display mode switching.")
S_UINT(video_fullscreen_x, VIDEO_FULLSCREEN_X,
      "video_fullscreen_x",
      DEFAULT_FULLSCREEN_X, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 7680, 8, 0, setting_action_ok_uint_special, NULL,
      "Fullscreen Width",
      "Set the custom width size for the non-windowed fullscreen mode. Leaving it unset will use the desktop resolution.")
S_UINT(video_fullscreen_y, VIDEO_FULLSCREEN_Y,
      "video_fullscreen_y",
      DEFAULT_FULLSCREEN_Y, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 4320, 8, 0, setting_action_ok_uint_special, NULL,
      "Fullscreen Height",
      "Set the custom height size for the non-windowed fullscreen mode. Leaving it unset will use the desktop resolution.")
