/* Single-source definitions: GameCube and Wii video group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "video_viwidth" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_NS(video_viwidth, VIDEO_VI_WIDTH,
      "video_vi_width",
      DEFAULT_VIDEO_VI_WIDTH, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 640, 720, 2, 0, NULL, NULL,
      "Set VI Screen Width")
#endif
S_BOOL_NS(video_vfilter, VIDEO_VFILTER,
      "video_vfilter",
      DEFAULT_VIDEO_VFILTER, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Deflicker")
S_UINT(video_overscan_correction_top, VIDEO_OVERSCAN_CORRECTION_TOP,
      "video_overscan_correction_top",
      DEFAULT_VIDEO_OVERSCAN_CORRECTION_TOP, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 24, 1, 0, NULL, NULL,
      "Overscan Correction (Top)",
      "Adjust display overscan cropping by reducing image size by specified number of scan lines (taken from top of screen). May introduce scaling artifacts.")
S_UINT(video_overscan_correction_bottom, VIDEO_OVERSCAN_CORRECTION_BOTTOM,
      "video_overscan_correction_bottom",
      DEFAULT_VIDEO_OVERSCAN_CORRECTION_BOTTOM, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 24, 1, 0, NULL, NULL,
      "Overscan Correction (Bottom)",
      "Adjust display overscan cropping by reducing image size by specified number of scan lines (taken from bottom of screen). May introduce scaling artifacts.")
