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
S_BOOL(video_record_allow_frame_drop, VIDEO_RECORD_ALLOW_FRAME_DROP,
      "video_record_allow_frame_drop",
      DEFAULT_VIDEO_RECORD_ALLOW_FRAME_DROP, SD_FLAG_NONE, 0, 0,
      "Drop Frames When Encoder Falls Behind",
      "When the recording or streaming encoder can't keep up, skip video frames and fill audio gaps with silence instead of slowing down the game. Disable to keep every frame at the cost of stutter.")
S_UINT(video_record_fifo_frames, VIDEO_RECORD_FIFO_FRAMES,
      "video_record_fifo_frames",
      DEFAULT_VIDEO_RECORD_FIFO_FRAMES, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, MIN_VIDEO_RECORD_FIFO_FRAMES, MAX_VIDEO_RECORD_FIFO_FRAMES, 4, MIN_VIDEO_RECORD_FIFO_FRAMES, setting_action_ok_uint, NULL,
      "Recording Video Buffer (Frames)",
      "Frames queued between the game and the encoder. Larger values absorb encoder hiccups but use more memory. Applies to the next recording.")
