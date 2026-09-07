/* Single-source definitions: WASAPI group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_WASAPI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_WASAPI) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(audio_wasapi_exclusive_mode, AUDIO_WASAPI_EXCLUSIVE_MODE,
      "audio_wasapi_exclusive_mode",
      DEFAULT_WASAPI_EXCLUSIVE_MODE, SD_FLAG_NONE, 0, 0,
      "WASAPI Exclusive Mode",
      "Allow the WASAPI driver to take exclusive control of the audio device. If disabled, it will use shared mode instead.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_WASAPI; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_WASAPI) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(audio_wasapi_sh_buffer_length, AUDIO_WASAPI_SH_BUFFER_LENGTH,
      "audio_wasapi_sh_buffer_length",
      DEFAULT_WASAPI_SH_BUFFER_LENGTH, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32.0f * 200, 32.0f, 0, setting_action_ok_uint_special, setting_get_string_representation_uint_audio_wasapi_sh_buffer_length, NULL, NULL, NULL, NULL, 0,
      "WASAPI Shared Buffer Length",
      "The intermediate buffer length (in frames) when using the WASAPI driver in shared mode.")
#endif
