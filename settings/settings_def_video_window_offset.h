/* Single-source definitions: window offset group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_WINDOW_OFFSET); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_WINDOW_OFFSET) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(video_window_offset_x, VIDEO_WINDOW_OFFSET_X,
      "video_window_offset_x",
      DEFAULT_WINDOW_OFFSET_X, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -50, 50, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Screen Horizontal Offset",
      "Forces a certain offset horizontally to the video. The offset is applied globally.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_WINDOW_OFFSET); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_WINDOW_OFFSET) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(video_window_offset_y, VIDEO_WINDOW_OFFSET_Y,
      "video_window_offset_y",
      DEFAULT_WINDOW_OFFSET_Y, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -50, 50, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Screen Vertical Offset",
      "Forces a certain offset vertically to the video. The offset is applied globally.")
#endif
