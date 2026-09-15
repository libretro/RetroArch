/* Single-source definitions: shader delay setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(video_shader_delay, VIDEO_SHADER_DELAY,
      "video_shader_delay",
      DEFAULT_SHADER_DELAY, SD_FLAG_ADVANCED, SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN, 0, 0, 0, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Auto-Shader Delay",
      "Delay auto-loading shaders (in ms). Can work around graphical glitches when using 'screen grabbing' software.")
#endif
