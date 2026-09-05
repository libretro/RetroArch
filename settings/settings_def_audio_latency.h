/* Single-source definitions: audio/microphone latency selectors.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Defaults resolve at build time from frontend-provided values via
 * the _DF variant; range and Lakka-advanced flag are row data. */
S_UINT_DF(audio_latency, AUDIO_LATENCY,
      "audio_latency",
      settings_def_audio_latency,
      SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 512, 1, 0, setting_action_ok_uint, NULL, 0,
      "Audio Latency (ms)",
      "Desired audio latency in milliseconds. Might not be honored if the audio driver can't provide it.")
#ifdef HAVE_MICROPHONE
S_UINT_DF(microphone_latency, MICROPHONE_LATENCY,
      "microphone_latency",
      settings_def_microphone_latency,
      SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 512, 1, 0, setting_action_ok_uint, NULL, 0,
      "Microphone Latency (ms)",
      "Desired microphone latency in milliseconds. Might not be honored if the microphone driver can't provide it.")
#endif
