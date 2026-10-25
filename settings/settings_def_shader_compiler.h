/* Single-source definitions: shader compiler driver setting.
 * Grammar identical to settings_def_video_sync.h; the descriptor
 * argument span matches SDESC_<kind>_ROW; row order is menu display
 * order; h2json.py parses these rows for the Crowdin source upload.
 * The setting exists only where both compilers are built in. */

#if defined(HAVE_RSLC) && defined(HAVE_GLSLANG)
S_UINT(video_shader_compiler, VIDEO_SHADER_COMPILER,
      "video_shader_compiler",
      DEFAULT_VIDEO_SHADER_COMPILER, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_shader_compiler,
      "Shader Compiler",
      "Compiler for slang shaders: rslc, built into RetroArch, or the system's glslang and SPIRV-Cross. Takes effect the next time a shader preset is loaded.")
#endif
