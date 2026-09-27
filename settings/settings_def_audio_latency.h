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
S_UINT(audio_latency_floor, AUDIO_LATENCY_FLOOR,
      "audio_latency_floor",
      DEFAULT_AUDIO_LATENCY_FLOOR, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, 16, 1, 0,
      setting_action_ok_uint, NULL,
      "Minimum Audio Latency (ms)",
      "The lowest audio latency RetroArch will ask a driver for. Eight milliseconds by default, which is where this was fixed for a long time: a setting of zero used to reach the drivers and they handled it inconsistently. Drivers that talk to the device directly - WASAPI in exclusive mode, ASIO, WDM-KS - can often negotiate a shorter period than that, so lowering this lets them. A driver that cannot go lower will not; it keeps its own hardware floor either way.")
#ifdef HAVE_MICROPHONE
S_UINT_DF(microphone_latency, MICROPHONE_LATENCY,
      "microphone_latency",
      settings_def_microphone_latency,
      SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 512, 1, 0, setting_action_ok_uint, NULL, 0,
      "Microphone Latency (ms)",
      "Desired microphone latency in milliseconds. Might not be honored if the microphone driver can't provide it.")
#endif
