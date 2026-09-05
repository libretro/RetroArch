/* Single-source definitions: Dingux IPU group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if defined(DINGUX)
S_UINT_EX(video_dingux_ipu_filter_type, VIDEO_DINGUX_IPU_FILTER_TYPE,
      "video_dingux_ipu_filter_type",
      DEFAULT_DINGUX_IPU_FILTER_TYPE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, DINGUX_IPU_FILTER_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_dingux_ipu_filter_type, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Image Interpolation",
      "Specify image interpolation method when scaling content via the internal IPU. 'Bicubic' or 'Bilinear' is recommended when using CPU-powered video filters. This option has no performance impact.")
#endif
