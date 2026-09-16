/* Single-source definitions: audio sync setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(audio_sync, AUDIO_SYNC,
      "audio_sync",
      DEFAULT_AUDIO_SYNC, SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_NONE,
      "Synchronization",
      "Synchronize audio. Recommended.")

S_BOOL(audio_threaded_pipeline, AUDIO_THREADED_PIPELINE,
      "audio_threaded_pipeline",
      DEFAULT_AUDIO_THREADED_PIPELINE, SD_FLAG_NONE, 0, CMD_EVENT_AUDIO_REINIT,
      "Threaded Pipeline",
      "Resample, filter and mix audio on the audio thread instead of inside each frame. Same latency as the frame-synchronous path at any Audio Latency setting, with rate control measured at the device's own pace and the resampler out of the frame budget. Audio drivers that cannot wake on the device keep the frame-synchronous path.")

S_BOOL(audio_time_stretch, AUDIO_TIME_STRETCH,
      "audio_time_stretch",
      false, SD_FLAG_ADVANCED, 0, CMD_EVENT_AUDIO_REINIT,
      "Pitch-Preserving Speed Changes",
      "Preserve pitch during slow motion and fast-forward. Supports threaded and frame-synchronous playback, including negotiated multichannel audio. Adds processing cost, memory and buffering while enabled. Requires a content rate of 8000-192000 Hz. Unsupported speeds or source formats use ordinary playback. Inline layout changes reset buffered audio. Threaded playback resumes pitch preservation after queued audio drains.")

S_BOOL(audio_time_stretch_lowpass, AUDIO_TIME_STRETCH_LOWPASS,
      "audio_time_stretch_lowpass",
      false, SD_FLAG_ADVANCED, 0, CMD_EVENT_AUDIO_REINIT,
      "Speed-Linked Low-Pass Filter",
      "Soften high frequencies during accelerated playback, with or without Pitch-Preserving Speed Changes. Supports the same content rates and source formats. Adds processing and prepared memory while enabled; normal-speed playback is unfiltered. This optional effect does not replace the resampler's anti-aliasing filter.")

S_BOOL(audio_thread_priority, AUDIO_THREAD_PRIORITY,
      "audio_thread_priority",
      DEFAULT_AUDIO_THREAD_PRIORITY, SD_FLAG_NONE, 0, CMD_EVENT_AUDIO_REINIT,
      "Elevate Audio Thread Priority",
      "Ask the operating system to schedule the audio thread ahead of the rest of the frontend, so a busy frame is less likely to starve the audio device. Lets Audio Latency go lower on systems that grant it; a system that refuses keeps the default priority and nothing else changes. Applies to the audio thread the Threaded Pipeline and core audio callbacks run on.")

S_BOOL(audio_sink_rate_estimation, AUDIO_SINK_RATE_ESTIMATION,
      "audio_sink_rate_estimation",
      true, SD_FLAG_ADVANCED, 0, CMD_EVENT_NONE,
      "Sink Rate Estimation",
      "Measure how fast the audio device really consumes samples against the system clock and trim the resampler by that amount. Every sound card's crystal is a few parts per million off; with Synchronization off nothing else corrects it and the buffer slowly drifts into a glitch no buffer size cures. The correction is tiny and inaudible. With Synchronization on the core already follows the device and nothing is applied. Only drivers that report consumption take part; the overlay shows the rate as 'Sink'.")
