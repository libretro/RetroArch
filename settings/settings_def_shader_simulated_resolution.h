/* Single-source definitions: simulated input resolution for slang.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are HAVE_SLANG only; the string
 * tables always carry these rows via the strings pass. A change
 * reloads the preset: the size is taken when the passes are built. */
#if defined(HAVE_SLANG) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(video_shader_simulated_input_resolution, VIDEO_SHADER_SIMULATED_INPUT_RES,
      "video_shader_simulated_input_resolution",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION, SD_FLAG_CMD_APPLY_AUTO, SDESC_FLG_REFRESH, CMD_EVENT_SHADERS_APPLY_CHANGES,
      "Simulated Input Resolution",
      "Tell the first shader pass the image has the size below rather than its real size, so resolution-sensitive shaders behave on large images as they do on small ones.")
S_UINT(video_shader_simulated_input_resolution_width, VIDEO_SHADER_SIMULATED_INPUT_RES_WIDTH,
      "video_shader_simulated_input_resolution_width",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_SHADERS_APPLY_CHANGES, MIN_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, MAX_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, 1, 0, setting_action_ok_uint, NULL,
      "Simulated Width",
      "Width the first shader pass is told the image has.")
S_UINT(video_shader_simulated_input_resolution_height, VIDEO_SHADER_SIMULATED_INPUT_RES_HEIGHT,
      "video_shader_simulated_input_resolution_height",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_SHADERS_APPLY_CHANGES, MIN_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, MAX_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, 1, 0, setting_action_ok_uint, NULL,
      "Simulated Height",
      "Height the first shader pass is told the image has.")
#endif
