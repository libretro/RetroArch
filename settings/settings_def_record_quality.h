/* Single-source definitions: record quality setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX_NS(video_record_quality, VIDEO_RECORD_QUALITY,
      "video_record_quality",
      RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, RECORD_CONFIG_TYPE_RECORDING_CUSTOM, RECORD_CONFIG_TYPE_RECORDING_APNG, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_record_quality, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Recording Quality")
