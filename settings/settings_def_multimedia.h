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
