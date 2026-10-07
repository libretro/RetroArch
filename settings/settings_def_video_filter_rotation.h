/* Single-source definitions: video filter and rotation group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_gpu_screenshot, VIDEO_GPU_SCREENSHOT,
      "video_gpu_screenshot",
      DEFAULT_GPU_SCREENSHOT, SD_FLAG_ADVANCED, 0, CMD_EVENT_NONE,
      "Screenshot: Use GPU",
      "Screenshots capture GPU shaded material if available.")
S_BOOL(video_crop_overscan, VIDEO_CROP_OVERSCAN,
      "video_crop_overscan",
      DEFAULT_CROP_OVERSCAN, SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_NONE,
      "Crop Overscan (Restart required)",
      "Cut off a few pixels around the edges of the image customarily left blank by developers which sometimes also contain garbage pixels.")
/* Descriptor and configuration rows are #ifdef HAVE_VIDEO_FILTER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_VIDEO_FILTER) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "video_filter_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(video_filter_enable, VIDEO_FILTER_ENABLE,
      "filter_enable",
      true, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Video Filter Enable",
      "Apply Video Filter. Is a hint that does not necessarily have to be honored by the video driver.")
#endif
S_UINT(video_filter_threads, VIDEO_FILTER_THREADS,
      "video_filter_threads",
      DEFAULT_VIDEO_FILTER_THREADS, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, MAXIMUM_VIDEO_FILTER_THREADS, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_filter_threads,
      "Video Filter Threads",
      "How many threads a CPU video filter may run on. 'Automatic' uses the cores left after the emulation, video, audio and task threads have theirs, at most 8, and fewer for light filters that run slower when spread out. Heavy filters such as NTSC gain the most from more threads. A running filter picks up the change straight away.")
#endif
