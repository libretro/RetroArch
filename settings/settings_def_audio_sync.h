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
      DEFAULT_AUDIO_THREADED_PIPELINE, SD_FLAG_ADVANCED, 0, CMD_EVENT_AUDIO_REINIT,
      "Threaded Pipeline",
      "Resample, filter and mix audio on a separate thread instead of inside each frame. Lets the audio device run smaller buffers and takes the resampler out of the frame budget. Reinitializes the audio driver when changed.")
