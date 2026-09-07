/* Single-source definitions: direct SPIR-V shader ingestion setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_gl_direct_spirv, VIDEO_GL_DIRECT_SPIRV,
      "video_gl_direct_spirv",
      DEFAULT_VIDEO_GL_DIRECT_SPIRV, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Direct SPIR-V Support (Hint)",
      "Hand shaders to the graphics driver as SPIR-V instead of cross-compiling them to GLSL first, which can shorten shader preset loading times. This is only a hint: it requires the 'GL_ARB_gl_spirv' driver extension, and any shader or driver that cannot use it silently falls back to cross-compilation. Takes effect the next time a shader preset is loaded.")
