/* Single-source definitions: Metal argument buffers toggle.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef __APPLE__
/* Default resolves at build time from GPU-family detection. */
S_BOOL_DF(video_use_metal_arg_buffers, VIDEO_USE_METAL_ARG_BUFFERS,
      "video_use_metal_arg_buffers",
      settings_def_metal_arg_buffers,
      SD_FLAG_NONE, 0, 0, 0, NULL, NULL, NULL, NULL, NULL, 0,
      "Use Metal Argument Buffers",
      "Enable Metal argument buffers for improved performance.")
#endif
