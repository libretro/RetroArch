/* Single-source definitions: audio format negotiation group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(audio_format_negotiation, AUDIO_FORMAT_NEGOTIATION,
      "audio_format_negotiation",
      DEFAULT_AUDIO_FORMAT_NEGOTIATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, AUDIO_FORMAT_NEGOTIATION_INT16, AUDIO_FORMAT_NEGOTIATION_FLOAT, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_format_negotiation, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Audio Format Negotiation (Hint)",
      "Sample format the audio driver requests from the output device. 'Float' asks for 32-bit floating-point, 'Int16' for 16-bit integer. Only affects drivers that can negotiate the format (WASAPI, DirectSound, XAudio2, ALSA, SDL2); others use their fixed format. A hint only: a driver falls back if the device rejects the requested format. 'Int16' pairs with the 'Resample to Fixed Integer' hint to keep the whole audio path in the integer domain.")
