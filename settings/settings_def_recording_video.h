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
S_BOOL(video_record_game_only, VIDEO_RECORD_GAME_ONLY,
      "video_record_game_only",
      DEFAULT_VIDEO_RECORD_GAME_ONLY, SD_FLAG_NONE, 0, 0,
      "Record Game Only",
      "Leave the menu, notifications, on-screen messages and overlays out of GPU recordings, and leave out the time spent in the menu. When off, recordings capture exactly what is on screen.")
S_BOOL(video_record_hw_encoder, VIDEO_RECORD_HW_ENCODER,
      "video_record_hw_encoder",
      DEFAULT_VIDEO_RECORD_HW_ENCODER, SD_FLAG_NONE, 0, 0,
      "Use Hardware Video Encoder",
      "Encode H.264 with a hardware encoder when one is available, falling back to software (libx264). Applies to the built-in quality presets, not to custom configs.")
S_UINT(video_record_fifo_frames, VIDEO_RECORD_FIFO_FRAMES,
      "video_record_fifo_frames",
      DEFAULT_VIDEO_RECORD_FIFO_FRAMES, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, MIN_VIDEO_RECORD_FIFO_FRAMES, MAX_VIDEO_RECORD_FIFO_FRAMES, 4, MIN_VIDEO_RECORD_FIFO_FRAMES, setting_action_ok_uint, NULL,
      "Recording Video Buffer (Frames)",
      "Frames queued between the game and the encoder. Larger values absorb encoder hiccups but use more memory. Applies to the next recording.")

#if defined(HAVE_PYROWAVE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(pyrowave_discovery_enable, PYROWAVE_DISCOVERY_ENABLE,
      "pyrowave_discovery_enable", false, SD_FLAG_NONE, 0, 0,
      "PyroWave LAN Discovery",
      "Find nearby RetroArch receivers and allow stream requests on this network. Incoming streams require your approval.")
S_BOOL(pyrowave_full_output, PYROWAVE_FULL_OUTPUT,
      "pyrowave_full_output", true, SD_FLAG_NONE, 0, 0,
      "Stream Entire RetroArch",
      "Include menus, notifications and the built-in player with mixed frontend audio. Disable to use the content viewport and core audio. Applies to the next PyroWave stream.")
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(pyrowave_device_name, PYROWAVE_DEVICE_NAME,
      "pyrowave_device_name", "", SD_FLAG_ALLOW_INPUT, 0,
      NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "PyroWave Device Name", "Name shown to other RetroArch devices. Leave empty to use this computer's name.")
S_DIR(directory_pyrowave_shaders, PYROWAVE_SHADER_DIRECTORY,
      "pyrowave_shader_directory", "", DIRECTORY_DEFAULT, SD_FLAG_ADVANCED, 0,
      directory_action_start_generic,
      "PyroWave Shader Directory", "Optional development override. Leave empty to use the shaders embedded in RetroArch.")
#endif
#endif
