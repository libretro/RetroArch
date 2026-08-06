/* Single-source definitions: Dingux IPU aspect setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#if defined(DINGUX)
S_BOOL(video_dingux_ipu_keep_aspect, VIDEO_DINGUX_IPU_KEEP_ASPECT,
      "video_dingux_ipu_keep_aspect",
      DEFAULT_DINGUX_IPU_KEEP_ASPECT, SD_FLAG_NONE, 0, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES,
      "Keep Aspect Ratio",
      "Maintain 1:1 pixel aspect ratios when scaling content via the internal IPU. If disabled, images will be stretched to fill the entire display.")
#endif
