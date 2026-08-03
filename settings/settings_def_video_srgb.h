/* Single-source definitions: sRGB FBO setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_force_srgb_disable, VIDEO_FORCE_SRGB_DISABLE,
      "video_force_srgb_disable",
      false, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_ADVANCED, 0, CMD_EVENT_REINIT,
      "Force-Disable sRGB FBO",
      "Forcibly disable sRGB FBO support. Some Intel OpenGL drivers on Windows have video problems with sRGB FBOs. Enabling this can work around it.")
