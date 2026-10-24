/* Single-source definitions: builtin multimedia group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "builtin_mediaplayer_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(multimedia_builtin_mediaplayer_enable, USE_BUILTIN_PLAYER,
      "use_builtin_player",
      DEFAULT_BUILTIN_MEDIAPLAYER_ENABLE, SD_FLAG_NONE, 0, 0,
      "Use Built-In Media Player",
      "Show Media Player supported files in File Browser.")
#endif
/* The menu image viewer does not require the legacy core. */
#if defined(HAVE_IMAGEVIEWER) || defined(HAVE_MENU) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "builtin_imageviewer_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(multimedia_builtin_imageviewer_enable, USE_BUILTIN_IMAGE_VIEWER,
      "use_builtin_image_viewer",
      DEFAULT_BUILTIN_IMAGEVIEWER_ENABLE, SD_FLAG_NONE, 0, 0,
      "Use Built-In Image Viewer",
      "Show Image Viewer supported files in File Browser.")
#endif
#endif

S_BOOL(media_player_continuous, MEDIA_PLAYER_CONTINUOUS,
      "media_player_continuous",
      false, SD_FLAG_NONE, 0, 0,
      "Continuous Playback",
      "Play the remaining movies in the current folder in natural filename order. Stop at the end of the folder or on a playback error.")

S_BOOL(media_player_crt_switch, MEDIA_PLAYER_CRT_SWITCH,
      "media_player_crt_switch",
      false, SD_FLAG_NONE, 0, 0,
      "Use CRT SwitchRes for Fullscreen Playback",
      "Use movie dimensions and measured frame rate with the configured CRT SwitchRes engine during fullscreen playback.")

S_BOOL(media_player_refresh_rate, MEDIA_PLAYER_REFRESH_RATE,
      "media_player_refresh_rate",
      false, SD_FLAG_NONE, 0, 0,
      "Match Display Refresh Rate",
      "Use an advertised display refresh rate matching fullscreen movie cadence. Restore the previous mode when playback closes. CRT SwitchRes takes priority.")

S_UINT(media_player_seek_seconds, MEDIA_PLAYER_SEEK_SECONDS,
      "media_player_seek_seconds",
      5, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 600, 1, 0,
      setting_action_ok_uint, NULL,
      "Seek Step (Seconds)",
      "How far fullscreen media controls seek forwards or backwards.")
S_UINT(media_player_controls_timeout, MEDIA_PLAYER_CONTROLS_TIMEOUT,
      "media_player_controls_timeout",
      3, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 60, 1, 0,
      setting_action_ok_uint, NULL,
      "Controls Auto-Hide Delay (Seconds)",
      "Hide idle fullscreen playback controls after this delay. Zero keeps controls visible. Controls remain visible while paused, seeking or in use.")

S_BOOL(media_player_subtitles, MEDIA_PLAYER_SUBTITLES,
      "media_player_subtitles",
      true, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Subtitles",
      "Display a UTF-8 SubRip (.srt) file with the same basename as the active movie.")
S_INT(media_player_subtitle_delay, MEDIA_PLAYER_SUBTITLE_DELAY,
      "media_player_subtitle_delay",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE,
      -600000, 600000, 100, 0, setting_action_ok_uint, NULL,
      "Subtitle Delay (Milliseconds)",
      "Positive values display subtitles later; negative values display them earlier.")
