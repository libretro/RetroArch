/* Single-source definitions: adaptive vsync setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_adaptive_vsync, VIDEO_ADAPTIVE_VSYNC,
      "video_adaptive_vsync",
      DEFAULT_ADAPTIVE_VSYNC, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Adaptive VSync",
      "VSync is enabled until performance falls below the target refresh rate. Can minimize stuttering when performance falls below real time, and be more energy efficient. Not compatible with 'Frame Delay'.")
