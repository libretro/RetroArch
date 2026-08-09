/* Single-source definitions: context scaling setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_ODROIDGO2; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ODROIDGO2) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_LV(video_ctx_scaling, VIDEO_CTX_SCALING, VIDEO_RGA_SCALING,
      "video_ctx_scaling",
      DEFAULT_VIDEO_CTX_SCALING, SD_FLAG_NONE, 0, CMD_EVENT_REINIT,
      "RGA Scaling",
      "Hardware context scaling (if available).")
#endif
