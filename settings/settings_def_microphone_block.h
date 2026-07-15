/* Single-source definitions: microphone block frames setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_MICROPHONE
#ifdef RARCH_MOBILE
S_UINT_NS(microphone_block_frames, MICROPHONE_BLOCK_FRAMES,
      "microphone_block_frames",
      0, SD_FLAG_ADVANCED, 0, 0, 0, 0, 0, 0, NULL, NULL,
      "Microphone Block Frames")
#endif
#endif
