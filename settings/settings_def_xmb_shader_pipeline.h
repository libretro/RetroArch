/* Single-source definitions: XMB shader pipeline setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_XMB #if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL) #ifdef HAVE_SHADERPIPELINE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_XMB) && (defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)) && defined(HAVE_SHADERPIPELINE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "menu_shader_pipeline" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(menu_xmb_shader_pipeline, XMB_RIBBON_ENABLE,
      "xmb_ribbon_enable",
      DEFAULT_MENU_SHADER_PIPELINE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, XMB_SHADER_PIPELINE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_xmb_shader_pipeline, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Shader Pipeline",
      "Select an animated background effect. Can be GPU-intensive depending on the effect. If performance is unsatisfactory, either turn this off or revert to a simpler effect.")
#endif
#endif
