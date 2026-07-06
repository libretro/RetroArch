/* Single-source definitions: resampler quality group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(audio_resampler_quality, AUDIO_RESAMPLER_QUALITY,
      "audio_resampler_quality",
      DEFAULT_AUDIO_RESAMPLER_QUALITY_LEVEL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, RESAMPLER_QUALITY_DONTCARE, RESAMPLER_QUALITY_HIGHEST, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_resampler_quality, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Resampler Quality",
      "Lower this value to favor performance/lower latency over audio quality, increase for better audio quality at the expense of performance/lower latency.")
S_BOOL(audio_fastpath_s16, AUDIO_FASTPATH_S16,
      "audio_fastpath_s16",
      DEFAULT_AUDIO_FASTPATH_S16, SD_FLAG_ADVANCED, 0, CMD_EVENT_NONE,
      "Resample to Fixed Integer (Hint)",
      "Use the fixed-point (integer) resampler instead of the floating-point one when a core outputs 16-bit audio. Avoids the integer-to-float round-trip and produces bit-identical output on every platform, which helps netplay determinism. Has no effect on cores that output floating-point audio, and falls back to the floating-point path while an incompatible DSP filter is active.")
