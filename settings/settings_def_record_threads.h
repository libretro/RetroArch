/* Single-source definitions: record threads setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX_NS(video_record_threads, VIDEO_RECORD_THREADS,
      "video_record_threads",
      DEFAULT_VIDEO_RECORD_THREADS, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, 8, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Recording Threads")
