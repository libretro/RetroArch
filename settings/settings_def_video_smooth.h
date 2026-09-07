/* Single-source definitions: bilinear filtering setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_smooth, VIDEO_SMOOTH,
      "video_smooth",
      DEFAULT_VIDEO_SMOOTH, SD_FLAG_NONE, 0, CMD_EVENT_REINIT,
      "Bilinear Filtering",
      "Add a slight blur to the image to soften hard pixel edges. This option has very little impact on performance. Should be disabled if using shaders.")
