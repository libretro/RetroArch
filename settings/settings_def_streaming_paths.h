/* Single-source definitions: streaming paths group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "video_stream_config" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_NS(path_stream_config, STREAM_CONFIG,
      "stream_config",
      "", SD_FLAG_NONE, 0, "cfg", NULL, 0,
      "Custom Streaming Configuration")
#endif
/* config key "video_stream_url" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_P_NS(path_stream_url, STREAMING_URL,
      "streaming_url",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Stream URL")
#endif
