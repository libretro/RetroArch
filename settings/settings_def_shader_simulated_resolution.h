/* Single-source definitions: shader simulated input resolution group.
 * These settings override the SourceSize uniform seen by slang shaders. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_H
#define S_BOOL_H S_BOOL
#endif
#endif

S_BOOL(video_shader_simulated_input_resolution, VIDEO_SHADER_SIMULATED_INPUT_RES,
      "video_shader_simulated_input_resolution",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION, SD_FLAG_NONE, 0, CMD_EVENT_SHADERS_APPLY_CHANGES,
      "Use Simulated Resolution",
      "Override the SourceSize uniform with custom dimensions for slang shaders.")
S_UINT(video_shader_simulated_input_resolution_width, VIDEO_SHADER_SIMULATED_INPUT_RES_WIDTH,
      "video_shader_simulated_input_resolution_width",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, MIN_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, MAX_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_WIDTH, 1, 0, setting_action_ok_uint, NULL,
      "Simulated Width",
      "Custom width used for the SourceSize uniform when simulated resolution is enabled.")
S_UINT(video_shader_simulated_input_resolution_height, VIDEO_SHADER_SIMULATED_INPUT_RES_HEIGHT,
      "video_shader_simulated_input_resolution_height",
      DEFAULT_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, MIN_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, MAX_VIDEO_SHADER_SIMULATED_INPUT_RESOLUTION_HEIGHT, 1, 0, setting_action_ok_uint, NULL,
      "Simulated Height",
      "Custom height used for the SourceSize uniform when simulated resolution is enabled.")
