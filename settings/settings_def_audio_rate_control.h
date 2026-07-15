/* Single-source definitions: audio rate control delta.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(audio_rate_control_delta, AUDIO_RATE_CONTROL_DELTA,
      "audio_rate_control_delta",
      DEFAULT_RATE_CONTROL_DELTA, "%.3f", SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0.0, 0.020, 0.001, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Dynamic Audio Rate Control",
      "Helps smooth out imperfections in timing when synchronizing audio and video. Be aware that if disabled, proper synchronization is nearly impossible to obtain.")
