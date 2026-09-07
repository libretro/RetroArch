/* Single-source definitions: stream quality setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX_NS(video_stream_quality, VIDEO_STREAM_QUALITY,
      "video_stream_quality",
      RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, RECORD_CONFIG_TYPE_STREAMING_CUSTOM, RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY, 1, RECORD_CONFIG_TYPE_STREAMING_CUSTOM, setting_action_ok_uint, setting_get_string_representation_video_stream_quality, NULL, NULL, NULL, NULL, 0,
      "Streaming Quality")
