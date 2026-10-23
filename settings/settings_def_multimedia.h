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
/* Descriptor and configuration rows are #ifdef HAVE_IMAGEVIEWER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_IMAGEVIEWER) || defined(SETTINGS_DEF_STRINGS_PASS)
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
