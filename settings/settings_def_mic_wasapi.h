/* Single-source definitions: microphone WASAPI group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_MICROPHONE
#ifdef HAVE_WASAPI
S_BOOL(microphone_wasapi_exclusive_mode, MICROPHONE_WASAPI_EXCLUSIVE_MODE,
      "microphone_wasapi_exclusive_mode",
      DEFAULT_WASAPI_EXCLUSIVE_MODE, SD_FLAG_NONE, 0, 0,
      "WASAPI Exclusive Mode",
      "Allow RetroArch to take exclusive control of the microphone device when using the WASAPI microphone driver. If disabled, RetroArch will use shared mode instead.")
#endif
#endif
#ifdef HAVE_MICROPHONE
#ifdef HAVE_WASAPI
S_BOOL(microphone_wasapi_float_format, MICROPHONE_WASAPI_FLOAT_FORMAT,
      "microphone_wasapi_float_format",
      DEFAULT_WASAPI_FLOAT_FORMAT, SD_FLAG_NONE, 0, 0,
      "WASAPI Float Format",
      "Use floating-point input for the WASAPI driver, if supported by your audio device.")
#endif
#endif
#ifdef HAVE_MICROPHONE
#ifdef HAVE_WASAPI
S_UINT_EX(microphone_wasapi_sh_buffer_length, MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
      "microphone_wasapi_sh_buffer_length",
      DEFAULT_WASAPI_MICROPHONE_SH_BUFFER_LENGTH, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 32.0f * 200, 32.0f, 0, setting_action_ok_uint_special, setting_get_string_representation_uint_microphone_wasapi_sh_buffer_length, NULL, NULL, NULL, NULL, 0,
      "WASAPI Shared Buffer Length",
      "The intermediate buffer length (in frames) when using the WASAPI driver in shared mode.")
#endif
#endif
