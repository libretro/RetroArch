/* Single-source definitions: recording video-path group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_post_filter_record, VIDEO_POST_FILTER_RECORD,
      "video_post_filter_record",
      DEFAULT_POST_FILTER_RECORD, SD_FLAG_NONE, 0, 0,
      "Use Post Filter Recording",
      "Capture the image after filters (but not shaders) are applied. The video will look as fancy as what you see on your screen.")
S_BOOL(video_gpu_record, VIDEO_GPU_RECORD,
      "video_gpu_record",
      DEFAULT_GPU_RECORD, SD_FLAG_NONE, 0, 0,
      "Use GPU Recording",
      "Record output of GPU shaded material if available.")
