/* Single-source definitions: shader enable and preset-reference group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_shader_enable, VIDEO_SHADERS_ENABLE,
      "video_shader_enable",
      DEFAULT_SHADER_ENABLE, SD_FLAG_NONE, 0, 0,
      "Video Shaders",
      "Enable video shader pipeline.")
/* config key "video_shader_preset_save_reference_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(video_shader_preset_save_reference_enable, VIDEO_SHADER_PRESET_SAVE_REFERENCE,
      "video_shader_preset_save_reference",
      DEFAULT_VIDEO_SHADER_PRESET_SAVE_REFERENCE_ENABLE, SD_FLAG_NONE, 0, 0,
      "Simple Presets",
      "Save a shader preset which has a link to the original preset loaded and includes only the parameter changes you made.")
#endif
